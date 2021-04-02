/*
 * Phoenix-RTOS
 *
 * VGA library internal interface based on XFree86 implementation
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


void vga_lock(vga_t *vga)
{
	vga_writecrtc(vga, 0x11, vga_readcrtc(vga, 0x11) | 0x80);
}


void vga_unlock(vga_t *vga)
{
	vga_writecrtc(vga, 0x11, vga_readcrtc(vga, 0x11) & ~0x80);
}


void vga_mlock(vga_t *vga)
{
	/* Disable display and sequencer */
	vga_writeseq(vga, 0x01, vga_readseq(vga, 0x01) | 0x20);
	vga_writeseq(vga, 0x00, 0x01);
	vga_enablecmap(vga);
}


void vga_munlock(vga_t *vga)
{
	/* Enable sequencer and display */
	vga_writeseq(vga, 0x00, 0x03);
	vga_writeseq(vga, 0x01, vga_readseq(vga, 0x01) & ~0x20);
	vga_disablecmap(vga);
}


void vga_blank(vga_t *vga)
{
	unsigned char seq01 = vga_readseq(vga, 0x01);

	vga_writeseq(vga, 0x00, 0x01);
	vga_writeseq(vga, 0x01, seq01 | 0x20);
	vga_writeseq(vga, 0x00, 0x03);
}


void vga_unblank(vga_t *vga)
{
	unsigned char seq01 = vga_readseq(vga, 0x01);

	vga_writeseq(vga, 0x00, 0x01);
	vga_writeseq(vga, 0x01, seq01 & ~0x20);
	vga_writeseq(vga, 0x00, 0x03);
}


void vga_savemode(vga_t *vga, vga_state_t *state)
{
	unsigned int i;

	state->misc = vga_readmisc(vga);

	for (i = 0; i < sizeof(state->crtc); i++)
		state->crtc[i] = vga_readcrtc(vga, i);

	for (i = 1; i < sizeof(state->seq); i++)
		state->seq[i] = vga_readseq(vga, i);

	for (i = 0; i < sizeof(state->gfx); i++)
		state->gfx[i] = vga_readgfx(vga, i);

	vga_enablecmap(vga);
	for (i = 0; i < sizeof(state->attr); i++)
		state->attr[i] = vga_readattr(vga, i);
	vga_disablecmap(vga);
}


void vga_restoremode(vga_t *vga, vga_state_t *state)
{
	unsigned int i;

	vga_writemisc(vga, state->misc);

	/* Unlock restored CRTC[0-7] registers */
	vga_writecrtc(vga, 0x11, state->crtc[0x11] & ~0x80);
	for (i = 0; i < sizeof(state->crtc); i++)
		vga_writecrtc(vga, i, state->crtc[i]);

	for (i = 1; i < sizeof(state->seq); i++)
		vga_writeseq(vga, i, state->seq[i]);

	for (i = 0; i < sizeof(state->gfx); i++)
		vga_writegfx(vga, i, state->gfx[i]);

	vga_enablecmap(vga);
	for (i = 0; i < sizeof(state->attr); i++)
		vga_writeattr(vga, i, state->attr[i]);
	vga_disablecmap(vga);
}


void vga_savecmap(vga_t *vga, vga_state_t *state)
{
	unsigned int i;

	if (state->cmap == NULL)
		return;

	/* Assume DAC is readable */
	vga_writedac(vga, 0x00, 0xff);
	vga_writedac(vga, 0x01, 0x00);

	for (i = 0; i < VGA_CMAPSZ; i++) {
		state->cmap[i] = vga_readdac(vga, 0x03);

		/* DAC delay */
		vga_status(vga);
		vga_status(vga);
	}

	vga_disablecmap(vga);
}


void vga_restorecmap(vga_t *vga, vga_state_t *state)
{
	unsigned int i;

	if (state->cmap == NULL)
		return;

	/* Assume DAC is writable */
	vga_writedac(vga, 0x00, 0xff);
	vga_writedac(vga, 0x02, 0x00);

	for (i = 0; i < VGA_CMAPSZ; i++) {
		vga_writedac(vga, 0x03, state->cmap[i]);

		/* DAC delay */
		vga_status(vga);
		vga_status(vga);
	}

	vga_disablecmap(vga);
}


