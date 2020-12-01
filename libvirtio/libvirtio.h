/*
 * Phoenix-RTOS
 *
 * VirtIO library interface
 *
 * Copyright 2020 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBVIRTIO_H_
#define _LIBVIRTIO_H_

#include <stddef.h>
#include <stdint.h>

#include <sys/types.h>


typedef struct _virtio_seg_t virtio_seg_t;


struct _virtio_seg_t {
	void *buff;                     /* Buffer exposed to device */
	unsigned int len;               /* Buffer length */
	virtio_seg_t *prev, *next;      /* Doubly linked list */
};


typedef struct {
	virtio_seg_t *segs;             /* Request segments list */
	unsigned int rsegs;             /* Number of device readable segments */
	unsigned int wsegs;             /* Number of device writable segments */
} virtio_req_t;


typedef struct {
	uint64_t addr;                  /* Buffer physical address */
	uint32_t len;                   /* Buffer length */
	uint16_t flags;                 /* Descriptor flags */
	uint16_t next;                  /* Next chained descriptor index (if flags & 0x1) */
} __attribute__((packed)) virtio_desc_t;


typedef struct {
	uint16_t flags;                 /* Used buffer notification suppression */
	uint16_t idx;                   /* Next available request index */
	uint16_t ring[];                /* Available requests (descriptor chain IDs) */
} __attribute__((packed)) virtio_avail_t;


typedef struct {
	uint32_t id;                    /* Descriptor chain ID */
	uint32_t len;                   /* Bytes written into the descriptor chain buffers */
} __attribute__((packed)) virtio_used_elem_t;


typedef struct {
	uint16_t flags;                 /* Available buffer notification suppression */
	uint16_t idx;                   /* Next processed request ring index */
	virtio_used_elem_t ring[];      /* Processed requests */
} __attribute__((packed)) virtio_used_t;


typedef struct {
	/* Standard split virtqueue layout */
	volatile virtio_desc_t *desc;   /* Descriptors */
	volatile virtio_avail_t *avail; /* Avail ring */
	volatile uint16_t *uevent;      /* Used event notification suppression */
	volatile virtio_used_t *used;   /* Used ring */
	volatile uint16_t *aevent;      /* Avail event notification suppression */

	/* Custom helper fields */
	void **buffs;                   /* Descriptors buffers */
	void *mem;                      /* Allocated virtqueue memory */
	unsigned int memsz;             /* Allocated virtqueue memory size */
	unsigned int idx;               /* Virtqueue index */
	unsigned int size;              /* Virtqueue size */
	unsigned int nfree;             /* Number of free descriptors */
	uint16_t free;                  /* Next free desriptor index */
	uint16_t last;                  /* Last processed request */

	/* Synchronization */
	handle_t dcond;                 /* Free descriptors condition variable */
	handle_t dlock;                 /* Free descriptors mutex */
	handle_t lock;                  /* Virtqueue mutex */
} virtqueue_t;


typedef struct _virtio_dev_t virtio_dev_t;


struct _virtio_dev_t {
	enum {
		vdevPCI,                    /* VirtIO PCI device */
		vdevMMIO                    /* VirtIO MMIO device */
	} type;                         /* VirtIO device type */
	uint64_t features;              /* Device features */
	unsigned int irq;               /* Interrupt number */
	void *base;                     /* Base registers address */
	void *ntf;                      /* Device notification register address */
	void *isr;                      /* Interrupt status register address */
	void *cfg;                      /* Device configuration registers address */
	virtio_dev_t *prev, *next;      /* Doubly linked list */
};


/* Enables virtqueue interrupts (hint for the host) */
void virtqueue_enableirq(virtio_dev_t *vdev, virtqueue_t *vq);


/* Disables virtqueue interrupts (hint for the host) */
void virtqueue_disableirq(virtio_dev_t *vdev, virtqueue_t *vq);


/* Enqueues request in virtqueue */
int virtqueue_enqueue(virtio_dev_t *vdev, virtqueue_t *vq, virtio_req_t *req);


/* Notifies device of available requests */
void virtqueue_notify(virtio_dev_t *vdev, virtqueue_t *vq);


/* Dequeues request from virtqueue (returns request head buffer) */
void *virtqueue_dequeue(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int *len);


/* Destroys virtqueue */
void virtqueue_destroy(virtio_dev_t *vdev, virtqueue_t *vq);


/* Initializes virtqueue */
int virtqueue_init(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int idx, unsigned int size);


/* Reads from VirtIO device configuration space */
uint8_t virtio_readConfig8(virtio_dev_t *vdev, unsigned int reg);


uint16_t virtio_readConfig16(virtio_dev_t *vdev, unsigned int reg);


uint32_t virtio_readConfig32(virtio_dev_t *vdev, unsigned int reg);


uint64_t virtio_readConfig64(virtio_dev_t *vdev, unsigned int reg);


/* Writes to VirtIO device configuration space */
void virtio_writeConfig8(virtio_dev_t *vdev, unsigned int reg, uint8_t val);


void virtio_writeConfig16(virtio_dev_t *vdev, unsigned int reg, uint16_t val);


void virtio_writeConfig32(virtio_dev_t *vdev, unsigned int reg, uint32_t val);


void virtio_writeConfig64(virtio_dev_t *vdev, unsigned int reg, uint64_t val);


/* Reads VirtIO device supported/negotiated features */
uint64_t virtio_readFeatures(virtio_dev_t *vdev);


/* Writes driver supported features, completes features negotiation */
int virtio_writeFeatures(virtio_dev_t *vdev, uint64_t features);


/* Reads VirtIO device status register */
uint8_t virtio_readStatus(virtio_dev_t *vdev);


/* Writes VirtIO device status register */
void virtio_writeStatus(virtio_dev_t *vdev, uint8_t status);


/* Reads interrupt status */
unsigned int virtio_isr(virtio_dev_t *vdev);


/* Resets VirtIO device */
void virtio_reset(virtio_dev_t *vdev);


/* Destroys VirtIO device */
void virtio_destroy(virtio_dev_t *vdev);


/* Detects VirtIO devices with specified ID and perform their generic initialization */
int virtio_init(unsigned int id, unsigned int vdevsz, int (*init)(void *), void **vdevs);


#endif
