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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <phoenix/arch/ia32.h>
#include <sys/io.h>
#include <sys/mman.h>

#include "libvirtio.h"


static uint8_t virtio_read8(virtio_dev_t *vdev, void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base;
	uint8_t val = 0;

	/* Read from IO-port */
	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

#ifdef TARGET_IA32
		val = inb((void *)addr);
#endif
	}
	/* Read from memory */
	else {
		addr &= ~0xf;
		addr += reg;

		val = *(volatile uint8_t *)addr;
	}

	return val;
}


static uint16_t virtio_read16(virtio_dev_t *vdev, void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base;
	uint16_t val = 0;

	/* Read from IO-port */
	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

#ifdef TARGET_IA32
		val = inw((void *)addr);
#endif
	}
	/* Read from memory */
	else {
		addr &= ~0xf;
		addr += reg;

		val = *(volatile uint16_t *)addr;
	}

	return virtio_vtog16(vdev, val);
}


static uint32_t virtio_read32(virtio_dev_t *vdev, void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base;
	uint32_t val = 0;

	/* Read from IO-port */
	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

#ifdef TARGET_IA32
		val = inl((void *)addr);
#endif
	}
	/* Read from memory */
	else {
		addr &= ~0xf;
		addr += reg;

		val = *(volatile uint32_t *)addr;
	}

	return virtio_vtog32(vdev, val);
}


static uint64_t virtio_read64(virtio_dev_t *vdev, void *base, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)base;
	uint64_t val = 0;

	/* Read from IO-port */
	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

#ifdef TARGET_IA32
		val = inl((void *)addr);
		val <<= 32;
		val |= inl((void *)(addr + 4));
#endif
	}
	/* Read from memory */
	else {
		addr &= ~0xf;
		addr += reg;

		val = *(volatile uint64_t *)addr;
	}

	return virtio_vtog64(vdev, val);
}


static void virtio_write8(virtio_dev_t *vdev, void *base, unsigned int reg, uint8_t val)
{
	uintptr_t addr = (uintptr_t)base;

	/* Write to IO-port */
	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

#ifdef TARGET_IA32
		outb((void *)addr, val);
#endif
	}
	/* Write to memory */
	else {
		addr &= ~0xf;
		addr += reg;

		*(volatile uint8_t *)addr = val;
	}
}


static void virtio_write16(virtio_dev_t *vdev, void *base, unsigned int reg, uint16_t val)
{
	uintptr_t addr = (uintptr_t)base;
	val = virtio_gtov16(vdev, val);

	/* Write to IO-port */
	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

#ifdef TARGET_IA32
		outw((void *)addr, val);
#endif
	}
	/* Write to memory */
	else {
		addr &= ~0xf;
		addr += reg;

		*(volatile uint16_t *)addr = val;
	}
}


static void virtio_write32(virtio_dev_t *vdev, void *base, unsigned int reg, uint32_t val)
{
	uintptr_t addr = (uintptr_t)base;
	val = virtio_gtov32(vdev, val);

	/* Write to IO-port */
	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

#ifdef TARGET_IA32
		outl((void *)addr, val);
#endif
	}
	/* Write to memory */
	else {
		addr &= ~0xf;
		addr += reg;

		*(volatile uint32_t *)addr = val;
	}
}


static void virtio_write64(virtio_dev_t *vdev, void *base, unsigned int reg, uint64_t val)
{
	uintptr_t addr = (uintptr_t)base;
	val = virtio_gtov64(vdev, val);

	/* Write to IO-port */
	if (addr & 0x1) {
		addr &= ~0x3;
		addr += reg;

#ifdef TARGET_IA32
		outl((void *)(addr + 4), val);
		outl((void *)addr, val >> 32);
#endif
	}
	/* Write memory */
	else {
		addr &= ~0xf;
		addr += reg;

		*(volatile uint64_t *)addr = val;
	}
}


virtio_cap_t *virtio_getCap(virtio_dev_t *vdev, unsigned int type)
{
	virtio_cap_t *cap = (virtio_cap_t *)vdev->pci.caps;

	if (virtio_legacy(vdev) || (vdev->type != VIRTIO_PCI) || (cap == NULL))
		return NULL;

	do {
		if ((cap->id == 0x9) || (cap->type == type))
			return cap;

		if (!cap->next)
			break;

		cap = (virtio_cap_t *)(vdev->pci.caps + cap->next);
	} while (1);

	return NULL;
}


