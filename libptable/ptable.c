/*
 * Phoenix-RTOS
 *
 * Partition table
 *
 * Copyright 2020, 2023 Phoenix Systems
 * Author: Hubert Buczynski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <ctype.h>
#include <endian.h>
#include <stddef.h>
#include <string.h>

/*
 * Per project board configuration may overwrite PTABLE_CHECKSUM definition
 * When compiling on a host the board config is not needed, associated with issue #671
 * It allows building host-utils separately for host-generic-pc
 */

#ifdef phoenix
#include <board_config.h>
#endif

#include "ptable.h"


/* Enable checksum for partition table */
#ifndef PTABLE_CHECKSUM
#define PTABLE_CHECKSUM 0
#endif


static uint32_t ptable_crc32(const void *data, size_t len)
{
	uint32_t crc = 0xffffffff;
	int i;

	while (len--) {
		crc ^= *(const uint8_t *)data++;
		for (i = 0; i < 8; i++) {
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
		}
	}

	return ~crc;
}


static int ptable_partVerify(const ptable_t *ptable, const ptable_part_t *part, uint32_t memsz, uint32_t blksz)
{
	const ptable_part_t *p;
	int i;

#if PTABLE_CHECKSUM
	/* Verify partition checksum */
	if (part->crc != ptable_crc32(part, offsetof(ptable_part_t, crc))) {
		return -1;
	}
#endif

	/* Verify offset and size */
	if (part->size == 0) {
		return -1;
	}

	if (part->size % blksz != 0) {
		return -1;
	}

	if (part->offset % blksz != 0) {
		return -1;
	}

	if (part->offset + part->size > memsz) {
		return -1;
	}

	/* Check for overflow */
	if (part->offset + part->size < part->offset) {
		return -1;
	}

	/* Verify partition type */
	switch (part->type) {
		case ptable_raw:
		case ptable_jffs2:
		case ptable_meterfs:
			break;

		default:
			return -1;
	}

	/* Verify partition name */
	for (i = 0; i < sizeof(part->name); i++) {
		if (!isalnum(part->name[i])) {
			break;
		}
	}

	if ((i == 0) || (i >= sizeof(part->name)) || (part->name[i] != '\0')) {
		return -1;
	}

	/* Compare against previous partitions */
	for (p = ptable->parts; p != part; p++) {
		/* Check for range overlap */
		if ((part->offset <= p->offset + p->size - 1) && (part->offset + part->size - 1 >= p->offset)) {
			return -1;
		}

		/* Check for name duplicate */
		if (strcmp((const char *)part->name, (const char *)p->name) == 0) {
			return -1;
		}
	}

	return 0;
}


static int ptable_verify(const ptable_t *ptable, uint32_t memsz, uint32_t blksz)
{
	uint32_t size, i;

#if PTABLE_CHECKSUM
	/* Verify header checksum */
	if (ptable->crc != ptable_crc32(ptable, offsetof(ptable_t, crc))) {
		return -1;
	}
#endif

	/* Verify partition table size */
	size = ptable_size(ptable->count);
	if (size > blksz) {
		return -1;
	}

	/* Verify magic signature */
	if (memcmp((const uint8_t *)ptable + size - sizeof(ptable_magic), ptable_magic, sizeof(ptable_magic)) != 0) {
		return -1;
	}

	/* Verify partitions */
	for (i = 0; i < ptable->count; i++) {
		if (ptable_partVerify(ptable, ptable->parts + i, memsz, blksz) < 0) {
			return -1;
		}
	}

	return 0;
}


int ptable_deserialize(ptable_t *ptable, uint32_t memsz, uint32_t blksz)
{
	uint32_t i;

	if (ptable == NULL) {
		return -1;
	}

	ptable->count = le32toh(ptable->count);
	ptable->crc = le32toh(ptable->crc);

	for (i = 0; i < ptable->count; i++) {
		ptable->parts[i].offset = le32toh(ptable->parts[i].offset);
		ptable->parts[i].size = le32toh(ptable->parts[i].size);
		ptable->parts[i].crc = le32toh(ptable->parts[i].crc);
	}

	return ptable_verify(ptable, memsz, blksz);
}


int ptable_serialize(ptable_t *ptable, uint32_t memsz, uint32_t blksz)
{
	uint32_t i;

	if (ptable == NULL) {
		return -1;
	}

	/* Calculate checksums */
	ptable->crc = ptable_crc32(ptable, offsetof(ptable_t, crc));
	for (i = 0; i < ptable->count; i++) {
		ptable->parts[i].crc = ptable_crc32(ptable->parts + i, offsetof(ptable_part_t, crc));
	}

	/* Add magic signature */
	memcpy((uint8_t *)ptable + ptable_size(ptable->count) - sizeof(ptable_magic), ptable_magic, sizeof(ptable_magic));

	if (ptable_verify(ptable, memsz, blksz) < 0) {
		return -1;
	}

	for (i = 0; i < ptable->count; i++) {
		ptable->parts[i].offset = htole32(ptable->parts[i].offset);
		ptable->parts[i].size = htole32(ptable->parts[i].size);
		ptable->parts[i].crc = htole32(ptable->parts[i].crc);
	}

	ptable->count = htole32(ptable->count);
	ptable->crc = htole32(ptable->crc);

	return 0;
}
