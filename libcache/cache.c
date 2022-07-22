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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#define LIBCACHE_NUM_WAYS   4
#define LIBCACHE_ADDR_WIDTH 64


typedef struct cacheline_s cacheline_t;


struct cacheline_s {
	uint64_t tag;
	uint32_t *data;
	unsigned char *isValid;
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
	uint64_t offsetMask;

	off_t offsetWidth;
	off_t setBits;
	off_t tagBits;

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

	cache->offsetWidth = log2(lineSize);
	off_t setBits = log2(cache->numSets);
	off_t tagBits = LIBCACHE_ADDR_WIDTH - setBits - cache->offsetWidth;

	cache->tagMask = cache_genMask(tagBits);
	cache->setMask = cache_genMask(setBits);
	cache->offsetMask = cache_genMask(cache->offsetWidth);

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


ssize_t cache_read(cachectx_t *cache, const off_t addr, void *buffer)
{
}


ssize_t cache_write(cachectx_t *cache, const off_t addr, void *buffer)
{
}


int cache_flush(cachectx_t *cache, const off_t begAddr, const uint64_t endAddr)
{
}


int cache_invalidate(cachectx_t *cache, const off_t begAddr, const uint64_t endAddr)
{
}