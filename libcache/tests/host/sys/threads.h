/*
 * Phoenix-RTOS
 *
 * libcache unit tests - stand-in for libphoenix <sys/threads.h> on the build host
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBCACHE_HOST_SYS_THREADS_H_
#define _LIBCACHE_HOST_SYS_THREADS_H_

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#ifndef EOK
#define EOK 0
#endif


typedef pthread_mutex_t *handle_t;


static inline int mutexCreate(handle_t *h)
{
	*h = malloc(sizeof(pthread_mutex_t));
	if (*h == NULL) {
		return -ENOMEM;
	}

	return -pthread_mutex_init(*h, NULL);
}


static inline int mutexLock(handle_t h)
{
	return -pthread_mutex_lock(h);
}


static inline int mutexUnlock(handle_t h)
{
	return -pthread_mutex_unlock(h);
}


static inline int resourceDestroy(handle_t h)
{
	int err = -pthread_mutex_destroy(h);

	free(h);

	return err;
}


#endif
