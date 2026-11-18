/*
 * Phoenix-RTOS
 *
 * libcache unit tests - failing and short transfers of the source memory
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
#define LINES_CNT 8U /* Two sets: lines 0, 2, 4, ... share the set 0 */
#define DEV_SIZE  (40U * LINE_SIZE)

#define LINE_ADDR(line) ((uint64_t)(line) * LINE_SIZE)


static cachectx_t *cache;
static unsigned char pattern[2 * LINE_SIZE];
static unsigned char buff[4 * LINE_SIZE];


TEST_GROUP(test_faults);


TEST_SETUP(test_faults)
{
	test_devInit(DEV_SIZE);
	for (size_t i = 0; i < DEV_SIZE; ++i) {
		test_dev.mem[i] = (unsigned char)(i * 7U + (i >> 4));
	}
	for (size_t i = 0; i < sizeof(pattern); ++i) {
		pattern[i] = (unsigned char)(0xc0U + i);
	}
	memset(buff, 0xa5, sizeof(buff));

	cache = cache_init(DEV_SIZE, LINE_SIZE, LINES_CNT, &test_devOps);
	TEST_ASSERT_NOT_NULL(cache);
}


TEST_TEAR_DOWN(test_faults)
{
	if (cache != NULL) {
		test_dev.failRead = false;
		test_dev.failWrite = false;
		TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(cache));
		cache = NULL;
	}
}


TEST(test_faults, read_errorCodeIsPropagated)
{
	static const int errors[] = { -EIO, -EBADMSG, -EUCLEAN, -ETIMEDOUT };

	test_dev.failRead = true;
	for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
		test_dev.failResult = errors[i];
		TEST_ASSERT_EQUAL_INT(errors[i], cache_read(cache, LINE_ADDR(3) + 2, buff, 4));
		TEST_ASSERT_EACH_EQUAL_HEX8(0xa5, buff, sizeof(buff));
	}
}


TEST(test_faults, read_failedLineIsNotCached)
{
	test_dev.failRead = true;
	test_dev.failResult = -EBADMSG;
	TEST_ASSERT_EQUAL_INT(-EBADMSG, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(1, test_dev.reads);

	test_dev.failRead = false;
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(2, test_dev.reads);
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + LINE_ADDR(3), buff, LINE_SIZE);
}


