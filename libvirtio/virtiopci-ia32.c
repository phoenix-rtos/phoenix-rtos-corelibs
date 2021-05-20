/*
 * Phoenix-RTOS
 *
 * VirtIO PCI low level interface (IA32)
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

#include <phoenix/arch/ia32.h>

#include <sys/io.h>
#include <sys/platform.h>

#include "virtio.h"
#include "virtiolow.h"


typedef union {
	/* Direct PCI device detection status */
	unsigned char found;

	/* PCI device detection progress */
	struct {
		unsigned char bus;  /* PCI bus index */
		unsigned char dev;  /* PCI device index */
		unsigned char func; /* PCI function index */
	};
} virtiopci_ctx_t;


uint8_t virtiopci_read8(void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base;

	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

		return inb((void *)addr);
	}

	return *(volatile uint8_t *)(addr + reg);
}


uint16_t virtiopci_read16(void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base;

	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

		return inw((void *)addr);
	}

	return *(volatile uint16_t *)(addr + reg);
}


uint32_t virtiopci_read32(void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base;

	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

		return inl((void *)addr);
	}

	return *(volatile uint32_t *)(addr + reg);
}


uint64_t virtiopci_read64(void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base;
	uint64_t val;

	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

		val = inl((void *)(addr + 4));
		val <<= 32;
		val |= inl((void *)addr);

		return val;
	}

	val = *(volatile uint32_t *)(addr + reg + 4);
	val <<= 32;
	val |= *(volatile uint32_t *)(addr + reg);

	return val;
}


void virtiopci_write8(void *base, unsigned int reg, uint8_t val)
{
	uintptr_t addr = (uintptr_t)base;

	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

		outb((void *)addr, val);
		return;
	}

	*(volatile uint8_t *)(addr + reg) = val;
}


void virtiopci_write16(void *base, unsigned int reg, uint16_t val)
{
	uintptr_t addr = (uintptr_t)base;

	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

		outw((void *)addr, val);
		return;
	}

	*(volatile uint16_t *)(addr + reg) = val;
}


void virtiopci_write32(void *base, unsigned int reg, uint32_t val)
{
	uintptr_t addr = (uintptr_t)base;

	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

		outl((void *)addr, val);
		return;
	}

	*(volatile uint32_t *)(addr + reg) = val;
}


void virtiopci_write64(void *base, unsigned int reg, uint64_t val)
{
	uintptr_t addr = (uintptr_t)base;

	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

		outl((void *)(addr + 4), val >> 32);
		outl((void *)addr, val);
		return;
	}

	*(volatile uint32_t *)(addr + reg + 4) = val >> 32;
	*(volatile uint32_t *)(addr + reg) = val;
}


int virtiopci_find(const virtio_devinfo_t *info, virtio_dev_t *vdev, virtio_ctx_t *vctx)
{
	platformctl_t pctl = { .action = pctl_get, .type = pctl_pci };
	virtiopci_ctx_t *ctx = (virtiopci_ctx_t *)vctx->ctx;
	unsigned char caps[192];
	virtiopci_cap_t *cap;
	int err;

	if (vctx->reset)
		memset(vctx, 0, sizeof(virtio_ctx_t));
	vdev->info = *info;

	/* Direct PCI configuration */
	if (info->base.len) {
		if (ctx->found)
			return -ENODEV;

		ctx->found = 1;
		return EOK;
	}

	/* Enumerate PCI bus */
	pctl.pci.id.vendor = 0x1af4;
	pctl.pci.id.device = info->id;
	pctl.pci.id.subvendor = PCI_ANY;
	pctl.pci.id.subdevice = PCI_ANY;
	pctl.pci.id.cl = PCI_ANY;
	pctl.pci.dev.bus = ctx->bus;
	pctl.pci.dev.dev = ctx->dev;
	pctl.pci.dev.func = ctx->func;
	pctl.pci.caps = caps;

	do {
		if ((err = platformctl(&pctl)) < 0)
			break;

		vdev->features = (pctl.pci.dev.revision && (pctl.pci.dev.status & (1 << 4))) ? (1ULL << 32) : 0ULL;
		vdev->info.irq = pctl.pci.dev.irq;

		if (virtio_legacy(vdev)) {
			if ((err = virtiopci_initReg(pctl.pci.dev.resources[0].base, pctl.pci.dev.resources[0].limit, pctl.pci.dev.resources[0].flags, !pctl.pci.dev.resources[1].base, &vdev->info.base)) < 0)
				break;

			err = EOK;
			break;
		}

		if (((cap = virtiopci_getCap((virtiopci_cap_t *)caps, 0x01)) == NULL) || (cap->size < 0x38) || (cap->bar > 5)) {
			err = -ENOENT;
			break;
		}

		if ((err = virtiopci_initReg(pctl.pci.dev.resources[cap->bar].base, pctl.pci.dev.resources[cap->bar].limit, pctl.pci.dev.resources[cap->bar].flags, (cap->bar < 5) && !pctl.pci.dev.resources[cap->bar + 1].base, &vdev->info.base)) < 0)
			break;
		vdev->info.base.addr = (void *)((uintptr_t)vdev->info.base.addr + cap->offs);

		if (((cap = virtiopci_getCap((virtiopci_cap_t *)caps, 0x02)) == NULL) || (cap->size < 0x14) || (cap->bar > 5)) {
			err = -ENOENT;
			break;
		}

		if ((err = virtiopci_initReg(pctl.pci.dev.resources[cap->bar].base, pctl.pci.dev.resources[cap->bar].limit, pctl.pci.dev.resources[cap->bar].flags, (cap->bar < 5) && !pctl.pci.dev.resources[cap->bar + 1].base, &vdev->info.ntf)) < 0)
			break;
		vdev->info.ntf.addr = (void *)((uintptr_t)vdev->info.ntf.addr + cap->offs);
		vdev->info.xntf = *(uint32_t *)((uintptr_t)cap + sizeof(virtiopci_cap_t));

		if (((cap = virtiopci_getCap((virtiopci_cap_t *)caps, 0x03)) == NULL) || (cap->size < 0x1) || (cap->bar > 5)) {
			err = -ENOENT;
			break;
		}

		if ((err = virtiopci_initReg(pctl.pci.dev.resources[cap->bar].base, pctl.pci.dev.resources[cap->bar].limit, pctl.pci.dev.resources[cap->bar].flags, (cap->bar < 5) && !pctl.pci.dev.resources[cap->bar + 1].base, &vdev->info.isr)) < 0)
			break;
		vdev->info.isr.addr = (void *)((uintptr_t)vdev->info.isr.addr + cap->offs);

		if (((cap = virtiopci_getCap((virtiopci_cap_t *)caps, 0x04)) == NULL) || (cap->bar > 5)) {
			err = -ENOENT;
			break;
		}

		if ((err = virtiopci_initReg(pctl.pci.dev.resources[cap->bar].base, pctl.pci.dev.resources[cap->bar].limit, pctl.pci.dev.resources[cap->bar].flags, (cap->bar < 5) && !pctl.pci.dev.resources[cap->bar + 1].base, &vdev->info.cfg)) < 0)
			break;
		vdev->info.cfg.addr = (void *)((uintptr_t)vdev->info.cfg.addr + cap->offs);

		err = EOK;
	} while (0);

	ctx->bus = pctl.pci.dev.bus;
	ctx->dev = pctl.pci.dev.dev;
	ctx->func = pctl.pci.dev.func + 1;

	return err;
}
