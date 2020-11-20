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

#include <sys/io.h>

#include "libvirtio.h"


static uint8_t virtio_read8(virtio_dev_t *vdev, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)vdev->base;
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


static uint16_t virtio_read16(virtio_dev_t *vdev, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)vdev->base;
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


static uint32_t virtio_read32(virtio_dev_t *vdev, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)vdev->base;
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


static uint64_t virtio_read64(virtio_dev_t *vdev, unsigned int reg)
{
	uintptr_t addr = (uintptr_t)vdev->base;
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


static void virtio_write8(virtio_dev_t *vdev, unsigned int reg, uint8_t val)
{
	uintptr_t addr = (uintptr_t)vdev->base;

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


static void virtio_write16(virtio_dev_t *vdev, unsigned int reg, uint16_t val)
{
	uintptr_t addr = (uintptr_t)vdev->base;
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


static void virtio_write32(virtio_dev_t *vdev, unsigned int reg, uint32_t val)
{
	uintptr_t addr = (uintptr_t)vdev->base;
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


static void virtio_write64(virtio_dev_t *vdev, unsigned int reg, uint64_t val)
{
	uintptr_t addr = (uintptr_t)vdev->base;
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


/*void virtio_select(virtio_dev_t *vdev, virtqueue_t *vq)
{
	if (virtio_mmio(vdev))
		virtio_write32(vdev, 0x30, vq->idx);
	else
		virtio_write16(vdev, 0xe, vq->idx);
}*/
