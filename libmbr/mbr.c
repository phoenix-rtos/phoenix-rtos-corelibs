/*
 * Phoenix-RTOS
 *
 * Master Boot Record
 *
 * Copyright 2017, 2020, 2024 Phoenix Systems
 * Author: Kamil Amanowicz, Lukasz Kosinski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <endian.h>

/* le16toh() and le32toh() not defined on MacOS */
#ifdef __APPLE__

#include <libkern/OSByteOrder.h>
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#endif

#include "mbr.h"


int mbr_deserialize(mbr_t *mbr)
{
	if (le16toh(mbr->magic) != MBR_MAGIC) {
		return -1;
	}

	for (int i = 0; i < MBR_PARTITIONS; i++) {
		mbr->pent[i].start = le32toh(mbr->pent[i].start);
		mbr->pent[i].sectors = le32toh(mbr->pent[i].sectors);
	}

	return 0;
}
