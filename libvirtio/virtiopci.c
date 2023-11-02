/*
 * Phoenix-RTOS
 *
 * VirtIO PCI common interface
 *
 * Copyright 2020 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stddef.h>

#include <sys/mman.h>

#include "virtio.h"
#include "virtiolow.h"


static void virtiopci_unmapReg(virtio_reg_t *reg)
{
	uintptr_t addr = (uintptr_t)reg->addr;

	if (addr & 0x1)
		return;

	munmap((void *)(addr & ~(_PAGE_SIZE - 1)), (reg->len + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1));
}


static int virtiopci_mapReg(virtio_reg_t *reg)
{
	uintptr_t addr = (uintptr_t)reg->addr;
	unsigned int offs = addr & (_PAGE_SIZE - 1);

	if (addr & 0x1)
		return EOK;

	if ((reg->addr = mmap(NULL, (reg->len + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1), PROT_READ | PROT_WRITE, MAP_DEVICE | MAP_UNCACHED | MAP_PHYSMEM | MAP_ANONYMOUS, -1, addr & ~(_PAGE_SIZE - 1))) == MAP_FAILED)
		return -ENOMEM;
	reg->addr = (void *)((uintptr_t)reg->addr + offs);

	return EOK;
}


void virtiopci_destroyDev(virtio_dev_t *vdev)
{
	virtiopci_unmapReg(&vdev->info.base);

	if (virtio_modern(vdev)) {
		virtiopci_unmapReg(&vdev->info.ntf);
		virtiopci_unmapReg(&vdev->info.isr);
		virtiopci_unmapReg(&vdev->info.cfg);
	}
}


int virtiopci_initDev(virtio_dev_t *vdev)
{
	int err;

	if ((err = virtiopci_mapReg(&vdev->info.base)) < 0)
		return err;

	if (virtio_modern(vdev)) {
		if ((err = virtiopci_mapReg(&vdev->info.ntf)) < 0) {
			virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 7));
			virtiopci_unmapReg(&vdev->info.base);
			return err;
		}

		if ((err = virtiopci_mapReg(&vdev->info.isr)) < 0) {
			virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 7));
			virtiopci_unmapReg(&vdev->info.base);
			virtiopci_unmapReg(&vdev->info.ntf);
			return err;
		}

		if ((err = virtiopci_mapReg(&vdev->info.cfg)) < 0) {
			virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 7));
			virtiopci_unmapReg(&vdev->info.base);
			virtiopci_unmapReg(&vdev->info.ntf);
			virtiopci_unmapReg(&vdev->info.isr);
			return err;
		}
	}

	virtio_reset(vdev);
	virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 0) | (1 << 1));
	vdev->features = virtio_getFeatures(vdev);

	return EOK;
}


virtiopci_cap_t *virtiopci_getCap(virtiopci_cap_t *caps, unsigned char type)
{
	virtiopci_cap_t *cap = caps;

	do {
		if ((cap->id == 0x09) && (cap->type == type))
			return cap;
	} while ((cap = (virtiopci_cap_t *)((uintptr_t)caps + cap->next)) != caps);

	return NULL;
}


int virtiopci_initReg(unsigned long base, unsigned long len, unsigned char flags, unsigned char ext, virtio_reg_t *reg)
{
	if (!base || !len)
		return -ENOENT;

	/* Check for IO space register */
	if (flags & 0x1) {
		reg->addr = (void *)(base | 0x1);
		reg->len = len;

		return EOK;
	}

	/* Check for 64-bit memory space register support */
	if ((flags & (1 << 2)) && (sizeof(void *) < 8) && (!ext || (base > base + len)))
		return -ENOTSUP;

	reg->addr = (void *)base;
	reg->len = len;

	return EOK;
}