/*void virtio_select(virtio_dev_t *vdev, virtqueue_t *vq)
{
	if (virtio_mmio(vdev))
		virtio_write32(vdev, 0x30, vq->idx);
	else
		virtio_write16(vdev, 0xe, vq->idx);
}*/


/* Get VirtIO device supported features */
static uint64_t virtio_getFeatures(virtio_dev_t *vdev)
{
	if (vdev->type == VIRTIO_PCI) {
		if (virtio_legacy(vdev))
			virtio_read32(vdev, 0);
	}
}


void virtio_setFeatures(virtio_dev_t *vdev, uint64_t features)
{
	if ()
}


uint8_t virtio_readStatus(virtio_dev_t *vdev)
{
	uint8_t status = 0;

	switch (vdev->type) {
	case VIRTIO_PCI:
		status = virtio_read8(vdev, vdev->pci.base, (virtio_legacy(vdev)) ? 0x12 : 0x14);
		break;

	case VIRTIO_MMIO:
		status = (uint8_t)virtio_read32(vdev, vdev->mmio.base, 0x70);
		break;
	}

	return status;
}


void virtio_writeStatus(virtio_dev_t *vdev, uint8_t status)
{
	switch (vdev->type) {
	case VIRTIO_PCI:
		virtio_write8(vdev, vdev->pci.base, (virtio_legacy(vdev)) ? 0x12 : 0x14, status);
		break;

	case VIRTIO_MMIO:
		virtio_write32(vdev, vdev->mmio.base, 0x70, status);
		break;
	}
}


void virtio_reset(virtio_dev_t *vdev)
{
	/* Writing zero means device reset */
	virtio_writeStatus(vdev, 0x0);

	if (vdev->type == VIRTIO_PCI) {
		/* Reading status flushes device writes */
		/* Modern VirtIO devices return zero on finished reset */
		if (virtio_legacy(vdev)) {
			virtio_readStatus(vdev);
		}
		else {
			while (virtio_readStatus(vdev))
				usleep(10);
		}
	}
}


void virtio_destroy(virtio_dev_t *vdev)
{
	unsigned int i;

	switch (vdev->type) {
	case VIRTIO_PCI:
		/* Unmap memory space BARs */
		for (i = 0; i < sizeof(vdev->pci.bar) / sizeof(vdev->pci.bar[0]); i++) {
			if ((vdev->pci.bar[i] != NULL) && !((uintptr_t)vdev->pci.bar[i] & 0x1))
				munmap(vdev->pci.bar[i], vdev->pci.len[i]);
		}
		free(vdev->pci.caps);
		break;

	case VIRTIO_MMIO:
		munmap(vdev->mmio.base, _PAGE_SIZE);
		break;
	}
}


#ifdef TARGET_IA32
/* Initializes VirtIO PCI device capability list */
static int virtio_initCaps(pci_device_t *pdev, virtio_dev_t *vdev)
{
	virtio_cap_t *cap = (virtio_cap_t *)vdev->pci.caps;
	uint32_t dev, *data = (uint32_t *)cap;
	uint8_t offs, len;

	/* Check if device uses capability list */
	if (!(pdev->status & 0x10))
		return -ENOENT;

	/* Get capability list head pointer */
	dev = 0x80000000 | ((uint32_t)pdev->b << 16) | ((uint32_t)pdev->d << 11) | ((uint32_t)pdev->f << 8);
	outl((void *)0xcf8, dev | (0xd << 2));
	offs = inl((void *)0xcfc) & 0xff;

	do {
		if ((offs < 64) || (offs % 4))
			return -EFAULT;

		/* Get capability header */
		offs /= 4;
		outl((void *)0xcf8, dev | (offs++ << 2));
		*data++ = inl((void *)0xcfc);

		/* Get capability length */
		if ((len = (cap->len >= 4) ? cap->len - 4 : 0) % 4)
			len = (len + 3) & ~3;

		/* Get capability data */
		while (len) {
			outl((void *)0xcf8, dev | (offs++ << 2));
			*data++ = inl((void *)0xcfc);
			len -= 4;
		}

		offs = cap->next;
		cap->next = ((uint8_t *)cap - vdev->pci.caps) + cap->len;
		cap = (virtio_cap_t *)((uint8_t *)data + cap->len);
		data = (uint32_t *)cap;
	} while (offs);

	return EOK;
}
#endif