/* Copies VGA fonts and text */
static void vga_copytext(vga_t *vga, vga_state_t *state, unsigned char dir)
{
	unsigned char misc, gr01, gr03, gr04, gr05, gr06, gr08, seq02, seq04;

	/* Save registers */
	misc = vga_readmisc(vga);
	gr01 = vga_readgfx(vga, 0x01);
	gr03 = vga_readgfx(vga, 0x03);
	gr04 = vga_readgfx(vga, 0x04);
	gr05 = vga_readgfx(vga, 0x05);
	gr06 = vga_readgfx(vga, 0x06);
	gr08 = vga_readgfx(vga, 0x08);
	seq02 = vga_readseq(vga, 0x02);
	seq04 = vga_readseq(vga, 0x04);

	/* Force into color mode */
	vga_writemisc(vga, misc | 0x01);
	vga_blank(vga);

	vga_writeseq(vga, 0x04, 0x06); /* Enable plane graphics */
	vga_writegfx(vga, 0x01, 0x00); /* All planes come from CPU */
	vga_writegfx(vga, 0x03, 0x00); /* Don't rotate, write unmodified */
	vga_writegfx(vga, 0x05, 0x00); /* Write mode 0, read mode 0 */
	vga_writegfx(vga, 0x06, 0x05); /* Set graphics */
	vga_writegfx(vga, 0x08, 0xff); /* Write all bits in a byte */

	if (state->font1 != NULL) {
		/* Read/Write plane 2 */
		vga_writeseq(vga, 0x02, 0x04);
		vga_writegfx(vga, 0x04, 0x02);
		if (dir)
			memcpy(vga->mem, state->font1, VGA_FONTSZ);
		else
			memcpy(state->font1, vga->mem, VGA_FONTSZ);
	}

	if (state->font2 != NULL) {
		/* Read/Write plane 3 */
		vga_writeseq(vga, 0x02, 0x08);
		vga_writegfx(vga, 0x04, 0x03);
		if (dir)
			memcpy(vga->mem, state->font2, VGA_FONTSZ);
		else
			memcpy(state->font2, vga->mem, VGA_FONTSZ);
	}

	if (state->text != NULL) {
		/* Read/Write plane 0 */
		vga_writeseq(vga, 0x02, 0x01);
		vga_writegfx(vga, 0x04, 0x00);
		if (dir)
			memcpy(vga->mem, state->text, VGA_TEXTSZ >> 1);
		else
			memcpy(state->text, vga->mem, VGA_TEXTSZ >> 1);

		/* Read/Write plane 1 */
		vga_writeseq(vga, 0x02, 0x02);
		vga_writegfx(vga, 0x04, 0x01);
		if (dir)
			memcpy(vga->mem, state->text + (VGA_TEXTSZ >> 1), VGA_TEXTSZ >> 1);
		else
			memcpy(state->text + (VGA_TEXTSZ >> 1), vga->mem, VGA_TEXTSZ >> 1);
	}

	/* Restore registers */
	vga_writeseq(vga, 0x04, seq04);
	vga_writeseq(vga, 0x02, seq02);
	vga_writegfx(vga, 0x08, gr08);
	vga_writegfx(vga, 0x06, gr06);
	vga_writegfx(vga, 0x05, gr05);
	vga_writegfx(vga, 0x04, gr04);
	vga_writegfx(vga, 0x03, gr03);
	vga_writegfx(vga, 0x01, gr01);

	/* Restore mode */
	vga_writemisc(vga, misc);
	vga_unblank(vga);
}


void vga_savetext(vga_t *vga, vga_state_t *state)
{
	/* No fonts and text in graphics mode */
	if (vga_readattr(vga, 0x10) & 0x01)
		return;

	vga_copytext(vga, state, 0);
}


void vga_restoretext(vga_t *vga, vga_state_t *state)
{
	vga_copytext(vga, state, 1);
}


void vga_save(vga_t *vga, vga_state_t *state)
{
	vga_savetext(vga, state);
	vga_savecmap(vga, state);
	vga_savemode(vga, state);
}


void vga_restore(vga_t *vga, vga_state_t *state)
{
	vga_restoremode(vga, state);
	vga_restorecmap(vga, state);
	vga_restoretext(vga, state);
}
