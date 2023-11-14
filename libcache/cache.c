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
#include <sys/threads.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


#define LIBCACHE_NUM_WAYS   4
#define LIBCACHE_ADDR_WIDTH 64


#define LIBCACHE_TIMESTAMPS_UPDATE    1
#define LIBCACHE_TIMESTAMPS_NO_UPDATE 0


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
	cacheline_t *prev, *next; /* Circular doubly linked list */
	void *data;
	unsigned char flags;
};


typedef struct {
	cacheline_t *timestamps;
	cacheline_t *tags[LIBCACHE_NUM_WAYS];
	cacheline_t lines[LIBCACHE_NUM_WAYS];
	size_t count;
} cacheset_t;


struct cachectx_s {
	cacheset_t *sets;

	size_t srcMemSize;
	size_t lineSize;
	size_t linesCnt;
	size_t numSets;

	uint64_t tagMask;
	uint64_t setMask;
	uint64_t offMask;

	uint8_t offBitsNum; /* Number of bits in memory address corresponding to offset */
	uint8_t setBitsNum; /* Number of bits in memory address corresponding to set index */
	uint8_t tagBitsNum;

	cache_ops_t ops;

	handle_t lock;
};

static void cache_invalidateLine(cacheset_t *setPtr, cacheline_t *linePtr);

static uint64_t cache_generateMask(int numBits)
{
	return ((uint64_t)1 << numBits) - 1;
}


cachectx_t *cache_init(size_t srcMemSize, size_t lineSize, size_t linesCnt, const cache_ops_t *ops)
{
	int err;
	cachectx_t *cache = NULL;

	if (srcMemSize == 0 || linesCnt == 0 || lineSize == 0) {
		return NULL;
	}

	if (linesCnt % LIBCACHE_NUM_WAYS != 0) {
		return NULL;
	}

	cache = malloc(sizeof(cachectx_t));

	if (cache != NULL) {
		cache->srcMemSize = srcMemSize;
		cache->lineSize = lineSize;
		cache->linesCnt = linesCnt;

		cache->numSets = cache->linesCnt / LIBCACHE_NUM_WAYS;

		cache->sets = calloc(cache->numSets, sizeof(cacheset_t));

		if (cache->sets == NULL) {
			free(cache);
			cache = NULL;
		}
		else {
			cache->offBitsNum = LOG2(cache->lineSize);
			cache->setBitsNum = LOG2(cache->numSets);
			cache->tagBitsNum = LIBCACHE_ADDR_WIDTH - cache->setBitsNum - cache->offBitsNum;

			cache->tagMask = cache_generateMask(cache->tagBitsNum);
			cache->setMask = cache_generateMask(cache->setBitsNum);
			cache->offMask = cache_generateMask(cache->offBitsNum);

			cache->ops = *ops;

			err = mutexCreate(&cache->lock);
			if (err < 0) {
				free(cache->sets);
				free(cache);
				cache = NULL;
			}
		}
	}

	return cache;
}


static uint64_t cache_computeAddr(const cachectx_t *cache, uint64_t tag, uint64_t setIndex)
{
	uint64_t temp = (tag << cache->setBitsNum) | setIndex;
	return temp << cache->offBitsNum;
}


static int cache_flushLine(cachectx_t *cache, cacheline_t *linePtr, uint64_t addr)
{
	ssize_t writeCount = 0, position = 0;
	size_t left = cache->lineSize;

	if ((linePtr != NULL) && IS_VALID(linePtr->flags) && IS_DIRTY(linePtr->flags)) {
		while (left > 0) {
			writeCount = cache->ops.writeCb(addr, (const unsigned char *)linePtr->data + position, left, cache->ops.ctx);
			if (writeCount <= 0) {
				return -EIO;
			}

			left -= writeCount;
			addr += writeCount;
			position += writeCount;
		}

		CLEAR_DIRTY(linePtr->flags);
	}

	return 0;
}


