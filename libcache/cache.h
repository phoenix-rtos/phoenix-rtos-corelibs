/*
 * Phoenix-RTOS
 *
 * Cache library
 *
 * Copyright 2022 Phoenix Systems
 * Author: Malgorzata Wrobel
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _CACHE_H
#define _CACHE_H

#include <stdio.h>
#include <stdint.h>


/* Cache write policies */
#define LIBCACHE_WRITE_BACK    0
#define LIBCACHE_WRITE_THROUGH 1


typedef struct cachectx_s cachectx_t;


typedef struct cache_devCtx_s cache_devCtx_t; /* Device driver context should be defined by flash driver */


/*
 * Callbacks return the number of transferred bytes or a negative error code, which is returned to the caller
 * of the cache function. A transfer shorter than count is continued, a transfer of 0 bytes ends with -EIO.
 * They are called with the cache locked, so they must not use the cache.
 */
typedef ssize_t (*cache_readCb_t)(uint64_t offset, void *buffer, size_t count, cache_devCtx_t *ctx);


typedef ssize_t (*cache_writeCb_t)(uint64_t offset, const void *buffer, size_t count, cache_devCtx_t *ctx);


/* Cached source memory interface */
typedef struct {
	cache_readCb_t readCb;
	cache_writeCb_t writeCb; /* NULL: read-only cache */
	cache_devCtx_t *ctx;     /* Device driver context */
} cache_ops_t;


/* lineSize and linesCnt / 4 (number of sets, 4 lines each) have to be powers of two */
cachectx_t *cache_init(uint64_t srcMemSize, size_t lineSize, size_t linesCnt, const cache_ops_t *ops);


/* Writes not written lines first, the cache stays usable when it fails */
int cache_deinit(cachectx_t *cache);


ssize_t cache_read(cachectx_t *cache, uint64_t addr, void *buffer, size_t count);


ssize_t cache_write(cachectx_t *cache, uint64_t addr, const void *buffer, size_t count, int policy);


int cache_flush(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr);


int cache_invalidate(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr);


int cache_clean(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr);


#endif
