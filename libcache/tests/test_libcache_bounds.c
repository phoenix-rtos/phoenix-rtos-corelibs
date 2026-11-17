/*
 * Phoenix-RTOS
 *
 * libcache unit tests - ends of the source memory
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

#define LINE_SIZE 16U
#define LINES_CNT 8U
#define DEV_LINES 40U
#define TAIL_SIZE 7U /* Last, incomplete line */
#define DEV_SIZE  (DEV_LINES * LINE_SIZE + TAIL_SIZE)

#define LINE_ADDR(line) ((uint64_t)(line) * LINE_SIZE)


static cachectx_t *cache;
static unsigned char buff[2 * LINE_SIZE];
static const unsigned char pattern[LINE_SIZE] = "0123456789abcdef";


TEST_GROUP(test_bounds);


TEST_SETUP(test_bounds)
{
	test_devInit(DEV_SIZE);
	for (size_t i = 0; i < DEV_SIZE; ++i) {
		test_dev.mem[i] = (unsigned char)(i * 7U + (i >> 4));
	}
	memset(buff, 0xa5, sizeof(buff));

	cache = cache_init(DEV_SIZE, LINE_SIZE, LINES_CNT, &test_devOps);
	TEST_ASSERT_NOT_NULL(cache);
}


TEST_TEAR_DOWN(test_bounds)
{
	if (cache != NULL) {
		TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(cache));
		cache = NULL;
	}
}


TEST(test_bounds, read_atEndReadsNothing)
{
	TEST_ASSERT_EQUAL_INT(0, cache_read(cache, DEV_SIZE, buff, sizeof(buff)));
	TEST_ASSERT_EQUAL_UINT(0, test_dev.reads);
	TEST_ASSERT_EACH_EQUAL_HEX8(0xa5, buff, sizeof(buff));

	TEST_ASSERT_EQUAL_INT(-EINVAL, cache_read(cache, DEV_SIZE + 1, buff, sizeof(buff)));
}


TEST(test_bounds, write_atEndWritesNothing)
{
	TEST_ASSERT_EQUAL_INT(0, cache_write(cache, DEV_SIZE, pattern, sizeof(pattern), LIBCACHE_WRITE_THROUGH));
	TEST_ASSERT_EQUAL_UINT(0, test_dev.reads);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.writes);

	TEST_ASSERT_EQUAL_INT(-EINVAL, cache_write(cache, DEV_SIZE + 1, pattern, sizeof(pattern), LIBCACHE_WRITE_THROUGH));
}


TEST(test_bounds, read_lastLineIsShort)
{
	TEST_ASSERT_EQUAL_INT(TAIL_SIZE + 2, cache_read(cache, DEV_SIZE - TAIL_SIZE - 2, buff, sizeof(buff)));
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + DEV_SIZE - TAIL_SIZE - 2, buff, TAIL_SIZE + 2);
	TEST_ASSERT_EACH_EQUAL_HEX8(0xa5, buff + TAIL_SIZE + 2, sizeof(buff) - TAIL_SIZE - 2);

	/* Nothing past the end of the source memory is requested */
	TEST_ASSERT_EQUAL_UINT(2, test_dev.reads);
	TEST_ASSERT_TRUE(test_dev.lastReadAddr == LINE_ADDR(DEV_LINES));
	TEST_ASSERT_EQUAL_UINT(TAIL_SIZE, test_dev.lastReadLen);

	TEST_ASSERT_EQUAL_INT(1, cache_read(cache, DEV_SIZE - 1, buff, sizeof(buff)));
	TEST_ASSERT_EQUAL_HEX8(test_dev.mem[DEV_SIZE - 1], buff[0]);
	TEST_ASSERT_EQUAL_UINT(2, test_dev.reads);
}


TEST(test_bounds, write_lastLineIsShort)
{
	TEST_ASSERT_EQUAL_INT(3, cache_write(cache, DEV_SIZE - 3, pattern, sizeof(pattern), LIBCACHE_WRITE_BACK));
	TEST_ASSERT_EQUAL_UINT(1, test_dev.reads);
	TEST_ASSERT_EQUAL_UINT(TAIL_SIZE, test_dev.lastReadLen);

	TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, 0, UINT64_MAX));
	TEST_ASSERT_EQUAL_UINT(1, test_dev.writes);
	TEST_ASSERT_TRUE(test_dev.lastWriteAddr == LINE_ADDR(DEV_LINES));
	TEST_ASSERT_EQUAL_UINT(TAIL_SIZE, test_dev.lastWriteLen);
	TEST_ASSERT_EQUAL_MEMORY(pattern, test_dev.mem + DEV_SIZE - 3, 3);
}


