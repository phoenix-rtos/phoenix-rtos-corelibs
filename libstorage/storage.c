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


#define REQTHR_PRIORITY     1
#define POOLTHR_PRIORITY    1
#define POOL_FLAG_PRIO_UPGR (1U << 0) /* Allow upgrading priority when handling the requests */


/* clang-format off */
enum { state_exit = -1, state_stop, state_run };
/* clang-format on */


typedef struct {
	char name[16];           /* Filesystem name */
	storage_mount_t mount;   /* Filesystem mount */
	storage_umount_t umount; /* Filesystem umount */
	rbnode_t node;           /* RB tree node */
} storage_fsHandler_t;


typedef struct _request_t request_t;
typedef struct _storage_fsctx_t storage_fsctx_t;


typedef struct {
	int state;                                  /* Context state */
	unsigned int port;                          /* Context port */
	unsigned int nreqs;                         /* Number of actively processed requests */
	void (*msgHandler)(void *data, msg_t *msg); /* Message handler */
	void *data;                                 /* Message handling data */
	request_t *stopped;                         /* Stopped requests */
	handle_t scond;                             /* Stopped requests condition variable */
	handle_t lock;                              /* Context mutex */
	struct _storage_pool_t *poolctx;            /* Pool context */
	char stack[512] __attribute__((aligned(8)));
} request_ctx_t;


typedef struct {
	unsigned int port;                     /* Limited port */
	request_ctx_t *target;                 /* Target request context */
	int (*limitF)(void *data, msg_t *msg); /* Limiting handler */
	void *data;                            /* Limiting handler data */
	char stack[] __attribute__((aligned(8)));
} limited_request_ctx_t;


struct _storage_fsctx_t {
	request_ctx_t reqctx;         /* Filesystem requests context */
	storage_fsHandler_t *handler; /* Filesystem data */
};


struct _request_t {
	msg_t msg;              /* Request message */
	msg_rid_t rid;          /* Request message receiving context */
	request_ctx_t *ctx;     /* Request handling context */
	unsigned int port;      /* Request port */
	request_t *prev, *next; /* Doubly linked list */
};


typedef struct {
	request_t *reqs; /* Requests queue */
	handle_t lock;   /* Queue mutex */
} queue_t;


typedef struct _storage_pool_t {
	int state;         /* Pool handling state */
	size_t threads;    /* Number of remaining pool threads */
	handle_t lock;     /* Pool handling mutex */
	queue_t free;      /* Free requests queue */
	queue_t ready;     /* Ready requests queue */
	handle_t fcond;    /* Free requests condition variable */
	handle_t rcond;    /* Ready requests condition variable */
	char *stacks;      /* Pool threads stacks */
	request_t *reqs;   /* Pool threads context */
	unsigned int prio; /* Pool threads priority */
	unsigned int flags;
} storage_pool_t;


static struct {
	idtree_t strgs;      /* Storages */
	rbtree_t fss;        /* Registered filesystems */
	storage_pool_t pool; /* Default request pool */
	request_ctx_t ctx;   /* Storage devices requests context */
} storage_common;


static request_t *queue_pop(queue_t *q)
{
	request_t *req;

	mutexLock(q->lock);

	if (q->reqs == NULL) {
		mutexUnlock(q->lock);
		return NULL;
	}

	req = q->reqs;
	LIST_REMOVE(&q->reqs, req);

	mutexUnlock(q->lock);

	return req;
}


static void queue_push(queue_t *q, request_t *req)
{
	mutexLock(q->lock);

	LIST_ADD(&q->reqs, req);

	mutexUnlock(q->lock);
}


static void queue_done(queue_t *q)
{
	resourceDestroy(q->lock);
}


static int queue_init(queue_t *q)
{
	int err;

	err = mutexCreate(&q->lock);
	if (err < 0)
		return err;

	q->reqs = NULL;

	return EOK;
}


