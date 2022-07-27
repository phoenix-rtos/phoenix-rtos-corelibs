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

#include <sys/list.h>
// #include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#define LIBCACHE_NUM_WAYS   4
#define LIBCACHE_ADDR_WIDTH 64


typedef struct cacheline_s cacheline_t;


struct cacheline_s {
	uint64_t tag;
	uint32_t *data;
	unsigned char isValid;
	unsigned char isDirty;
	cacheline_t *prev, *next; /* Circular doubly linked list */
};


typedef struct cacheset_s {
	cacheline_t *timestamps;
	cacheline_t **tags;
	cacheline_t lines[LIBCACHE_NUM_WAYS];
	size_t count;
} cacheset_t;


struct cachectx_s {
	cacheset_t *sets;

	size_t numLines;
	size_t numSets;

	uint64_t tagMask;
	uint64_t setMask;
	uint64_t offMask;

	off_t offBitNum;
	off_t setBitsNum;
	off_t tagBitsNum;

	cache_readCb_t readCb;
	cache_writeCb_t writeCb;
};


cacheset_t *cache_setInit(void)
{
	cacheset_t *set = malloc(sizeof(cacheset_t));

	if (set == NULL) {
		return NULL;
	}

	set->timestamps = NULL;
	set->tags = calloc(LIBCACHE_NUM_WAYS, sizeof(cacheline_t *));
	memset(set->lines, 0, sizeof(set->lines));

	if (set->tags == NULL) {
		free(set);
		return NULL;
	}

	return set;
}


/* Generates mask of type uint64_t with numBits set to 1 */
uint64_t cache_genMask(int numBits)
{
	return ((uint64_t)1 << (uint64_t)numBits) - (uint64_t)1;
}


cachectx_t *cache_init(size_t size, size_t lineSize, cache_writeCb_t writeCb, cache_readCb_t readCb)
{
	int i = 0, j = 0;
	cacheset_t *set = NULL;

	cachectx_t *cache = malloc(sizeof(cachectx_t));

	if (cache == NULL) {
		return NULL;
	}

	if (size % lineSize != 0 || size % LIBCACHE_NUM_WAYS != 0) {
		fprintf(stderr, "Invalid size parameters: size in bytes must be multiple of lineSize and multiple of %d\n", LIBCACHE_NUM_WAYS);
		free(cache);
		return NULL;
	}

	cache->numLines = size / lineSize;

	cache->numSets = cache->numLines / LIBCACHE_NUM_WAYS;

	cache->sets = calloc(cache->numSets, sizeof(cacheset_t));

	if (cache->sets == NULL) {
		free(cache);
		return NULL;
	}

	for (; i < cache->numSets; ++i) {
		set = cache_setInit();

		if (set == NULL) {
			for (; j < i; ++j) {
				free(cache->sets[j].tags);
			}
			free(cache->sets);
			free(cache);

			return NULL;
		}
		cache->sets[i] = *set;

		cache->sets[i].count = 0;

		free(set);
	}

	cache->offBitNum = log2(lineSize);
	off_t setBits = log2(cache->numSets);
	off_t tagBits = LIBCACHE_ADDR_WIDTH - setBits - cache->offBitNum;

	cache->tagMask = cache_genMask(tagBits);
	cache->setMask = cache_genMask(setBits);
	cache->offMask = cache_genMask(cache->offBitNum);

	cache->readCb = readCb;
	cache->writeCb = writeCb;

	return cache;
}


int cache_deinit(cachectx_t *cache)
{
	int i = 0;

	for (; i < cache->numSets; ++i) {
		if (cache->sets != NULL) {
			free(cache->sets[i].tags);
		}
	}

	free(cache->sets);

	free(cache);

	return 0;
}


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


