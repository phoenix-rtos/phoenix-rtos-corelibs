/*
 * Phoenix-RTOS
 *
 * libcache unit tests - threads sharing a cache which is much smaller than what they use
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include <unity_fixture.h>

#include <cache.h>

#include "test_libcache_dev.h"

#define LINE_SIZE   16U
#define LINES_CNT   8U
#define THREADS     4U
#define REGION_SIZE (8U * LINE_SIZE) /* Written by a single thread */
#define SHARED_SIZE (8U * LINE_SIZE) /* Read and invalidated by every thread */
#define SHARED_ADDR (THREADS * REGION_SIZE)
#define DEV_SIZE    (SHARED_ADDR + SHARED_SIZE)
#define ITERATIONS  3000U


typedef struct {
	pthread_t tid;
	uint64_t addr;
	uint32_t rngState;
	unsigned int failedAt; /* Iteration + 1 of the first failure */
	unsigned char shadow[REGION_SIZE];
} worker_t;


static cachectx_t *cache;
static worker_t workers[THREADS];
static unsigned char shared[SHARED_SIZE];


static uint32_t rng(worker_t *w)
{
	w->rngState ^= w->rngState << 13;
	w->rngState ^= w->rngState >> 17;
	w->rngState ^= w->rngState << 5;

	return w->rngState;
}


/* Unity assertions are not usable outside of the test thread */
static bool worker_step(worker_t *w)
{
	unsigned char buff[3 * LINE_SIZE];
	uint32_t op = rng(w) % 8U;
	size_t offs = rng(w) % REGION_SIZE;
	size_t len = 1 + (rng(w) % sizeof(buff));

	if (len > REGION_SIZE - offs) {
		len = REGION_SIZE - offs;
	}

	if (op < 2U) {
		for (size_t i = 0; i < len; ++i) {
			w->shadow[offs + i] = (unsigned char)rng(w);
		}
		return cache_write(cache, w->addr + offs, w->shadow + offs, len, ((op & 1U) != 0U) ? LIBCACHE_WRITE_BACK : LIBCACHE_WRITE_THROUGH) == (ssize_t)len;
	}
	if (op < 4U) {
		return (cache_read(cache, w->addr + offs, buff, len) == (ssize_t)len) && (memcmp(buff, w->shadow + offs, len) == 0);
	}
	if (op < 6U) {
		return (cache_read(cache, SHARED_ADDR + offs, buff, len) == (ssize_t)len) && (memcmp(buff, shared + offs, len) == 0);
	}
	if (op < 7U) {
		return cache_invalidate(cache, SHARED_ADDR + offs, SHARED_ADDR + offs + len) == EOK;
	}

	return cache_flush(cache, w->addr + offs, w->addr + offs + len) == EOK;
}


static void *worker_thread(void *arg)
{
	worker_t *w = arg;

	for (unsigned int i = 0; (i < ITERATIONS) && (w->failedAt == 0); ++i) {
		if (!worker_step(w)) {
			w->failedAt = i + 1;
		}
	}

	return NULL;
}


TEST_GROUP(test_concurrent);


TEST_SETUP(test_concurrent)
{
	test_devInit(DEV_SIZE);
	for (size_t i = 0; i < DEV_SIZE; ++i) {
		test_dev.mem[i] = (unsigned char)(i * 7U + (i >> 4));
	}
	memcpy(shared, test_dev.mem + SHARED_ADDR, SHARED_SIZE);

	cache = cache_init(DEV_SIZE, LINE_SIZE, LINES_CNT, &test_devOps);
	TEST_ASSERT_NOT_NULL(cache);
}


TEST_TEAR_DOWN(test_concurrent)
{
	if (cache != NULL) {
		TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(cache));
		cache = NULL;
	}
}


TEST(test_concurrent, writersAndReadersOfOwnRegions)
{
	for (size_t i = 0; i < THREADS; ++i) {
		workers[i].addr = i * REGION_SIZE;
		workers[i].rngState = 0x9e3779b9U * (uint32_t)(i + 1);
		workers[i].failedAt = 0;
		memcpy(workers[i].shadow, test_dev.mem + workers[i].addr, REGION_SIZE);
	}

	for (size_t i = 0; i < THREADS; ++i) {
		TEST_ASSERT_EQUAL_INT(0, pthread_create(&workers[i].tid, NULL, worker_thread, &workers[i]));
	}
	for (size_t i = 0; i < THREADS; ++i) {
		TEST_ASSERT_EQUAL_INT(0, pthread_join(workers[i].tid, NULL));
	}

	TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, 0, DEV_SIZE));
	for (size_t i = 0; i < THREADS; ++i) {
		TEST_ASSERT_EQUAL_UINT(0, workers[i].failedAt);
		TEST_ASSERT_EQUAL_MEMORY(workers[i].shadow, test_dev.mem + workers[i].addr, REGION_SIZE);
	}
	TEST_ASSERT_EQUAL_MEMORY(shared, test_dev.mem + SHARED_ADDR, SHARED_SIZE);
}


TEST_GROUP_RUNNER(test_concurrent)
{
	RUN_TEST_CASE(test_concurrent, writersAndReadersOfOwnRegions);
}
