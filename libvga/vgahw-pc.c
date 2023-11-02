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
#include <stddef.h>

#include <sys/io.h>
#include <sys/mman.h>

#include "vga.h"


typedef struct {
	void *fcr;          /* Feature control register base address */
	void *misc;         /* Miscellaneous register base address */
	void *crtc;         /* CRT controller registers base address */
	void *seq;          /* Sequencer registers base address */
	void *gfx;          /* Graphics controller registers base address */
	void *attr;         /* Attribute controller registers base address */
	void *dac;          /* Digital to Analog Converter registers base address */
	void *mem;          /* Mapped VGA memory base address */
	unsigned int memsz; /* Mapped VGA memory size */
} vgahw_ctx_t;


void *vgahw_mem(void *hwctx)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	return ctx->mem;
}


unsigned char vgahw_status(void *hwctx)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	return inb(ctx->crtc + 6);
}


unsigned char vgahw_readfcr(void *hwctx)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	return inb(ctx->fcr);
}


void vgahw_writefcr(void *hwctx, unsigned char val)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->crtc + 6, val);
}


unsigned char vgahw_readmisc(void *hwctx)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	return inb(ctx->misc + 10);
}


void vgahw_writemisc(void *hwctx, unsigned char val)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->misc, val);
}


unsigned char vgahw_readcrtc(void *hwctx, unsigned char reg)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->crtc, reg);
	return inb(ctx->crtc + 1);
}


void vgahw_writecrtc(void *hwctx, unsigned char reg, unsigned char val)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->crtc, reg);
	outb(ctx->crtc + 1, val);
}


unsigned char vgahw_readseq(void *hwctx, unsigned char reg)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->seq, reg);
	return inb(ctx->seq + 1);
}


void vgahw_writeseq(void *hwctx, unsigned char reg, unsigned char val)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->seq, reg);
	outb(ctx->seq + 1, val);
}


unsigned char vgahw_readgfx(void *hwctx, unsigned char reg)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->gfx, reg);
	return inb(ctx->gfx + 1);
}


void vgahw_writegfx(void *hwctx, unsigned char reg, unsigned char val)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->gfx, reg);
	outb(ctx->gfx + 1, val);
}


unsigned char vgahw_readattr(void *hwctx, unsigned char reg)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	vgahw_status(hwctx);
	outb(ctx->attr, reg);
	return inb(ctx->attr + 1);
}


void vgahw_writeattr(void *hwctx, unsigned char reg, unsigned char val)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	vgahw_status(hwctx);
	outb(ctx->attr, reg);
	outb(ctx->attr, val);
}


unsigned char vgahw_readdac(void *hwctx, unsigned char reg)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	return inb(ctx->dac + reg);
}


void vgahw_writedac(void *hwctx, unsigned char reg, unsigned char val)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	outb(ctx->dac + reg, val);
}


void vgahw_enablecmap(void *hwctx)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	vgahw_status(hwctx);
	outb(ctx->attr, 0x00);
}


void vgahw_disablecmap(void *hwctx)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	vgahw_status(hwctx);
	outb(ctx->attr, 0x20);
}


void vgahw_done(void *hwctx)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	munmap(ctx->mem, ctx->memsz);
}


int vgahw_init(void *hwctx)
{
	vgahw_ctx_t *ctx = (vgahw_ctx_t *)hwctx;

	/* Map VGA memory (64KB) */
	ctx->memsz = (VGA_MEMSZ + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);
	if ((ctx->mem = mmap(NULL, ctx->memsz, PROT_READ | PROT_WRITE, MAP_DEVICE | MAP_SHARED | MAP_UNCACHED | MAP_ANONYMOUS | MAP_PHYSMEM, -1, 0xa0000)) == MAP_FAILED)
		return -ENOMEM;

	/* Set VGA ports */
	ctx->attr = (void *)0x3c0;
	ctx->misc = (void *)0x3c2;
	ctx->seq = (void *)0x3c4;
	ctx->dac = (void *)0x3c6;
	ctx->fcr = (void *)0x3ca;
	ctx->gfx = (void *)0x3ce;
	ctx->crtc = (vgahw_readmisc(hwctx) & 0x01) ? (void *)0x3d4 : (void *)0x3b4;

	return EOK;
}
