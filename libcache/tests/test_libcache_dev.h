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

#ifndef _TEST_LIBCACHE_DEV_H_
#define _TEST_LIBCACHE_DEV_H_

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <cache.h>

#ifndef EOK
#define EOK 0 /* libphoenix extension, missing on the build host */
#endif

#define TEST_DEV_SIZE_MAX 1024U


typedef struct {
	unsigned char mem[TEST_DEV_SIZE_MAX];
	size_t size;

	/* Callback calls */
	unsigned int reads;
	unsigned int writes;
	uint64_t lastReadAddr;
	uint64_t lastWriteAddr;
	size_t lastReadLen;
	size_t lastWriteLen;

	/* Fault injection: transfers touching [failBeg, failEnd) return failResult */
	bool failRead;
	bool failWrite;
	uint64_t failBeg;
	uint64_t failEnd;
	ssize_t failResult;

	size_t chunk; /* Longest transfer done by a single call, 0 - not limited */
} test_dev_t;


extern test_dev_t test_dev;


extern const cache_ops_t test_devOps;


/* Zeroed source memory of the given size, no faults */
extern void test_devInit(size_t size);


#endif
