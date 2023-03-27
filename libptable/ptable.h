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

#ifndef _LIB_PTABLE_H_
#define _LIB_PTABLE_H_

#include <stdint.h>

/* Changelog:
 * version 2: Add checksum and version fields
 */
#define PTABLE_VERSION 2


/* Structure of partition table
 *  _________________________________________________________________________
 * |       28 B      |                 32 B * n                |     4 B     |
 * |-----------------|-----------------------------------------|-------------|
 * | ptable_t header | ptable_part_t 0 | ... | ptable_part_t n | magic bytes |
 *  -------------------------------------------------------------------------
 * 
 *  NOTE: data in partition table should be stored in little endian
 */


/* Partition table magic signature */
static const uint8_t ptable_magic[] = { 0xde, 0xad, 0xfc, 0xbe };


/* Supported partition types */
enum { ptable_raw = 0x51, ptable_jffs2 = 0x72, ptable_meterfs = 0x75 };


typedef struct {
	uint8_t name[8];      /* Partition name */
	uint32_t offset;      /* Partition offset (in bytes) */
	uint32_t size;        /* Partition size (in bytes) */
	uint8_t type;         /* Partition type */
	uint8_t reserved[11]; /* Reserved bytes */
	uint32_t crc;         /* Partition checksum */
} ptable_part_t;


typedef struct {
	uint32_t count;         /* Number of partitions */
	uint8_t version;        /* Ptable struct version */
	uint8_t reserved[19];   /* Reserved bytes */
	uint32_t crc;           /* Header checksum */
	ptable_part_t parts[0]; /* Partitions */
} ptable_t;


/* Returns partition table size provided partition count */
static inline uint32_t ptable_size(uint32_t count)
{
	return sizeof(ptable_t) + count * sizeof(ptable_part_t) + sizeof(ptable_magic);
}


/* Converts partition table to host endianness and verifies it */
extern int ptable_deserialize(ptable_t *ptable, uint32_t memsz, uint32_t blksz);


/* Verifies partition table and converts it to little endian */
extern int ptable_serialize(ptable_t *ptable, uint32_t memsz, uint32_t blksz);


#endif
