/*
 * Phoenix-RTOS
 *
 * Graphics library
 *
 * Copyright 2022 Phoenix Systems
 * Author: Malgorzata Wrobel
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _CACHE_H
#define _CACHE_H

#include <stdint.h>
#include <sys/types.h>

/* N-way set-associative cache parameters */
#ifndef LIBCACHE_NUM_WAYS
#define LIBCACHE_NUM_WAYS        2
#endif

#ifndef LIBCACHE_NUM_SETS
#define LIBCACHE_NUM_SETS        16
#endif

#ifndef LIBCACHE_CACHE_LINE_SIZE
#define LIBCACHE_CACHE_LINE_SIZE 64
#endif


/* Line of cache */
typedef struct _cacheline_t {
	uint64_t tag;
	uint32_t *data;
	unsigned char isValid;
} cacheline_t;

/* Set of cache */
typedef struct _cacheset_t {
	cacheline_t **timestamps;
	cacheline_t **tags;
	cacheline_t *lines;
} cacheset_t;


/* Creates line of cache */
cacheline_t cache_createLine(uint64_t tag, uint32_t *data);

/* Creates cache set */
cacheset_t *cache_createSet(void);

/* Destroys cache set and frees memory */
int cache_destroySet(cacheset_t *cacheSet);

/* Frees cache set */
void cache_freeSet(cacheset_t *cacheSet);

/* Compares values of cache line tags*/
int cache_compareTags(const void *lhs, const void *rhs);

/* Sorts cache lines in set by tag */
void cache_sortSetByTags(cacheset_t *cacheSet);

#endif