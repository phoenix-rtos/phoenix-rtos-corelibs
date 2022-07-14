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
#define LIBCACHE_NUM_WAYS 2
#endif

#ifndef LIBCACHE_NUM_SETS
#define LIBCACHE_NUM_SETS 16
#endif

#ifndef LIBCACHE_CACHE_LINE_SIZE
#define LIBCACHE_CACHE_LINE_SIZE 64
#endif


#endif
