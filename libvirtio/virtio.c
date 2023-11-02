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
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

#include "virtio.h"
#include "virtiolow.h"


typedef union {
	/* Direct MMIO device detection status */
	unsigned char found;

	/* MMIO device detection progress */
	/* TODO: track MMIO device enumeration */
} virtiommio_ctx_t;


/* Returns VirtIO device configuraton space address */
static void *virtio_configBase(virtio_dev_t *vdev)
{
	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev))
			return vdev->info.base.addr;

		return vdev->info.cfg.addr;
	}

	return vdev->info.base.addr;
}


/* Returns VirtIO device configuration space registers offset */
static unsigned int virtio_configReg(virtio_dev_t *vdev, unsigned int reg)
{
	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev))
			return reg + 0x14;

		return reg;
	}

	return reg + 0x100;
}


/* Returns modern VirtIO device configuration generation */
static unsigned int virtio_configGen(virtio_dev_t *vdev)
{
	if (vdev->info.type == vdevPCI)
		return virtio_read8(vdev, vdev->info.base.addr, 0x15);

	return virtio_read32(vdev, vdev->info.base.addr, 0xfc);
}


uint8_t virtio_readConfig8(virtio_dev_t *vdev, unsigned int reg)
{
	return virtio_read8(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg));
}


uint16_t virtio_readConfig16(virtio_dev_t *vdev, unsigned int reg)
{
	return virtio_read16(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg));
}


uint32_t virtio_readConfig32(virtio_dev_t *vdev, unsigned int reg)
{
	return virtio_read32(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg));
}


uint64_t virtio_readConfig64(virtio_dev_t *vdev, unsigned int reg)
{
	unsigned int gen1, gen2;
	uint64_t val;

	if (virtio_legacy(vdev)) {
		do {
			val = virtio_read64(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg));
		} while (val != virtio_read64(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg)));

		return val;
	}

	do {
		gen1 = virtio_configGen(vdev);
		val = virtio_read64(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg));
		gen2 = virtio_configGen(vdev);
	} while (gen1 != gen2);

	return val;
}


void virtio_writeConfig8(virtio_dev_t *vdev, unsigned int reg, uint8_t val)
{
	virtio_write8(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg), val);
}


void virtio_writeConfig16(virtio_dev_t *vdev, unsigned int reg, uint16_t val)
{
	virtio_write16(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg), val);
}


void virtio_writeConfig32(virtio_dev_t *vdev, unsigned int reg, uint32_t val)
{
	virtio_write32(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg), val);
}


void virtio_writeConfig64(virtio_dev_t *vdev, unsigned int reg, uint64_t val)
{
	virtio_write64(vdev, virtio_configBase(vdev), virtio_configReg(vdev, reg), val);
}


uint64_t virtio_getFeatures(virtio_dev_t *vdev)
{
	uint64_t features;

	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev))
			return virtio_read32(vdev, vdev->info.base.addr, 0x00);

		virtio_write32(vdev, vdev->info.base.addr, 0x00, 1);
		virtio_mb();
		features = virtio_read32(vdev, vdev->info.base.addr, 0x04);
		features <<= 32;
		virtio_write32(vdev, vdev->info.base.addr, 0x00, 0);
		virtio_mb();
		features |= virtio_read32(vdev, vdev->info.base.addr, 0x04);

		return features;
	}

	virtio_write32(vdev, vdev->info.base.addr, 0x14, 1);
	virtio_mb();
	features = virtio_read32(vdev, vdev->info.base.addr, 0x10);
	features <<= 32;
	virtio_write32(vdev, vdev->info.base.addr, 0x14, 0);
	virtio_mb();
	features |= virtio_read32(vdev, vdev->info.base.addr, 0x10);

	return features;
}


/* Writes VirtIO driver supported features */
static void virtio_setFeatures(virtio_dev_t *vdev, uint64_t features)
{
	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev)) {
			virtio_write32(vdev, vdev->info.base.addr, 0x04, features);
			return;
		}

		virtio_write32(vdev, vdev->info.base.addr, 0x08, 1);
		virtio_mb();
		virtio_write32(vdev, vdev->info.base.addr, 0x0c, features >> 32);
		virtio_write32(vdev, vdev->info.base.addr, 0x08, 0);
		virtio_mb();
		virtio_write32(vdev, vdev->info.base.addr, 0x0c, features);
		return;
	}

	virtio_write32(vdev, vdev->info.base.addr, 0x24, 1);
	virtio_mb();
	virtio_write32(vdev, vdev->info.base.addr, 0x20, features >> 32);
	virtio_write32(vdev, vdev->info.base.addr, 0x24, 0);
	virtio_mb();
	virtio_write32(vdev, vdev->info.base.addr, 0x20, features);
}


uint64_t virtio_readFeatures(virtio_dev_t *vdev)
{
	return vdev->features;
}


