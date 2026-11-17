/*
 * Phoenix-RTOS
 *
 * libcache unit tests - line lookup, LRU order and write-back checked against a reference model
 *
 * The source memory is an array which counts the callback calls, so a test can tell
 * a hit from a miss and a line kept in the cache from a line written to the device.
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <unity_fixture.h>

#include <cache.h>

#include "test_libcache_dev.h"

#define LINE_SIZE 16U
#define NUM_WAYS  4U
#define NUM_SETS  2U
#define DEV_LINES 40U
#define DEV_SIZE  (DEV_LINES * LINE_SIZE)

#define LINE_ADDR(line) ((uint64_t)(line) * LINE_SIZE)


/* Reference model: way[0] is the least recently used line of a set */
typedef struct {
	size_t line;
	bool dirty;
} model_line_t;


typedef struct {
	model_line_t way[NUM_WAYS];
	size_t count;
} model_set_t;


static struct {
	model_set_t sets[NUM_SETS];
	unsigned char data[TEST_DEV_SIZE_MAX]; /* What a read through the cache has to return */
	size_t size;
	unsigned int reads;
	unsigned int writes;
} model;


static cachectx_t *cache;
static uint32_t rngState;


static uint32_t rng(void)
{
	rngState ^= rngState << 13;
	rngState ^= rngState >> 17;
	rngState ^= rngState << 5;

	return rngState;
}


static void fill(unsigned char *buff, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		buff[i] = (unsigned char)rng();
	}
}


static size_t lineLen(size_t line)
{
	size_t left = model.size - line * LINE_SIZE;

	return (left < LINE_SIZE) ? left : LINE_SIZE;
}


static void model_init(size_t size)
{
	memset(&model, 0, sizeof(model));
	model.size = size;
	memcpy(model.data, test_dev.mem, size);
}


static void model_remove(model_set_t *set, size_t way)
{
	memmove(&set->way[way], &set->way[way + 1], (set->count - way - 1) * sizeof(set->way[0]));
	set->count--;
}


static bool model_find(model_set_t *set, size_t line, size_t *way)
{
	for (size_t i = 0; i < set->count; ++i) {
		if (set->way[i].line == line) {
			*way = i;
			return true;
		}
	}

	return false;
}


/* Read (policy < 0) or write of len bytes of a line */
static void model_access(size_t line, size_t len, int policy)
{
	model_set_t *set = &model.sets[line % NUM_SETS];
	model_line_t entry = { .line = line, .dirty = false };
	size_t way;

	if (model_find(set, line, &way)) {
		entry = set->way[way];
		model_remove(set, way);
	}
	else {
		if (set->count == NUM_WAYS) {
			if (set->way[0].dirty) {
				model.writes++;
			}
			model_remove(set, 0);
		}
		/* A line which is overwritten as a whole is not fetched */
		if ((policy < 0) || (len < LINE_SIZE)) {
			model.reads++;
		}
	}

	if (policy == LIBCACHE_WRITE_BACK) {
		entry.dirty = true;
	}
	else if (policy == LIBCACHE_WRITE_THROUGH) {
		entry.dirty = false;
		model.writes++;
	}

	set->way[set->count++] = entry;
}


static void model_io(uint64_t addr, size_t len, int policy)
{
	while (len > 0) {
		size_t chunk = LINE_SIZE - (size_t)(addr % LINE_SIZE);
		if (chunk > len) {
			chunk = len;
		}
		model_access((size_t)(addr / LINE_SIZE), chunk, policy);
		addr += chunk;
		len -= chunk;
	}
}


typedef enum {
	range_flush,
	range_invalidate,
	range_clean
} range_op_t;


