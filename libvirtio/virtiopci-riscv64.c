/*
 * Phoenix-RTOS
 *
 * VirtIO PCI low level interface (RISCV64)
 *
 * Copyright 2020 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "virtiopci.h"


uint8_t virtiopci_read8(void *base, unsigned int reg)
{
	return 0;
}


uint16_t virtiopci_read16(void *base, unsigned int reg)
{
	return 0;
}


uint32_t virtiopci_read32(void *base, unsigned int reg)
{
	return 0;
}


uint64_t virtiopci_read64(void *base, unsigned int reg)
{
	return 0;
}


void virtiopci_write8(void *base, unsigned int reg, uint8_t val)
{
	return;
}


void virtiopci_write16(void *base, unsigned int reg, uint16_t val)
{
	return;
}


void virtiopci_write32(void *base, unsigned int reg, uint32_t val)
{
	return;
}


void virtiopci_write64(void *base, unsigned int reg, uint64_t val)
{
	return;
}
