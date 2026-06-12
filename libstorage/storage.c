/*
 * Phoenix-RTOS
 *
 * Storage devices
 *
 * Copyright 2021-2022 Phoenix Systems
 * Author: Lukasz Kosinski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/list.h>
#include <sys/rb.h>
#include <sys/threads.h>

#include <posix/idtree.h>

#include "include/storage/storage.h"


#define REQTHR_PRIORITY  1
#define POOLTHR_PRIORITY 1

/* clang-format off */
enum { state_exit = -1, state_stop, state_run };
/* clang-format on */


typedef struct {
	char name[16];           /* Filesystem name */
	storage_mount_t mount;   /* Filesystem mount */
	storage_umount_t umount; /* Filesystem umount */
	rbnode_t node;           /* RB tree node */
} storage_fsHandler_t;


typedef struct _storage_fsctx_t storage_fsctx_t;


typedef struct _request_ctx_t {
	int state;                                  /* Context state */
	unsigned int port;                          /* Context port */
	unsigned int nreqs;                         /* Number of actively processed requests */
	void (*msgHandler)(void *data, msg_t *msg); /* Message handler */
	void *data;                                 /* Message handling data */
	handle_t scond;                             /* Condition: nreqs==0 or state changed */
	handle_t lock;                              /* Context mutex */
	struct _request_ctx_t *prev, *next;         /* Context registry list */
	char stack[4 * 512] __attribute__((aligned(8)));
} request_ctx_t;


struct _storage_fsctx_t {
	request_ctx_t reqctx;         /* Filesystem requests context */
	storage_fsHandler_t *handler; /* Filesystem data */
};


static struct {
	idtree_t strgs;      /* Storages */
	rbtree_t fss;        /* Registered filesystems */
	unsigned int wport;  /* Worker pool port */
	request_ctx_t *ctxs; /* Active context registry (protected by lock) */
	handle_t lock;       /* Storage handling mutex */
	request_ctx_t ctx;   /* Storage devices requests context */
} storage_common;


static void storage_reqthr(void *arg)
{
	request_ctx_t *ctx = (request_ctx_t *)arg;
	msg_t msg;
	msg_rid_t rid;
	int err;

	for (;;) {
		err = msgRecv(ctx->port, &msg, &rid);
		if (err < 0) {
			if (err == -EINVAL) {
				/* Port closed - exit */
				endthread();
			}
			continue;
		}

		mutexLock(ctx->lock);
		while (ctx->state == state_stop)
			condWait(ctx->scond, ctx->lock, 0);

		if (ctx->state == state_exit) {
			mutexUnlock(ctx->lock);
			msg.o.err = -EINVAL;
			(void)msgRespond(ctx->port, &msg, rid);
			endthread();
		}

		ctx->nreqs++;
		mutexUnlock(ctx->lock);

		/* Forward to worker pool; msgSend blocks until a poolthr processes and responds */
		/* TODO: replace this with nonblocking send OR MAYBE DELEGATE THE reply cap
		 * or whatever that sel4 crap is so that the worker thread does the below
		 * msgRespond instead
		 *
		 * ooooh maybe msgForward?
		 * */
		err = msgSend(storage_common.wport, &msg);
		if (err < 0)
			msg.o.err = err;

		(void)msgRespond(ctx->port, &msg, rid);

		mutexLock(ctx->lock);
		if (--ctx->nreqs == 0)
			condSignal(ctx->scond);
		mutexUnlock(ctx->lock);
	}
}


static request_ctx_t *storage_ctx_find(unsigned int port)
{
	request_ctx_t *ctx;

	mutexLock(storage_common.lock);
	ctx = storage_common.ctxs;
	if (ctx != NULL) {
		do {
			if (ctx->port == port) {
				mutexUnlock(storage_common.lock);
				return ctx;
			}
			ctx = ctx->next;
		} while (ctx != storage_common.ctxs);
	}
	mutexUnlock(storage_common.lock);
	return NULL;
}


