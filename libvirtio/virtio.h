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

#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <endian.h>
#include <stdint.h>

#include <sys/types.h>

/* clang-format off */
typedef enum {
	vdevNONE = 0x00, /* No VirtIO device */
	vdevPCI  = 0x01, /* VirtIO PCI device */
	vdevMMIO = 0x02  /* VirtIO MMIO device */
} virtio_devtype_t;
/* clang-format on */

typedef struct _virtio_seg_t virtio_seg_t;


struct _virtio_seg_t {
	void *buff;                /* Buffer exposed to device */
	unsigned int len;          /* Buffer length */
	virtio_seg_t *prev, *next; /* Doubly linked list */
};


typedef struct {
	virtio_seg_t *segs; /* Request segments list */
	unsigned int rsegs; /* Number of device readable segments */
	unsigned int wsegs; /* Number of device writable segments */
} virtio_req_t;


typedef struct {
	uint64_t addr;  /* Buffer physical address */
	uint32_t len;   /* Buffer length */
	uint16_t flags; /* Descriptor flags */
	uint16_t next;  /* Next chained descriptor index (if flags & 0x1) */
} __attribute__((packed)) virtio_desc_t;


typedef struct {
	uint16_t flags;  /* Used buffer notification suppression */
	uint16_t idx;    /* Next available request index */
	uint16_t ring[]; /* Available requests (descriptor chain IDs) */
} __attribute__((packed)) virtio_avail_t;


typedef struct {
	uint32_t id;  /* Descriptor chain ID */
	uint32_t len; /* Bytes written into the descriptor chain buffers */
} __attribute__((packed)) virtio_used_elem_t;


typedef struct {
	uint16_t flags;            /* Available buffer notification suppression */
	uint16_t idx;              /* Next processed request ring index */
	virtio_used_elem_t ring[]; /* Processed requests */
} __attribute__((packed)) virtio_used_t;


typedef struct {
	/* Standard split virtqueue layout */
	volatile virtio_desc_t *desc;   /* Descriptors */
	volatile virtio_avail_t *avail; /* Avail ring */
	volatile uint16_t *uevent;      /* Used event notification suppression */
	volatile virtio_used_t *used;   /* Used ring */
	volatile uint16_t *aevent;      /* Avail event notification suppression */

	/* Custom helper fields */
	void **buffs;       /* Descriptors buffers */
	void *mem;          /* Allocated virtqueue memory */
	unsigned int memsz; /* Allocated virtqueue memory size */
	unsigned int idx;   /* Virtqueue index */
	unsigned int size;  /* Virtqueue size */
	unsigned int noffs; /* Virtqueue notification area offset (modern VirtIO PCI device only) */
	unsigned int nfree; /* Number of free descriptors */
	uint16_t free;      /* Next free descriptor index */
	uint16_t last;      /* Last processed request index */

	/* Synchronization */
	handle_t cond; /* Free descriptors condition variable */
	handle_t lock; /* Virtqueue mutex */
} virtqueue_t;


typedef struct {
	void *addr; /* Base register address */
	size_t len; /* Registers memory length */
} virtio_reg_t;


typedef struct {
	virtio_devtype_t type; /* VirtIO device type */
	unsigned int id;       /* VirtIO device ID */
	unsigned int irq;      /* Interrupt number */
	unsigned int xntf;     /* Notification registers offset multiplier */
	virtio_reg_t base;     /* Base registers */
	virtio_reg_t ntf;      /* Notification register */
	virtio_reg_t isr;      /* Interrupt status register */
	virtio_reg_t cfg;      /* Configuration registers */
} virtio_devinfo_t;


typedef struct {
	virtio_devinfo_t info; /* VirtIO device core data */
	uint64_t features;     /* VirtIO device features */
} virtio_dev_t;


typedef struct {
	unsigned char reset;   /* Indicates that context needs reset */
	unsigned char ctx[32]; /* VirtIO device detection context */
} virtio_ctx_t;


/* VirtIO legacy device */
static inline int virtio_legacy(virtio_dev_t *vdev)
{
	return !(vdev->features & (1ULL << 32));
}


/* VirtIO modern device */
static inline int virtio_modern(virtio_dev_t *vdev)
{
	return !virtio_legacy(vdev);
}

