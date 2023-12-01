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

#ifndef _VIRTIOLOW_H_
#define _VIRTIOLOW_H_

#include "virtio.h"
#include "virtiopci.h"


/* Reads VirtIO device supported features */
extern uint64_t virtio_getFeatures(virtio_dev_t *vdev);


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
#elif __BYTE_ORDER == __BIG_ENDIAN
	val = *(volatile uint32_t *)addr;
	val <<= 32;
	val |= *(volatile uint32_t *)(addr + 4);
#else
#error "Unsupported byte order"
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
#elif __BYTE_ORDER == __BIG_ENDIAN
	*(volatile uint32_t *)addr = val >> 32;
	*(volatile uint32_t *)(addr + 4) = val;
#else
#error "Unsupported byte order"
#endif
}


#endif
