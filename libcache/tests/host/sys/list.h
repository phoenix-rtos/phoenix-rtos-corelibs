/*
 * Phoenix-RTOS
 *
 * libcache unit tests - stand-in for libphoenix <sys/list.h> on the build host
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBCACHE_HOST_SYS_LIST_H_
#define _LIBCACHE_HOST_SYS_LIST_H_

#include <stddef.h>


/* Circular doubly linked list, a new element becomes the tail (as in libphoenix) */
#define LIST_ADD(list, t) \
	do { \
		if (*(list) == NULL) { \
			(t)->next = (t); \
			(t)->prev = (t); \
			*(list) = (t); \
		} \
		else { \
			(t)->prev = (*(list))->prev; \
			(t)->next = *(list); \
			(*(list))->prev->next = (t); \
			(*(list))->prev = (t); \
		} \
	} while (0)


#define LIST_REMOVE(list, t) \
	do { \
		if ((t)->next == (t)) { \
			*(list) = NULL; \
		} \
		else { \
			(t)->prev->next = (t)->next; \
			(t)->next->prev = (t)->prev; \
			if (*(list) == (t)) { \
				*(list) = (t)->next; \
			} \
		} \
		(t)->next = NULL; \
		(t)->prev = NULL; \
	} while (0)


#endif
