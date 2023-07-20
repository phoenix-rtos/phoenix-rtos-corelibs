/*
 * Phoenix-RTOS
 *
 * Filesystem interface
 *
 * Copyright 2022 Phoenix Systems
 * Author: Hubert Buczynski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _STORAGE_FILESYSTEM_H_
#define _STORAGE_FILESYSTEM_H_

#include <dirent.h>
#include <sys/msg.h>
#include <sys/types.h>


struct _storage_t;
struct _storage_fsctx_t;


typedef struct {
	int (*open)(void *info, oid_t *oid);
	int (*close)(void *info, oid_t *oid);

	ssize_t (*read)(void *info, oid_t *oid, offs_t offs, void *data, size_t len);
	ssize_t (*write)(void *info, oid_t *oid, offs_t offs, const void *data, size_t len);
	int (*setattr)(void *info, oid_t *oid, int type, long long attr, void *data, size_t len);
	int (*getattr)(void *info, oid_t *oid, int type, long long *attr);
	int (*truncate)(void *info, oid_t *oid, size_t size);
	void (*devctl)(void *info, oid_t *oid, const void *in, void *out);

	int (*create)(void *info, oid_t *oid, const char *name, oid_t *dev, unsigned mode, int type, oid_t *res);
	int (*destroy)(void *info, oid_t *oid);

	int (*lookup)(void *info, oid_t *oid, const char *name, oid_t *res, oid_t *dev, char *lnk, int lnksz);
	int (*link)(void *info, oid_t *oid, const char *name, oid_t *res);
	int (*unlink)(void *info, oid_t *oid, const char *name);
	int (*readdir)(void *info, oid_t *oid, offs_t offs, struct dirent *dent, size_t size);
	int (*statfs)(void *info, void *buf, size_t len);
	int (*sync)(void *info, oid_t *oid);
} storage_fsops_t;


typedef struct {
	oid_t *mnt;                     /* Filesystem mountpoint (NULL if mounted as rootfs) */
	void *info;                     /* Specific information for the filesystem */
	const storage_fsops_t *ops;     /* Callbacks to operations on the filesystem */
	struct _storage_fsctx_t *fsctx; /* File system context used internally by the storage library */
} storage_fs_t;


/* File system callbacks used within registration in the storage library */
typedef int (*storage_mount_t)(struct _storage_t *dev, storage_fs_t *fs, const char *data, unsigned long mode, oid_t *root);
typedef int (*storage_umount_t)(storage_fs_t *fs);


extern void storage_fsHandler(void *data, msg_t *msg);

#endif
