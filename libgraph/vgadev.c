/*
 * Phoenix-RTOS
 *
 * Generic VGA device driver based on XFree86 implementation
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/minmax.h>

#include <libvga.h>

#include "graph.h"


/* Graphics mode flags */
enum {
	HSYNCP    = (1 << 0),            /* HSYNC polarity */
	VSYNCP    = (1 << 1),            /* VSYNC polarity */
	DBLSCAN   = (1 << 2),            /* Double scan */
	CLKDIV    = (1 << 3),            /* Half the clock */
	INTERLACE = (1 << 4)             /* Interlace mode */
};


typedef struct {
	graph_mode_t mode;               /* Graphics mode */
	unsigned int depth;              /* Color depth */
	union {
		/* Power management mode */
		struct {
			unsigned char seq01;     /* Power management configuration */
			unsigned char crtc17;    /* Power management configuration */
		} pwm;
		/* Graphics mode */
		struct {
			graph_freq_t freq;       /* Screen refresh rate */
			unsigned int clock;      /* Pixel clock frequency */
			unsigned int hres;       /* Horizontal resolution */
			unsigned int hsyncs;     /* Horizontal sync start */
			unsigned int hsynce;     /* Horizontal sync end */
			unsigned int htotal;     /* Horizontal total pixels */
			unsigned int hskew;      /* Horizontal skew */
			unsigned int vres;       /* Vertical resolution */
			unsigned int vsyncs;     /* Vertical sync start */
			unsigned int vsynce;     /* Vertical sync end */
			unsigned int vtotal;     /* Vertical total lines */
			unsigned int vscan;      /* Vertical scan multiplier */
			unsigned int flags;      /* Mode flags */
		} gfx;
	};
} vgadev_mode_t;


typedef struct {
	vga_t vga;                       /* VGA data */
	vga_state_t state;               /* Saved state */
	unsigned char cmap[VGA_CMAPSZ];  /* Saved color map */
	unsigned char font1[VGA_FONTSZ]; /* Saved font1 */
	unsigned char font2[VGA_FONTSZ]; /* Saved font2 */
	unsigned char text[VGA_TEXTSZ];  /* Saved text */
} vgadev_t;


static const vgadev_mode_t modes[] = {
	/* Control modes */
	{ GRAPH_ON,        0, .pwm = { 0x00, 0x80 } },
	{ GRAPH_OFF,       0, .pwm = { 0x20, 0x00 } },
	/* 1-byte color */
	{ GRAPH_320x200x8, 1, .gfx = { GRAPH_60Hz, 25176, 320, 336, 384, 400, 0, 200, 206, 207, 224, 2, VSYNCP | CLKDIV } },
	/* No mode */
	{ GRAPH_NOMODE }
};


/* Schedules and executes new task */
extern int graph_schedule(graph_t *graph);


int vgadev_cursorpos(graph_t *graph, unsigned int x, unsigned int y)
{
	return EOK;
}


int vgadev_cursorset(graph_t *graph, const unsigned char *and, const unsigned char *xor, unsigned int bg, unsigned int fg)
{
	return EOK;
}


int vgadev_cursorhide(graph_t *graph)
{
	return EOK;
}


int vgadev_cursorshow(graph_t *graph)
{
	return EOK;
}


int vgadev_colorset(graph_t *graph, const unsigned char *colors, unsigned int first, unsigned int last)
{
	return -ENOTSUP;
}


int vgadev_colorget(graph_t *graph, unsigned char *colors, unsigned int first, unsigned int last)
{
	return -ENOTSUP;
}


int vgadev_isbusy(graph_t *graph)
{
	return 0;
}


int vgadev_commit(graph_t *graph)
{
	return EOK;
}


int vgadev_trigger(graph_t *graph)
{
	return graph_schedule(graph);
}


int vgadev_vsync(graph_t *graph)
{
	return 1;
}


