/*
 * Phoenix-RTOS
 *
 * Storage devices interface
 *
 * Copyright 2021-2022 Phoenix Systems
 * Author: Lukasz Kosinski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "fs.h"
#include "dev.h"

#include <sys/msg.h>
#include <sys/types.h>

#include <posix/idtree.h>


typedef struct _storage_t {
	off_t start;                    /* Storage start */
	size_t size;                    /* Storage size */
	storage_dev_t *dev;             /* Storage device */
	storage_fs_t *fs;               /* Mounted filesystem */
	struct _storage_t *parts;       /* Storage partitions */
	struct _storage_t *parent;      /* Storage parent */
	struct _storage_t *prev, *next; /* Doubly linked list */
	idnode_t node;                  /* ID tree node */
} storage_t;


/* Returns registered storage device instance */
extern storage_t *storage_get(int id);


/* Registers new supported filesystem */
extern int storage_registerfs(const char *name, storage_mount_t mount, storage_umount_t umount);


/* Unregisters supported filesystem */
extern int storage_unregisterfs(const char *name);


/* Mounts filesystem */
extern int storage_mountfs(storage_t *strg, const char *name, const char *data, unsigned long mode, oid_t *mnt, oid_t *root);


/* Returns filesystem mountpoint (-ENOENT is returned if storage is mounted as rootfs) */
extern int storage_mountpoint(storage_t *strg, oid_t *mnt);


/* Unmounts filesystem */
extern int storage_umountfs(storage_t *strg);


/* Registers a new storage. The oid's fields are completed by function. */
extern int storage_add(storage_t *strg, oid_t *oid);


/* Removes registered storage device */
extern int storage_remove(storage_t *strg);


/* Starts storage requests handling */
extern int storage_run(unsigned int nthreads, unsigned int stacksz);


/* Initializes storage handling */
extern int storage_init(void (*msgHandler)(void *data, msg_t *msg), unsigned int queuesz);


#endif
