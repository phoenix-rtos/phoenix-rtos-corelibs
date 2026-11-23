/*
 * Phoenix-RTOS
 *
 * libalgo unit tests - a3buf shared by a concurrent writer and reader
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Smolaga
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <unity_fixture.h>
#include <a3buf.h>


#define FRAME_WORDS  64U
#define FRAMES_COUNT 200000U


typedef struct {
	uint32_t w[FRAME_WORDS];
} frame_t;


static struct {
	a3buf_t a3;
	frame_t frames[A3BUF_COUNT];
	unsigned int overwritten;
	unsigned int fetched;
	unsigned int torn;
	unsigned int reordered;
	unsigned int disturbed;
} common;


static void fillFrame(frame_t *f, uint32_t seq)
{
	for (unsigned int k = 0; k < FRAME_WORDS; ++k) {
		f->w[k] = seq * FRAME_WORDS + k;
	}
}


static void *writerThread(void *arg)
{
	for (uint32_t seq = 1U; seq <= FRAMES_COUNT; ++seq) {
		fillFrame(&common.frames[a3buf_wrIdx(&common.a3)], seq);
		if (a3buf_submit(&common.a3)) {
			common.overwritten++;
		}
	}

	return NULL;
}


static void *readerThread(void *arg)
{
	uint32_t last = 0U;

	while (last < FRAMES_COUNT) {
		bool fresh = a3buf_rdFetch(&common.a3);
		const frame_t *f = &common.frames[a3buf_rdIdx(&common.a3)];
		uint32_t seq = f->w[0] / FRAME_WORDS;

		for (unsigned int k = 1; k < FRAME_WORDS; ++k) {
			if (f->w[k] != seq * FRAME_WORDS + k) {
				common.torn++;
				break;
			}
		}

		if (fresh) {
			if (seq <= last) {
				common.reordered++;
			}
			common.fetched++;
		}
		else if (seq != last) {
			common.disturbed++;
		}
		last = seq;
	}

	return NULL;
}


TEST_GROUP(test_a3buf);


TEST_SETUP(test_a3buf)
{
	common.overwritten = 0;
	common.fetched = 0;
	common.torn = 0;
	common.reordered = 0;
	common.disturbed = 0;
	for (unsigned int i = 0; i < A3BUF_COUNT; ++i) {
		fillFrame(&common.frames[i], 0U);
	}
	a3buf_init(&common.a3);
}


TEST_TEAR_DOWN(test_a3buf)
{
}


TEST(test_a3buf, writerAndReader)
{
	pthread_t writer, reader;

	TEST_ASSERT_EQUAL_INT(0, pthread_create(&reader, NULL, readerThread, NULL));
	TEST_ASSERT_EQUAL_INT(0, pthread_create(&writer, NULL, writerThread, NULL));
	TEST_ASSERT_EQUAL_INT(0, pthread_join(writer, NULL));
	TEST_ASSERT_EQUAL_INT(0, pthread_join(reader, NULL));

	TEST_ASSERT_EQUAL_UINT(0U, common.torn);
	TEST_ASSERT_EQUAL_UINT(0U, common.reordered);
	TEST_ASSERT_EQUAL_UINT(0U, common.disturbed);
	TEST_ASSERT_EQUAL_UINT(FRAMES_COUNT, common.fetched + common.overwritten);
}


TEST_GROUP_RUNNER(test_a3buf)
{
	RUN_TEST_CASE(test_a3buf, writerAndReader);
}


static void runner(void)
{
	RUN_TEST_GROUP(test_a3buf);
}


int main(int argc, char *argv[])
{
	return (UnityMain(argc, (const char **)argv, runner) == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
