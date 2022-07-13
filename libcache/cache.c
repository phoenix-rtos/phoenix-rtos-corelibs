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

cacheline_t cache_createLine(uint64_t tag, uint32_t *data)
{
	cacheline_t line = { tag, data, '1' };

	return line;
}

cacheset_t *cache_createSet(void)
{
	cacheset_t *cacheSet = malloc(sizeof(cacheset_t));

	if (cacheSet == NULL) {
		return NULL;
	}

	cacheSet->timestamps = calloc(NUM_WAYS, sizeof(cacheline_t *));
	cacheSet->tags = calloc(NUM_WAYS, sizeof(cacheline_t *));
	cacheSet->lines = calloc(NUM_WAYS, sizeof(cacheline_t));

	if (cacheSet->timestamps == NULL || cacheSet->tags == NULL || cacheSet->lines == NULL) {
		cache_freeSet(cacheSet);
		return NULL;
	}

	return cacheSet;
}

int cache_destroySet(cacheset_t *cacheSet)
{
	if (cacheSet == NULL) {
		return -1;
	}

	cache_freeSet(cacheSet);

	return 0;
}

void cache_freeSet(cacheset_t *cacheSet)
{
	free(cacheSet->lines);
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