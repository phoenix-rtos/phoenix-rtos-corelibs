/*
 * Phoenix-RTOS
 *
 * Memory Technology Device (MTD) Interface
 *
 * Copyright 2022 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _MTD_INTERFACE_H_
#define _MTD_INTERFACE_H_


#include <storage/storage.h>


/* MTD erase operation states */
#define MTD_ERASE_PENDING 0x01
#define MTD_ERASING       0x02
#define MTD_ERASE_SUSPEND 0x04
#define MTD_ERASE_DONE    0x08
#define MTD_ERASE_FAILED  0x10

#define MTD_FAIL_ADDR_UNKNOWN -1LL

/* Types of the MTD devices */
#define MTD_NORFLASH     3
#define MTD_NANDFLASH    4
#define MTD_DATAFLASH    6
#define MTD_UBIVOLUME    7
#define MTD_MLCNANDFLASH 8

/* MTD device flags */
#define MTD_WRITEABLE     0x400
#define MTD_BIT_WRITEABLE 0x800

/* MTD oob ops modes */
#define MTD_OPS_PLACE_OOB 0
#define MTD_OPS_AUTO_OOB  1
#define MTD_OPS_RAW       2


/* Vector structure used by MTD */
struct kvec {
	void *iov_base;
	size_t iov_len;
};


/* If the erase fails, fail_addr might indicate exactly which block failed. If
   fail_addr = MTD_FAIL_ADDR_UNKNOWN, the failure was not at the device level
   or was not specific to any particular block. */
struct erase_info {
	struct mtd_info *mtd;                      /* MTD device */
	uint64_t addr;                             /* Address in bytes on MTD device */
	uint64_t len;                              /* Length of the memory to be erased*/
	uint64_t fail_addr;                        /* Address of sector/block which doesn't succeed */
	void (*callback)(struct erase_info *self); /* Function is called when the erase ends */
	u_long priv;                               /* Private data */
	unsigned char state;                       /* Current state */
	struct erase_info *next;                   /* Next erase info in erase queue */
};


struct mtd_oob_ops {
	unsigned int mode; /* Operation mode */
	size_t len;        /* Number of data bytes to read or write */
	size_t retlen;     /* Number of data bytes successfully read or written */
	size_t ooblen;     /* Number of out-of-band (OOB) bytes to read or write */
	size_t oobretlen;  /* Number of data bytes successfully read or written */
	uint32_t ooboffs;  /* Offset of oob data in the oob area (only relevant when
                          mode = MTD_OPS_PLACE_OOB or MTD_OPS_RAW */
	uint8_t *datbuf;   /* if NULL only oob data are read/written */
	uint8_t *oobbuf;   /* oob data buffer */
};


struct mtd_info {
	unsigned char type;    /* Type of the MTD device */
	int index;             /* Index of the MTD device */
	const char *name;      /* Name of the flash memory */
	uint32_t flags;        /* MTD device flags */
	uint64_t size;         /* Total size of the MTD device */
	uint32_t erasesize;    /* Block erase size */
	uint32_t writesize;    /* Minimal writable size unit. In case of NOR, it is 1, for NAND it is one NAND page */
	uint32_t writebufsize; /* Size of the write buffer used by the MTD. */
	uint32_t oobsize;      /* Amount of OOB per block */
	uint32_t oobavail;     /* Available OOB bytes per block*/
	storage_t *storage;    /* Pointer to storage logical device */
};


/* Erase is an asynchronous operation.  Device drivers are supposed
   to call instr->callback() whenever the operation completes, even
   if it completes with a failure.
   Callers are supposed to pass a callback function and wait for it
   to be called before writing to the block. */
extern int mtd_erase(struct mtd_info *mtd, struct erase_info *instr);


/* Function for eXecute-In-Place mechanism. Phys is optional and may be set to NULL */
extern int mtd_point(struct mtd_info *mtd, off_t from, size_t len, size_t *retlen,
	void **virt, addr_t *phys);


/* Unpoint selected memory area */
extern int mtd_unpoint(struct mtd_info *mtd, off_t from, size_t len);


/* Allow NOMMU mmap() to directly map the device (if not NULL)
    - return the address to which the offset maps
    - return -ENOSYS to indicate refusal to do the mapping */
extern unsigned long mtd_get_unmapped_area(struct mtd_info *mtd, unsigned long len,
	unsigned long offset, unsigned long flags);


/* Read data from defined offset from flash memory */
extern int mtd_read(struct mtd_info *mtd, off_t from, size_t len, size_t *retlen,
	unsigned char *buf);


/* Write data at defined offset to flash memory */
extern int mtd_write(struct mtd_info *mtd, off_t to, size_t len, size_t *retlen,
	const unsigned char *buf);


/* Read oob data from defined offset */
extern int mtd_read_oob(struct mtd_info *mtd, off_t from, struct mtd_oob_ops *ops);


/* Write oob data to defined offset */
extern int mtd_write_oob(struct mtd_info *mtd, off_t to, struct mtd_oob_ops *ops);


/* The vector-based MTD write method */
extern int mtd_writev(struct mtd_info *mtd, const struct kvec *vecs,
	unsigned long count, off_t to, size_t *retlen);


/* Wait until flash operation is completed */
extern void mtd_sync(struct mtd_info *mtd);


/* Lock from writing and erasing a specific flash memory area */
extern int mtd_lock(struct mtd_info *mtd, off_t ofs, uint64_t len);


/* Unlock from writing and erasing a specific flash memory area */
extern int mtd_unlock(struct mtd_info *mtd, off_t ofs, uint64_t len);


/* Check locking bit of a specific flash memory area */
extern int mtd_is_locked(struct mtd_info *mtd, off_t ofs, uint64_t len);


/* Check whether at defined offset is a reserved block */
extern int mtd_block_isreserved(struct mtd_info *mtd, off_t ofs);


/* Check whether at defined offset is a bad block */
extern int mtd_block_isbad(struct mtd_info *mtd, off_t ofs);


/* Mark bad block */
extern int mtd_block_markbad(struct mtd_info *mtd, off_t ofs);


/* Suspend a flash memory */
extern int mtd_suspend(struct mtd_info *mtd);


/* Resume a flash memory */
extern void mtd_resume(struct mtd_info *mtd);


/* Allocate a contiguous buffer up to the specified size.
   This function also makes sure that the allocated buffer is aligned to
   the MTD device's min. I/O unit, i.e. the "mtd->writesize" value.

TODO: return aligned buffer
*/
extern void *mtd_kmalloc_up_to(const struct mtd_info *mtd, size_t *size);


/* Check if err is EUCLEAN (Structure needs cleaning). Common usage to check result of a mtd_read_oob function */
extern int mtd_is_bitflip(int err);


#endif
