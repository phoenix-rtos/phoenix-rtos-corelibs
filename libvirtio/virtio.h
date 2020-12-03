/*
 * Phoenix-RTOS
 *
 * VirtIO low level interface
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

#include "libvirtio.h"
#include "virtiopci.h"


/* VirtIO modern device */
static inline int virtio_modern(virtio_dev_t *vdev)
{
	return (vdev->features & (1ULL << 32));
}


/* VirtIO legacy device */
static inline int virtio_legacy(virtio_dev_t *vdev)
{
	return !virtio_modern(vdev);
}


/* VirtIO memory barrier */
static inline void virtio_mb(void)
{
	__asm__ __volatile__("" ::: "memory");
}


/* Reads VirtIO device supported features */
extern uint64_t virtio_getFeatures(virtio_dev_t *vdev);


/* VirtIO device to guest endian */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define virtio_vtog(n) \
	static inline uint##n##_t virtio_vtog##n(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		return val; \
	}
#else
	#define virtio_vtog(n) \
	static inline uint##n##_t virtio_vtog##n(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		if (virtio_modern(vdev)) \
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
	static inline uint##n##_t virtio_gtov##n(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		return val; \
	}
#else
	#define virtio_gtov(n) \
	static inline uint##n##_t virtio_gtov##n(virtio_dev_t *vdev, uint##n##_t val) \
	{ \
		if (virtio_modern(vdev)) \
			val = htole##n##(val); \
		return val; \
	}
#endif


virtio_gtov(16)
virtio_gtov(32)
virtio_gtov(64)


static inline uint8_t virtio_read8(virtio_dev_t *vdev, void *base, unsigned int reg)
{
	if (vdev->info.type == vdevPCI)
		return virtiopci_read8(base, reg);

	return *(volatile uint8_t *)((uintptr_t)base + reg);
}


static inline uint16_t virtio_read16(virtio_dev_t *vdev, void *base, unsigned int reg)
{
	if (vdev->info.type == vdevPCI)
		return virtio_vtog16(vdev, virtiopci_read16(base, reg));

	return virtio_vtog16(vdev, *(volatile uint16_t *)((uintptr_t)base + reg));
}


static inline uint32_t virtio_read32(virtio_dev_t *vdev, void *base, unsigned int reg)
{
	if (vdev->info.type == vdevPCI)
		return virtio_vtog32(vdev, virtiopci_read32(base, reg));

	return virtio_vtog32(vdev, *(volatile uint32_t *)((uintptr_t)base + reg));
}


static inline uint64_t virtio_read64(virtio_dev_t *vdev, void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base + reg;
	uint64_t val;

	if (vdev->info.type == vdevPCI)
		return virtio_vtog64(vdev, virtiopci_read64(base, reg));

#if __BYTE_ORDER == __LITTLE_ENDIAN
	val = *(volatile uint32_t *)(addr + 4);
	val <<= 32;
	val |= *(volatile uint32_t *)addr;
#else
	val = *(volatile uint32_t *)addr;
	val <<= 32;
	val |= *(volatile uint32_t *)(addr + 4);
#endif

	return virtio_vtog64(vdev, val);
}


static inline void virtio_write8(virtio_dev_t *vdev, void *base, unsigned int reg, uint8_t val)
{
	if (vdev->info.type == vdevPCI) {
		virtiopci_write8(base, reg, val);
		return;
	}

	*(volatile uint8_t *)((uintptr_t)base + reg) = val;
}


static inline void virtio_write16(virtio_dev_t *vdev, void *base, unsigned int reg, uint16_t val)
{
	val = virtio_gtov16(vdev, val);

	if (vdev->info.type == vdevPCI) {
		virtiopci_write16(base, reg, val);
		return;
	}

	*(volatile uint16_t *)((uintptr_t)base + reg) = val;
}


static inline void virtio_write32(virtio_dev_t *vdev, void *base, unsigned int reg, uint32_t val)
{
	val = virtio_gtov32(vdev, val);

	if (vdev->info.type == vdevPCI) {
		virtiopci_write32(base, reg, val);
		return;
	}

	*(volatile uint32_t *)((uintptr_t)base + reg) = val;
}


static inline void virtio_write64(virtio_dev_t *vdev, void *base, unsigned int reg, uint64_t val)
{
	uintptr_t addr = (uintptr_t)base + reg;
	val = virtio_gtov64(vdev, val);

	if (vdev->info.type == vdevPCI) {
		virtiopci_write64(base, reg, val);
		return;
	}

#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(volatile uint32_t *)(addr + 4) = val >> 32;
	*(volatile uint32_t *)addr = val;
#else
	*(volatile uint32_t *)addr = val >> 32;
	*(volatile uint32_t *)(addr + 4) = val;
#endif
}


#endif