/* clang-format off */
/* VirtIO memory barrier */
static inline void virtio_mb(void)
{
	__asm__ __volatile__("" ::: "memory");
}


/* VirtIO device to guest endian */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define virtio_vtog(n) \
	static inline uint##n##_t virtio_vtog##n(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		return val; \
	}
#elif __BYTE_ORDER == __BIG_ENDIAN
	#define virtio_vtog(n) \
	static inline uint##n##_t virtio_vtog##n(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		if (virtio_modern(vdev)) \
			val = le##n##toh(val); \
		return val; \
	}
#else
#error "Unsupported byte order"
#endif


virtio_vtog(16)
virtio_vtog(32)
virtio_vtog(64)


/* Guest to VirtIO device endian */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define virtio_gtov(n) \
	static inline uint##n##_t virtio_gtov##n(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		return val; \
	}
#elif __BYTE_ORDER == __BIG_ENDIAN
	#define virtio_gtov(n) \
	static inline uint##n##_t virtio_gtov##n(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		if (virtio_modern(vdev)) \
			val = htole##n(val); \
		return val; \
	}
#else
#error "Unsupported byte order"
#endif


virtio_gtov(16)
virtio_gtov(32)
virtio_gtov(64)


/* Enables virtqueue interrupts (hint for the host) */
extern void virtqueue_enableIRQ(virtio_dev_t *vdev, virtqueue_t *vq);
/* clang-format on */

/* Disables virtqueue interrupts (hint for the host) */
extern void virtqueue_disableIRQ(virtio_dev_t *vdev, virtqueue_t *vq);


/* Enqueues request in virtqueue */
extern int virtqueue_enqueue(virtio_dev_t *vdev, virtqueue_t *vq, virtio_req_t *req);


/* Notifies device of available requests */
extern void virtqueue_notify(virtio_dev_t *vdev, virtqueue_t *vq);


/* Dequeues request from virtqueue (returns request head buffer) */
extern void *virtqueue_dequeue(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int *len);


/* Destroys virtqueue */
extern void virtqueue_destroy(virtio_dev_t *vdev, virtqueue_t *vq);


/* Initializes virtqueue */
extern int virtqueue_init(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int idx, unsigned int size);


/* Reads from VirtIO device configuration space */
extern uint8_t virtio_readConfig8(virtio_dev_t *vdev, unsigned int reg);


extern uint16_t virtio_readConfig16(virtio_dev_t *vdev, unsigned int reg);


extern uint32_t virtio_readConfig32(virtio_dev_t *vdev, unsigned int reg);


extern uint64_t virtio_readConfig64(virtio_dev_t *vdev, unsigned int reg);


/* Writes to VirtIO device configuration space */
extern void virtio_writeConfig8(virtio_dev_t *vdev, unsigned int reg, uint8_t val);


extern void virtio_writeConfig16(virtio_dev_t *vdev, unsigned int reg, uint16_t val);


extern void virtio_writeConfig32(virtio_dev_t *vdev, unsigned int reg, uint32_t val);


extern void virtio_writeConfig64(virtio_dev_t *vdev, unsigned int reg, uint64_t val);


/* Reads VirtIO device supported/negotiated features */
extern uint64_t virtio_readFeatures(virtio_dev_t *vdev);


/* Writes driver supported features, completes features negotiation */
extern int virtio_writeFeatures(virtio_dev_t *vdev, uint64_t features);


/* Reads VirtIO device status register */
extern uint8_t virtio_readStatus(virtio_dev_t *vdev);


/* Writes VirtIO device status register */
extern void virtio_writeStatus(virtio_dev_t *vdev, uint8_t status);


/* Reads interrupt status */
extern unsigned int virtio_isr(virtio_dev_t *vdev);


/* Resets VirtIO device */
extern void virtio_reset(virtio_dev_t *vdev);


/* Destroys VirtIO device */
extern void virtio_destroyDev(virtio_dev_t *vdev);


/* Initializes VirtIO device */
extern int virtio_initDev(virtio_dev_t *vdev);


/* Detects next VirtIO device matching given device descriptor */
extern int virtio_find(const virtio_devinfo_t *info, virtio_dev_t *vdev, virtio_ctx_t *vctx);


/* Destroys VirtIO library */
extern void virtio_done(void);


/* Initializes VirtIO library */
extern int virtio_init(void);


#endif