static void storage_limitedthr(void *arg)
{
	limited_request_ctx_t *lctx = (limited_request_ctx_t *)arg;
	request_ctx_t *ctx = lctx->target;
	request_t *req = NULL;
	int err;

	mutexLock(ctx->lock);
	for (;;) {
		while ((ctx->state != state_exit) && ((ctx->state == state_stop) || ((req = queue_pop(&ctx->poolctx->free)) == NULL)))
			condWait(ctx->poolctx->fcond, ctx->lock, 0);

		if (ctx->state == state_exit) {
			mutexUnlock(ctx->lock);

			free(lctx->data);
			free(lctx);
			endthread();
		}

		mutexUnlock(ctx->lock);

		while ((err = msgRecv(lctx->port, &req->msg, &req->rid)) < 0) {
			/* Closed port */
			if (err == -EINVAL)
				break;
		}

		if (err == EOK) {
			if ((err = lctx->limitF(lctx->data, &req->msg)) < 0) {
				req->msg.o.err = err;
				msgRespond(lctx->port, &req->msg, req->rid);

				mutexLock(ctx->lock);
				queue_push(&ctx->poolctx->free, req);
				condSignal(ctx->poolctx->fcond);
				continue;
			}
		}

		req->port = lctx->port;
		req->ctx = ctx;
		mutexLock(ctx->lock);

		if ((err < 0) || (ctx->state == state_exit)) {
			queue_push(&ctx->poolctx->free, req);
			condSignal(ctx->poolctx->fcond);
			mutexUnlock(ctx->lock);
			free(lctx->data);
			free(lctx);
			endthread();
		}
		else if (ctx->state == state_stop) {
			LIST_ADD(&ctx->stopped, req);
		}
		else if (ctx->state == state_run) {
			queue_push(&ctx->poolctx->ready, req);
			condSignal(ctx->poolctx->rcond);
		}
	}
}


static void storage_reqthr(void *arg)
{
	request_ctx_t *ctx = (request_ctx_t *)arg;
	request_t *req = NULL;
	int err;

	mutexLock(ctx->lock);
	for (;;) {
		while ((ctx->state != state_exit) && ((ctx->state == state_stop) || ((req = queue_pop(&ctx->poolctx->free)) == NULL)))
			condWait(ctx->poolctx->fcond, ctx->lock, 0);

		if (ctx->state == state_exit) {
			mutexUnlock(ctx->lock);

			endthread();
		}

		mutexUnlock(ctx->lock);

		while ((err = msgRecv(ctx->port, &req->msg, &req->rid)) < 0) {
			/* Closed port */
			if (err == -EINVAL)
				break;
		}

		req->ctx = ctx;
		req->port = ctx->port;
		mutexLock(ctx->lock);

		if ((err < 0) || (ctx->state == state_exit)) {
			queue_push(&ctx->poolctx->free, req);
			condSignal(ctx->poolctx->fcond);
			mutexUnlock(ctx->lock);
			endthread();
		}
		else if (ctx->state == state_stop) {
			LIST_ADD(&ctx->stopped, req);
		}
		else if (ctx->state == state_run) {
			queue_push(&ctx->poolctx->ready, req);
			condSignal(ctx->poolctx->rcond);
		}
	}
}


static void storage_poolthr(void *arg)
{
	request_ctx_t *ctx;
	request_t *req = NULL;
	storage_pool_t *poolctx = (storage_pool_t *)arg;
	unsigned int handlingPriority;

	for (;;) {
		mutexLock(poolctx->lock);

		while ((poolctx->state != state_exit) && ((poolctx->state == state_stop) || ((req = queue_pop(&poolctx->ready)) == NULL)))
			condWait(poolctx->rcond, poolctx->lock, 0);

		if (poolctx->state == state_exit) {
			poolctx->threads--;
			condSignal(poolctx->rcond);
			mutexUnlock(poolctx->lock);

			endthread();
		}

		mutexUnlock(poolctx->lock);

		ctx = req->ctx;
		mutexLock(ctx->lock);

		if (ctx->state == state_stop) {
			LIST_ADD(&ctx->stopped, req);

			mutexUnlock(ctx->lock);
		}
		else {
			ctx->nreqs++;

			mutexUnlock(ctx->lock);

			handlingPriority = req->msg.priority;
			if (((storage_common.pool.flags & POOL_FLAG_PRIO_UPGR) == 0) &&
					(handlingPriority < ctx->poolctx->prio)) {
				handlingPriority = ctx->poolctx->prio;
			}
			priority(handlingPriority);
			ctx->msgHandler(ctx->data, &req->msg);
			priority(poolctx->prio);

			msgRespond(req->port, &req->msg, req->rid);

			mutexLock(ctx->lock);
			queue_push(&poolctx->free, req);
			condSignal(poolctx->fcond);

			if ((--ctx->nreqs == 0) && (ctx->state == state_stop))
				condSignal(ctx->scond);

			mutexUnlock(ctx->lock);
		}
	}
}


