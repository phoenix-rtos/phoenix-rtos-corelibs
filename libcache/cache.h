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

#ifndef _CACHE_H
#define _CACHE_H

#include <stdint.h>
#include <sys/types.h>


/* N-way set-associative cache parameters */
#ifndef LIBCACHE_NUM_WAYS
#define LIBCACHE_NUM_WAYS 4
#endif

#ifndef LIBCACHE_NUM_SETS
#define LIBCACHE_NUM_SETS 8
#endif

#ifndef LIBCACHE_CACHE_LINE_SIZE
#define LIBCACHE_CACHE_LINE_SIZE 64
#endif

#ifndef LIBCACHE_MEM_SIZE
#define LIBCACHE_MEM_SIZE 2048
#endif

#ifndef LIBCACHE_ADDR_WIDTH
#define LIBCACHE_ADDR_WIDTH 64
#endif


typedef struct _cachetable_t cachetable_t;


/* Creates cache table */
cachetable_t *cache_create();


/* Adds line to cache table */
void cache_add(cachetable_t *cache, uint64_t addr, uint32_t *data);


/* Search address in cache */
uint32_t *cache_search(cachetable_t *cache, const uint64_t addr);


#endif