int vgadev_mode(graph_t *graph, graph_mode_t mode, graph_freq_t freq)
{
	unsigned int i, tmp, hblanks, hblanke, vres, vsyncs, vsynce, vtotal, vblanks, vblanke;
	vga_state_t state = { .cmap = NULL, .font1 = NULL, .font2 = NULL, .text = NULL };
	vgadev_t *vgadev = (vgadev_t *)graph->adapter;
	vga_t *vga = &vgadev->vga;

	for (i = 0; (modes[i].mode != mode) || (modes[i].depth && (modes[i].gfx.freq != freq)); i++)
		if (modes[i].mode == GRAPH_NOMODE)
			return -ENOTSUP;

	/* Power management mode */
	if (!modes[i].depth) {
		vga_writeseq(vga, 0x00, 0x01);
		vga_writeseq(vga, 0x01, (vga_readseq(vga, 0x01) & ~0x20) | modes[i].pwm.seq01);
		vga_writecrtc(vga, 0x17, (vga_readcrtc(vga, 0x17) & ~0x80) | modes[i].pwm.crtc17);
		vga_writeseq(vga, 0x00, 0x03);
		return EOK;
	}

	/* Set HSYNC & VSYNC polarity */
	state.misc = 0x23;
	if (!(modes[i].gfx.flags & HSYNCP))
		state.misc |= 0x40;
	if (!(modes[i].gfx.flags & VSYNCP))
		state.misc |= 0x80;

	/* Sequencer */
	state.seq[0] = 0x00;
	state.seq[1] = 0x01;//(modes[i].gfx.flags & CLKDIV) ? 0x09 : 0x01;
	state.seq[2] = 0x0f;
	state.seq[3] = 0x00;
	state.seq[4] = 0x0e;

	/* CRT controller */
	vres = modes[i].gfx.vres;
	vsyncs = modes[i].gfx.vsyncs;
	vsynce = modes[i].gfx.vsynce;
	vtotal = modes[i].gfx.vtotal;

	if (modes[i].gfx.flags & INTERLACE) {
		vres >>= 1;
		vsyncs >>= 1;
		vsynce >>= 1;
		vtotal >>= 1;
	}

	if (modes[i].gfx.flags & DBLSCAN) {
		vres <<= 1;
		vsyncs <<= 1;
		vsynce <<= 1;
		vtotal <<= 1;
	}

	if (modes[i].gfx.vscan > 1) {
		vres *= modes[i].gfx.vscan;
		vsyncs *= modes[i].gfx.vscan;
		vsynce *= modes[i].gfx.vscan;
		vtotal *= modes[i].gfx.vscan;
	}

	vblanks = min(vsyncs, vres);
	vblanke = max(vsynce, vtotal);
	if (vblanks + 127 < vblanke)
		vblanks = vblanke - 127;

	hblanks = min(modes[i].gfx.hsyncs, modes[i].gfx.hres);
	hblanke = max(modes[i].gfx.hsynce, modes[i].gfx.htotal);
	if (hblanks + 63 * 8 < hblanke)
		hblanks = hblanke - 63 * 8;

	state.crtc[0] = 0x5f;//(modes[i].gfx.htotal >> 3) - 5;
	state.crtc[1] = 0x4f;//(modes[i].gfx.hres >> 3) - 1;
	state.crtc[2] = 0x50;//(hblanks >> 3) - 1;
	state.crtc[3] = 0x82;//(((hblanke >> 3) - 1) & 0x1f) | 0x80;
	if ((tmp = ((modes[i].gfx.hskew << 2) + 0x10) & ~0x1f) < 0x80)
		state.crtc[3] |= tmp;
	state.crtc[4] = 0x54;//(modes[i].gfx.hsyncs >> 3) - 1;
	state.crtc[5] = 0x80;//((((hblanke >> 3) - 1) & 0x20) << 2) | (((modes[i].gfx.hsynce >> 3) - 1) & 0x1f);
	state.crtc[6] = 0xBF;//(vtotal - 2) & 0xff;
	state.crtc[7] = 0x1F;//(((vtotal - 2) & 0x100) >> 8) | (((vres - 1) & 0x100) >> 7) | (((vsyncs - 1) & 0x100) >> 6) | (((vblanks - 1) & 0x100) >> 5);
	//state.crtc[7] |= (((vtotal - 2) & 0x200) >> 4) | (((vres - 1) & 0x200) >> 3) | (((vsyncs - 1) & 0x200) >> 2) | 0x10;
	state.crtc[8] = 0x00;
	state.crtc[9] = 0x41;//(((vblanks - 1) & 0x200) >> 4) | 0x40;
	if (modes[i].gfx.flags & DBLSCAN)
		state.crtc[9] |= 0x80;
	if (modes[i].gfx.vscan >= 32)
		state.crtc[9] |= 0x1f;
	else if (modes[i].gfx.vscan > 1)
		state.crtc[9] |= modes[i].gfx.vscan - 1;
	state.crtc[10] = 0x00;
	state.crtc[11] = 0x00;
	state.crtc[12] = 0x00;
	state.crtc[13] = 0x00;
	state.crtc[14] = 0x00;
	state.crtc[15] = 0x00;
	state.crtc[16] = 0x9C;//(vsyncs - 1) & 0xff;
	state.crtc[17] = 0x8E;//((vsynce - 1) & 0xff) | 0x20;
	state.crtc[18] = 0x8F;//(vres - 1) & 0xff;
	state.crtc[19] = 0x28;//((modes[i].gfx.hres + 0x0f) & ~0x0f) >> 3;
	state.crtc[20] = 0x40;//0x00;
	state.crtc[21] = 0x96;//(vblanks - 1) & 0xff;
	state.crtc[22] = 0xB9;//(vblanke - 1) & 0xff;
	state.crtc[23] = 0xA3;//0xc3;
	state.crtc[24] = 0xff;

	/* Graphics controller */
	state.gfx[0] = 0x00;
	state.gfx[1] = 0x00;
	state.gfx[2] = 0x00;
	state.gfx[3] = 0x00;
	state.gfx[4] = 0x00;
	state.gfx[5] = 0x40;
	state.gfx[6] = 0x05;
	state.gfx[7] = 0x0f;
	state.gfx[8] = 0xff;

	/* Attributes controller */
	state.attr[0] = 0x00;
	state.attr[1] = 0x01;
	state.attr[2] = 0x02;
	state.attr[3] = 0x03;
	state.attr[4] = 0x04;
	state.attr[5] = 0x05;
	state.attr[6] = 0x06;
	state.attr[7] = 0x07;
	state.attr[8] = 0x08;
	state.attr[9] = 0x09;
	state.attr[10] = 0x0a;
	state.attr[11] = 0x0b;
	state.attr[12] = 0x0c;
	state.attr[13] = 0x0d;
	state.attr[14] = 0x0e;
	state.attr[15] = 0x0f;
	state.attr[16] = 0x41;
	state.attr[17] = 0x00;//0xff;
	state.attr[18] = 0x0f;
	state.attr[19] = 0x00;
	state.attr[20] = 0x00;

	/* Program VGA registers */
	vga_mlock(vga);
	vga_restore(vga, &state);
	vga_munlock(vga);

	/* Clear screen and update graph data */
	memset(vga->mem, 0, VGA_MEMSZ);
	graph->depth = modes[i].depth;
	graph->width = modes[i].gfx.hres;
	graph->height = modes[i].gfx.vres;

	return EOK;
}


