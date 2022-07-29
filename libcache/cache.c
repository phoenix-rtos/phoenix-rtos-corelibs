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
#include <assert.h>

#define LIBCACHE_NUM_WAYS   4
#define LIBCACHE_ADDR_WIDTH 64


#define IS_VALID(f) (((f) & (1 << 0)) != 0 ? 1 : 0)
#define IS_DIRTY(f) (((f) & (1 << 1)) != 0 ? 1 : 0)

#define SET_VALID(f) \
	do { \
		(f) |= (1 << 0); \
	} while (0)
#define CLEAR_VALID(f) \
	do { \
		(f) &= ~(1 << 0); \
	} while (0)

#define SET_DIRTY(f) \
	do { \
		(f) |= (1 << 1); \
	} while (0)
#define CLEAR_DIRTY(f) \
	do { \
		(f) &= ~(1 << 1); \
	} while (0)


typedef struct cacheline_s cacheline_t;


struct cacheline_s {
	uint64_t tag;
	void *data;
	uint64_t offset;
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

	size_t lineSize;
	size_t numLines;
	size_t numSets;

	uint64_t tagMask;
	uint64_t setMask;
	uint64_t offMask;

	uint64_t offBitNum;
	uint64_t setBitsNum;
	uint64_t tagBitsNum;

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

	cache->lineSize = lineSize;
	cache->numLines = size / cache->lineSize;

	cache->numSets = cache->numLines / LIBCACHE_NUM_WAYS;

	cache->sets = calloc(cache->numSets, sizeof(cacheset_t));

	if (cache->sets == NULL) {
		free(cache);
		return NULL;
	}

	for (; i < cache->numSets; ++i) {
		cache_setInit(&cache->sets[i]);
	}

	cache->offBitNum = log2(cache->lineSize);
	cache->setBitsNum = log2(cache->numSets);
	cache->tagBitsNum = LIBCACHE_ADDR_WIDTH - cache->setBitsNum - cache->offBitNum;

	cache->tagMask = cache_genMask(cache->tagBitsNum);
	cache->setMask = cache_genMask(cache->setBitsNum);
	cache->offMask = cache_genMask(cache->offBitNum);

	cache->readCb = readCb;
	cache->writeCb = writeCb;

	return cache;
}


