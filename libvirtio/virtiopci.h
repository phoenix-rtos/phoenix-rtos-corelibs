/*
 * Phoenix-RTOS
 *
 * VirtIO PCI low level interface
 *
 * Copyright 2020 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _VIRTIOPCI_H_
#define _VIRTIOPCI_H_

#include <stdint.h>

#include "virtio.h"


typedef struct {
	uint8_t id;     /* Vendor ID */
	uint8_t next;   /* Next capability offset */
	uint8_t len;    /* Capability length */
	uint8_t type;   /* Capability type */
	uint8_t bar;    /* Capability BAR index */
	uint8_t pad[3]; /* Padding */
	uint32_t offs;  /* Offset within BAR */
	uint32_t size;  /* Capability structure size */
} __attribute__((packed)) virtiopci_cap_t;


extern uint8_t virtiopci_read8(void *base, unsigned int reg);


extern uint16_t virtiopci_read16(void *base, unsigned int reg);


extern uint32_t virtiopci_read32(void *base, unsigned int reg);


extern uint64_t virtiopci_read64(void *base, unsigned int reg);


extern void virtiopci_write8(void *base, unsigned int reg, uint8_t val);


extern void virtiopci_write16(void *base, unsigned int reg, uint16_t val);


extern void virtiopci_write32(void *base, unsigned int reg, uint32_t val);


extern void virtiopci_write64(void *base, unsigned int reg, uint64_t val);


/* Destroys VirtIO PCI device */
extern void virtiopci_destroyDev(virtio_dev_t *vdev);


/* Initializes VirtIO PCI device */
extern int virtiopci_initDev(virtio_dev_t *vdev);


/* Returns first VirtIO PCI capability matching given type */
extern virtiopci_cap_t *virtiopci_getCap(virtiopci_cap_t *caps, unsigned char type);


/* Initalizes VirtIO register based on PCI BAR data */
extern int virtiopci_initReg(unsigned long base, unsigned long len, unsigned char flags, unsigned char ext, virtio_reg_t *reg);


/* Detects next VirtIO PCI device matching info descriptor */
extern int virtiopci_find(const virtio_devinfo_t *info, virtio_dev_t *vdev, virtio_ctx_t *vctx);


#endif
