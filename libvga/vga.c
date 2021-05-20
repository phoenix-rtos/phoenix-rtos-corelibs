/*
 * Phoenix-RTOS
 *
 * VGA high level interface based on XFree86 implementation
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 * Copyright 1991-1999 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions, and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution, and in the same place and form as other copyright,
 *     license and disclaimer information.
 *
 * 3.  The end-user documentation included with the redistribution,
 *     if any, must include the following acknowledgment: "This product
 *     includes software developed by The XFree86 Project, Inc
 *     (http://www.xfree86.org/) and its contributors", in the same
 *     place and form as other third-party acknowledgments.  Alternately,
 *     this acknowledgment may appear in the software itself, in the
 *     same form and location as other such third-party acknowledgments.
 *
 * 4.  Except as contained in this notice, the name of The XFree86
 *     Project, Inc shall not be used in advertising or otherwise to
 *     promote the sale, use or other dealings in this Software without
 *     prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>

#include "vga.h"


void vga_lock(void *hwctx)
{
	vgahw_writecrtc(hwctx, 0x11, vgahw_readcrtc(hwctx, 0x11) | 0x80);
}


void vga_unlock(void *hwctx)
{
	vgahw_writecrtc(hwctx, 0x11, vgahw_readcrtc(hwctx, 0x11) & ~0x80);
}


void vga_mlock(void *hwctx)
{
	/* Disable display and sequencer */
	vgahw_writeseq(hwctx, 0x01, vgahw_readseq(hwctx, 0x01) | 0x20);
	vgahw_writeseq(hwctx, 0x00, 0x01);
	vgahw_enablecmap(hwctx);
}


void vga_munlock(void *hwctx)
{
	/* Enable sequencer and display */
	vgahw_writeseq(hwctx, 0x00, 0x03);
	vgahw_writeseq(hwctx, 0x01, vgahw_readseq(hwctx, 0x01) & ~0x20);
	vgahw_disablecmap(hwctx);
}


void vga_blank(void *hwctx)
{
	unsigned char sr01 = vgahw_readseq(hwctx, 0x01);

	vgahw_writeseq(hwctx, 0x00, 0x01);
	vgahw_writeseq(hwctx, 0x01, sr01 | 0x20);
	vgahw_writeseq(hwctx, 0x00, 0x03);
}


void vga_unblank(void *hwctx)
{
	unsigned char sr01 = vgahw_readseq(hwctx, 0x01);

	vgahw_writeseq(hwctx, 0x00, 0x01);
	vgahw_writeseq(hwctx, 0x01, sr01 & ~0x20);
	vgahw_writeseq(hwctx, 0x00, 0x03);
}


void vga_savemode(void *hwctx, vga_state_t *state)
{
	unsigned int i;

	state->mr = vgahw_readmisc(hwctx);

	for (i = 0; i < sizeof(state->cr); i++)
		state->cr[i] = vgahw_readcrtc(hwctx, i);

	for (i = 1; i < sizeof(state->sr); i++)
		state->sr[i] = vgahw_readseq(hwctx, i);

	for (i = 0; i < sizeof(state->gr); i++)
		state->gr[i] = vgahw_readgfx(hwctx, i);

	vgahw_enablecmap(hwctx);
	for (i = 0; i < sizeof(state->ar); i++)
		state->ar[i] = vgahw_readattr(hwctx, i);
	vgahw_disablecmap(hwctx);
}


void vga_restoremode(void *hwctx, vga_state_t *state)
{
	unsigned int i;

	vgahw_writemisc(hwctx, state->mr);

	/* Unlock restored CRTC[0-7] registers */
	vgahw_writecrtc(hwctx, 0x11, state->cr[0x11] & ~0x80);
	for (i = 0; i < sizeof(state->cr); i++)
		vgahw_writecrtc(hwctx, i, state->cr[i]);

	for (i = 1; i < sizeof(state->sr); i++)
		vgahw_writeseq(hwctx, i, state->sr[i]);

	for (i = 0; i < sizeof(state->gr); i++)
		vgahw_writegfx(hwctx, i, state->gr[i]);

	vgahw_enablecmap(hwctx);
	for (i = 0; i < sizeof(state->ar); i++)
		vgahw_writeattr(hwctx, i, state->ar[i]);
	vgahw_disablecmap(hwctx);
}


void vga_savecmap(void *hwctx, vga_state_t *state)
{
	unsigned int i;

	if (state->cmap == NULL)
		return;

	/* Assume DAC is readable */
	vgahw_writedac(hwctx, 0x00, 0xff);
	vgahw_writedac(hwctx, 0x01, 0x00);

	for (i = 0; i < VGA_CMAPSZ; i++) {
		vgahw_status(hwctx);
		state->cmap[i] = vgahw_readdac(hwctx, 0x03);
	}

	vgahw_disablecmap(hwctx);
}


