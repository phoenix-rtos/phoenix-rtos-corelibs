/*
 * Phoenix-RTOS
 *
 * Cache library
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
#include <stdint.h>
#include <math.h>


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


/* Cache table */
struct _cachetable_t {
	cacheset_t *sets;
	uint64_t tagMask;
	uint64_t setMask;
	uint64_t offsetMask;
	int numSets;
	int offsetWidth;
};


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

	cacheline_t **lhsLine = (cacheline_t **)lhs;
	cacheline_t **rhsLine = (cacheline_t **)rhs;

	if (*lhsLine == NULL && *rhsLine == NULL) {
		ret = 0;
	}
	else if (*lhsLine == NULL && *rhsLine != NULL) {
		ret = -1;
	}
	else if (*lhsLine != NULL && *rhsLine == NULL) {
		ret = 1;
	}
	else {
		if ((*lhsLine)->tag < (*rhsLine)->tag) {
			ret = -1;
		}
		else if ((*lhsLine)->tag == (*rhsLine)->tag) {
			ret = 0;
		}
		else {
			ret = 1;
		}
	}

	return ret;
}


/* Adds a line of cache to cache set */
void cache_addToSet(cacheset_t *cacheSet, cacheline_t *cacheLine)
{
	int i = 0;
	cacheline_t *oldest = NULL, **linePtr = NULL, *temp = NULL;

	oldest = cacheSet->timestamps[0];

	if (oldest == NULL) {
		for (i = 0; i < LIBCACHE_NUM_WAYS; ++i) {
			if (cacheSet->lines[i].isValid != '1') {
				cacheSet->lines[i] = *cacheLine;
				break;
			}
		}

		temp = &(cacheSet->lines[i]);

		for (i = 0; i < LIBCACHE_NUM_WAYS; ++i) {
			if (cacheSet->tags[i] == NULL) {
				cacheSet->tags[i] = temp;
				break;
			}
		}
	}
	else {
		for (i = 0; i < LIBCACHE_NUM_WAYS; ++i) {
			if (cacheSet->lines[i].tag == oldest->tag) {
				cacheSet->lines[i] = *cacheLine;
				break;
			}
		}

		temp = &(cacheSet->lines[i]);

		linePtr = bsearch(&oldest, cacheSet->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

		*linePtr = cacheLine;
	}

	for (; i < LIBCACHE_NUM_WAYS - 1; ++i) {
		cacheSet->timestamps[i] = cacheSet->timestamps[i + 1];
	}

	cacheSet->timestamps[LIBCACHE_NUM_WAYS - 1] = temp;

	qsort(cacheSet->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);
}


/* Search a line of cache in a cache set */
uint32_t *cache_searchInSet(cacheset_t *cacheSet, uint64_t tag)
{
	int i = 0, found = 0;
	uint32_t *ret = NULL;
	cacheline_t key, *keyPtr = &key, **linePtr = NULL;

	key.tag = tag;

	linePtr = bsearch(&keyPtr, cacheSet->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	if (linePtr != NULL) {
		ret = (*linePtr)->data;

		for (i = 0; i < LIBCACHE_NUM_WAYS - 1; ++i) {
			if (cacheSet->timestamps[i] == *linePtr) {
				found = 1;
			}
			if (found == 1) {
				cacheSet->timestamps[i] = cacheSet->timestamps[i + 1];
			}
		}

		cacheSet->timestamps[LIBCACHE_NUM_WAYS - 1] = *linePtr;
	}

	return ret;
}


/* Generates mask of type uint64_t with numBits set to 1 */
uint64_t cache_genMask(int numBits)
{
	return ((uint64_t)1 << (uint64_t)numBits) - (uint64_t)1;
}


cachetable_t *cache_create(void)
{
	int i = 0, j = 0;

	cachetable_t *cache = malloc(sizeof(cachetable_t));
	cacheset_t *set = NULL;

	if (cache == NULL) {
		return NULL;
	}

	int numLines = LIBCACHE_MEM_SIZE / LIBCACHE_CACHE_LINE_SIZE;
	cache->numSets = numLines / LIBCACHE_NUM_WAYS;

	cache->sets = calloc(cache->numSets, sizeof(cacheset_t));

	if (cache->sets == NULL) {
		free(cache);

		return NULL;
	}

	for (; i < cache->numSets; ++i) {
		set = cache_createSet();

		if (set == NULL) {
			for (; j < i; ++j) {
				free(cache->sets[j].timestamps);
				free(cache->sets[j].tags);
			}

			free(cache->sets);
			free(cache);
		}

		cache->sets[i] = *set;

		free(set);
	}

	cache->offsetWidth = log2(LIBCACHE_CACHE_LINE_SIZE);
	int setBits = log2(cache->numSets);
	int tagBits = LIBCACHE_ADDR_WIDTH - setBits - cache->offsetWidth;


	cache->tagMask = cache_genMask(tagBits);
	cache->setMask = cache_genMask(setBits);
	cache->offsetMask = cache_genMask(cache->offsetWidth);

	return cache;
}


void cache_add(cachetable_t *cache, const uint64_t addr, uint32_t *data)
{
	uint64_t set = addr >> cache->offsetWidth;
	set = (set & cache->setMask);
	/* uint64_t offset = addr & cache->offsetMask; */
	uint64_t tag = (addr ^ cache->tagMask);

	cacheline_t cacheLine = { tag, data, '1' };

	cache_addToSet(&cache->sets[set], &cacheLine);
}


uint32_t *cache_search(cachetable_t *cache, const uint64_t addr)
{
	uint64_t set = addr >> cache->offsetWidth;
	set = (set & cache->setMask);
	/* uint64_t offset = addr & cache->offsetMask; */
	uint64_t tag = (addr ^ cache->tagMask);

	return cache_searchInSet(&cache->sets[set], tag);
}