int virtio_init(int type, void *dev, virtio_dev_t *vdev)
{
#ifdef TARGET_IA32
	pci_device_t *pdev;
	virtio_cap_t *cap;
	unsigned char flags;
	unsigned long base, len;
	int i, j, idx, err = EOK;
#endif

	memset(vdev, 0, sizeof(virtio_dev_t));
	vdev->type = type;

	switch (type) {
	case VIRTIO_PCI:
#ifdef TARGET_IA32
		pdev = (pci_device_t *)dev;

		/* Initialize BARs */
		for (i = 0; i < sizeof(vdev->pci.bar) / sizeof(vdev->pci.bar[0]); i++) {
			base = pdev->resources[i].base;
			len = pdev->resources[i].limit;
			flags = pdev->resources[i].flags;
			idx = i;

			/* Skip not used BARs */
			if (!base || !len) {
				vdev->pci.bar[idx] = NULL;
				vdev->pci.len[idx] = 0;
				continue;
			}

			/* Initialize IO space BARs */
			if (flags & 0x1) {
				vdev->pci.bar[idx] = (void *)(base | 0x1);
				vdev->pci.len[idx] = len; 
				continue;
			}

			do {
				/* Accept 64-bit memory space BARs if their upper 32 bits are not used and base + len doesn't overflow */
				if ((flags & 0x4) {
					if ((i >= 5) || pdev.resources[i + 1].base || (base > base + len)) {
						err = -ENOTSUP;
						break;
					}
					i++;
				}

				vdev->pci.len[idx] = (len + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);
				if ((vdev->pci.bar[idx] = mmap(NULL, , PROT_READ | PROT_WRITE, MAP_DEVICE | MAP_UNCACHED, OID_PHYSMEM, base)) == MAP_FAILED) {
					err = -ENOMEM;
					break;
				}
			} while (0);

			if (err) {
				for (j = 0; j < idx; j++) {
					if ((vdev->pci.bar[j] != NULL) && !((uintptr_t)vdev->pci.bar[j] & 0x1))
						munmap(vdev->pci.bar[j], vdev->pci.len[j]);
				}
				return err;
			}
		}

		/* Non-zero PCI ABI revision indicates modern VirtIO device */
		if (pdev->revision) {
			vdev->features |= 0x100000000ULL;

			do {
				if ((vdev->pci.caps = malloc(192)) == NULL) {
					err = -ENOMEM;
					break;
				}

				/* Initialize device PCI capability list */
				if ((err = virtio_initCaps(pdev, vdev)) < 0)
					break;

				/* Get common configuration address */
				if (((cap = virtio_getCap(vdev, 0x1)) == NULL) || (vdev->bar[cap->bar] == NULL) || (cap->size < 0x38)) {
					err = -ENXIO;
					break;
				}
				vdev->pci.base = (void *)((uintprt_t)vdev->bar[cap->bar] + cap->offs);

				/* Get interrupt status address */
				if (((cap = virtio_getCap(vdev, 0x3)) == NULL) || (vdev->bar[cap->bar] == NULL) || (cap->size < 0x1)) {
					err = -ENXIO;
					break;
				}
				vdev->pci.isr = (void *)((uintprt_t)vdev->bar[cap->bar] + cap->offs);

				/* Get notification address */
				if (((cap = virtio_getCap(vdev, 0x2)) == NULL) || (cap->size < 0x14)) {
					err = -ENXIO;
					break;
				}
				vdev->pci.notify = (void *)cap;
			} while (0);

			virtio_destroy(vdev);
			return err;
		}

		vdev->features = 0;
		vdev->pci.caps = NULL;
#endif
		break;

	case VIRTIO_MMIO:
		mmio_device_t *mdev = (mmio_device_t *)dev;
	}

	
}