int cache_deinit(cachectx_t *cache)
{
	int i, j;

	for (i = 0; i < cache->numSets; ++i) {
		for (j = 0; j < LIBCACHE_NUM_WAYS; ++j) {
			if (IS_VALID(cache->sets[i].lines[j].flags)) {
				free(cache->sets[i].lines[j].data);
				CLEAR_VALID(cache->sets[i].lines[j].flags);
			}
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


ssize_t cache_writeToSet(cache_writeCb_t writeCb, cacheset_t *set, cacheline_t *line, const uint64_t offset, size_t count, int policy)
{
	int i, j;
	ssize_t written = 0;
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
			written = (*writeCb)(offset, temp->data, sizeof(*temp->data), count);
			free(temp->data);
		}

		for (i = 0; i < LIBCACHE_NUM_WAYS; ++i) {
			if (IS_VALID(set->lines[i].flags) && (set->lines[i].tag == temp->tag)) {
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
			written = count;
			break;
		case 1:
			/* write-through */
			written = (*writeCb)(offset, temp->data, sizeof(*temp->data), count);
			break;
		default:
			break;
	}

	qsort(set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	return written;
}


void *cache_readFromSet(cacheset_t *set, uint64_t tag, uint64_t offset)
{
	unsigned char *buf = NULL;
	cacheline_t key, *keyPtr = &key, **linePtr = NULL;

	key.tag = tag;

	linePtr = bsearch(&keyPtr, set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	if (linePtr != NULL) {
		buf = (unsigned char *)(*linePtr)->data + offset;

		LIST_REMOVE(&set->timestamps, *linePtr);
		LIST_ADD(&set->timestamps, *linePtr);
	}

	return buf;
}


uint64_t cache_compOffset(const cachectx_t *cache, const uint64_t addr)
{
	return addr & cache->offMask;
}


uint64_t cache_compSet(const cachectx_t *cache, const uint64_t addr)
{
	return (addr >> cache->offBitNum) & cache->setMask;
}


uint64_t cache_compTag(const cachectx_t *cache, const uint64_t addr)
{
	return (addr >> (cache->offBitNum + cache->setBitsNum)) & cache->tagMask;
}


ssize_t cache_write(cachectx_t *cache, const uint64_t addr, void *buffer, size_t count, int policy)
{
	ssize_t wrt = 0;
	void *dataPtr = NULL;
	unsigned char *buf = NULL, *dest = NULL, flags = 0;
	uint64_t index = 0, offset = 0, tag = 0;
	cacheline_t line;

	index = cache_compSet(cache, addr);
	offset = cache_compOffset(cache, addr);
	tag = cache_compTag(cache, addr);

	dataPtr = malloc(sizeof(unsigned char) * cache->lineSize);

	dest = (unsigned char *)dataPtr + offset;
	/* TODO: make proper writing */
	if (count > cache->lineSize - offset) {
		count = cache->lineSize - offset;
	}
	memcpy(dest, buffer, count);

	SET_VALID(flags);

	line.tag = tag;
	line.data = dataPtr;
	line.flags = flags;
	line.offset = offset;

	return cache_writeToSet(cache->writeCb, &cache->sets[index], &line, offset, count, policy);
}


ssize_t cache_readOnMiss(cachectx_t *cache, const uint64_t addr, const uint64_t offset, void *buffer, size_t position, size_t count)
{
	ssize_t read = 0, written = 0;
	void *temp = NULL;

	temp = calloc(cache->lineSize, sizeof(unsigned char));

	read = cache->readCb(addr, temp, sizeof(*temp), cache->lineSize);

	assert(read == cache->lineSize);

	written = cache_write(cache, addr, temp, cache->lineSize, -1);

	memcpy(buffer + position, temp + offset, count);

	free(temp);

	return count;
}


/* TODO: add error handling */
ssize_t cache_read(cachectx_t *cache, uint64_t addr, void *buffer, size_t count)
{
	ssize_t position = 0;
	void *data = NULL;
	uint64_t index = 0, offset = 0, tag = 0;
	size_t pieceSize, left = 0, remainder = 0;

	/* TODO: add argument checks */

	offset = cache_compOffset(cache, addr);

	left = count;

	while (1) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		if (left == count) {
			if (count <= (cache->lineSize - offset)) {
				pieceSize = count;
			}
			else {
				pieceSize = cache->lineSize - offset;
				remainder = (left - pieceSize) % cache->lineSize;
			}
			addr -= offset;
		}
		else if (left == remainder) {
			pieceSize = remainder;
		}
		else {
			pieceSize = cache->lineSize;
		}

		data = cache_readFromSet(&cache->sets[index], tag, offset);

		/* cache miss */
		if (data == NULL) {
			position += cache_readOnMiss(cache, addr, offset, buffer, position, pieceSize);
		}
		else {
			memcpy(buffer + position, data, pieceSize);
			position += pieceSize;
		}

		left -= pieceSize;

		if (left == 0) {
			break;
		}

		addr += cache->lineSize;
	}

	return position;
}


int cache_flush(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
}


int cache_invalidate(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
}


#ifdef CACHE_DEBUG
void cache_print(cachectx_t *cache)
{
	int i, j;
	unsigned char *data = NULL;
	uint64_t offset;

	printf("\n\n");
	for (i = 0; i < cache->numSets; ++i) {
		printf("set: %2d\n", i);
		printf("-------\n");
		for (j = 0; j < LIBCACHE_NUM_WAYS; ++j) {
			if (&cache->sets[i] != NULL) {
				if (cache->sets[i].lines[j].data == NULL) {
					offset = 0;
					data = NULL;
				}
				else {
					offset = cache->sets[i].lines[j].offset;
					data = (unsigned char *)(cache->sets[i].lines[j].data + offset);
				}
				printf("line: %1d\t\t%20lu\t%5li\t%20s\n", j, cache->sets[i].lines[j].tag, offset, data);
			}
		}
	}
}
#endif