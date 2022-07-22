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

#ifndef _CACHE_H
#define _CACHE_H

#include <stdio.h>
#include <stdint.h>


typedef struct cachectx_s cachectx_t;


typedef ssize_t (*cache_readCb_t)(off_t offset, void *buffer, size_t size, size_t count);


typedef ssize_t (*cache_writeCb_t)(off_t offset, const void *buffer, size_t size, size_t count);


cachectx_t *cache_init(size_t size, size_t lineSize, cache_writeCb_t writeCb, cache_readCb_t readCb);


int cache_deinit(cachectx_t *cache);


ssize_t cache_read(cachectx_t *cache, const off_t addr, void *buffer);


ssize_t cache_write(cachectx_t *cache, const off_t addr, void *buffer);


int cache_flush(cachectx_t *cache, const off_t begAddr, const uint64_t endAddr);


int cache_invalidate(cachectx_t *cache, const off_t begAddr, const uint64_t endAddr);


#endif