static void requestctx_run(request_ctx_t *ctx)
{
	request_t *req;

	mutexLock(ctx->lock);

	ctx->state = state_run;
	while (ctx->stopped != NULL) {
		req = ctx->stopped->prev;
		LIST_REMOVE(&ctx->stopped, req);
		queue_push(&ctx->poolctx->ready, req);
		condSignal(ctx->poolctx->rcond);
	}

	mutexUnlock(ctx->lock);
	condBroadcast(ctx->poolctx->fcond);
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
	request_t *req;
	msg_t msg = { 0 };

	requestctx_stop(ctx);

	/* ensure reqthr on named port gets waked up */
	msg.type = mtOpen;
	msg.oid.id = 0;
	msg.oid.port = ctx->port;
	msgSend(ctx->port, &msg);

	mutexLock(ctx->lock);

	ctx->state = state_exit;
	while ((req = ctx->stopped) != NULL) {
		LIST_REMOVE(&ctx->stopped, req);
		queue_push(&ctx->poolctx->free, req);
	}

	mutexUnlock(ctx->lock);

	do {
		condBroadcast(ctx->poolctx->fcond);
	} while (threadJoin(-1, 10000) < 0);

	resourceDestroy(ctx->scond);
	resourceDestroy(ctx->lock);
}


