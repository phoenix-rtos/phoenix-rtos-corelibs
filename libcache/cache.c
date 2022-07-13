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


void cache_freeSet(cacheset_t *cacheSet)
{
	free(cacheSet->tags);
	free(cacheSet->timestamps);

	free(cacheSet);
}


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


void cache_sortSetByTags(cacheset_t *cacheSet)
{
	qsort(cacheSet->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);
}