static void storage_poolthr(void *arg)
{
	request_ctx_t *ctx;
	msg_t msg;
	msg_rid_t rid;
	int err;

	for (;;) {
		err = msgRecv(storage_common.wport, &msg, &rid);
		if (err < 0) {
			if (err == -EINVAL) {
				/* Worker port closed - exit */
				endthread();
			}
			continue;
		}

		ctx = storage_ctx_find(msg.oid.port);
		if (ctx == NULL) {
			msg.o.err = -ENODEV;
			msgRespond(storage_common.wport, &msg, rid);
			continue;
		}

		ctx->msgHandler(ctx->data, &msg);

		msgRespond(storage_common.wport, &msg, rid);
	}
}


static void requestctx_run(request_ctx_t *ctx)
{
	mutexLock(ctx->lock);
	ctx->state = state_run;
	mutexUnlock(ctx->lock);
	condBroadcast(ctx->scond);
}


static void requestctx_stop(request_ctx_t *ctx)
{
	mutexLock(ctx->lock);

	ctx->state = state_stop;
	while (ctx->nreqs)
		condWait(ctx->scond, ctx->lock, 0);

	mutexUnlock(ctx->lock);
}


static void requestctx_done(request_ctx_t *ctx)
{
	requestctx_stop(ctx);

	mutexLock(storage_common.lock);
	LIST_REMOVE(&storage_common.ctxs, ctx);
	mutexUnlock(storage_common.lock);

	mutexLock(ctx->lock);
	portDestroy(ctx->port);
	ctx->state = state_exit;
	condBroadcast(ctx->scond);
	mutexUnlock(ctx->lock);

	while (threadJoin(-1, 10000) < 0)
		condBroadcast(ctx->scond);

	resourceDestroy(ctx->scond);
	resourceDestroy(ctx->lock);
}


static int storagectx_init(request_ctx_t *ctx, void (*msgHandler)(void *data, msg_t *msg))
{
	int err;

	err = mutexCreate(&ctx->lock);
	if (err < 0)
		return err;

	err = condCreate(&ctx->scond);
	if (err < 0) {
		resourceDestroy(ctx->lock);
		return err;
	}

	err = portCreate(&ctx->port);
	if (err < 0) {
		resourceDestroy(ctx->scond);
		resourceDestroy(ctx->lock);
		return err;
	}

	ctx->msgHandler = msgHandler;
	ctx->data = NULL;
	ctx->nreqs = 0;
	ctx->state = state_stop;
	ctx->prev = ctx->next = NULL;

	mutexLock(storage_common.lock);
	LIST_ADD(&storage_common.ctxs, ctx);
	mutexUnlock(storage_common.lock);

	err = beginthread(storage_reqthr, REQTHR_PRIORITY, ctx->stack, sizeof(ctx->stack), ctx);
	if (err < 0) {
		mutexLock(storage_common.lock);
		LIST_REMOVE(&storage_common.ctxs, ctx);
		mutexUnlock(storage_common.lock);
		portDestroy(ctx->port);
		resourceDestroy(ctx->scond);
		resourceDestroy(ctx->lock);
		return err;
	}

	return EOK;
}


storage_t *storage_get(int id)
{
	return lib_treeof(storage_t, node, idtree_find(&storage_common.strgs, id));
}


static storage_fsHandler_t *storage_getfs(const char *name)
{
	storage_fsHandler_t fs;

	strncpy(fs.name, name, sizeof(fs.name));
	fs.name[sizeof(fs.name) - 1] = '\0';

	return lib_treeof(storage_fsHandler_t, node, lib_rbFind(&storage_common.fss, &fs.node));
}


