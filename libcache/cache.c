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
	uint64_t offset;          /* TODO: remove */
	uint64_t addr;            /* TODO: remove */
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

	handle_t lock;
};


static uint64_t cache_genMask(int numBits)
{
	return ((uint64_t)1 << numBits) - 1;
}


cachectx_t *cache_init(size_t srcMemSize, size_t size, size_t lineSize, cache_writeCb_t writeCb, cache_readCb_t readCb)
{
	int err;
	cachectx_t *cache = NULL;

	if (srcMemSize == 0 || size == 0 || lineSize == 0) {
		return NULL;
	}

	if (size % lineSize != 0 || (size / lineSize) % LIBCACHE_NUM_WAYS != 0) {
		return NULL;
	}

	cache = malloc(sizeof(cachectx_t));

	if (cache == NULL) {
		return NULL;
	}

	cache->srcMemSize = srcMemSize;
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

	err = mutexCreate(&cache->lock);
	if (err < 0) {
		free(cache->sets);
		free(cache);

		return NULL;
	}

	return cache;
}


static uint64_t cache_compAddr(const cachectx_t *cache, uint64_t tag, uint64_t setIndex)
{
	uint64_t temp = (tag << cache->setBitsNum) | setIndex;
	return temp <<= cache->offBitsNum;
}


static ssize_t cache_flushLine(cachectx_t *cache, cacheline_t *linePtr, uint64_t addr)
{
	ssize_t written = 0, position = 0;
	size_t left = cache->lineSize;

	if (linePtr != NULL && IS_VALID(linePtr->flags) && IS_DIRTY(linePtr->flags)) {
		while (left > 0) {
			written = cache->writeCb(addr, (unsigned char *)linePtr->data + position, left);

			if (written <= 0) {
				return -EIO;
			}

			left -= written;
			addr += written;
			position += written;
		}

		CLEAR_DIRTY(linePtr->flags);
	}

	return written;
}


