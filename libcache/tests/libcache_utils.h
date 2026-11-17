/*
 * Phoenix-RTOS
 *
 * libcache unit tests - utilities
 *
 * Copyright 2022 Phoenix Systems
 * Author: Malgorzata Wrobel
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBCACHE_UTILS_H_
#define _LIBCACHE_UTILS_H_


#include <errno.h>
#include <stdbool.h>

#include <cache.h>

#ifndef EOK
#define EOK 0 /* libphoenix extension, missing on the build host */
#endif


#define LIBCACHE_SRC_MEM_SIZE 0x2800ULL /* Imitates the maximum capacity of cached source memory (in bytes) */
#define LIBCACHE_LINES_CNT    32        /* Number of cache lines */
#define LIBCACHE_LINE_SIZE    64        /* Size of a single cache line (in bytes) */

#define LOG2(x) ((uint8_t)(8 * sizeof(unsigned long) - __builtin_clzl((x)) - 1))


typedef struct {
	cachectx_t *cache;
	uint64_t addr;
	void *buffer;
	size_t count;
	int policy;
	ssize_t actualCount;
} test_write_args_t;


typedef struct {
	cachectx_t *cache;
	uint64_t addr;
	void *buffer;
	size_t count;
	ssize_t actualCount;
} test_read_args_t;


struct cache_devCtx_s {
	/* Empty device driver context */
};


/* Fills the simulated source memory with random bytes */
extern void test_genSrcMem(void);


extern ssize_t test_readCb(uint64_t offset, void *buffer, size_t count, cache_devCtx_t *ctx);


extern ssize_t test_writeCb(uint64_t offset, const void *buffer, size_t count, cache_devCtx_t *ctx);


extern ssize_t test_readCbErr(uint64_t offset, void *buffer, size_t count, cache_devCtx_t *ctx);


/* Fails with -EIO until test_writeCbErrOff is set (lets a test flush and release a cache with dirty lines) */
extern ssize_t test_writeCbErr(uint64_t offset, const void *buffer, size_t count, cache_devCtx_t *ctx);


extern bool test_writeCbErrOff;


extern void *test_cache_write(void *args);


extern void *test_cache_read(void *args);

#endif