void vga_restorecmap(void *hwctx, vga_state_t *state)
{
	unsigned int i;

	if (state->cmap == NULL)
		return;

	/* Assume DAC is writable */
	vgahw_writedac(hwctx, 0x00, 0xff);
	vgahw_writedac(hwctx, 0x02, 0x00);

	for (i = 0; i < VGA_CMAPSZ; i++) {
		vgahw_status(hwctx);
		vgahw_writedac(hwctx, 0x03, state->cmap[i]);
	}

	vgahw_disablecmap(hwctx);
}


/* Copies VGA fonts and text */
static void vga_copytext(void *hwctx, vga_state_t *state, unsigned char dir)
{
	unsigned char mr, gr01, gr03, gr04, gr05, gr06, gr08, sr02, sr04;

	/* Save registers */
	mr = vgahw_readmisc(hwctx);
	gr01 = vgahw_readgfx(hwctx, 0x01);
	gr03 = vgahw_readgfx(hwctx, 0x03);
	gr04 = vgahw_readgfx(hwctx, 0x04);
	gr05 = vgahw_readgfx(hwctx, 0x05);
	gr06 = vgahw_readgfx(hwctx, 0x06);
	gr08 = vgahw_readgfx(hwctx, 0x08);
	sr02 = vgahw_readseq(hwctx, 0x02);
	sr04 = vgahw_readseq(hwctx, 0x04);

	/* Force into color mode */
	vgahw_writemisc(hwctx, mr | 0x01);
	vga_blank(hwctx);

	vgahw_writeseq(hwctx, 0x04, 0x06); /* Enable plane graphics */
	vgahw_writegfx(hwctx, 0x01, 0x00); /* All planes come from CPU */
	vgahw_writegfx(hwctx, 0x03, 0x00); /* Don't rotate, write unmodified */
	vgahw_writegfx(hwctx, 0x05, 0x00); /* Write mode 0, read mode 0 */
	vgahw_writegfx(hwctx, 0x06, 0x05); /* Set graphics */
	vgahw_writegfx(hwctx, 0x08, 0xff); /* Write all bits in a byte */

	if (state->font1 != NULL) {
		/* Read/Write plane 2 */
		vgahw_writeseq(hwctx, 0x02, 0x04);
		vgahw_writegfx(hwctx, 0x04, 0x02);
		if (dir)
			memcpy(vgahw_mem(hwctx), state->font1, VGA_FONTSZ);
		else
			memcpy(state->font1, vgahw_mem(hwctx), VGA_FONTSZ);
	}

	if (state->font2 != NULL) {
		/* Read/Write plane 3 */
		vgahw_writeseq(hwctx, 0x02, 0x08);
		vgahw_writegfx(hwctx, 0x04, 0x03);
		if (dir)
			memcpy(vgahw_mem(hwctx), state->font2, VGA_FONTSZ);
		else
			memcpy(state->font2, vgahw_mem(hwctx), VGA_FONTSZ);
	}

	if (state->text != NULL) {
		/* Read/Write plane 0 */
		vgahw_writeseq(hwctx, 0x02, 0x01);
		vgahw_writegfx(hwctx, 0x04, 0x00);
		if (dir)
			memcpy(vgahw_mem(hwctx), state->text, VGA_TEXTSZ >> 1);
		else
			memcpy(state->text, vgahw_mem(hwctx), VGA_TEXTSZ >> 1);

		/* Read/Write plane 1 */
		vgahw_writeseq(hwctx, 0x02, 0x02);
		vgahw_writegfx(hwctx, 0x04, 0x01);
		if (dir)
			memcpy(vgahw_mem(hwctx), state->text + (VGA_TEXTSZ >> 1), VGA_TEXTSZ >> 1);
		else
			memcpy(state->text + (VGA_TEXTSZ >> 1), vgahw_mem(hwctx), VGA_TEXTSZ >> 1);
	}

	/* Restore registers */
	vgahw_writeseq(hwctx, 0x04, sr04);
	vgahw_writeseq(hwctx, 0x02, sr02);
	vgahw_writegfx(hwctx, 0x08, gr08);
	vgahw_writegfx(hwctx, 0x06, gr06);
	vgahw_writegfx(hwctx, 0x05, gr05);
	vgahw_writegfx(hwctx, 0x04, gr04);
	vgahw_writegfx(hwctx, 0x03, gr03);
	vgahw_writegfx(hwctx, 0x01, gr01);

	/* Restore mode */
	vgahw_writemisc(hwctx, mr);
	vga_unblank(hwctx);
}


