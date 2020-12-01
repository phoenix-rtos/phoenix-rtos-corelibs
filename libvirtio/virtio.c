/*
 * Phoenix-RTOS
 *
 * VirtIO core interface
 *
 * Copyright 2020 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <phoenix/arch/ia32.h>
#include <sys/mman.h>

#include "virtio.h"


uint8_t virtio_readConfig8(virtio_dev_t *vdev, unsigned int reg)
{
	uint8_t val;

	do {
		val = virtio_read8(vdev, vdev->cfg, reg);
	} while (virtio_legacy(vdev) && (val != virtio_read8(vdev, vdev->cfg, reg)));

	return val;
}


uint16_t virtio_readConfig16(virtio_dev_t *vdev, unsigned int reg)
{
	uint16_t val;

	do {
		val = virtio_read16(vdev, vdev->cfg, reg);
	} while (virtio_legacy(vdev) && (val != virtio_read16(vdev, vdev->cfg, reg)));

	return val;
}


uint32_t virtio_readConfig32(virtio_dev_t *vdev, unsigned int reg)
{
	uint32_t val, tmp;

	val = virtio_read32(vdev, vdev->cfg, reg);

	if (virtio_legacy(vdev)) {
		while (val != (tmp = virtio_read32(vdev, vdev->cfg, reg)))
			val = tmp;
	}

	return val;
}


uint64_t virtio_readConfig64(virtio_dev_t *vdev, unsigned int reg)
{
	uint32_t gen1, gen2;
	uint64_t val;

	if (virtio_legacy(vdev)) {
		do {
			val = virtio_read64(vdev, vdev->cfg, reg);
		} while (val != virtio_read64(vdev, vdev->cfg, reg));

		return val;
	}

	do {
		gen1 = virtio_read32(vdev, vdev->base, 0xfc);
		val = virtio_read64(vdev, vdev->cfg, reg);
		gen2 = virtio_read32(vdev, vdev->base, 0xfc);
	} while (gen1 != gen2);

	return val;
}


void virtio_writeConfig8(virtio_dev_t *vdev, unsigned int reg, uint8_t val)
{
	virtio_write8(vdev, vdev->cfg, reg, val);
}


void virtio_writeConfig16(virtio_dev_t *vdev, unsigned int reg, uint16_t val)
{
	virtio_write16(vdev, vdev->cfg, reg, val);
}


void virtio_writeConfig32(virtio_dev_t *vdev, unsigned int reg, uint32_t val)
{
	virtio_write32(vdev, vdev->cfg, reg, val);
}


void virtio_writeConfig64(virtio_dev_t *vdev, unsigned int reg, uint64_t val)
{
	virtio_write64(vdev, vdev->cfg, reg, val);
}


/* Reads VirtIO device supported features */
uint64_t virtio_getFeatures(virtio_dev_t *vdev)
{
	uint64_t features;

	switch (vdev->type) {
	case VIRTIO_PCI:
		if (virtio_legacy(vdev)) {
			features = virtio_read32(vdev, vdev->base, 0x0);
		}
		else {
			virtio_write32(vdev, vdev->base, 0x0, 1);
			features = virtio_read32(vdev, vdev->base, 0x4);
			features <<= 32;
			virtio_write32(vdev, vdev->base, 0x0, 0);
			features |= virtio_read32(vdev, vdev->base, 0x4);
		}
		break;

	case VIRTIO_MMIO:
		virtio_write32(vdev, vdev->base, 0x14, 1);
		features = virtio_read32(vdev, vdev->base, 0x10);
		features <<= 32;
		virtio_write32(vdev, vdev->base, 0x14, 0);
		features |= virtio_read32(vdev, vdev->base, 0x10);
		break;
	}

	return features;
}


