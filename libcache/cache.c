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

#include "cache.h"

#include <stdlib.h>
#include <string.h>


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
	cacheline_t lines[LIBCACHE_NUM_WAYS];
} cacheset_t;


/* Frees cache set */
void cache_freeSet(cacheset_t *cacheSet)
{
	free(cacheSet->tags);
	free(cacheSet->timestamps);

	free(cacheSet);
}


/* Creates cache set */
cacheset_t *cache_createSet(void)
{
	cacheset_t *cacheSet = malloc(sizeof(cacheset_t));

	if (cacheSet == NULL) {
		return NULL;
	}

	cacheSet->timestamps = calloc(LIBCACHE_NUM_WAYS, sizeof(cacheline_t *));
	cacheSet->tags = calloc(LIBCACHE_NUM_WAYS, sizeof(cacheline_t *));
	memset(cacheSet->lines, 0, sizeof(cacheSet->lines));

	if (cacheSet->timestamps == NULL || cacheSet->tags == NULL) {
		cache_freeSet(cacheSet);
		return NULL;
	}

	return cacheSet;
}


/* Compares values of cache line tags */
int cache_compareTags(const void *lhs, const void *rhs)
{
	int ret = 0;

	cacheline_t *lhsLine = (cacheline_t *)lhs, *rhsLine = (cacheline_t *)rhs;

	if (lhsLine->tag < rhsLine->tag) {
		ret = -1;
	}
	else if (lhsLine->tag == rhsLine->tag) {
		ret = 0;
	}
	else {
		ret = 1;
	}

	return ret;
}


/* Sorts cache lines in set by tag */
void cache_sortSetByTags(cacheset_t *cacheSet)
{
	qsort(cacheSet->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);
}