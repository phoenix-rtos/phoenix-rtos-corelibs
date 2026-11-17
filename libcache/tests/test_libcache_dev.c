/*
 * Phoenix-RTOS
 *
 * libcache unit tests - source memory which counts the callback calls and injects faults
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "test_libcache_dev.h"


test_dev_t test_dev;


static ssize_t test_devCheck(uint64_t offset, size_t count, bool fail, cache_devCtx_t *ctx)
{
	if (ctx != (cache_devCtx_t *)&test_dev) {
		return -EFAULT;
	}

	if (fail && (offset < test_dev.failEnd) && (offset + count > test_dev.failBeg)) {
		return test_dev.failResult;
	}

	if ((count == 0) || (offset >= test_dev.size) || (count > test_dev.size - offset)) {
		return -EINVAL;
	}

	if ((test_dev.chunk != 0) && (count > test_dev.chunk)) {
		count = test_dev.chunk;
	}

	return (ssize_t)count;
}


static ssize_t test_devRead(uint64_t offset, void *buffer, size_t count, cache_devCtx_t *ctx)
{
	ssize_t ret = test_devCheck(offset, count, test_dev.failRead, ctx);

	test_dev.reads++;
	test_dev.lastReadAddr = offset;
	test_dev.lastReadLen = count;

	if (ret > 0) {
		memcpy(buffer, test_dev.mem + offset, (size_t)ret);
	}

	return ret;
}


static ssize_t test_devWrite(uint64_t offset, const void *buffer, size_t count, cache_devCtx_t *ctx)
{
	ssize_t ret = test_devCheck(offset, count, test_dev.failWrite, ctx);

	test_dev.writes++;
	test_dev.lastWriteAddr = offset;
	test_dev.lastWriteLen = count;

	if (ret > 0) {
		memcpy(test_dev.mem + offset, buffer, (size_t)ret);
	}

	return ret;
}


const cache_ops_t test_devOps = {
	.readCb = test_devRead,
	.writeCb = test_devWrite,
	.ctx = (cache_devCtx_t *)&test_dev,
};


void test_devInit(size_t size)
{
	memset(&test_dev, 0, sizeof(test_dev));
	test_dev.size = size;
	test_dev.failEnd = UINT64_MAX;
	test_dev.failResult = -EIO;
}