/* Writes VirtIO driver supported features */
void virtio_setFeatures(virtio_dev_t *vdev, uint64_t features)
{
	switch (vdev->type) {
	case VIRTIO_PCI:
		if (virtio_legacy(vdev)) {
			virtio_write32(vdev, vdev->base, 0x4, (uint32_t)features);
		}
		else {
			virtio_write32(vdev, vdev->base, 0x8, 1);
			virtio_write32(vdev, vdev->base, 0xc, (uint32_t)(features >> 32));
			virtio_write32(vdev, vdev->base, 0x8, 0);
			virtio_write32(vdev, vdev->base, 0xc, (uint32_t)features);
		}
		break;

	case VIRTIO_MMIO:
		virtio_write32(vdev, vdev->base, 0x24, 1);
		virtio_write32(vdev, vdev->base, 0x20, (uint32_t)(features >> 32));
		virtio_write32(vdev, vdev->base, 0x24, 0);
		virtio_write32(vdev, vdev->base, 0x20, (uint32_t)features);
		break;
	}
}


uint64_t virtio_readFeatures(virtio_dev_t *vdev)
{
	return vdev->features;
}


int virtio_writeFeatures(virtio_dev_t *vdev, uint64_t features)
{
	/* Limit features to what device and driver support. Preserve transport features */
	vdev->features &= features | 0x3ff0000000ULL;
	virtio_writeFeatures(vdev, vdev->features);

	if (virtio_legacy(vdev))
		return EOK;

	/* Set finished features negotiation status bit */
	virtio_writeStatus(vdev, virtio_readStatus(vdev) | 0x8);

	/* Check if device supports selected features */
	if (!(virtio_readStatus(vdev) & 0x8))
		return -ENOTSUP;

	return EOK;
}


uint8_t virtio_readStatus(virtio_dev_t *vdev)
{
	uint8_t status;

	switch (vdev->type) {
	case VIRTIO_PCI:
		status = virtio_read8(vdev, vdev->base, (virtio_legacy(vdev)) ? 0x12 : 0x14);
		break;

	case VIRTIO_MMIO:
		status = (uint8_t)virtio_read32(vdev, vdev->base, 0x70);
		break;
	}

	return status;
}


void virtio_writeStatus(virtio_dev_t *vdev, uint8_t status)
{
	switch (vdev->type) {
	case VIRTIO_PCI:
		virtio_write8(vdev, vdev->base, (virtio_legacy(vdev)) ? 0x12 : 0x14, status);
		break;

	case VIRTIO_MMIO:
		virtio_write32(vdev, vdev->base, 0x70, status);
		break;
	}
}


unsigned int virtio_isr(virtio_dev_t *vdev)
{
	unsigned int isr;

	switch (vdev->type) {
	case VIRTIO_PCI:
		isr = virtio_read8(vdev, vdev->isr, 0x0);

	case VIRTIO_MMIO:
		if ((isr = virtio_read32(vdev, vdev->isr, 0x0)))
			virtio_write32(vdev, vdev->base, 0x64, isr);
	}

	return isr;
}


void virtio_reset(virtio_dev_t *vdev)
{
	/* Writing zero means device reset */
	virtio_writeStatus(vdev, 0x00);

	if (vdev->type == VIRTIO_PCI) {
		/* Reading status flushes device writes */
		/* Modern VirtIO PCI devices return zero on finished reset */
		if (virtio_legacy(vdev)) {
			virtio_readStatus(vdev);
			return;
		}

		while (virtio_readStatus(vdev))
			usleep(10);
	}
}


void virtio_destroy(virtio_dev_t *vdev)
{
	if (vdev->type == vdevPCI) {
		if (!((uintptr_t)vdev->base & 0x1))
			munmap((void *)((uintprt_t)vdev->base & ~(_PAGE_SIZE - 1)), _PAGE_SIZE);

		if (virtio_legacy(vdev))
			return;

		if (!((uintptr_t)vdev->ntf & 0x1))
			munmap((void *)((uintprt_t)vdev->ntf & ~(_PAGE_SIZE - 1)), _PAGE_SIZE);

		if (!((uintptr_t)vdev->isr & 0x1))
			munmap((void *)((uintprt_t)vdev->isr & ~(_PAGE_SIZE - 1)), _PAGE_SIZE);

		if (!((uintptr_t)vdev->cfg & 0x1))
			munmap((void *)((uintprt_t)vdev->base & ~(_PAGE_SIZE - 1)), _PAGE_SIZE);

		return;
	}

	munmap(vdev->base, _PAGE_SIZE);
}