int cache_deinit(cachectx_t *cache)
{
	int i, j, err;
	uint64_t addr;
	ssize_t written;
	cacheline_t *linePtr;

	if (cache == NULL) {
		return -EINVAL;
	}

	for (i = 0; i < cache->numSets; ++i) {
		for (j = 0; j < LIBCACHE_NUM_WAYS; ++j) {
			linePtr = &(cache->sets[i].lines[j]);

			if (IS_VALID(linePtr->flags)) {
				if (IS_DIRTY(linePtr->flags)) {
					addr = cache_compAddr(cache, linePtr->tag, i);
					written = cache_flushLine(cache, linePtr, addr);

					if (written < cache->lineSize) {
						return -EIO;
					}
				}

				free(linePtr->data);
				CLEAR_VALID(linePtr->flags);
			}
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


static ssize_t cache_executePolicy(cachectx_t *cache, cacheline_t *linePtr, uint64_t addr, int policy)
{
	ssize_t written = cache->lineSize;

	if (policy == 1) {
		written = cache_flushLine(cache, linePtr, addr);

		if (written <= 0) {
			return -EIO;
		}
	}

	return written;
}


static cacheline_t *cache_allocateLine(cachectx_t *cache, const uint64_t setIndex, const uint64_t tag)
{
	int i, lineIndex;
	uint64_t addr = 0;
	cacheline_t *temp = NULL;
	ssize_t written = cache->lineSize;
	cacheline_t *line;
	unsigned char flags = 0;
	void *data;
	cacheset_t *set = &cache->sets[setIndex];

	if (set->count < LIBCACHE_NUM_WAYS) {
		set->count++;

		for (i = 0; i < set->count; ++i) {
			if (!IS_VALID(set->lines[i].flags)) {
				lineIndex = i;
				break;
			}
		}

		temp = &(set->lines[lineIndex]);

		for (i = 0; i < set->count; ++i) {
			if (set->tags[i] == NULL) {
				set->tags[i] = temp;
				break;
			}
		}

		LIST_ADD(&set->timestamps, temp);
	}
	else {
		temp = set->timestamps;

		if (IS_DIRTY(temp->flags)) {
			addr = cache_compAddr(cache, temp->tag, setIndex);
			written = cache_flushLine(cache, temp, addr);

			if (written < cache->lineSize) {
				return NULL;
			}
		}

		LIST_REMOVE(&set->timestamps, temp);
		free(temp->data);

		for (i = 0; i < set->count; ++i) {
			if (IS_VALID(set->lines[i].flags) && (set->lines[i].tag == temp->tag)) {
				lineIndex = i;
				break;
			}
		}

		LIST_ADD(&set->timestamps, temp);
	}

	line = &(set->lines[lineIndex]);

	data = malloc(cache->lineSize * sizeof(unsigned char));

	if (data == NULL) {
		return NULL;
	}

	line->data = data;
	line->tag = tag;
	SET_VALID(flags);
	line->flags = flags;

	qsort(set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	return line;
}


static cacheline_t *cache_findLine(cacheset_t *set, uint64_t tag, int update)
{
	cacheline_t key, *keyPtr = &key, **linePtr = NULL;

	key.tag = tag;

	linePtr = bsearch(&keyPtr, set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	if (linePtr != NULL) {
		if (update != 0) {
			LIST_REMOVE(&set->timestamps, *linePtr);
			LIST_ADD(&set->timestamps, *linePtr);
		}

		return *linePtr;
	}

	return NULL;
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


static size_t cache_compPieceSize(const size_t left, const size_t lineSize, uint64_t *addr, uint64_t *offset, const size_t count, const size_t remainder)
{
	size_t pieceSize = 0;

	if (left == count) {
		if (count <= (lineSize - *offset)) {
			pieceSize = count;
		}
		else {
			pieceSize = lineSize - *offset;
		}
		*addr -= *offset;
	}
	else if (left == remainder) {
		pieceSize = remainder;
		*offset = 0;
	}
	else {
		pieceSize = lineSize;
		*offset = 0;
	}

	return pieceSize;
}


static ssize_t cache_fetchLine(cachectx_t *cache, cacheline_t *line, const uint64_t addr)
{
	uint64_t tempAddr = 0;
	ssize_t read = 0, position = 0, written = 0;
	size_t left = cache->lineSize;

	tempAddr = addr;

	while (left > 0) {
		read = cache->readCb(tempAddr, (unsigned char *)line->data + position, left);

		if (read <= 0) {
			free(line->data);
			return -EIO;
		}

		left -= read;
		tempAddr += read;
		position += read;
	}

	return position;
}


ssize_t cache_write(cachectx_t *cache, uint64_t addr, void *buffer, size_t count, int policy)
{
	int err = -1;
	ssize_t position = 0, written = 0;
	uint64_t index = 0, offset = 0, tag = 0;
	size_t pieceSize = 0, left = 0, remainder = 0;
	cacheline_t *linePtr = NULL;
	void *data = NULL;

	if (cache == NULL || buffer == NULL || (policy != LIBCACHE_WRITE_BACK && policy != LIBCACHE_WRITE_THROUGH)) {
		return -EINVAL;
	}

	if (count == 0) {
		return 0;
	}

	if (addr > cache->srcMemSize) {
		return -EINVAL;
	}
	else if ((addr < cache->srcMemSize) && (addr + count > cache->srcMemSize)) {
		count = cache->srcMemSize - addr;
	}

	left = count;
	offset = cache_compOffset(cache, addr);
	remainder = (left - (cache->lineSize - offset)) % cache->lineSize;

	mutexLock(cache->lock);

	while (left > 0) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		pieceSize = cache_compPieceSize(left, cache->lineSize, &addr, &offset, count, remainder);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_UPDATE);

		/* cache miss */
		if (linePtr == NULL) {
			linePtr = cache_allocateLine(cache, index, tag);

			if (linePtr == NULL) {
				return -ENOMEM;
			}

			if (pieceSize < cache->lineSize) {
				err = cache_fetchLine(cache, linePtr, addr);
				if (err < cache->lineSize) {
					CLEAR_VALID(linePtr->flags);
					return err;
				}
			}
		}
		/* cache hit */
		memcpy((unsigned char *)linePtr->data + offset, (unsigned char *)buffer + position, pieceSize);

		SET_DIRTY(linePtr->flags);

		written = cache_executePolicy(cache, linePtr, addr, policy);
		if (written < cache->lineSize) {
			return -EIO;
		}

		position += pieceSize;
		left -= pieceSize;
		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return position;
}


ssize_t cache_read(cachectx_t *cache, uint64_t addr, void *buffer, size_t count)
{
	int err = -1;
	ssize_t position = 0;
	void *data = NULL;
	cacheline_t *linePtr = NULL;
	uint64_t index = 0, offset = 0, tag = 0;
	size_t pieceSize, left = count, remainder = 0;

	if (cache == NULL || buffer == NULL) {
		return -EINVAL;
	}

	if (count == 0) {
		return 0;
	}

	if (addr > cache->srcMemSize) {
		return -EINVAL;
	}
	else if (addr < cache->srcMemSize && addr + count > cache->srcMemSize) {
		left = cache->srcMemSize - addr;
	}

	left = count;
	offset = cache_compOffset(cache, addr);
	remainder = (left - (cache->lineSize - offset)) % cache->lineSize;

	mutexLock(cache->lock);

	while (left > 0) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		pieceSize = cache_compPieceSize(left, cache->lineSize, &addr, &offset, count, remainder);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_UPDATE);

		/* cache miss */
		if (linePtr == NULL) {
			linePtr = cache_allocateLine(cache, index, tag);

			if (linePtr == NULL) {
				return -ENOMEM;
			}

			err = cache_fetchLine(cache, linePtr, addr);
			if (err < cache->lineSize) {
				CLEAR_VALID(linePtr->flags);
				return err;
			}
		}
		/* cache hit */
		memcpy((unsigned char *)buffer + position, (unsigned char *)linePtr->data + offset, pieceSize);

		position += pieceSize;
		left -= pieceSize;
		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return position;
}


int cache_flush(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
	ssize_t written = 0, writtenPiece = 0;
	uint64_t addr = 0, end = endAddr, tag = 0, index = 0, begOffset = 0;
	cacheline_t *linePtr = NULL;

	if (cache == NULL) {
		return -EINVAL;
	}

	if (begAddr > endAddr) {
		return -EINVAL;
	}

	if (begAddr > cache->srcMemSize) {
		return -EINVAL;
	}
	else if (begAddr < cache->srcMemSize && endAddr > cache->srcMemSize) {
		end = cache->srcMemSize;
	}

	begOffset = cache_compOffset(cache, begAddr);
	addr = begAddr - begOffset;

	mutexLock(cache->lock);

	while (addr < end) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);

		if (linePtr != NULL && IS_DIRTY(linePtr->flags)) {
			writtenPiece = cache_flushLine(cache, linePtr, addr);

			if (writtenPiece < cache->lineSize) {
				return -EIO;
			}

			written += writtenPiece;
		}

		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return EOK;
}


static void cache_invalidateLine(cacheset_t *set, cacheline_t *linePtr)
{
	if (linePtr != NULL && IS_VALID(linePtr->flags)) {
		CLEAR_VALID(linePtr->flags);
		free(linePtr->data);
		linePtr->data = NULL;

		LIST_REMOVE(set->timestamps, linePtr);

		if (set->count == 1) {
			set->timestamps = NULL;
		}

		set->count -= 1;
	}
}


int cache_invalidate(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
	uint64_t addr = 0, end = endAddr, tag = 0, index = 0, begOffset = 0;
	cacheline_t *linePtr = NULL;

	if (cache == NULL) {
		return -EINVAL;
	}

	if (begAddr > endAddr) {
		return -EINVAL;
	}

	if (begAddr > cache->srcMemSize) {
		return -EINVAL;
	}
	else if (begAddr < cache->srcMemSize && endAddr > cache->srcMemSize) {
		end = cache->srcMemSize;
	}

	begOffset = cache_compOffset(cache, begAddr);
	addr = begAddr - begOffset;

	mutexLock(cache->lock);

	while (addr < end) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);

		cache_invalidateLine(&cache->sets[index], linePtr);

		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return EOK;
}

/* TODO: remove the code below */
#ifdef CACHE_DEBUG
void cache_print(cachectx_t *cache)
{
	{
		int i, j, valid, dirty;
		unsigned char *data = NULL;
		uint64_t offset;

		printf("\n\n");
		for (i = 0; i < cache->numSets; ++i) {
			printf("\nset: %2d\n", i);
			printf("-------\n");
			for (j = 0; j < LIBCACHE_NUM_WAYS; ++j) {
				if (cache->sets[i].lines[j].data == NULL) {
					continue;
				}
				else {
					offset = cache->sets[i].lines[j].offset;
					valid = (IS_VALID(cache->sets[i].lines[j].flags)) ? 1 : 0;
					dirty = (IS_DIRTY(cache->sets[i].lines[j].flags)) ? 1 : 0;
					printf("LINE: %d\n", j);
					printf("ADDR: %lu\n", cache->sets[i].lines[j].addr);
					printf("TAG: %lu\n", cache->sets[i].lines[j].tag);
					printf("OFFSET: %lu\n", cache->sets[i].lines[j].offset);
					printf("VALID: %d\n", valid);
					printf("DIRTY: %d\n", dirty);
					printf("DATA: %.64s\n\n", (unsigned char *)(cache->sets[i].lines[j].data));
				}
			}
		}
	}
}
#endif