int virtio_writeFeatures(virtio_dev_t *vdev, uint64_t features)
{
	vdev->features &= features | (1ULL << 32);
	virtio_setFeatures(vdev, vdev->features);

	if (virtio_legacy(vdev))
		return EOK;

	virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 3));

	if (!(virtio_readStatus(vdev) & (1 << 3)))
		return -ENOTSUP;

	return EOK;
}


uint8_t virtio_readStatus(virtio_dev_t *vdev)
{
	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev))
			return virtio_read8(vdev, vdev->info.base.addr, 0x12);

		return virtio_read8(vdev, vdev->info.base.addr, 0x14);
	}

	return virtio_read32(vdev, vdev->info.base.addr, 0x70);
}


void virtio_writeStatus(virtio_dev_t *vdev, uint8_t status)
{
	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev)) {
			virtio_write8(vdev, vdev->info.base.addr, 0x12, status);
			virtio_mb();
			return;
		}

		virtio_write8(vdev, vdev->info.base.addr, 0x14, status);
		virtio_mb();
		return;
	}

	virtio_write32(vdev, vdev->info.base.addr, 0x70, status);
	virtio_mb();
}


unsigned int virtio_isr(virtio_dev_t *vdev)
{
	unsigned int isr;

	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev))
			return virtio_read8(vdev, vdev->info.base.addr, 0x13);

		return virtio_read8(vdev, vdev->info.isr.addr, 0x00);
	}

	isr = virtio_read32(vdev, vdev->info.base.addr, 0x60);
	virtio_write32(vdev, vdev->info.base.addr, 0x64, isr);

	return isr;
}


void virtio_reset(virtio_dev_t *vdev)
{
	virtio_writeStatus(vdev, 0);

	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev)) {
			virtio_readStatus(vdev);
			return;
		}

		while (virtio_readStatus(vdev))
			usleep(10);
	}
}


void virtio_destroyDev(virtio_dev_t *vdev)
{
	if (vdev->info.type == vdevPCI) {
		virtiopci_destroyDev(vdev);
		return;
	}

	munmap(vdev->info.base.addr, (vdev->info.base.len + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1));
}


int virtio_initDev(virtio_dev_t *vdev)
{
	unsigned int ver, id;
	int err;

	if (vdev->info.type == vdevPCI)
		return virtiopci_initDev(vdev);

	if ((vdev->info.base.addr = mmap(NULL, (vdev->info.base.len + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1), PROT_READ | PROT_WRITE, MAP_DEVICE | MAP_UNCACHED | MAP_PHYSMEM | MAP_ANONYMOUS, -1, (uintptr_t)vdev->info.base.addr)) == MAP_FAILED)
		return -ENOMEM;

	vdev->features = 0ULL;
	if (virtio_read32(vdev, vdev->info.base.addr, 0x00) != 0x74726976) {
		vdev->features = (1ULL << 32);

		if (virtio_read32(vdev, vdev->info.base.addr, 0x00) != 0x74726976)
			return -ENODEV;
	}

	do {
		if ((ver = virtio_read32(vdev, vdev->info.base.addr, 0x04)) == 1) {
			vdev->features = 0;
		}
		else if (ver == 2) {
			vdev->features = (1ULL << 32);
		}
		else {
			err = -ENOTSUP;
			break;
		}

		if (!(id = virtio_read32(vdev, vdev->info.base.addr, 0x08)) || (vdev->info.id && (vdev->info.id != id))) {
			err = -ENODEV;
			break;
		}
		vdev->info.id = id;

		virtio_reset(vdev);
		virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 0) | (1 << 1));
		vdev->features = virtio_getFeatures(vdev);

		if (virtio_legacy(vdev)) {
			if (ver == 2) {
				err = -EFAULT;
				break;
			}
			virtio_write32(vdev, vdev->info.base.addr, 0x28, _PAGE_SIZE);
		}

		return EOK;
	} while (0);

	virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 7));
	virtio_destroyDev(vdev);

	return err;
}


int virtio_find(const virtio_devinfo_t *info, virtio_dev_t *vdev, virtio_ctx_t *vctx)
{
	virtiommio_ctx_t *ctx = (virtiommio_ctx_t *)vctx->ctx;

	if (info->type == vdevPCI)
		return virtiopci_find(info, vdev, vctx);

	if (vctx->reset)
		memset(vctx, 0, sizeof(virtio_ctx_t));
	vdev->info = *info;

	/* Direct MMIO configuration */
	if (info->base.len) {
		if (ctx->found)
			return -ENODEV;

		ctx->found = 1;
		return EOK;
	}

	/* TODO: enumerate MMIO devices */
	return -ENODEV;
}


void virtio_done(void)
{
	return;
}


int virtio_init(void)
{
	return EOK;
}