TEST(test_faults, read_errorOnFurtherLineKeepsFetchedOnes)
{
	test_dev.failRead = true;
	test_dev.failResult = -EBADMSG;
	test_dev.failBeg = LINE_ADDR(3);
	test_dev.failEnd = LINE_ADDR(4);

	TEST_ASSERT_EQUAL_INT(-EBADMSG, cache_read(cache, LINE_ADDR(2), buff, 3 * LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(2, test_dev.reads);

	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(2), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(2, test_dev.reads);
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + LINE_ADDR(2), buff, LINE_SIZE);
}


TEST(test_faults, read_noProgressIsAnError)
{
	test_dev.failRead = true;
	test_dev.failResult = 0;
	TEST_ASSERT_EQUAL_INT(-EIO, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(1, test_dev.reads);
}


TEST(test_faults, read_shortTransfersAreContinued)
{
	test_dev.chunk = 5;
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + LINE_ADDR(3), buff, LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(4, test_dev.reads);
	TEST_ASSERT_TRUE(test_dev.lastReadAddr == LINE_ADDR(3) + 15);
	TEST_ASSERT_EQUAL_UINT(1, test_dev.lastReadLen);
}


TEST(test_faults, write_shortTransfersAreContinued)
{
	test_dev.chunk = 5;
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_write(cache, LINE_ADDR(3), pattern, LINE_SIZE, LIBCACHE_WRITE_THROUGH));
	TEST_ASSERT_EQUAL_MEMORY(pattern, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(4, test_dev.writes);
	TEST_ASSERT_TRUE(test_dev.lastWriteAddr == LINE_ADDR(3) + 15);
	TEST_ASSERT_EQUAL_UINT(1, test_dev.lastWriteLen);
}


TEST(test_faults, write_fetchErrorCodeIsPropagated)
{
	test_dev.failRead = true;
	test_dev.failResult = -EBADMSG;
	TEST_ASSERT_EQUAL_INT(-EBADMSG, cache_write(cache, LINE_ADDR(3) + 1, pattern, 2, LIBCACHE_WRITE_THROUGH));
	TEST_ASSERT_EQUAL_UINT(0, test_dev.writes);

	/* The line which could not be fetched is not left in the cache with the written bytes */
	test_dev.failRead = false;
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + LINE_ADDR(3), buff, LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(2, test_dev.reads);
}


TEST(test_faults, writeThrough_errorCodeIsPropagated)
{
	test_dev.failWrite = true;
	test_dev.failResult = -ENOSPC;
	TEST_ASSERT_EQUAL_INT(-ENOSPC, cache_write(cache, LINE_ADDR(3), pattern, LINE_SIZE, LIBCACHE_WRITE_THROUGH));
	TEST_ASSERT_EQUAL_UINT(1, test_dev.writes);
}


TEST(test_faults, writeThrough_failedLineIsDropped)
{
	unsigned char orig[LINE_SIZE];

	memcpy(orig, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
	test_dev.failWrite = true;
	test_dev.failResult = -EIO;
	TEST_ASSERT_EQUAL_INT(-EIO, cache_write(cache, LINE_ADDR(3), pattern, LINE_SIZE, LIBCACHE_WRITE_THROUGH));

	/* Reads get what the source holds, not the data which failed to be written */
	test_dev.failWrite = false;
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(orig, buff, LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(1, test_dev.reads);

	/* The cache does not retry the write behind the caller's back */
	TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, 0, DEV_SIZE));
	TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(cache));
	cache = NULL;
	TEST_ASSERT_EQUAL_UINT(1, test_dev.writes);
	TEST_ASSERT_EQUAL_MEMORY(orig, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
}


TEST(test_faults, writeThrough_failedCachedLineIsDropped)
{
	unsigned char orig[LINE_SIZE];

	memcpy(orig, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));

	test_dev.failWrite = true;
	test_dev.failResult = -EIO;
	TEST_ASSERT_EQUAL_INT(-EIO, cache_write(cache, LINE_ADDR(3) + 2, pattern, 4, LIBCACHE_WRITE_THROUGH));

	test_dev.failWrite = false;
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(orig, buff, LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(2, test_dev.reads);
}


TEST(test_faults, writeThrough_errorKeepsWrittenLines)
{
	test_dev.failWrite = true;
	test_dev.failResult = -EIO;
	test_dev.failBeg = LINE_ADDR(4);
	test_dev.failEnd = LINE_ADDR(5);
	TEST_ASSERT_EQUAL_INT(-EIO, cache_write(cache, LINE_ADDR(3), pattern, 2 * LINE_SIZE, LIBCACHE_WRITE_THROUGH));
	TEST_ASSERT_EQUAL_MEMORY(pattern, test_dev.mem + LINE_ADDR(3), LINE_SIZE);

	/* The written line is a hit, the failed one is read again */
	test_dev.failWrite = false;
	TEST_ASSERT_EQUAL_INT(2 * LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, 2 * LINE_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + LINE_ADDR(3), buff, 2 * LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(1, test_dev.reads);
	TEST_ASSERT_TRUE(test_dev.lastReadAddr == LINE_ADDR(4));
}


TEST(test_faults, writeThrough_errorKeepsNotWrittenData)
{
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_write(cache, LINE_ADDR(3), pattern, LINE_SIZE, LIBCACHE_WRITE_BACK));

	test_dev.failWrite = true;
	test_dev.failResult = -ENOSPC;
	TEST_ASSERT_EQUAL_INT(-ENOSPC, cache_write(cache, LINE_ADDR(3), pattern + 1, 2, LIBCACHE_WRITE_THROUGH));

	/* The line held write-back data, which only the cache has: it stays dirty, so a flush writes it */
	test_dev.failWrite = false;
	TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, 0, DEV_SIZE));
	TEST_ASSERT_EQUAL_UINT(2, test_dev.writes);
	TEST_ASSERT_EQUAL_MEMORY(pattern + 1, test_dev.mem + LINE_ADDR(3), 2);
	TEST_ASSERT_EQUAL_MEMORY(pattern + 2, test_dev.mem + LINE_ADDR(3) + 2, LINE_SIZE - 2);
}


TEST(test_faults, flush_errorCodeIsPropagated)
{
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_write(cache, LINE_ADDR(3), pattern, LINE_SIZE, LIBCACHE_WRITE_BACK));

	test_dev.failWrite = true;
	test_dev.failResult = -ENOSPC;
	TEST_ASSERT_EQUAL_INT(-ENOSPC, cache_flush(cache, 0, DEV_SIZE));
	test_dev.failResult = 0;
	TEST_ASSERT_EQUAL_INT(-EIO, cache_flush(cache, 0, DEV_SIZE));
	TEST_ASSERT_EQUAL_UINT(2, test_dev.writes);

	test_dev.failWrite = false;
	TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, 0, DEV_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(pattern, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
}


