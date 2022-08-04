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


#define LOG2(x) ((uint8_t)(8 * sizeof(unsigned long) - __builtin_clzl((x)) - 1))


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

	uint8_t offBitsNum;
	uint8_t setBitsNum;
	uint8_t tagBitsNum;

	cache_readCb_t readCb;
	cache_writeCb_t writeCb;
};


/* Generates mask of type uint64_t with numBits set to 1 */
static uint64_t cache_genMask(int numBits)
{
	return ((uint64_t)1 << numBits) - 1;
}


cachectx_t *cache_init(size_t size, size_t lineSize, cache_writeCb_t writeCb, cache_readCb_t readCb)
{
	cachectx_t *cache = NULL;

	if (size == 0 || lineSize == 0 || size < lineSize) {
		return NULL;
	}

	if (size % lineSize != 0 || (size / lineSize) % LIBCACHE_NUM_WAYS != 0) {
		return NULL;
	}

	cache = malloc(sizeof(cachectx_t));

	if (cache == NULL) {
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

	cache->offBitsNum = LOG2(cache->lineSize);
	cache->setBitsNum = LOG2(cache->numSets);
	cache->tagBitsNum = LIBCACHE_ADDR_WIDTH - cache->setBitsNum - cache->offBitsNum;

	cache->tagMask = cache_genMask(cache->tagBitsNum);
	cache->setMask = cache_genMask(cache->setBitsNum);
	cache->offMask = cache_genMask(cache->offBitsNum);

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


static int cache_compareTags(const void *lhs, const void *rhs)
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


static ssize_t cache_executePolicy(cache_writeCb_t writeCb, cacheline_t *linePtr, const uint64_t addr, size_t count, int policy)
{
	ssize_t written = 0;

	if (IS_DIRTY(linePtr->flags)) {
		written = (*writeCb)(addr, linePtr->data, sizeof(*linePtr->data), count);
		CLEAR_DIRTY(linePtr->flags);
	}

	switch (policy) {
		case 0:
			/* write-back */
			SET_DIRTY(linePtr->flags);
			written = count;
			break;
		case 1:
			/* write-through */
			written = (*writeCb)(addr, linePtr->data, sizeof(*linePtr->data), count);
			break;
		default:
			break;
	}

	return written;
}


static ssize_t cache_writeToSet(cache_writeCb_t writeCb, cacheset_t *set, cacheline_t *line, const uint64_t offset, size_t count, int policy)
{
	int i, j;
	ssize_t written = 0;
	cacheline_t *temp = NULL;

	if (set->count < LIBCACHE_NUM_WAYS) {
		set->count++;

		for (i = 0; i < set->count; ++i) {
			if (!IS_VALID(set->lines[i].flags)) {
				set->lines[i] = *line;
				break;
			}
		}

		temp = &(set->lines[i]);

		for (j = 0; j < set->count; ++j) {
			if (set->tags[j] == NULL) {
				set->tags[j] = temp;
				break;
			}
		}

		LIST_ADD(&set->timestamps, temp);
	}
	else {
		temp = set->timestamps;
		LIST_REMOVE(&set->timestamps, temp);

		/* write-back */
		if (IS_DIRTY(temp->flags)) {
			free(temp->data);
		}

		for (i = 0; i < set->count; ++i) {
			if (IS_VALID(set->lines[i].flags) && (set->lines[i].tag == temp->tag)) {
				set->lines[i] = *line;
				break;
			}
		}

		LIST_ADD(&set->timestamps, temp);
	}

	written = cache_executePolicy(writeCb, temp, offset, count, policy);

	qsort(set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	return written;
}


static cacheline_t *cache_findLine(cacheset_t *set, uint64_t tag)
{
	cacheline_t key, *keyPtr = &key, **linePtr = NULL;

	key.tag = tag;

	linePtr = bsearch(&keyPtr, set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	return (linePtr != NULL) ? *linePtr : NULL;
}


static cacheline_t *cache_updateTimestamps(cacheset_t *set, uint64_t tag)
{
	cacheline_t *linePtr = NULL;

	linePtr = cache_findLine(set, tag);

	if (linePtr != NULL) {
		LIST_REMOVE(&set->timestamps, linePtr);
		LIST_ADD(&set->timestamps, linePtr);

		return linePtr;
	}

	return NULL;
}


static void *cache_readFromSet(cacheset_t *set, uint64_t tag, uint64_t offset)
{
	cacheline_t *linePtr = NULL;

	linePtr = cache_updateTimestamps(set, tag);

	return (linePtr != NULL) ? (unsigned char *)(linePtr)->data + offset : NULL;
}


static uint64_t cache_compOffset(const cachectx_t *cache, const uint64_t addr)
{
	return addr & cache->offMask;
}


static uint64_t cache_compSet(const cachectx_t *cache, const uint64_t addr)
{
	return (addr >> cache->offBitsNum) & cache->setMask;
}


static uint64_t cache_compTag(const cachectx_t *cache, const uint64_t addr)
{
	return (addr >> (cache->offBitsNum + cache->setBitsNum)) & cache->tagMask;
}


/* TODO: add error handling */
ssize_t cache_write(cachectx_t *cache, uint64_t addr, void *buffer, size_t count, int policy)
{
	ssize_t position = 0, ret = 0;
	void *data = NULL, *dest = NULL;
	unsigned char flags = 0;
	uint64_t index = 0, offset = 0, tag = 0;
	size_t pieceSize, left = 0, remainder = 0;
	cacheline_t line, *linePtr;

	if (cache == NULL || buffer == NULL) {
		return -1;
	}

	if (count == 0) {
		return 0;
	}

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
			offset = 0;
		}
		else {
			pieceSize = cache->lineSize;
			offset = 0;
		}

		data = cache_readFromSet(&cache->sets[index], tag, offset);

		if (data == NULL) {
			data = calloc(cache->lineSize, sizeof(unsigned char));
			dest = (unsigned char *)data + offset;
			memcpy(dest, buffer + position, pieceSize);
			SET_VALID(flags);

			line.tag = tag;
			line.data = data;

			line.flags = flags;
			line.offset = offset;

			position += cache_writeToSet(cache->writeCb, &cache->sets[index], &line, addr, pieceSize / sizeof(*data), -1);
		}
		else {
			linePtr = cache_updateTimestamps(&cache->sets[index], tag);
			dest = (unsigned char *)linePtr->data + offset;
			memcpy(dest, buffer + position, pieceSize);

			ret = cache_executePolicy(cache->writeCb, linePtr, addr, cache->lineSize, policy);

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


static ssize_t cache_readOnMiss(cachectx_t *cache, const uint64_t addr, const uint64_t offset, void *buffer, size_t position, size_t count)
{
	ssize_t read = 0, written = 0;
	void *temp = NULL;

	temp = calloc(cache->lineSize, sizeof(unsigned char));

	read = cache->readCb(addr, temp, sizeof(*temp), cache->lineSize);

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

	if (cache == NULL || buffer == NULL) {
		return -1;
	}

	if (count == 0) {
		return 0;
	}

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
			offset = 0;
		}
		else {
			pieceSize = cache->lineSize;
			offset = 0;
		}

		data = cache_readFromSet(&cache->sets[index], tag, offset);

		/* cache miss */
		if (data == NULL) {
			position += cache_readOnMiss(cache, addr, offset, buffer, position, pieceSize);
		}
		/* cache hit */
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


/* TODO: add checks, test */
int cache_flush(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
	int ret = 0;
	size_t written = 0;
	uint64_t addr = 0, tag = 0, index = 0, begOffset = 0, endOffset = 0;
	cacheline_t *linePtr = NULL;

	if (begAddr > endAddr) {
		return -1;
	}

	begOffset = cache_compOffset(cache, begAddr);
	endOffset = cache_compOffset(cache, endAddr);

	addr = begAddr - begOffset;

	while (1) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		linePtr = cache_findLine(&cache->sets[index], tag);

		if (linePtr != NULL && IS_VALID(linePtr->flags) && IS_DIRTY(linePtr->flags)) {
			written = cache->writeCb(addr, linePtr->data, sizeof(*(linePtr->data)), cache->lineSize);
			if (written != cache->lineSize) {
				ret = -1;
				break;
			}
			CLEAR_DIRTY(linePtr->flags);
		}

		addr += cache->lineSize;

		if (addr == (endAddr - endOffset + cache->lineSize)) {
			break;
		}
	}

	return ret;
}


int cache_invalidate(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
	uint64_t addr = 0, tag = 0, index = 0, begOffset = 0, endOffset = 0;
	cacheline_t *linePtr = NULL;

	if (begAddr > endAddr) {
		return -1;
	}

	begOffset = cache_compOffset(cache, begAddr);
	endOffset = cache_compOffset(cache, endAddr);

	addr = begAddr - begOffset;

	while (1) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		linePtr = cache_findLine(&cache->sets[index], tag);

		if (linePtr != NULL && IS_VALID(linePtr->flags)) {
			CLEAR_VALID(linePtr->flags);
			free(linePtr->data);
			linePtr->data = NULL;

			LIST_REMOVE(cache->sets[index].timestamps, linePtr);
			if (cache->sets[index].count == 1) {
				cache->sets[index].timestamps = NULL;
			}
			cache->sets[index].count -= 1;
		}

		addr += cache->lineSize;

		if (addr == (endAddr - endOffset + cache->lineSize)) {
			break;
		}
	}

	return 0;
}


#ifdef CACHE_DEBUG
void cache_print(cachectx_t *cache)
{
	int i, j, valid, dirty;
	unsigned char *data = NULL;
	uint64_t offset;

	printf("\n\n");
	for (i = 0; i < cache->numSets; ++i) {
		printf("set: %2d\n", i);
		printf("-------\n");
		printf("\t\t\t\ttag\toffset\tvalid\tdirty\t\t\tdata\n");
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
				valid = (IS_VALID(cache->sets[i].lines[j].flags)) ? 1 : 0;

				dirty = (IS_DIRTY(cache->sets[i].lines[j].flags)) ? 1 : 0;

				printf("line: %1d\t\t%20lu\t%5li\t%d\t%d\t%20s\n", j, cache->sets[i].lines[j].tag, offset, valid, dirty, data);
			}
		}
	}
}
#endif