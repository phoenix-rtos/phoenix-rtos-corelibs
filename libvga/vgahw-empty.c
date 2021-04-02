/*
 * Phoenix-RTOS
 *
 * VGA low level interface (empty)
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>

#include "vga.h"


unsigned char vga_status(vga_t *vga)
{
	return 0;
}


unsigned char vga_readmisc(vga_t *vga)
{
	return 0;
}


void vga_writemisc(vga_t *vga, unsigned char val)
{
	return;
}


unsigned char vga_readcrtc(vga_t *vga, unsigned char reg)
{
	return 0;
}


void vga_writecrtc(vga_t *vga, unsigned char reg, unsigned char val)
{
	return;
}


unsigned char vga_readseq(vga_t *vga, unsigned char reg)
{
	return 0;
}


void vga_writeseq(vga_t *vga, unsigned char reg, unsigned char val)
{
	return;
}


unsigned char vga_readgfx(vga_t *vga, unsigned char reg)
{
	return 0;
}


void vga_writegfx(vga_t *vga, unsigned char reg, unsigned char val)
{
	return;
}


unsigned char vga_readattr(vga_t *vga, unsigned char reg)
{
	return 0;
}


void vga_writeattr(vga_t *vga, unsigned char reg, unsigned char val)
{
	return;
}


unsigned char vga_readdac(vga_t *vga, unsigned char reg)
{
	return 0;
}


void vga_writedac(vga_t *vga, unsigned char reg, unsigned char val)
{
	return;
}


void vga_disablecmap(vga_t *vga)
{
	return;
}


void vga_enablecmap(vga_t *vga)
{
	return;
}


void vga_done(vga_t *vga)
{
	return;
}


int vga_init(vga_t *vga)
{
	return -ENODEV;
}