int storage_registerfs(const char *name, storage_mount_t mount, storage_umount_t umount)
{
	storage_fsHandler_t *handler;

	if ((name == NULL) || (mount == NULL) || (umount == NULL))
		return -EINVAL;

	handler = malloc(sizeof(storage_fsHandler_t));
	if (handler == NULL)
		return -ENOMEM;

	strncpy(handler->name, name, sizeof(handler->name));
	handler->name[sizeof(handler->name) - 1] = '\0';
	handler->mount = mount;
	handler->umount = umount;

	if (lib_rbInsert(&storage_common.fss, &handler->node) != NULL) {
		free(handler);
		return -EEXIST;
	}

	return EOK;
}


/* TODO: this function remove handler from storage_common.fss, pointer to the elements of fss is used by storage_fsHandler_t
		 The case when fs is unregistered before umount can cause undefined behaviour. */
int storage_unregisterfs(const char *name)
{
	storage_fsHandler_t *fs = storage_getfs(name);

	if (fs == NULL)
		return -EINVAL;

	lib_rbRemove(&storage_common.fss, &fs->node);
	free(fs);

	return EOK;
}


int storage_mountfs(storage_t *strg, const char *name, const char *data, unsigned long mode, oid_t *mnt, oid_t *root)
{
	int err;
	storage_fsctx_t *fsctx;
	storage_fsHandler_t *handler = storage_getfs(name);

	if ((strg == NULL) || (strg->dev == NULL) || (strg->parts != NULL) || (handler == NULL) || (root == NULL))
		return -EINVAL;

	if (strg->fs != NULL)
		return -EBUSY;

	fsctx = malloc(sizeof(storage_fsctx_t));
	if (fsctx == NULL)
		return -ENOMEM;

	strg->fs = malloc(sizeof(storage_fs_t));
	if (strg->fs == NULL) {
		free(fsctx);
		return -ENOMEM;
	}

	/* Set filesystem mountpoint */
	if (mnt != NULL) {
		strg->fs->mnt = malloc(sizeof(oid_t));
		if (strg->fs->mnt == NULL) {
			free(fsctx);
			free(strg->fs);
			return -ENOMEM;
		}
		*strg->fs->mnt = *mnt;
	}
	/* Mounting rootfs, no mountpoint */
	else {
		strg->fs->mnt = NULL;
	}

	err = storagectx_init(&fsctx->reqctx, storage_fsHandler);
	if (err < 0) {
		free(fsctx);
		free(strg->fs->mnt);
		free(strg->fs);
		strg->fs = NULL;
		return err;
	}

	/* Assign the port to a filesystem from a newly created request context */
	root->port = fsctx->reqctx.port;
	/* Pointer to the storage_fs_t is held by a request context and passed to a message handler */
	fsctx->reqctx.data = strg->fs;
	/* Set filesystem handler */
	fsctx->handler = handler;
	/* The filesystem context has to be assign to the storage_fs_t to make umount operation */
	strg->fs->fsctx = fsctx;

	err = handler->mount(strg, strg->fs, data, mode, root);
	if (err < 0) {
		requestctx_done(&fsctx->reqctx);
		free(fsctx);
		free(strg->fs->mnt);
		free(strg->fs);
		strg->fs = NULL;
		return err;
	}

	requestctx_run(&fsctx->reqctx);

	return EOK;
}


int storage_mountpoint(storage_t *strg, oid_t *mnt)
{
	if ((strg == NULL) || (strg->fs == NULL) || (mnt == NULL)) {
		return -EINVAL;
	}

	/* Mounted rootfs, no mountpoint */
	if (strg->fs->mnt == NULL) {
		return -ENOENT;
	}
	*mnt = *strg->fs->mnt;

	return EOK;
}


