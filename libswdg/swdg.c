/*
 * Phoenix-RTOS
 *
 * Software watchdog library
 *
 * Copyright 2022 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "swdg.h"

#include <sys/threads.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>

#if 0
#include <stdio.h>
#define DEBUG(fmt, ...) printf("swdg: " fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif


static struct {
	struct {
		swdg_callback_t callback;
		time_t limit;
		time_t last;
		int enabled;
	} *chan;
	size_t chancnt;
	handle_t lock;
	handle_t cond;
	unsigned char stack[1024] __attribute__ ((aligned(8)));
} swdg_common;


static void _swdg_reload(int no, time_t now)
{
	swdg_common.chan[no].last = now;
}


static void swdg_thread(void *arg)
{
	time_t timeout, diff, now, deadline;
	size_t i;

	mutexLock(swdg_common.lock);

	for (;;) {
		timeout = 0;
		gettime(&now, NULL);

		DEBUG("Now %llu\n", now);

		/* Calculate timeout */
		for (i = 0; i < swdg_common.chancnt; ++i) {
			if ((swdg_common.chan[i].enabled != 0) && (swdg_common.chan[i].limit != 0)) {
				deadline = swdg_common.chan[i].last + swdg_common.chan[i].limit;

				DEBUG("Channel %zu active, deadline  %llu\n", i, deadline);

				if (now >= deadline) {
					/* Watchdog deadline has passed, execute callback */
					DEBUG("Channel %zu timeout!\n", i);
					swdg_common.chan[i].callback(i);

					/* Reload if there was no reset */
					_swdg_reload(i, now);
				}
				else {
					diff = deadline - now;
					DEBUG("Channel %zu diff = %llu\n", i, diff);
					if ((timeout == 0) || (timeout > diff)) {
						timeout = diff;
					}
				}
			}
		}

		DEBUG("Sleep timeout = %llu\n", timeout);

		condWait(swdg_common.cond, swdg_common.lock, timeout);
	}
}


void swdg_reload(int no)
{
	time_t now;

	DEBUG("Channel %d reload\n", no);

	if ((no >= 0) && (no < swdg_common.chancnt)) {
		gettime(&now, NULL);
		mutexLock(swdg_common.lock);
		_swdg_reload(no, now);
		mutexUnlock(swdg_common.lock);
	}
}


void swdg_disable(int no)
{
	DEBUG("Channel %d disable\n", no);

	if ((no >= 0) && (no < swdg_common.chancnt)) {
		mutexLock(swdg_common.lock);
		swdg_common.chan[no].enabled = 0;
		mutexUnlock(swdg_common.lock);
	}
}


void swdg_enable(int no)
{
	time_t now;

	DEBUG("Channel %d enable\n", no);

	if ((no >= 0) && (no < swdg_common.chancnt)) {
		gettime(&now, NULL);
		mutexLock(swdg_common.lock);
		swdg_common.chan[no].enabled = 1;
		_swdg_reload(no, now);
		mutexUnlock(swdg_common.lock);
		condSignal(swdg_common.cond);
	}
}


void swdg_chanConfig(int no, swdg_callback_t callback, time_t limit)
{
	time_t now;

	DEBUG("Channel %d config: callback = 0x%p, limit = %llu\n", no, callback, limit);

	if ((no >= 0) && (no < swdg_common.chancnt)) {
		gettime(&now, NULL);
		mutexLock(swdg_common.lock);
		swdg_common.chan[no].callback = callback;
		swdg_common.chan[no].limit = limit;
		_swdg_reload(no, now);
		mutexUnlock(swdg_common.lock);
		condSignal(swdg_common.cond);
	}
}


int swdg_init(size_t chanCount, int priority)
{
	int err;

	if (priority < 0 || priority > 6 || chanCount == 0) { /* FIXME - no defines for min/max priority so hardcoded values */
		return -EINVAL;
	}

	err = mutexCreate(&swdg_common.lock);
	if (err != 0) {
		return err;
	}

	err = condCreate(&swdg_common.cond);
	if (err != 0) {
		resourceDestroy(swdg_common.lock);
		return err;
	}

	swdg_common.chan = calloc(chanCount, sizeof(*swdg_common.chan));
	if (swdg_common.chan == NULL) {
		resourceDestroy(swdg_common.cond);
		resourceDestroy(swdg_common.lock);
		return -ENOMEM;
	}

	swdg_common.chancnt = chanCount;

	err = beginthread(swdg_thread, priority, swdg_common.stack, sizeof(swdg_common.stack), NULL);
	if (err != 0) {
		resourceDestroy(swdg_common.cond);
		resourceDestroy(swdg_common.lock);
		swdg_common.chancnt = 0;
		free(swdg_common.chan);
	}

	return err;
}