void vgadev_close(graph_t *graph)
{
	vgadev_t *vgadev = (vgadev_t *)graph->adapter;

	/* Restore original VGA state */
	vga_mlock(&vgadev->vga);
	vga_restore(&vgadev->vga, &vgadev->state);
	vga_munlock(&vgadev->vga);

	/* Lock VGA registers and destroy device handle */
	vga_lock(&vgadev->vga);
	vga_done(&vgadev->vga);
	free(vgadev);
}


int vgadev_open(graph_t *graph)
{
	vgadev_t *vgadev;
	int err;

	if ((vgadev = malloc(sizeof(vgadev_t))) == NULL)
		return -ENOMEM;

	if ((err = vga_init(&vgadev->vga)) < 0) {
		free(vgadev);
		return err;
	}

	/* Unlock VGA registers and save current state */
	vgadev->state.cmap = vgadev->cmap;
	vgadev->state.font1 = vgadev->font1;
	vgadev->state.font2 = vgadev->font2;
	vgadev->state.text = vgadev->text;
	vga_unlock(&vgadev->vga);
	vga_save(&vgadev->vga, &vgadev->state);

	/* Initialize graph info */
	graph->adapter = vgadev;
	graph->data = vgadev->vga.mem;
	graph->width = 0;
	graph->height = 0;
	graph->depth = 0;

	/* Set graph functions */
	graph->close = vgadev_close;
	graph->mode = vgadev_mode;
	graph->vsync = vgadev_vsync;
	graph->isbusy = vgadev_isbusy;
	graph->trigger = vgadev_trigger;
	graph->commit = vgadev_commit;
	graph->colorset = vgadev_colorset;
	graph->colorget = vgadev_colorget;
	graph->cursorset = vgadev_cursorset;
	graph->cursorpos = vgadev_cursorpos;
	graph->cursorshow = vgadev_cursorshow;
	graph->cursorhide = vgadev_cursorhide;

	return EOK;
}


void vgadev_done(void)
{
	return;
}


int vgadev_init(void)
{
	return EOK;
}
