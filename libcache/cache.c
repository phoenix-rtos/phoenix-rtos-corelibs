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


/* Generates mask of type uint64_t with numBits set to 1 */
static uint64_t cache_genMask(int numBits)
{
	return ((uint64_t)1 << numBits) - 1;
}


cachectx_t *cache_init(size_t srcMemSize, size_t size, size_t lineSize, cache_writeCb_t writeCb, cache_readCb_t readCb)
{
	int err;
	cachectx_t *cache = NULL;

	if (size == 0 || lineSize == 0) {
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


int cache_deinit(cachectx_t *cache)
{
	int i, j;

	if (cache == NULL) {
		return 0;
	}

	for (i = 0; i < cache->numSets; ++i) {
		for (j = 0; j < LIBCACHE_NUM_WAYS; ++j) {
			if (IS_VALID(cache->sets[i].lines[j].flags)) {
				free(cache->sets[i].lines[j].data);
				CLEAR_VALID(cache->sets[i].lines[j].flags);
			}
		}
	}

	resourceDestroy(cache->lock);

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


static ssize_t cache_flushLine(cache_writeCb_t writeCb, cacheline_t *linePtr, uint64_t addr, size_t count, size_t srcMemSize)
{
	ssize_t ret = 0;
	ssize_t written = 0, position = 0;
	size_t left = count;

	if (linePtr != NULL && IS_VALID(linePtr->flags) && IS_DIRTY(linePtr->flags)) {
		while (left > 0) {
			written = (*writeCb)(addr, (unsigned char *)linePtr->data + position, left);

			if (written == -1) {
				return -1;
			}

			left -= written;
			addr += written;
			position += written;
		}

		if (written == count) {
			CLEAR_DIRTY(linePtr->flags);
		}
	}

	return written;
}


static ssize_t cache_executePolicy(cache_writeCb_t writeCb, cacheline_t *linePtr, uint64_t addr, size_t count, int policy, size_t srcMemSize)
{
	ssize_t written = count;
	cacheline_t *dest = NULL;

	if (!IS_DIRTY(linePtr->flags)) {
		SET_DIRTY(linePtr->flags);
	}

	switch (policy) {
		case 0:
			/* write-back */
			break;
		case 1:
			/* write-through */
			written = cache_flushLine(writeCb, linePtr, addr, count, srcMemSize);
			break;
		default:
			break;
	}

	return written;
}


static ssize_t cache_writeToSet(cache_writeCb_t writeCb, cacheset_t *set, cacheline_t *line, const uint64_t offset, size_t count, size_t srcMemSize)
{
	int i;
	ssize_t written = count;

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

		/* write-back */
		if (IS_DIRTY(temp->flags)) {
			written = cache_flushLine(writeCb, temp, offset, count, srcMemSize);
			if (written != count) {
				return written;
			}
		}

		LIST_REMOVE(&set->timestamps, temp);

		free(temp->data);

		for (i = 0; i < set->count; ++i) {
			if (IS_VALID(set->lines[i].flags) && (set->lines[i].tag == temp->tag)) {
				set->lines[i] = *line;
				break;
			}
		}

		LIST_ADD(&set->timestamps, temp);
	}

	qsort(set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	return written;
}


static cacheline_t *cache_findLine(cacheset_t *set, uint64_t tag, int update)
{
	cacheline_t key, *keyPtr = &key, **linePtr = NULL;

	key.tag = tag;

	linePtr = bsearch(&keyPtr, set->tags, LIBCACHE_NUM_WAYS, sizeof(cacheline_t *), cache_compareTags);

	if (linePtr != NULL) {
		if (update == LIBCACHE_TIMESTAMPS_UPDATE) {
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


static ssize_t cache_readFromMemOnMiss(cachectx_t *cache, const uint64_t addr, const uint64_t index, const uint64_t tag, const uint64_t offset)
{
	unsigned char flags = 0;
	ssize_t read = 0, readPos = 0, written = 0;
	void *temp = NULL;
	uint64_t tempAddr = 0;
	size_t left = cache->lineSize;
	cacheline_t line, *linePtr = NULL;

	temp = malloc(cache->lineSize * sizeof(unsigned char));

	if (temp == NULL) {
		return -1;
	}

	tempAddr = addr;

	while (left > 0) {
		read = cache->readCb(tempAddr, (unsigned char *)temp + readPos, left);

		if (read == -1) {
			return -1;
		}

		left -= read;
		tempAddr += read;
		readPos += read;
	}

	SET_VALID(flags);
	line.tag = tag;
	line.addr = addr;
	line.data = temp;
	line.flags = flags;
	line.offset = offset;

	written = cache_writeToSet(cache->writeCb, &cache->sets[index], &line, addr, cache->lineSize, cache->srcMemSize);

	return written;
}


ssize_t cache_write(cachectx_t *cache, uint64_t addr, void *buffer, size_t count, int policy)
{
	int ret = -1;
	ssize_t position = 0, readPos = 0, written = 0;
	void *data = NULL, *dest = NULL;
	unsigned char flags = 0;
	uint64_t index = 0, offset = 0, tag = 0, tempAddr = 0;
	size_t pieceSize = 0, left = 0, remainder = 0, leftToRead = 0;
	cacheline_t line, *linePtr = NULL;

	if (cache == NULL || buffer == NULL || (policy != LIBCACHE_WRITE_BACK && policy != LIBCACHE_WRITE_THROUGH)) {
		return -1;
	}

	if (count == 0) {
		return 0;
	}

	if (addr > cache->srcMemSize) {
		return 0;
	}
	else if ((addr < cache->srcMemSize) && (addr + count > cache->srcMemSize)) {
		count = cache->srcMemSize - addr;
	}

	left = count;

	leftToRead = cache->lineSize;
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
			if (pieceSize != cache->lineSize) {
				ret = cache_readFromMemOnMiss(cache, addr, index, tag, offset);
				printf("ret: %d\n", ret);
				if (ret != cache->lineSize) {
					if (ret != -1) {
						position += ret;
					}
					break;
				}

				linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);
				memcpy((unsigned char *)linePtr->data + offset, (unsigned char *)buffer + position, pieceSize);
			}
			else {
				data = calloc(cache->lineSize, sizeof(unsigned char));

				if (data == NULL) {
					break;
				}

				memcpy((unsigned char *)data + offset, (unsigned char *)buffer + position, pieceSize);

				SET_VALID(flags);
				line.tag = tag;
				line.addr = addr;
				line.data = data;
				line.flags = flags;
				line.offset = offset;

				written = cache_writeToSet(cache->writeCb, &cache->sets[index], &line, addr, cache->lineSize, cache->srcMemSize);

				if (written != cache->lineSize) {
					if (written != -1) {
						position += written;
						break;
					}
				}

				linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);
			}
		}
		/* cache hit */
		else {
			dest = (unsigned char *)linePtr->data + offset;
			memcpy(dest, (unsigned char *)buffer + position, pieceSize);
		}

		written = cache_executePolicy(cache->writeCb, linePtr, addr, cache->lineSize, policy, cache->srcMemSize);

		if (written != cache->lineSize) {
			if (written != -1) {
				position += written;
				break;
			}
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
	int ret = -1;
	ssize_t position = 0;
	void *data = NULL;
	cacheline_t *linePtr = NULL;
	uint64_t index = 0, offset = 0, tag = 0;
	size_t pieceSize, left = count, remainder = 0;

	if (cache == NULL || buffer == NULL) {
		return -1;
	}

	if (count == 0) {
		return 0;
	}

	if (addr > cache->srcMemSize) {
		return 0;
	}
	else if (addr < cache->srcMemSize && addr + count > cache->srcMemSize) {
		left = cache->srcMemSize - addr;
	}

	offset = cache_compOffset(cache, addr);

	left = count;
	remainder = (left - (cache->lineSize - offset)) % cache->lineSize;

	mutexLock(cache->lock);

	while (left > 0) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		pieceSize = cache_compPieceSize(left, cache->lineSize, &addr, &offset, count, remainder);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_UPDATE);

		/* cache miss */
		if (linePtr == NULL) {
			ret = cache_readFromMemOnMiss(cache, addr, index, tag, offset);

			if (ret == -1) {
				break;
			}

			linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);
			memcpy((unsigned char *)buffer + position, (unsigned char *)linePtr->data + offset, pieceSize);
		}
		/* cache hit */
		else {
			memcpy((unsigned char *)buffer + position, (unsigned char *)linePtr->data + offset, pieceSize);
		}

		position += pieceSize;
		left -= pieceSize;

		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return position;
}


int cache_flush(cachectx_t *cache, const uint64_t begAddr, const uint64_t endAddr)
{
	ssize_t written = 0, count = 0;
	uint64_t addr = 0, end = endAddr, tag = 0, index = 0, begOffset = 0, endOffset = 0;
	cacheline_t *linePtr = NULL;

	if (begAddr > endAddr) {
		return -1;
	}

	if (begAddr > cache->srcMemSize) {
		return -1;
	}
	else if (begAddr < cache->srcMemSize && endAddr > cache->srcMemSize) {
		end = cache->srcMemSize;
	}

	begOffset = cache_compOffset(cache, begAddr);
	addr = begAddr - begOffset;

	mutexLock(cache->lock);

	while (addr < endAddr) {
		index = cache_compSet(cache, addr);
		tag = cache_compTag(cache, addr);

		linePtr = cache_findLine(&cache->sets[index], tag, LIBCACHE_TIMESTAMPS_NO_UPDATE);

		if (linePtr != NULL) {
			count += cache->lineSize;
		}

		written += cache_flushLine(cache->writeCb, linePtr, addr, cache->lineSize, cache->srcMemSize);

		addr += cache->lineSize;
	}

	mutexUnlock(cache->lock);

	return (written == count) ? 0 : -1;
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

	if (begAddr > endAddr) {
		return -1;
	}

	if (begAddr > cache->srcMemSize) {
		return -1;
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

	return 0;
}


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