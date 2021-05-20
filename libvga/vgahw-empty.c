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
#include <stddef.h>

#include "vga.h"


void *vgahw_mem(void *hwctx)
{
	return NULL;
}


unsigned char vgahw_status(void *hwctx)
{
	return 0;
}


unsigned char vgahw_readfcr(void *hwctx)
{
	return 0;
}


void vgahw_writefcr(void *hwctx, unsigned char val)
{
	return;
}


unsigned char vgahw_readmisc(void *hwctx)
{
	return 0;
}


void vgahw_writemisc(void *hwctx, unsigned char val)
{
	return;
}


unsigned char vgahw_readcrtc(void *hwctx, unsigned char reg)
{
	return 0;
}


void vgahw_writecrtc(void *hwctx, unsigned char reg, unsigned char val)
{
	return;
}


unsigned char vgahw_readseq(void *hwctx, unsigned char reg)
{
	return 0;
}


void vgahw_writeseq(void *hwctx, unsigned char reg, unsigned char val)
{
	return;
}


unsigned char vgahw_readgfx(void *hwctx, unsigned char reg)
{
	return 0;
}


void vgahw_writegfx(void *hwctx, unsigned char reg, unsigned char val)
{
	return;
}


unsigned char vgahw_readattr(void *hwctx, unsigned char reg)
{
	return 0;
}


void vgahw_writeattr(void *hwctx, unsigned char reg, unsigned char val)
{
	return;
}


unsigned char vgahw_readdac(void *hwctx, unsigned char reg)
{
	return 0;
}


void vgahw_writedac(void *hwctx, unsigned char reg, unsigned char val)
{
	return;
}


void vgahw_disablecmap(void *hwctx)
{
	return;
}


void vgahw_enablecmap(void *hwctx)
{
	return;
}


void vgahw_done(void *hwctx)
{
	return;
}


int vgahw_init(void *hwctx)
{
	return -ENODEV;
}