int cache_deinit(cachectx_t *cache)
{
	int err;

	for (size_t i = 0; i < cache->numSets; ++i) {
		for (size_t j = 0; j < LIBCACHE_NUM_WAYS; ++j) {
			cacheline_t *linePtr = &(cache->sets[i].lines[j]);

			if (!IS_VALID(linePtr->flags)) {
				continue;
			}

			if (IS_DIRTY(linePtr->flags)) {
				uint64_t addr = cache_computeAddr(cache, linePtr->tag, i);
				err = cache_flushLine(cache, linePtr, addr);
				if (err < 0) {
					return err;
				}
			}

			cache_invalidateLine(&cache->sets[i], linePtr);
		}
	}

	err = resourceDestroy(cache->lock);
	if (err < 0) {
		return err;
	}

	free(cache->sets);
	free(cache);

	return EOK;
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


static int cache_executePolicy(cachectx_t *cache, cacheline_t *linePtr, uint64_t addr, int policy)
{
	if (policy == LIBCACHE_WRITE_THROUGH) {
		return cache_flushLine(cache, linePtr, addr);
	}

	return 0;
}


static cacheline_t *cache_allocateLine(cachectx_t *cache, const uint64_t setIndex, const uint64_t tag)
{
	int i;
	uint64_t addr = 0;
	cacheline_t *linePtr = NULL;
	cacheset_t *setPtr = &cache->sets[setIndex];

	if (setPtr->count < LIBCACHE_NUM_WAYS) {
		/* Set is not full, so there must be an invalid line in set */
		setPtr->count++;

		for (i = 0; i < setPtr->count; ++i) {
			if (!IS_VALID(setPtr->lines[i].flags)) {
				linePtr = &(setPtr->lines[i]);
				break;
			}
		}

		for (i = 0; i < setPtr->count; ++i) {
			if (setPtr->tags[i] == NULL) {
				setPtr->tags[i] = linePtr;
				break;
			}
		}

		if (linePtr->data == NULL) {
			linePtr->data = malloc(cache->lineSize);

			if (linePtr->data == NULL) {
				setPtr->count--;
				return NULL;
			}
		}
	}
	else {
		/* Set is full, take least recently used valid line from set */
		linePtr = setPtr->timestamps;

		if (IS_DIRTY(linePtr->flags)) {
			addr = cache_computeAddr(cache, linePtr->tag, setIndex);
			int err = cache_flushLine(cache, linePtr, addr);
			if (err < 0) {
				return NULL;
			}
		}

		LIST_REMOVE(&setPtr->timestamps, linePtr);
	}

	LIST_ADD(&setPtr->timestamps, linePtr);
	linePtr->tag = tag;
	unsigned char flags = 0;
	SET_VALID(flags);
	linePtr->flags = flags;

	qsort(setPtr->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	return linePtr;
}


static cacheline_t *cache_findLine(cacheset_t *setPtr, uint64_t tag, int update)
{
	cacheline_t key, *keyPtr = &key, *linePtr = NULL, **temp = NULL;

	key.tag = tag;

	temp = bsearch(&keyPtr, setPtr->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	if (temp != NULL && IS_VALID((*temp)->flags)) {
		if (update != LIBCACHE_TIMESTAMPS_NO_UPDATE) {
			LIST_REMOVE(&setPtr->timestamps, *temp);
			LIST_ADD(&setPtr->timestamps, *temp);
		}

		linePtr = *temp;
	}

	return linePtr;
}


static uint64_t cache_computeOffset(const cachectx_t *cache, const uint64_t addr)
{
	return addr & cache->offMask;
}


static uint64_t cache_computeSetIndex(const cachectx_t *cache, const uint64_t addr)
{
	return (addr >> cache->offBitsNum) & cache->setMask;
}


static uint64_t cache_computeTag(const cachectx_t *cache, const uint64_t addr)
{
	return (addr >> (cache->offBitsNum + cache->setBitsNum)) & cache->tagMask;
}


static size_t cache_computeTempCount(const size_t left, const size_t lineSize, uint64_t *addr, uint64_t *offset, const size_t count, const size_t remainder)
{
	size_t tempCount = 0;

	if (left == count) {
		if (count <= (lineSize - *offset)) {
			tempCount = count;
		}
		else {
			tempCount = lineSize - *offset;
		}
		*addr -= *offset;
	}
	else {
		*offset = 0;
		tempCount = (left == remainder) ? remainder : lineSize;
	}

	return tempCount;
}


static int cache_fetchLine(cachectx_t *cache, cacheline_t *linePtr, const uint64_t addr)
{
	uint64_t tempAddr = 0;
	ssize_t readCount = 0, position = 0;
	size_t left = cache->lineSize;

	tempAddr = addr;

	while (left > 0) {
		readCount = cache->ops.readCb(tempAddr, (unsigned char *)linePtr->data + position, left, cache->ops.ctx);
		if (readCount <= 0) {
			return -EIO;
		}

		left -= readCount;
		tempAddr += readCount;
		position += readCount;
	}

	return 0;
}


ssize_t cache_write(cachectx_t *cache, uint64_t addr, const void *buffer, size_t count, int policy)
{
	ssize_t position = 0;
	uint64_t index = 0, offset = 0, tag = 0;
	size_t tempCount = 0, left = 0, remainder = 0;
	cacheline_t *linePtr = NULL;

	if (buffer == NULL || (policy != LIBCACHE_WRITE_BACK && policy != LIBCACHE_WRITE_THROUGH) || addr > cache->srcMemSize) {
		return -EINVAL;
	}
	if (count == 0) {
		return 0;
	}
	if ((addr < cache->srcMemSize) && (addr + count > cache->srcMemSize)) {
		count = cache->srcMemSize - addr;
	}

	left = count;
	offset = cache_computeOffset(cache, addr);
	remainder = (left - (cache->lineSize - offset)) % cache->lineSize;

	mutexLock(cache->lock);

	while (left > 0) {
		index = cache_computeSetIndex(cache, addr);
		tag = cache_computeTag(cache, addr);

		tempCount = cache_computeTempCount(left, cache->lineSize, &addr, &offset, count, remainder);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_UPDATE);

		/* cache miss */
		if (linePtr == NULL) {
			linePtr = cache_allocateLine(cache, index, tag);

			if (linePtr == NULL) {
				position = -ENOMEM;
				break;
			}

			if (tempCount < cache->lineSize) {
				int err = cache_fetchLine(cache, linePtr, addr);
				if (err < 0) {
					cache_invalidateLine(&cache->sets[index], linePtr);
					position = err;
					break;
				}
			}
		}
		/* cache hit */
		memcpy((unsigned char *)linePtr->data + offset, (const unsigned char *)buffer + position, tempCount);

		SET_DIRTY(linePtr->flags);

		int err = cache_executePolicy(cache, linePtr, addr, policy);
		if (err < 0) {
			position = err;
			break;
		}

		position += tempCount;
		left -= tempCount;
		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return position;
}


ssize_t cache_read(cachectx_t *cache, uint64_t addr, void *buffer, size_t count)
{
	ssize_t position = 0;
	cacheline_t *linePtr = NULL;
	uint64_t index = 0, offset = 0, tag = 0;
	size_t tempCount, left = count, remainder = 0;

	if (buffer == NULL || addr > cache->srcMemSize) {
		return -EINVAL;
	}
	if (count == 0) {
		return 0;
	}
	if ((addr < cache->srcMemSize) && (addr + count > cache->srcMemSize)) {
		count = cache->srcMemSize - addr;
	}

	left = count;
	offset = cache_computeOffset(cache, addr);
	remainder = (left - (cache->lineSize - offset)) % cache->lineSize;

	mutexLock(cache->lock);

	while (left > 0) {
		index = cache_computeSetIndex(cache, addr);
		tag = cache_computeTag(cache, addr);

		tempCount = cache_computeTempCount(left, cache->lineSize, &addr, &offset, count, remainder);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_UPDATE);

		/* cache miss */
		if (linePtr == NULL) {
			linePtr = cache_allocateLine(cache, index, tag);

			if (linePtr == NULL) {
				position = -ENOMEM;
				break;
			}

			int err = cache_fetchLine(cache, linePtr, addr);
			if (err < 0) {
				cache_invalidateLine(&cache->sets[index], linePtr);
				position = err;
				break;
			}
		}
		/* cache hit */
		memcpy((unsigned char *)buffer + position, (const unsigned char *)linePtr->data + offset, tempCount);

		position += tempCount;
		left -= tempCount;
		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return position;
}


int cache_flush(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
	int ret = EOK;
	uint64_t addr = 0, end = endAddr, tag = 0, index = 0, begOffset = 0;
	cacheline_t *linePtr = NULL;

	if (begAddr > endAddr || begAddr > cache->srcMemSize) {
		return -EINVAL;
	}

	if (begAddr < cache->srcMemSize && endAddr > cache->srcMemSize) {
		end = cache->srcMemSize;
	}

	begOffset = cache_computeOffset(cache, begAddr);
	addr = begAddr - begOffset;

	mutexLock(cache->lock);

	while (addr < end) {
		index = cache_computeSetIndex(cache, addr);
		tag = cache_computeTag(cache, addr);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);

		if (linePtr != NULL && IS_DIRTY(linePtr->flags)) {
			ret = cache_flushLine(cache, linePtr, addr);
			if (ret < 0) {
				break;
			}
		}

		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return ret;
}


static void cache_invalidateLine(cacheset_t *setPtr, cacheline_t *linePtr)
{
	LIST_REMOVE(&setPtr->timestamps, linePtr);

	CLEAR_VALID(linePtr->flags);
	free(linePtr->data);
	linePtr->data = NULL;
	linePtr->tag = 0;

	setPtr->count -= 1;
}


int cache_invalidate(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
	uint64_t addr = 0, end = endAddr, tag = 0, index = 0, begOffset = 0;
	cacheline_t *linePtr = NULL;

	if (begAddr > endAddr || begAddr > cache->srcMemSize) {
		return -EINVAL;
	}
	if (begAddr < cache->srcMemSize && endAddr > cache->srcMemSize) {
		end = cache->srcMemSize;
	}

	begOffset = cache_computeOffset(cache, begAddr);
	addr = begAddr - begOffset;

	mutexLock(cache->lock);

	while (addr < end) {
		index = cache_computeSetIndex(cache, addr);
		tag = cache_computeTag(cache, addr);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);

		if (linePtr != NULL) {
			cache_invalidateLine(&cache->sets[index], linePtr);
		}

		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return EOK;
}


int cache_clean(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
	int ret = EOK;
	uint64_t addr = 0, end = endAddr, tag = 0, index = 0, begOffset = 0;
	cacheline_t *linePtr = NULL;

	if (begAddr > endAddr || begAddr > cache->srcMemSize) {
		return -EINVAL;
	}
	if (begAddr < cache->srcMemSize && endAddr > cache->srcMemSize) {
		end = cache->srcMemSize;
	}

	begOffset = cache_computeOffset(cache, begAddr);
	addr = begAddr - begOffset;

	mutexLock(cache->lock);

	while (addr < end) {
		index = cache_computeSetIndex(cache, addr);
		tag = cache_computeTag(cache, addr);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);

		if (linePtr != NULL) {
			if (IS_DIRTY(linePtr->flags)) {
				ret = cache_flushLine(cache, linePtr, addr);
				if (ret < 0) {
					break;
				}
			}

			cache_invalidateLine(&cache->sets[index], linePtr);
		}

		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return ret;
}
