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

#include <sys/io.h>

#include "virtiopci.h"


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
	uint64_t val;

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