int storage_umountfs(storage_t *strg)
{
	int err;
	storage_fsctx_t *fsctx;

	if ((strg == NULL) || (strg->fs == NULL) || (strg->fs->fsctx == NULL) || (strg->fs->fsctx->handler == NULL))
		return -EINVAL;

	fsctx = strg->fs->fsctx;
	requestctx_stop(&fsctx->reqctx);

	err = fsctx->handler->umount(strg->fs);
	if (err < 0) {
		requestctx_run(&fsctx->reqctx);
		return err;
	}

	requestctx_done(&fsctx->reqctx);
	free(fsctx);
	free(strg->fs->mnt);
	free(strg->fs);

	strg->fs = NULL;

	return EOK;
}


int storage_add(storage_t *strg, oid_t *oid)
{
	int res;
	storage_t *pstrg, *part;

	if ((strg == NULL) || (strg->dev == NULL) || (strg->size == 0))
		return -EINVAL;

	if ((pstrg = strg->parent) != NULL) {
		if ((strg->start < pstrg->start) || (strg->start + strg->size > pstrg->start + pstrg->size))
			return -EINVAL;

		if ((part = pstrg->parts) != NULL) {
			do {
				if (strg->start + strg->size <= part->start)
					break;
				else if (strg->start >= part->start + part->size)
					part = part->next;
				else
					return -EINVAL;
			} while (part != pstrg->parts);
		}

		if ((part == NULL) || ((part == pstrg->parts) && (strg->start + strg->size <= part->start)))
			pstrg->parts = strg;
		LIST_ADD(&part, strg);
	}

	strg->fs = NULL;
	strg->parts = NULL;

	res = idtree_alloc(&storage_common.strgs, &strg->node);
	if (res < 0)
		return res;

	oid->id = res;
	oid->port = storage_common.ctx.port;

	return EOK;
}


int storage_remove(storage_t *strg)
{
	if ((strg == NULL) || (strg->parts != NULL))
		return -EINVAL;

	if (strg->fs != NULL)
		return -EBUSY;

	if (strg->parent != NULL)
		LIST_REMOVE(&strg->parent->parts, strg);

	idtree_remove(&storage_common.strgs, &strg->node);

	return EOK;
}


int storage_run(unsigned int nthreads, unsigned int stacksz)
{
	unsigned int i, j;
	char *stacks;
	int err;

	stacks = malloc(nthreads * stacksz);
	if (stacks == NULL)
		return -ENOMEM;

	for (i = 0; i < nthreads; i++) {
		err = beginthread(storage_poolthr, POOLTHR_PRIORITY, stacks + i * stacksz, stacksz, NULL);
		if (err < 0) {
			/* Wake and drain already-started poolthrs by closing wport */
			portDestroy(storage_common.wport);
			for (j = 0; j < i; j++) {
				while (threadJoin(-1, 10000) < 0)
					;
			}
			free(stacks);
			return err;
		}
	}

	priority(POOLTHR_PRIORITY);
	storage_poolthr(NULL);

	return EOK;
}


static int storage_cmpfs(rbnode_t *n1, rbnode_t *n2)
{
	storage_fsHandler_t *fs1 = lib_treeof(storage_fsHandler_t, node, n1);
	storage_fsHandler_t *fs2 = lib_treeof(storage_fsHandler_t, node, n2);

	return strncmp(fs1->name, fs2->name, sizeof(fs1->name));
}


int storage_init(void (*msgHandler)(void *data, msg_t *msg), unsigned int queuesz)
{
	int err;

	(void)queuesz;

	err = mutexCreate(&storage_common.lock);
	if (err < 0)
		goto lock_fail;

	storage_common.ctxs = NULL;

	err = portCreate(&storage_common.wport);
	if (err < 0)
		goto wport_fail;

	err = storagectx_init(&storage_common.ctx, msgHandler);
	if (err < 0)
		goto ctx_fail;

	lib_rbInit(&storage_common.fss, storage_cmpfs, NULL);
	idtree_init(&storage_common.strgs);
	requestctx_run(&storage_common.ctx);

	return EOK;

ctx_fail:
	portDestroy(storage_common.wport);
wport_fail:
	resourceDestroy(storage_common.lock);
lock_fail:
	return err;
}