static void model_range(uint64_t beg, uint64_t end, range_op_t op)
{
	for (size_t line = (size_t)(beg / LINE_SIZE); LINE_ADDR(line) < end; ++line) {
		model_set_t *set = &model.sets[line % NUM_SETS];
		size_t way;
		bool cached = model_find(set, line, &way);

		if (cached && (op != range_invalidate) && set->way[way].dirty) {
			model.writes++;
			set->way[way].dirty = false;
		}
		if (cached && (op != range_flush)) {
			model_remove(set, way);
		}
		if (op == range_invalidate) {
			/* Not written data is dropped: the device content is the valid one */
			memcpy(model.data + LINE_ADDR(line), test_dev.mem + LINE_ADDR(line), lineLen(line));
		}
	}
}


static void checkCalls(void)
{
	TEST_ASSERT_EQUAL_UINT(model.reads, test_dev.reads);
	TEST_ASSERT_EQUAL_UINT(model.writes, test_dev.writes);
}


static void doRead(uint64_t addr, size_t len)
{
	unsigned char buff[4 * LINE_SIZE];

	TEST_ASSERT_LESS_OR_EQUAL(sizeof(buff), len);
	TEST_ASSERT_EQUAL_INT(len, cache_read(cache, addr, buff, len));
	TEST_ASSERT_EQUAL_MEMORY(model.data + addr, buff, len);
	model_io(addr, len, -1);
	checkCalls();
}


static void doWrite(uint64_t addr, size_t len, int policy)
{
	unsigned char buff[4 * LINE_SIZE];

	TEST_ASSERT_LESS_OR_EQUAL(sizeof(buff), len);
	fill(buff, len);
	TEST_ASSERT_EQUAL_INT(len, cache_write(cache, addr, buff, len, policy));
	memcpy(model.data + addr, buff, len);
	model_io(addr, len, policy);
	checkCalls();
}


static void checkWritten(uint64_t beg, uint64_t end)
{
	beg -= beg % LINE_SIZE;
	if (end > model.size) {
		end = model.size;
	}
	if (beg < end) {
		TEST_ASSERT_EQUAL_MEMORY(model.data + beg, test_dev.mem + beg, (size_t)(end - beg));
	}
}


static void doRange(uint64_t beg, uint64_t end, range_op_t op)
{
	if (op == range_flush) {
		TEST_ASSERT_EQUAL_INT(EOK, cache_flush(cache, beg, end));
	}
	else if (op == range_invalidate) {
		TEST_ASSERT_EQUAL_INT(EOK, cache_invalidate(cache, beg, end));
	}
	else {
		TEST_ASSERT_EQUAL_INT(EOK, cache_clean(cache, beg, end));
	}

	model_range(beg, end, op);
	checkCalls();
	if (op != range_invalidate) {
		checkWritten(beg, end);
	}
}


/* The device changes behind the cache (e.g. a flash page is programmed) and the range is invalidated */
static void doProgram(size_t line, size_t lines)
{
	fill(test_dev.mem + LINE_ADDR(line), (size_t)(LINE_ADDR(lines)));
	doRange(LINE_ADDR(line), LINE_ADDR(line + lines), range_invalidate);
}


static void setup(size_t size, size_t linesCnt)
{
	rngState = 0x2545f491U;
	test_devInit(size);
	fill(test_dev.mem, size);
	model_init(size);

	cache = cache_init(size, LINE_SIZE, linesCnt, &test_devOps);
	TEST_ASSERT_NOT_NULL(cache);
}


static void teardown(void)
{
	if (cache != NULL) {
		test_dev.failRead = false;
		test_dev.failWrite = false;
		TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(cache));
		cache = NULL;
	}
}


TEST_GROUP(test_model);


TEST_SETUP(test_model)
{
	setup(DEV_SIZE, NUM_SETS * NUM_WAYS);
}


TEST_TEAR_DOWN(test_model)
{
	teardown();
}