TEST(test_faults, clean_errorKeepsLine)
{
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_write(cache, LINE_ADDR(3), pattern, LINE_SIZE, LIBCACHE_WRITE_BACK));

	test_dev.failWrite = true;
	test_dev.failResult = -ENOSPC;
	TEST_ASSERT_EQUAL_INT(-ENOSPC, cache_clean(cache, 0, DEV_SIZE));

	/* Not written data is still served from the cache */
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(pattern, buff, LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.reads);

	test_dev.failWrite = false;
	TEST_ASSERT_EQUAL_INT(EOK, cache_clean(cache, 0, DEV_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(pattern, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(3), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(1, test_dev.reads);
}


TEST(test_faults, eviction_writeErrorCodeIsPropagated)
{
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_write(cache, LINE_ADDR(0), pattern, LINE_SIZE, LIBCACHE_WRITE_BACK));
	for (size_t line = 2; line < 8; line += 2) {
		TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(line), buff, LINE_SIZE));
	}

	/* The set is full and its least recently used line has to be written before it is replaced */
	test_dev.failWrite = true;
	test_dev.failResult = -ENOSPC;
	TEST_ASSERT_EQUAL_INT(-ENOSPC, cache_read(cache, LINE_ADDR(8), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_INT(-ENOSPC, cache_write(cache, LINE_ADDR(8), pattern, 1, LIBCACHE_WRITE_BACK));
	TEST_ASSERT_EQUAL_UINT(2, test_dev.writes);
	TEST_ASSERT_EQUAL_UINT(3, test_dev.reads);

	/* Nothing is lost: the line is still there and gets written once the device works again */
	test_dev.failWrite = false;
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(8), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(test_dev.mem + LINE_ADDR(8), buff, LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(3, test_dev.writes);
	TEST_ASSERT_EQUAL_MEMORY(pattern, test_dev.mem + LINE_ADDR(0), LINE_SIZE);
}


TEST(test_faults, deinit_writeErrorKeepsCache)
{
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_write(cache, LINE_ADDR(3), pattern, LINE_SIZE, LIBCACHE_WRITE_BACK));
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_write(cache, LINE_ADDR(5), pattern + 1, LINE_SIZE, LIBCACHE_WRITE_BACK));

	/* The second dirty line cannot be written */
	test_dev.failWrite = true;
	test_dev.failResult = -ENOSPC;
	test_dev.failBeg = LINE_ADDR(5);
	TEST_ASSERT_EQUAL_INT(-ENOSPC, cache_deinit(cache));

	/* The cache is still usable and holds the data */
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(5), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(pattern + 1, buff, LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.reads);

	test_dev.failWrite = false;
	TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(cache));
	cache = NULL;
	TEST_ASSERT_EQUAL_MEMORY(pattern, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
	TEST_ASSERT_EQUAL_MEMORY(pattern + 1, test_dev.mem + LINE_ADDR(5), LINE_SIZE);
}


TEST_GROUP_RUNNER(test_faults)
{
	RUN_TEST_CASE(test_faults, read_errorCodeIsPropagated);
	RUN_TEST_CASE(test_faults, read_failedLineIsNotCached);
	RUN_TEST_CASE(test_faults, read_errorOnFurtherLineKeepsFetchedOnes);
	RUN_TEST_CASE(test_faults, read_noProgressIsAnError);
	RUN_TEST_CASE(test_faults, read_shortTransfersAreContinued);
	RUN_TEST_CASE(test_faults, write_shortTransfersAreContinued);
	RUN_TEST_CASE(test_faults, write_fetchErrorCodeIsPropagated);
	RUN_TEST_CASE(test_faults, writeThrough_errorCodeIsPropagated);
	RUN_TEST_CASE(test_faults, writeThrough_failedLineIsDropped);
	RUN_TEST_CASE(test_faults, writeThrough_failedCachedLineIsDropped);
	RUN_TEST_CASE(test_faults, writeThrough_errorKeepsWrittenLines);
	RUN_TEST_CASE(test_faults, writeThrough_errorKeepsNotWrittenData);
	RUN_TEST_CASE(test_faults, flush_errorCodeIsPropagated);
	RUN_TEST_CASE(test_faults, clean_errorKeepsLine);
	RUN_TEST_CASE(test_faults, eviction_writeErrorCodeIsPropagated);
	RUN_TEST_CASE(test_faults, deinit_writeErrorKeepsCache);
}
