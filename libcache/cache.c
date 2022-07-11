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

cacheline_t cache_createLine(uint64_t tag, uint32_t *data)
{
	cacheline_t line = {tag, data, '1'};

	return line;	
}
