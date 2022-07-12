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

#include "cache.h"

#include <stdlib.h>

cacheline_t cache_createLine(uint64_t tag, uint32_t *data)
{
	cacheline_t line = {tag, data, '1'};

	return line;	
}

cacheset_t *cache_createSet(void)
{
	int i = 0;
	cacheset_t *cache_set = malloc(sizeof(cacheset_t)); 

	if (cache_set == NULL) {
		return NULL;
	}
	
	cache_set->timestamps = calloc(NUM_WAYS, sizeof(cacheline_t*));
	cache_set->tags = calloc(NUM_WAYS, sizeof(cacheline_t*));
	cache_set->lines = calloc(NUM_WAYS, sizeof(cacheline_t));

	return cache_set;
}
