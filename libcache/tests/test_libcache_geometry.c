/*
 * Phoenix-RTOS
 *
 * libcache unit tests - cache geometry and callbacks accepted by cache_init()
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <unity_fixture.h>

#include <cache.h>

#include "test_libcache_dev.h"

#define DEV_SIZE       640U
#define NAND_PAGE_SIZE 256U
#define NAND_PAGES_CNT 256U


static cachectx_t *cache;


TEST_GROUP(test_geometry);


TEST_SETUP(test_geometry)
{
	test_devInit(DEV_SIZE);
	for (size_t i = 0; i < DEV_SIZE; ++i) {
		test_dev.mem[i] = (unsigned char)(i * 7U + (i >> 4));
	}
	cache = NULL;
}


TEST_TEAR_DOWN(test_geometry)
{
	if (cache != NULL) {
		TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(cache));
	}
}


TEST(test_geometry, init_lineSizeNotPowerOfTwo)
{
	static const size_t sizes[] = { 3, 12, 100, 4095 };

	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
		cache = cache_init(DEV_SIZE, sizes[i], 8, &test_devOps);
		TEST_ASSERT_NULL(cache);
	}
}


TEST(test_geometry, init_setsCountNotPowerOfTwo)
{
	static const size_t counts[] = { 12, 20, 24, 4092 };

	for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
		cache = cache_init(DEV_SIZE, 16, counts[i], &test_devOps);
		TEST_ASSERT_NULL(cache);
	}
}


TEST(test_geometry, init_noReadCallback)
{
	cache_ops_t ops = test_devOps;

	ops.readCb = NULL;
	cache = cache_init(DEV_SIZE, 16, 8, &ops);
	TEST_ASSERT_NULL(cache);

	cache = cache_init(DEV_SIZE, 16, 8, NULL);
	TEST_ASSERT_NULL(cache);
}


TEST(test_geometry, readOnlyCache)
{
	cache_ops_t ops = test_devOps;
	unsigned char buff[16];

	ops.writeCb = NULL;
	cache = cache_init(DEV_SIZE, 16, 8, &ops);
	TEST_ASSERT_NOT_NULL(cache);

	TEST_ASSERT_EQUAL_INT(-ENOSYS, cache_write(cache, 32, buff, sizeof(buff), LIBCACHE_WRITE_BACK));
	TEST_ASSERT_EQUAL_INT(-ENOSYS, cache_write(cache, 32, buff, sizeof(buff), LIBCACHE_WRITE_THROUGH));

	TEST_ASSERT_EQUAL_INT(sizeof(buff), cache_read(cache, 32, buff, sizeof(buff)));
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + 32, buff, sizeof(buff));
	TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, 0, DEV_SIZE));
	TEST_ASSERT_EQUAL_INT(EOK, cache_clean(cache, 0, DEV_SIZE));
}


/* Smallest geometry: lines of one byte in a single set, so the whole address is the tag */
TEST(test_geometry, singleSetOfByteLines)
{
	unsigned char buff[8];

	cache = cache_init(DEV_SIZE, 1, 4, &test_devOps);
	TEST_ASSERT_NOT_NULL(cache);

	for (uint64_t addr = 0; addr < DEV_SIZE - sizeof(buff); addr += 61) {
		TEST_ASSERT_EQUAL_INT(sizeof(buff), cache_read(cache, addr, buff, sizeof(buff)));
		TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + addr, buff, sizeof(buff));
	}
}


/* Geometry of a read cache of a NAND flash: lines of a page in many sets */
TEST(test_geometry, manySetsOfPageLines)
{
	static unsigned char buff[2 * NAND_PAGE_SIZE];
	unsigned int reads;

	cache = cache_init(DEV_SIZE, NAND_PAGE_SIZE, NAND_PAGES_CNT, &test_devOps);
	TEST_ASSERT_NOT_NULL(cache);

	TEST_ASSERT_EQUAL_INT(sizeof(buff), cache_read(cache, 100, buff, sizeof(buff)));
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + 100, buff, sizeof(buff));
	reads = test_dev.reads;
	TEST_ASSERT_EQUAL_UINT(3, reads);

	TEST_ASSERT_EQUAL_INT(DEV_SIZE - 300, cache_read(cache, 300, buff, sizeof(buff)));
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + 300, buff, DEV_SIZE - 300);
	TEST_ASSERT_EQUAL_UINT(reads, test_dev.reads);
}


TEST_GROUP_RUNNER(test_geometry)
{
	RUN_TEST_CASE(test_geometry, init_lineSizeNotPowerOfTwo);
	RUN_TEST_CASE(test_geometry, init_setsCountNotPowerOfTwo);
	RUN_TEST_CASE(test_geometry, init_noReadCallback);
	RUN_TEST_CASE(test_geometry, readOnlyCache);
	RUN_TEST_CASE(test_geometry, singleSetOfByteLines);
	RUN_TEST_CASE(test_geometry, manySetsOfPageLines);
}
