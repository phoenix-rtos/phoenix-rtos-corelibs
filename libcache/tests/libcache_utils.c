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

#include "libcache_utils.h"
#include "unity_fixture.h"

#include <stdlib.h>
#include <string.h>


bool test_writeCbErrOff;


/* Simulates the cached source memory */
static unsigned char srcMem[LIBCACHE_SRC_MEM_SIZE];


void test_genSrcMem(void)
{
	for (size_t i = 0; i < sizeof(srcMem); ++i) {
		srcMem[i] = (unsigned char)rand();
	}
}


ssize_t test_readCb(uint64_t offset, void *buffer, size_t count, cache_devCtx_t *ctx)
{
	if ((offset > sizeof(srcMem)) || (count > sizeof(srcMem) - offset)) {
		return -EINVAL;
	}

	memcpy(buffer, srcMem + offset, count);

	return (ssize_t)count;
}


ssize_t test_writeCb(uint64_t offset, const void *buffer, size_t count, cache_devCtx_t *ctx)
{
	if ((offset > sizeof(srcMem)) || (count > sizeof(srcMem) - offset)) {
		return -EINVAL;
	}

	memcpy(srcMem + offset, buffer, count);

	return (ssize_t)count;
}


ssize_t test_readCbErr(uint64_t offset, void *buffer, size_t count, cache_devCtx_t *ctx)
{
	return -EIO;
}

ssize_t test_writeCbErr(uint64_t offset, const void *buffer, size_t count, cache_devCtx_t *ctx)
{
	return test_writeCbErrOff ? test_writeCb(offset, buffer, count, ctx) : -EIO;
}

void *test_cache_write(void *args)
{
	test_write_args_t *arguments = args;

	arguments->actualCount = cache_write(arguments->cache, arguments->addr, arguments->buffer, arguments->count, arguments->policy);

	return NULL;
}

void *test_cache_read(void *args)
{
	test_read_args_t *arguments = args;

	arguments->actualCount = cache_read(arguments->cache, arguments->addr, arguments->buffer, arguments->count);

	return NULL;
}