ssize_t cache_writeToSet(cache_writeCb_t writeCb, cacheset_t *set, cacheline_t *line, const off_t offset, int policy)
{
	int i = 0;
	ssize_t wrt = 0;
	cacheline_t *temp = NULL, *oldest = NULL, **linePtr = NULL;

	if (set->count < LIBCACHE_NUM_WAYS) {
		for (; i < LIBCACHE_NUM_WAYS; ++i) {
			if (set->lines[i].isValid != '1') {
				set->lines[i] = *line;
				break;
			}
		}

		temp = &(set->lines[i]);

		switch (policy) {
			case 0: {
				/* write-back */
				set->lines[i].isDirty = '1';
				break;
			}
			case 1: {
				/* write-through */
				wrt = (*writeCb)(offset, temp->data, sizeof *(temp->data), 1);
				break;
			}
			default: {
				break;
			}
		}

		for (i = 0; i < LIBCACHE_NUM_WAYS; ++i) {
			if (set->tags[i] == NULL) {
				set->tags[i] = temp;
				break;
			}
		}

		LIST_ADD(&set->timestamps, temp);
		set->count++;
	}
	else {
		oldest = set->timestamps;
		LIST_REMOVE(&set->timestamps, oldest);

		/* write-back */
		if (oldest->isDirty == '1') {
			wrt = (*writeCb)(offset, oldest->data, sizeof *(oldest->data), 1);
		}

		for (; i < LIBCACHE_NUM_WAYS; ++i) {
			if (set->lines[i].tag == oldest->tag) {
				set->lines[i] = *line;
				break;
			}
		}

		switch (policy) {
			case 0: {
				/* write-back */
				set->lines[i].isDirty = '1';
				break;
			}
			case 1: {
				/* write-through */
				wrt = (*writeCb)(offset, oldest->data, sizeof *(oldest->data), 1);
				break;
			}
			default: {
				break;
			}
		}

		LIST_ADD(&set->timestamps, oldest);
	}
	qsort(set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	return wrt;
}


uint32_t *cache_readFromSet(cacheset_t *set, uint64_t tag, off_t offset)
{
	uint32_t *ret = NULL;
	cacheline_t key, *keyPtr = &key, **linePtr = NULL;

	key.tag = tag;

	linePtr = bsearch(&keyPtr, set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	if (linePtr != NULL) {
		ret = (*linePtr)->data;

		LIST_REMOVE(&set->timestamps, (*linePtr));
		LIST_ADD(&set->timestamps, *linePtr);
	}

	return ret;
}


ssize_t cache_write(cachectx_t *cache, const off_t addr, void *buffer, int policy)
{
	ssize_t wrt = 0;
	uint32_t *data = (uint32_t *)buffer;

	off_t setIndex = (addr >> cache->offBitNum) & cache->setMask;
	off_t offset = addr & cache->offMask;
	off_t tag = (addr & cache->tagMask);

	printf("set index: %lu\n", setIndex);
	printf("addr: %lu\n", addr);
	printf("tag: %lu\n", tag);

	cacheline_t line = { tag, data, '1', '0' };

	return cache_writeToSet(cache->writeCb, &cache->sets[setIndex], &line, offset, policy);
}


ssize_t cache_read(cachectx_t *cache, const off_t addr, void *buffer)
{
	uint32_t *data = (uint32_t *)buffer, *ret;
	ssize_t wrt = 0;

	off_t setIndex = (addr >> cache->offBitNum) & cache->setMask;
	off_t offset = addr & cache->offMask;
	off_t tag = (addr & cache->tagMask);

	printf("set index: %lu\n", setIndex);
	printf("offset: %lu\n", offset);
	printf("tag: %lu\n", tag);

	ret = cache_readFromSet(&cache->sets[setIndex], tag, offset);

	/* cache miss */
	if (ret == NULL) {
		cache->readCb(offset, data, sizeof *data, 1);
		printf("data read by callback: %u\n", *data);

		wrt = cache_write(cache, addr, data, -1);

		ret = cache_readFromSet(&cache->sets[setIndex], tag, offset);
	}
	/* cache hit */
	*data = *ret;
	printf("data: %u\n", *data);

	return wrt;
}


int cache_flush(cachectx_t *cache, const off_t begAddr, const uint64_t endAddr)
{
}


int cache_invalidate(cachectx_t *cache, const off_t begAddr, const uint64_t endAddr)
{
}


#ifdef CACHE_DEBUG
void cache_print(cachectx_t *cache)
{
	int i = 0, j = 0;
	uint32_t data = 0;

	for (i = 0; i < cache->numSets; ++i) {
		printf("set: %d\n", i);
		for (j = 0; j < LIBCACHE_NUM_WAYS; ++j) {
			if (&cache->sets[i] != NULL) {
				if (cache->sets[i].lines[j].data == NULL) {
					data = 0;
				}
				else {
					data = *(cache->sets[i].lines[j].data);
				}
				printf("line: %d\t\t%20lu\t%20u\n", j, cache->sets[i].lines[j].tag, data);
			}
		}
	}
}
#endif