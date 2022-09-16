/*
 * Phoenix-RTOS
 *
 * Storage device interface
 *
 * Copyright 2022 Phoenix Systems
 * Author: Hubert Buczynski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _STORAGE_DEV_H_
#define _STORAGE_DEV_H_

#include <sys/types.h>


struct _storage_t;
struct _storage_devCtx_t; /* Device driver context should be defined by flash driver */


/* Block device interface */

typedef struct {
	ssize_t (*read)(struct _storage_t *dev, off_t start, void *data, size_t size);
	ssize_t (*write)(struct _storage_t *dev, off_t start, const void *data, size_t size);
	int (*sync)(struct _storage_t *dev);
} storage_blkops_t;


typedef struct {
	const storage_blkops_t *ops; /* Pointer to operations on the block device */
} storage_blk_t;


/* Memory Technology Device (MTD) Interface */

typedef struct {
	int (*erase)(struct _storage_t *dev, off_t offs, size_t size);
	int (*unPoint)(struct _storage_t *dev, off_t offs, size_t size);
	ssize_t (*point)(struct _storage_t *dev, off_t offs, size_t size, size_t *retlen, void **virt, addr_t *phys);
	int (*read)(struct _storage_t *dev, off_t offs, void *data, size_t len, size_t *retlen);
	int (*write)(struct _storage_t *dev, off_t offs, const void *data, size_t len, size_t *retlen);

	int (*meta_read)(struct _storage_t *dev, off_t offs, void *data, size_t len, size_t *retlen);
	int (*meta_write)(struct _storage_t *dev, off_t offs, const void *data, size_t len, size_t *retlen);

	void (*sync)(struct _storage_t *dev);
	int (*lock)(struct _storage_t *dev, off_t offs, size_t len);
	int (*unLock)(struct _storage_t *dev, off_t offs, size_t len);
	int (*isLocked)(struct _storage_t *dev, off_t offs, size_t len);

	int (*block_isBad)(struct _storage_t *dev, off_t offs);
	int (*block_isReserved)(struct _storage_t *dev, off_t offs);
	int (*block_markBad)(struct _storage_t *dev, off_t offs);
	int (*block_maxBadNb)(struct _storage_t *dev, off_t offs, size_t len);
	int (*block_maxBitflips)(struct _storage_t *dev, off_t offs);

	int (*suspend)(struct _storage_t *dev);
	void (*resume)(struct _storage_t *dev);
	void (*reboot)(struct _storage_t *dev);
} storage_mtdops_t;


typedef enum { mtd_nandFlash = 0, mtd_norFlash } mtd_type_t;


typedef struct {
	mtd_type_t type;             /* MTD type: NOR or NAND flash memory */
	const char *name;            /* Flash memory name */
	size_t erasesz;              /* Erase size */
	size_t writesz;              /* Minimal writable flash unit size. For NOR it is 1, for NAND it is one page */
	size_t writeBuffsz;          /* For NOR flash it is page size, for NAND should be equal writesz */
	size_t metaSize;             /* Amount of meta data per block */
	size_t oobSize;              /* out-of-bound (oob) data size */
	size_t oobAvail;             /* available out-of-bound (oob) data size */
	const storage_mtdops_t *ops; /* Pointer to operations on the MTD device */
} storage_mtd_t;


/* Storage device structure */

typedef struct {
	storage_blk_t *blk;            /* Block device context */
	storage_mtd_t *mtd;            /* MTD device context */
	struct _storage_devCtx_t *ctx; /* Device driver context */
} storage_dev_t;


#endif