static int storagectx_init(request_ctx_t *ctx, void (*msgHandler)(void *data, msg_t *msg), storage_pool_t *poolctx, unsigned int reqthrpriority)
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

	ctx->poolctx = poolctx;
	ctx->msgHandler = msgHandler;
	ctx->data = NULL;
	ctx->stopped = NULL;
	ctx->nreqs = 0;
	ctx->state = state_stop;

	err = beginthread(storage_reqthr, reqthrpriority, ctx->stack, sizeof(ctx->stack), ctx);
	if (err < 0) {
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


static int storage_mountfsAny(storage_t *strg, storage_pool_t *poolctx, const char *name, const char *data, unsigned long mode, oid_t *mnt, oid_t *root, unsigned int rootPort, unsigned int reqthrpriority)
{
	int err;
	storage_fsctx_t *fsctx;
	storage_fsHandler_t *handler = storage_getfs(name);
	oid_t stackRoot;

	if ((strg == NULL) || (strg->dev == NULL) || (strg->parts != NULL) || (handler == NULL))
		return -EINVAL;

	if (root == NULL) {
		stackRoot.port = rootPort;
		root = &stackRoot;
	}

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

	fsctx->reqctx.port = root->port;

	err = storagectx_init(&fsctx->reqctx, storage_fsHandler, poolctx, reqthrpriority);
	if (err < 0) {
		free(fsctx);
		free(strg->fs->mnt);
		free(strg->fs);
		strg->fs = NULL;
		return err;
	}

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


int storage_mountfsShared(storage_t *strg, storage_pool_t *poolctx, const char *name, const char *data, unsigned long mode, oid_t *mnt, unsigned int rootPort, unsigned int reqthrpriority)
{
	return storage_mountfsAny(strg, poolctx, name, data, mode, mnt, NULL, rootPort, reqthrpriority);
}


int storage_mountfs(storage_t *strg, const char *name, const char *data, unsigned long mode, oid_t *mnt, oid_t *root)
{
	int err;

	if (root == NULL)
		return -EINVAL;

	err = portCreate(&root->port);
	if (err < 0) {
		return err;
	}

	err = storage_mountfsAny(strg, &storage_common.pool, name, data, mode, mnt, root, 0U, REQTHR_PRIORITY);
	if (err < 0) {
		portDestroy(root->port);
	}
	return err;
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

	/* Destroy port before waiting for request thread to finish */
	portDestroy(fsctx->reqctx.port); /* noop for shared ports */
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


int storage_bindLimitedPort(unsigned int port, int (*limitF)(void *data, msg_t *msg), void *data, unsigned int reqthrpriority, unsigned int stacksz)
{
	int err;
	limited_request_ctx_t *lctx = malloc(sizeof(limited_request_ctx_t) + stacksz);
	if (lctx == NULL) {
		return -ENOMEM;
	}

	/* Bind to flash operations */
	lctx->target = &storage_common.ctx;

	lctx->port = port;
	lctx->limitF = limitF;
	lctx->data = data;

	err = beginthread(storage_limitedthr, reqthrpriority, lctx->stack, stacksz, lctx);
	if (err < 0) {
		free(lctx);
		return err;
	}

	return EOK;
}


static void storage_poolDone(storage_pool_t *poolctx)
{
	mutexLock(poolctx->lock);

	poolctx->state = state_exit;

	condBroadcast(poolctx->rcond);

	while (poolctx->threads > 0)
		condWait(poolctx->rcond, poolctx->lock, 0);

	mutexUnlock(poolctx->lock);

	free(poolctx->stacks);
	free(poolctx->reqs);
	queue_done(&poolctx->free);
	queue_done(&poolctx->ready);
	resourceDestroy(poolctx->fcond);
	resourceDestroy(poolctx->rcond);
	resourceDestroy(poolctx->lock);
}


void storage_poolDestroy(storage_pool_t *poolctx)
{
	storage_poolDone(poolctx);
	free(poolctx);
}


static int storage_runPool(unsigned int nthreads, unsigned int stacksz, storage_pool_t *poolctx, unsigned int priority)
{
	unsigned int i;
	int err;

	poolctx->stacks = malloc(nthreads * stacksz);
	if (poolctx->stacks == NULL)
		return -ENOMEM;

	mutexLock(poolctx->lock);

	poolctx->prio = priority;
	poolctx->threads = 0;
	poolctx->state = state_run;

	for (i = 0; i < nthreads; i++) {
		poolctx->threads++;
		err = beginthread(storage_poolthr, priority, poolctx->stacks + i * stacksz, stacksz, poolctx);
		if (err < 0) {
			poolctx->threads--;
			mutexUnlock(poolctx->lock);
			return err;
		}
	}
	mutexUnlock(poolctx->lock);

	return EOK;
}


int storage_run(unsigned int nthreads, unsigned int stacksz)
{
	int err = storage_runPool(nthreads, stacksz, &storage_common.pool, POOLTHR_PRIORITY);

	if (err < 0) {
		storage_poolDone(&storage_common.pool);
		return err;
	}

	mutexLock(storage_common.pool.lock);
	storage_common.pool.threads++;
	mutexUnlock(storage_common.pool.lock);

	priority(POOLTHR_PRIORITY);
	storage_poolthr(&storage_common.pool);

	return err;
}


static int storage_cmpfs(rbnode_t *n1, rbnode_t *n2)
{
	storage_fsHandler_t *fs1 = lib_treeof(storage_fsHandler_t, node, n1);
	storage_fsHandler_t *fs2 = lib_treeof(storage_fsHandler_t, node, n2);

	return strncmp(fs1->name, fs2->name, sizeof(fs1->name));
}


static int storage_initPool(storage_pool_t *poolctx, unsigned int queuesz)
{
	unsigned int i;
	int err;

	err = mutexCreate(&poolctx->lock);
	if (err < 0)
		goto lock_fail;

	err = condCreate(&poolctx->rcond);
	if (err < 0)
		goto rcond_fail;

	err = condCreate(&poolctx->fcond);
	if (err < 0)
		goto fcond_fail;

	err = queue_init(&poolctx->ready);
	if (err < 0)
		goto ready_fail;

	err = queue_init(&poolctx->free);
	if (err < 0)
		goto free_fail;

	poolctx->reqs = malloc(queuesz * sizeof(request_t));
	if (poolctx->reqs == NULL) {
		err = -ENOMEM;
		goto reqs_fail;
	}

	for (i = 0; i < queuesz; i++)
		LIST_ADD(&poolctx->free.reqs, poolctx->reqs + i);

	poolctx->state = state_stop;
	poolctx->threads = 0;
	poolctx->flags = 0;

	return EOK;

reqs_fail:
	queue_done(&poolctx->free);
free_fail:
	queue_done(&poolctx->ready);
ready_fail:
	resourceDestroy(poolctx->fcond);
fcond_fail:
	resourceDestroy(poolctx->rcond);
rcond_fail:
	resourceDestroy(poolctx->lock);
lock_fail:
	return err;
}


storage_pool_t *storage_createPool(unsigned int queuesz, unsigned int nthreads, unsigned int stacksz, unsigned int priority)
{
	storage_pool_t *poolctx = malloc(sizeof(storage_pool_t));
	if (poolctx == NULL)
		return NULL;

	if (storage_initPool(poolctx, queuesz) < 0) {
		free(poolctx);
		return NULL;
	}

	if (storage_runPool(nthreads, stacksz, poolctx, priority) < 0) {
		storage_poolDestroy(poolctx);
		return NULL;
	}

	return poolctx;
}


int storage_init(void (*msgHandler)(void *data, msg_t *msg), unsigned int queuesz)
{
	int err;

	err = storage_initPool(&storage_common.pool, queuesz);
	if (err < 0) {
		return err;
	}
	storage_common.pool.flags |= POOL_FLAG_PRIO_UPGR;

	err = portCreate(&storage_common.ctx.port);
	if (err < 0) {
		storage_poolDone(&storage_common.pool);
		return err;
	}

	err = storagectx_init(&storage_common.ctx, msgHandler, &storage_common.pool, REQTHR_PRIORITY);
	if (err < 0) {
		portDestroy(storage_common.ctx.port);
		storage_poolDone(&storage_common.pool);
		return err;
	}

	lib_rbInit(&storage_common.fss, storage_cmpfs, NULL);
	idtree_init(&storage_common.strgs);
	requestctx_run(&storage_common.ctx);

	return EOK;
}