/* An empty range at the end is valid and must not walk the addresses past the source memory */
TEST(test_bounds, ranges_atEnd)
{
	TEST_ASSERT_EQUAL_INT(TAIL_SIZE, cache_write(cache, LINE_ADDR(DEV_LINES), pattern, TAIL_SIZE, LIBCACHE_WRITE_BACK));

	TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, DEV_SIZE, UINT64_MAX));
	TEST_ASSERT_EQUAL_INT(EOK, cache_clean(cache, DEV_SIZE, UINT64_MAX));
	TEST_ASSERT_EQUAL_INT(EOK, cache_invalidate(cache, DEV_SIZE, UINT64_MAX));
	TEST_ASSERT_EQUAL_UINT(0, test_dev.writes);

	TEST_ASSERT_EQUAL_INT(-EINVAL, cache_flush(cache, DEV_SIZE + 1, UINT64_MAX));
	TEST_ASSERT_EQUAL_INT(-EINVAL, cache_clean(cache, DEV_SIZE + 1, UINT64_MAX));
	TEST_ASSERT_EQUAL_INT(-EINVAL, cache_invalidate(cache, DEV_SIZE + 1, UINT64_MAX));

	/* The last byte belongs to the last line */
	TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, DEV_SIZE - 1, UINT64_MAX));
	TEST_ASSERT_EQUAL_UINT(1, test_dev.writes);
}


/* Source memory which does not fit in 32 bits: the content is a function of the address */
static ssize_t bigRead(uint64_t offset, void *buffer, size_t count, cache_devCtx_t *ctx)
{
	for (size_t i = 0; i < count; ++i) {
		uint64_t addr = offset + i;
		((unsigned char *)buffer)[i] = (unsigned char)(addr ^ (addr >> 8) ^ ((addr >> 32) * 0x5bU));
	}

	return (ssize_t)count;
}


TEST(test_bounds, sourceMemoryOver4GiB)
{
	static const cache_ops_t bigOps = { .readCb = bigRead };
	const uint64_t size = ((uint64_t)5 << 32) + 3 * LINE_SIZE;
	const uint64_t addrs[] = { LINE_ADDR(2) + 3, ((uint64_t)1 << 32) + LINE_ADDR(2) + 3, ((uint64_t)5 << 32) + LINE_ADDR(2) + 3 };
	unsigned char expected[LINE_SIZE];
	cachectx_t *big = cache_init(size, LINE_SIZE, LINES_CNT, &bigOps);

	TEST_ASSERT_NOT_NULL(big);

	/* Twice: the lines differ only in the address bits over 32 and all have to be kept apart in the cache */
	for (int i = 0; i < 2; ++i) {
		for (size_t j = 0; j < sizeof(addrs) / sizeof(addrs[0]); ++j) {
			(void)bigRead(addrs[j], expected, LINE_SIZE - 3, NULL);
			memset(buff, 0xa5, sizeof(buff));
			TEST_ASSERT_EQUAL_INT(LINE_SIZE - 3, cache_read(big, addrs[j], buff, LINE_SIZE - 3));
			TEST_ASSERT_EQUAL_MEMORY(expected, buff, LINE_SIZE - 3);
		}
	}

	TEST_ASSERT_EQUAL_INT(1, cache_read(big, size - 1, buff, sizeof(buff)));
	TEST_ASSERT_EQUAL_INT(0, cache_read(big, size, buff, sizeof(buff)));
	TEST_ASSERT_EQUAL_INT(-EINVAL, cache_read(big, size + 1, buff, sizeof(buff)));

	TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(big));
}


TEST_GROUP_RUNNER(test_bounds)
{
	RUN_TEST_CASE(test_bounds, read_atEndReadsNothing);
	RUN_TEST_CASE(test_bounds, write_atEndWritesNothing);
	RUN_TEST_CASE(test_bounds, read_lastLineIsShort);
	RUN_TEST_CASE(test_bounds, write_lastLineIsShort);
	RUN_TEST_CASE(test_bounds, ranges_atEnd);
	RUN_TEST_CASE(test_bounds, sourceMemoryOver4GiB);
}
