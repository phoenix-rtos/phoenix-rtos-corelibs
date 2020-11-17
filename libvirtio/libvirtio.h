/*
 * Phoenix-RTOS
 *
 * VirtIO device core interface
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

#include <endian.h>
#include <stdint.h>

#include <sys/types.h>


typedef struct _virtio_seg_t virtio_seg_t;


struct _virtio_seg_t {
	void *buff;                     /* Buffer exposed to device */
	unsigned int len;               /* Buffer length */
	_virtio_seg_t *prev, *next;     /* Doubly linked list */
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
	uint16_t last;                  /* Last processed request (trails used ring index) */

	/* Synchronization */
	handle_t dcond;                 /* Free descriptors condition variable */
	handle_t dlock;                 /* Free descriptors mutex */
	handle_t lock;                  /* Virtqueue mutex */
} virtqueue_t;


typedef struct {
	void* base;                    /* Base registers address */
	uint64_t features;             /* Device features */
} virtio_dev_t;


/* VirtIO memory barrier */
static inline void virtio_mb(void)
{
	__asm__ __volatile__("" ::: "memory");
}


/* VirtIO device version */
static inline int virtio_legacy(virtio_dev_t *vdev)
{
	return !(vdev->features & 0x100000000ULL);
}


/* VirtIO device to guest endian */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define virtio_vtog(n) \
	static inline uint##n##_t virtio_vtog##n##(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		return val; \
	}
#elif __BYTE_ORDER == __BIG_ENDIAN
	#define virtio_vtog(n) \
	static inline uint##n##_t virtio_vtog##n##(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		if (!virtio_legacy(vdev)) \
			val = le##n##toh(val); \
		return val; \
	}
#endif


virtio_vtog(16)
virtio_vtog(32)
virtio_vtog(64)


/* Guest to VirtIO device endian */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define virtio_gtov(n) \
	static inline uint##n##_t virtio_gtov##n##(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		return val; \
	}
#elif __BYTE_ORDER == __BIG_ENDIAN
	#define virtio_gtov(n) \
	static inline uint##n##_t virtio_gtov##n##(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		if (!virtio_legacy(vdev)) \
			val = htole##n##(val); \
		return val; \
	}
#endif


virtio_gtov(16)
virtio_gtov(32)
virtio_gtov(64)


/* Virtqueue data access interface */
uint8_t virtqueue_read8(virtio_dev_t *vdev, volatile void *addr);


uint16_t virtqueue_read16(virtio_dev_t *vdev, volatile void *addr);


uint32_t virtqueue_read32(virtio_dev_t *vdev, volatile void *addr);


uint64_t virtqueue_read64(virtio_dev_t *vdev, volatile void *addr);


void virtqueue_write8(virtio_dev_t *vdev, volatile void *addr, uint8_t val);


void virtqueue_write16(virtio_dev_t *vdev, volatile void *addr, uint16_t val);


void virtqueue_write32(virtio_dev_t *vdev, volatile void *addr, uint32_t val);


void virtqueue_write64(virtio_dev_t *vdev, volatile void *addr, uint64_t val);


/* Enables virtqueue interrupts (only a hint for the host) */
void virtqueue_enableirq(virtio_dev_t *vdev, virtqueue_t *vq);


/* Disables virtqueue interrupts (only a hint for the host) */
void virtqueue_disableirq(virtio_dev_t *vdev, virtqueue_t *vq);


/* Allocates virtqueue */
virtqueue_t *virtqueue_alloc(unsigned int idx, unsigned int size);


/* Releases virtqueue */
void virtqueue_free(virtqueue_t *vq);


/* Enqueues request in avail ring */
int virtqueue_enqueue(virtio_dev_t *vdev, virtqueue_t *vq, virtio_req_t *req);


/* Notifies device of new requests in virtqueue */
void virtqueue_notify(virtio_dev_t *vdev, virtqueue_t *vq);


/* Dequeues request from used ring (returns request head buffer) */
void *virtqueue_dequeue(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int *len);


/* Polls for a processed request (returns request head buffer) */
void *virtqueue_poll(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int *len);


/* VirtIO device interface */
uint8_t virtio_read8(void *vdev, unsigned int reg);
uint16_t virtio_read16(void *vdev, unsigned int reg);
uint32_t virtio_read32(void *vdev, unsigned int reg);
uint64_t virtio_read64(void *vdev, unsigned int reg);

void virtio_write8(void *vdev, unsigned int reg, uint8_t val);
void virtio_write16(void *vdev, unsigned int reg, uint16_t val);
void virtio_write32(void *vdev, unsigned int reg, uint32_t val);
void virtio_write64(void *vdev, unsigned int reg, uint64_t val);

int virtio_init(void *base, virtio_dev_t *vdev);

#endif
