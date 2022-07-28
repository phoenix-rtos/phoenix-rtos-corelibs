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

// #include <sys/list.h>
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#define LIBCACHE_NUM_WAYS   4
#define LIBCACHE_ADDR_WIDTH 64


#define IS_VALID(f) (((f) & (1 << 0)) != 0 ? 1 : 0)
#define IS_DIRTY(f) (((f) & (1 << 1)) != 0 ? 1 : 0)

#define SET_VALID(f) \
	do { \
		(f) |= (1 << 0); \
	} while (0)
#define CLEAR_VALID \
	do { \
		(f) &= ~(1 << 0); \
	} while (0)

#define SET_DIRTY(f) \
	do { \
		(f) |= (1 << 1); \
	} while (0)
#define CLEAR_DIRTY \
	do { \
		(f) &= ~(1 << 1); \
	} while (0)


typedef struct cacheline_s cacheline_t;


struct cacheline_s {
	uint64_t tag;
	uint32_t *data;
	unsigned char flags;
	cacheline_t *prev, *next; /* Circular doubly linked list */
};


typedef struct cacheset_s {
	cacheline_t *timestamps;
	cacheline_t *tags[LIBCACHE_NUM_WAYS];
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


void cache_setInit(cacheset_t *set)
{
	set->timestamps = NULL;
	memset(set->tags, 0, sizeof(set->tags));
	memset(set->lines, 0, sizeof(set->lines));

	set->count = 0;
}


/* Generates mask of type uint64_t with numBits set to 1 */
uint64_t cache_genMask(int numBits)
{
	return ((uint64_t)1 << numBits) - (uint64_t)1;
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
		cache_setInit(&cache->sets[i]);
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
	int i, j;
	ssize_t wrt = 0;
	cacheline_t *temp = NULL, **linePtr = NULL;

	if (set->count < LIBCACHE_NUM_WAYS) {
		for (i = 0; i < LIBCACHE_NUM_WAYS; ++i) {
			if (!IS_VALID(set->lines[i].flags)) {
				set->lines[i] = *line;
				break;
			}
		}

		temp = &(set->lines[i]);

		for (j = 0; j < LIBCACHE_NUM_WAYS; ++j) {
			if (set->tags[j] == NULL) {
				set->tags[j] = temp;
				break;
			}
		}

		LIST_ADD(&set->timestamps, temp);
		set->count++;
	}
	else {
		temp = set->timestamps;
		LIST_REMOVE(&set->timestamps, temp);

		/* write-back */
		if (IS_DIRTY(temp->flags)) {
			wrt = (*writeCb)(offset, temp->data, sizeof(*temp->data), 1);
		}

		for (i = 0; i < LIBCACHE_NUM_WAYS; ++i) {
			if (set->lines[i].tag == temp->tag) {
				set->lines[i] = *line;
				break;
			}
		}

		LIST_ADD(&set->timestamps, temp);
	}

	switch (policy) {
		case 0:
			/* write-back */
			SET_DIRTY(set->lines[i].flags);
			break;
		case 1:
			/* write-through */
			wrt = (*writeCb)(offset, temp->data, sizeof(*temp->data), 1);
			break;
		default:
			break;
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
	unsigned char flags = 0;
	uint32_t *data = (uint32_t *)buffer;

	off_t setIndex = (addr >> cache->offBitNum) & cache->setMask;
	off_t offset = addr & cache->offMask;
	off_t tag = (addr & cache->tagMask);

	printf("set index: %lu\n", setIndex);
	printf("addr: %lu\n", addr);
	printf("tag: %lu\n", tag);

	SET_VALID(flags);

	cacheline_t line = { tag, data, flags };

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
		cache->readCb(offset, data, sizeof(*data), 1);
		printf("data read by callback: %u\n", *data);

		wrt = cache_write(cache, addr, data, -1);
	}
	/* cache hit */
	// *data = *ret;
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