/* Lines 0, 2, 4, ... share the set 0 */
TEST(test_model, lru_evictionOrder)
{
	unsigned char buff[LINE_SIZE];

	for (size_t line = 0; line < 8; line += 2) {
		doRead(LINE_ADDR(line), LINE_SIZE);
	}
	TEST_ASSERT_EQUAL_UINT(4, test_dev.reads);

	/* A hit makes the line 0 the most recently used one, so the line 2 is evicted first */
	doRead(LINE_ADDR(0) + 3, 1);
	TEST_ASSERT_EQUAL_UINT(4, test_dev.reads);
	doRead(LINE_ADDR(8), LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(5, test_dev.reads);

	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(0), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(5, test_dev.reads);
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(2), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(6, test_dev.reads);
	TEST_ASSERT_TRUE(test_dev.lastReadAddr == LINE_ADDR(2));

	/* The line 2 took the place of the line 4, the lines 6, 8 and 0 are still there */
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(6), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(8), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(0), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(6, test_dev.reads);
	TEST_ASSERT_EQUAL_INT(LINE_SIZE, cache_read(cache, LINE_ADDR(4), buff, LINE_SIZE));
	TEST_ASSERT_EQUAL_UINT(7, test_dev.reads);
}


/* The other set is not affected by evictions */
TEST(test_model, lru_setsAreIndependent)
{
	doRead(LINE_ADDR(1), LINE_SIZE);
	for (size_t line = 0; line < 12; line += 2) {
		doRead(LINE_ADDR(line), LINE_SIZE);
	}
	TEST_ASSERT_EQUAL_UINT(7, test_dev.reads);
	doRead(LINE_ADDR(1), LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(7, test_dev.reads);
}


TEST(test_model, invalidate_keepsOtherLinesOfSet)
{
	for (size_t line = 10; line < 18; line += 2) {
		doRead(LINE_ADDR(line), LINE_SIZE);
	}

	doRange(LINE_ADDR(14), LINE_ADDR(15), range_invalidate);

	/* Lines with a lower and a higher address than the invalidated one are still hits */
	doRead(LINE_ADDR(10), LINE_SIZE);
	doRead(LINE_ADDR(12), LINE_SIZE);
	doRead(LINE_ADDR(16), LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(4, test_dev.reads);

	/* The freed way is used before anything gets evicted */
	doRead(LINE_ADDR(18), LINE_SIZE);
	doRead(LINE_ADDR(10), LINE_SIZE);
	doRead(LINE_ADDR(12), LINE_SIZE);
	doRead(LINE_ADDR(16), LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(5, test_dev.reads);
}


TEST(test_model, invalidate_reusedLineIsFound)
{
	doRead(LINE_ADDR(0), LINE_SIZE);
	doRange(LINE_ADDR(0), LINE_ADDR(1), range_invalidate);

	for (int i = 0; i < 2; ++i) {
		for (size_t line = 0; line < 8; line += 2) {
			doRead(LINE_ADDR(line), LINE_SIZE);
		}
	}
	TEST_ASSERT_EQUAL_UINT(5, test_dev.reads);
}


TEST(test_model, invalidate_noStaleDataAfterProgram)
{
	for (size_t line = 10; line < 18; line += 2) {
		doRead(LINE_ADDR(line), LINE_SIZE);
	}
	doProgram(14, 1);
	doRead(LINE_ADDR(10), LINE_SIZE);
	doRead(LINE_ADDR(14), LINE_SIZE);

	/* Every line in the range is dropped, so the new content of each one is read */
	doProgram(10, 8);
	for (size_t line = 10; line < 18; ++line) {
		doRead(LINE_ADDR(line), LINE_SIZE);
	}
}


TEST(test_model, writeBack_writtenOnEviction)
{
	doWrite(LINE_ADDR(2) + 5, 3, LIBCACHE_WRITE_BACK);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.writes);
	TEST_ASSERT_NOT_EQUAL(0, memcmp(model.data + LINE_ADDR(2), test_dev.mem + LINE_ADDR(2), LINE_SIZE));

	for (size_t line = 4; line < 12; line += 2) {
		doRead(LINE_ADDR(line), LINE_SIZE);
	}
	TEST_ASSERT_EQUAL_UINT(1, test_dev.writes);
	TEST_ASSERT_TRUE(test_dev.lastWriteAddr == LINE_ADDR(2));
	TEST_ASSERT_EQUAL_MEMORY(model.data + LINE_ADDR(2), test_dev.mem + LINE_ADDR(2), LINE_SIZE);
}


TEST(test_model, writeBack_flushWritesDirtyLinesInRangeOnce)
{
	doWrite(LINE_ADDR(2) + 9, 2 * LINE_SIZE, LIBCACHE_WRITE_BACK); /* Lines 2, 3 and 4 */
	doRead(LINE_ADDR(5), LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.writes);

	doRange(LINE_ADDR(3) + 1, LINE_ADDR(6), range_flush);
	TEST_ASSERT_EQUAL_UINT(2, test_dev.writes);
	TEST_ASSERT_NOT_EQUAL(0, memcmp(model.data + LINE_ADDR(2), test_dev.mem + LINE_ADDR(2), LINE_SIZE));

	doRange(0, DEV_SIZE, range_flush);
	TEST_ASSERT_EQUAL_UINT(3, test_dev.writes);
	doRange(0, DEV_SIZE, range_flush);
	TEST_ASSERT_EQUAL_UINT(3, test_dev.writes);

	/* Flushed lines stay in the cache */
	doRead(LINE_ADDR(2), 3 * LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(3, test_dev.reads);
}


TEST(test_model, writeBack_writtenOnDeinit)
{
	doWrite(LINE_ADDR(7) + 15, 2, LIBCACHE_WRITE_BACK);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.writes);

	TEST_ASSERT_EQUAL_INT(EOK, cache_deinit(cache));
	cache = NULL;

	TEST_ASSERT_EQUAL_UINT(2, test_dev.writes);
	TEST_ASSERT_EQUAL_MEMORY(model.data, test_dev.mem, DEV_SIZE);
}


TEST(test_model, writeThrough_writtenAtOnce)
{
	doWrite(LINE_ADDR(7) + 15, 2, LIBCACHE_WRITE_THROUGH);
	TEST_ASSERT_EQUAL_UINT(2, test_dev.writes);
	TEST_ASSERT_EQUAL_MEMORY(model.data, test_dev.mem, DEV_SIZE);

	/* Nothing is left to write */
	doRange(0, DEV_SIZE, range_flush);
	TEST_ASSERT_EQUAL_UINT(2, test_dev.writes);
}


TEST(test_model, invalidate_dropsNotWrittenData)
{
	unsigned char old[LINE_SIZE];

	memcpy(old, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
	doWrite(LINE_ADDR(3), LINE_SIZE, LIBCACHE_WRITE_BACK);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.reads);

	doRange(LINE_ADDR(3), LINE_ADDR(4), range_invalidate);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.writes);
	TEST_ASSERT_EQUAL_MEMORY(old, test_dev.mem + LINE_ADDR(3), LINE_SIZE);

	doRead(LINE_ADDR(3), LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(1, test_dev.reads);
}


TEST(test_model, clean_writesAndDrops)
{
	unsigned char old[LINE_SIZE];

	memcpy(old, test_dev.mem + LINE_ADDR(3), LINE_SIZE);
	doWrite(LINE_ADDR(3), LINE_SIZE, LIBCACHE_WRITE_BACK);
	doRead(LINE_ADDR(4), LINE_SIZE);

	doRange(LINE_ADDR(3), LINE_ADDR(5), range_clean);
	TEST_ASSERT_EQUAL_UINT(1, test_dev.writes);
	TEST_ASSERT_NOT_EQUAL(0, memcmp(old, test_dev.mem + LINE_ADDR(3), LINE_SIZE));

	/* Both the written and the clean line are gone from the cache */
	doRead(LINE_ADDR(3), 2 * LINE_SIZE);
	TEST_ASSERT_EQUAL_UINT(3, test_dev.reads);
}


/* Every offset in a line combined with lengths ending before, at and after the line boundaries */
TEST(test_model, partialLines)
{
	for (size_t offs = 0; offs < LINE_SIZE; ++offs) {
		for (size_t len = 1; len <= 3 * LINE_SIZE + 1; ++len) {
			uint64_t addr = LINE_ADDR(1 + (rng() % 30)) + offs;

			doWrite(addr, len, ((rng() & 1U) != 0U) ? LIBCACHE_WRITE_BACK : LIBCACHE_WRITE_THROUGH);
			doRead(addr - offs, offs + len);
			doRead(addr, len);
		}
	}

	doRange(0, DEV_SIZE, range_flush);
	TEST_ASSERT_EQUAL_MEMORY(model.data, test_dev.mem, DEV_SIZE);
}


/* readOnly: only reads and changes of the device followed by an invalidation (a read cache of a flash) */
static void randomOps(unsigned int ops, bool readOnly)
{
	for (unsigned int i = 0; i < ops; ++i) {
		uint32_t op = readOnly ? (((rng() % 4U) != 0U) ? 0U : 15U) : (rng() % 16U);
		uint64_t addr = rng() % model.size;
		size_t len = 1 + (rng() % (3 * LINE_SIZE));
		size_t line = (size_t)(addr / LINE_SIZE);

		if (len > model.size - addr) {
			len = (size_t)(model.size - addr);
		}

		if (op < 6U) {
			doRead(addr, len);
		}
		else if (op < 9U) {
			doWrite(addr, len, LIBCACHE_WRITE_BACK);
		}
		else if (op < 10U) {
			doWrite(addr, len, LIBCACHE_WRITE_THROUGH);
		}
		else if (op < 12U) {
			doRange(addr, addr + len, range_invalidate);
		}
		else if (op < 13U) {
			doRange(addr, addr + len, range_flush);
		}
		else if (op < 14U) {
			doRange(addr, addr + len, range_clean);
		}
		else {
			size_t lines = 1 + (rng() % 4U);
			if (lines > model.size / LINE_SIZE - line) {
				lines = model.size / LINE_SIZE - line;
			}
			if (lines > 0) {
				doProgram(line, lines);
			}
		}
	}

	doRange(0, model.size, range_flush);
	TEST_ASSERT_EQUAL_MEMORY(model.data, test_dev.mem, model.size);
}


TEST(test_model, randomOperations)
{
	randomOps(20000, false);
}


TEST(test_model, randomReadsOfChangingDevice)
{
	randomOps(20000, true);
	TEST_ASSERT_EQUAL_UINT(0, test_dev.writes);
}


/* The source memory ends in the middle of a line */
TEST(test_model, randomOperationsShortLastLine)
{
	teardown();
	setup(DEV_SIZE + 7, NUM_SETS * NUM_WAYS);
	randomOps(20000, false);
}


TEST_GROUP_RUNNER(test_model)
{
	RUN_TEST_CASE(test_model, lru_evictionOrder);
	RUN_TEST_CASE(test_model, lru_setsAreIndependent);
	RUN_TEST_CASE(test_model, invalidate_keepsOtherLinesOfSet);
	RUN_TEST_CASE(test_model, invalidate_reusedLineIsFound);
	RUN_TEST_CASE(test_model, invalidate_noStaleDataAfterProgram);
	RUN_TEST_CASE(test_model, writeBack_writtenOnEviction);
	RUN_TEST_CASE(test_model, writeBack_flushWritesDirtyLinesInRangeOnce);
	RUN_TEST_CASE(test_model, writeBack_writtenOnDeinit);
	RUN_TEST_CASE(test_model, writeThrough_writtenAtOnce);
	RUN_TEST_CASE(test_model, invalidate_dropsNotWrittenData);
	RUN_TEST_CASE(test_model, clean_writesAndDrops);
	RUN_TEST_CASE(test_model, partialLines);
	RUN_TEST_CASE(test_model, randomOperations);
	RUN_TEST_CASE(test_model, randomReadsOfChangingDevice);
	RUN_TEST_CASE(test_model, randomOperationsShortLastLine);
}
