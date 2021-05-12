/*
 * Phoenix-RTOS
 *
 * VGA low level interface (IA32)
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdlib.h>

#include <sys/io.h>
#include <sys/mman.h>

#include "libvga.h"


unsigned char vga_status(vga_t *vga)
{
	return inb(vga->crtc + 6);
}


unsigned char vga_readmisc(vga_t *vga)
{
	return inb(vga->misc + 10);
}


void vga_writemisc(vga_t *vga, unsigned char val)
{
	outb(vga->misc, val);
}


unsigned char vga_readcrtc(vga_t *vga, unsigned char reg)
{
	outb(vga->crtc, reg);
	return inb(vga->crtc + 1);
}


void vga_writecrtc(vga_t *vga, unsigned char reg, unsigned char val)
{
	outb(vga->crtc, reg);
	outb(vga->crtc + 1, val);
}


unsigned char vga_readseq(vga_t *vga, unsigned char reg)
{
	outb(vga->seq, reg);
	return inb(vga->seq + 1);
}


void vga_writeseq(vga_t *vga, unsigned char reg, unsigned char val)
{
	outb(vga->seq, reg);
	outb(vga->seq + 1, val);
}


unsigned char vga_readgfx(vga_t *vga, unsigned char reg)
{
	outb(vga->gfx, reg);
	return inb(vga->gfx + 1);
}


void vga_writegfx(vga_t *vga, unsigned char reg, unsigned char val)
{
	outb(vga->gfx, reg);
	outb(vga->gfx + 1, val);
}


unsigned char vga_readattr(vga_t *vga, unsigned char reg)
{
	vga_status(vga);
	outb(vga->attr, reg);
	return inb(vga->attr + 1);
}


void vga_writeattr(vga_t *vga, unsigned char reg, unsigned char val)
{
	vga_status(vga);
	outb(vga->attr, reg);
	outb(vga->attr, val);
}


unsigned char vga_readdac(vga_t *vga, unsigned char reg)
{
	return inb(vga->dac + reg);
}


void vga_writedac(vga_t *vga, unsigned char reg, unsigned char val)
{
	outb(vga->dac + reg, val);
}


void vga_disablecmap(vga_t *vga)
{
	vga_status(vga);
	outb(vga->attr, 0x20);
}


void vga_enablecmap(vga_t *vga)
{
	vga_status(vga);
	outb(vga->attr, 0x00);
}


void vga_done(vga_t *vga)
{
	munmap(vga->mem, vga->memsz);
}


int vga_init(vga_t *vga)
{
	/* Set VGA ports */
	vga->attr = (void *)0x3c0;
	vga->misc = (void *)0x3c2;
	vga->seq = (void *)0x3c4;
	vga->dac = (void *)0x3c6;
	vga->gfx = (void *)0x3ce;
	vga->crtc = (vga_readmisc(vga) & 0x01) ? (void *)0x3d4 : (void *)0x3b4;

	/* Map VGA memory (64KB) */
	vga->memsz = (VGA_MEMSZ + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);
	if ((vga->mem = mmap(NULL, vga->memsz, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_UNCACHED | MAP_DEVICE, OID_PHYSMEM, 0xa0000)) == MAP_FAILED)
		return -ENOMEM;

	return EOK;
}