void vga_savetext(void *hwctx, vga_state_t *state)
{
	/* No fonts and text in graphics mode */
	if (vgahw_readattr(hwctx, 0x10) & 0x01)
		return;

	vga_copytext(hwctx, state, 0);
}


void vga_restoretext(void *hwctx, vga_state_t *state)
{
	vga_copytext(hwctx, state, 1);
}


void vga_save(void *hwctx, vga_state_t *state)
{
	vga_savetext(hwctx, state);
	vga_savecmap(hwctx, state);
	vga_savemode(hwctx, state);
}


void vga_restore(void *hwctx, vga_state_t *state)
{
	vga_restoremode(hwctx, state);
	vga_restorecmap(hwctx, state);
	vga_restoretext(hwctx, state);
}


void vga_initstate(vga_cfg_t *cfg, vga_state_t *state)
{
	unsigned int i, vres = cfg->vres, vsyncs = cfg->vsyncs, vsynce = cfg->vsynce, vtotal = cfg->vtotal;

	/* Adjust vertical timings */
	if (cfg->flags & VGA_DBLSCAN) {
		vres <<= 1;
		vsyncs <<= 1;
		vsynce <<= 1;
		vtotal <<= 1;
	}

	if (cfg->flags & VGA_INTERLACE) {
		vres >>= 1;
		vsyncs >>= 1;
		vsynce >>= 1;
		vtotal >>= 1;
	}

	/* Miscellaneous register */
	state->mr = ((cfg->clkidx & 0x03) << 2) | 0x23;
	if (!(cfg->flags & VGA_HSYNCP))
		state->mr |= 0x40;
	if (!(cfg->flags & VGA_VSYNCP))
		state->mr |= 0x80;

	/* Sequencer registers */
	state->sr[0] = 0x00;
	state->sr[1] = (cfg->flags & VGA_CLKDIV) ? 0x09 : 0x01;
	state->sr[2] = 0x0f;
	state->sr[3] = 0x00;
	state->sr[4] = 0x0e;

	/* CRT controller registers */
	state->cr[0] = (cfg->htotal >> 3) - 5;
	state->cr[1] = (cfg->hres >> 3) - 1;
	state->cr[2] = (cfg->hsyncs >> 3) - 1;
	state->cr[3] = (((cfg->hsynce >> 3) - 1) & 0x1f) | 0x80;
	state->cr[4] = (cfg->hsyncs >> 3) - 1;
	state->cr[5] = ((((cfg->hsynce >> 3) - 1) & 0x20) << 2) | (((cfg->hsynce >> 3) - 1) & 0x1f);
	state->cr[6] = (vtotal - 2) & 0xff;
	state->cr[7] =
		(((vtotal - 2) & 0x100) >> 8) | (((vres - 1) & 0x100) >> 7) | (((vsyncs - 1) & 0x100) >> 6) | (((vsyncs - 1) & 0x100) >> 5) |
		(((vtotal - 2) & 0x200) >> 4) | (((vres - 1) & 0x200) >> 3) | (((vsyncs - 1) & 0x200) >> 2) | 0x10;
	state->cr[8] = 0x00;
	state->cr[9] = (((vsyncs - 1) & 0x200) >> 4) | 0x40;
	if (cfg->flags & VGA_DBLSCAN)
		state->cr[9] |= 0x80;
	state->cr[10] = 0x00;
	state->cr[11] = 0x00;
	state->cr[12] = 0x00;
	state->cr[13] = 0x00;
	state->cr[14] = 0x00;
	state->cr[15] = 0x00;
	state->cr[16] = (vsyncs - 1) & 0xff;
	state->cr[17] = ((vsynce - 1) & 0x0f) | 0x20;
	state->cr[18] = (vres - 1) & 0xff;
	state->cr[19] = cfg->hres >> 4;
	state->cr[20] = 0x00;
	state->cr[21] = (vsyncs - 1) & 0xff;
	state->cr[22] = (vsynce - 1) & 0xff;
	state->cr[23] = 0xc3;
	state->cr[24] = 0xff;

	/* Graphics controller registers */
	state->gr[0] = 0x00;
	state->gr[1] = 0x00;
	state->gr[2] = 0x00;
	state->gr[3] = 0x00;
	state->gr[4] = 0x00;
	state->gr[5] = 0x40;
	state->gr[6] = 0x05;
	state->gr[7] = 0x0f;
	state->gr[8] = 0xff;

	/* Attributes controller registers */
	for (i = 0; i < 16; i++)
		state->ar[i] = i;
	state->ar[16] = 0x41;
	state->ar[17] = 0x00;
	state->ar[18] = 0x0f;
	state->ar[19] = 0x00;
	state->ar[20] = 0x00;
}
