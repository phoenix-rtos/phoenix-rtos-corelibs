/*
 * Phoenix-RTOS
 *
 * Generic VGA driver based on XFree86 implementation
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

#include <sys/threads.h>

#include <vga.h>

#include "graph.h"


/* Default graphics mode index */
#define DEFMODE 4 /* 320x200x8 @ 70Hz */


typedef struct {
	graph_mode_t mode;   /* Graphics mode */
	unsigned char depth; /* Color depth */
	union {
		/* Power management mode */
		struct {
			unsigned char sr01; /* DPMS sr01 register configuration */
			unsigned char cr17; /* DPMS gr0e register configuration */
		} pwm;
		/* Graphics mode */
		struct {
			graph_freq_t freq;    /* Screen refresh rate */
			const vga_cfg_t *cfg; /* Mode configuration */
		} gfx;
	};
} vga_mode_t;


typedef struct {
	unsigned char ctx[VGA_CTXSZ];    /* VGA context */
	unsigned char cmap[VGA_CMAPSZ];  /* Saved color map */
	unsigned char text[VGA_TEXTSZ];  /* Saved text */
	unsigned char font1[VGA_FONTSZ]; /* Saved font1 */
	unsigned char font2[VGA_FONTSZ]; /* Saved font2 */
	vga_state_t state;               /* Saved video state */
} vga_dev_t;

/* clang-format off */
/* Graphics modes configuration table */
static const vga_cfg_t cfgs[] = {
	{ 0, 12588, 320, 328, 376, 400, 200, 206, 207, 225, VGA_VSYNCP | VGA_CLKDIV | VGA_DBLSCAN }, /* 320x200 @ 70Hz */
	/* No configuration */
	{ 0 }
};


/* Graphics modes table */
static const vga_mode_t modes[] = {
	/* Power management modes */
	{ GRAPH_ON,        0, .pwm = { 0x00, 0x80 } },       /* 0, Screen: on,  HSync: on,  VSync: on */
	{ GRAPH_OFF,       0, .pwm = { 0x20, 0x00 } },       /* 1, Screen: off, HSync: off, VSync: off */
	{ GRAPH_STANDBY,   0, .pwm = { 0x20, 0x80 } },       /* 2, Screen: off, HSync: off, VSync: on */
	{ GRAPH_SUSPEND,   0, .pwm = { 0x20, 0x80 } },       /* 3, Screen: off, HSync: on,  VSync: off */
	/* 8-bit color palette */
	{ GRAPH_320x200x8, 1, .gfx = { GRAPH_70Hz, cfgs } }, /* 4 */
	/* No mode */
	{ 0 }
};
/* clang-format on */

/* Schedules and executes tasks */
extern int graph_schedule(graph_t *graph);


int vga_vsync(graph_t *graph)
{
	vga_dev_t *vga = (vga_dev_t *)graph->adapter;
	void *ctx = vga->ctx;

	/* Wait for current vertical retrace end */
	while (vgahw_status(ctx) & 0x08)
		;
	/* Wait for next vertical retrace start */
	while (!(vgahw_status(ctx) & 0x08))
		;

	return 1;
}


int vga_isbusy(graph_t *graph)
{
	return 0;
}


int vga_trigger(graph_t *graph)
{
	if (vga_isbusy(graph))
		return -EBUSY;

	return graph_schedule(graph);
}


int vga_commit(graph_t *graph)
{
	return EOK;
}


int vga_colorset(graph_t *graph, const unsigned char *colors, unsigned char first, unsigned char last)
{
	vga_dev_t *vga = (vga_dev_t *)graph->adapter;
	void *ctx = vga->ctx;
	unsigned int i;

	if (graph->depth != 1)
		return -ENOTSUP;

	vgahw_writedac(ctx, 0x00, 0xff);
	vgahw_writedac(ctx, 0x02, first);

	for (i = first; i <= last; i++) {
		vgahw_status(ctx);
		vgahw_writedac(ctx, 0x03, *colors++ >> 2);
		vgahw_status(ctx);
		vgahw_writedac(ctx, 0x03, *colors++ >> 2);
		vgahw_status(ctx);
		vgahw_writedac(ctx, 0x03, *colors++ >> 2);
	}

	return EOK;
}


int vga_colorget(graph_t *graph, unsigned char *colors, unsigned char first, unsigned char last)
{
	vga_dev_t *vga = (vga_dev_t *)graph->adapter;
	void *ctx = vga->ctx;
	unsigned int i;

	if (graph->depth != 1)
		return -ENOTSUP;

	vgahw_writedac(ctx, 0x00, 0xff);
	vgahw_writedac(ctx, 0x01, first);

	for (i = first; i <= last; i++) {
		vgahw_status(ctx);
		*colors++ = vgahw_readdac(ctx, 0x03) << 2;
		vgahw_status(ctx);
		*colors++ = vgahw_readdac(ctx, 0x03) << 2;
		vgahw_status(ctx);
		*colors++ = vgahw_readdac(ctx, 0x03) << 2;
	}

	return EOK;
}


int vga_cursorset(graph_t *graph, const unsigned char *amask, const unsigned char *xmask, unsigned int bg, unsigned int fg)
{
	return -ENOTSUP;
}


int vga_cursorpos(graph_t *graph, unsigned int x, unsigned int y)
{
	return -ENOTSUP;
}


int vga_cursorhide(graph_t *graph)
{
	return -ENOTSUP;
}


int vga_cursorshow(graph_t *graph)
{
	return -ENOTSUP;
}


int vga_mode(graph_t *graph, graph_mode_t mode, graph_freq_t freq)
{
	unsigned int i = DEFMODE;
	vga_dev_t *vga = (vga_dev_t *)graph->adapter;
	void *ctx = vga->ctx;
	vga_state_t state;
	vga_cfg_t cfg;

	if (mode != GRAPH_DEFMODE) {
		for (i = 0; (modes[i].mode != mode) || (modes[i].depth && (freq != GRAPH_DEFFREQ) && (modes[i].gfx.freq != freq)); i++) {
			if (!modes[i].mode)
				return -ENOTSUP;
		}
	}

	/* Power management mode (DPMS) */
	if (!modes[i].depth) {
		vgahw_writeseq(ctx, 0x00, 0x01);
		vgahw_writeseq(ctx, 0x01, (vgahw_readseq(ctx, 0x01) & ~0x20) | modes[i].pwm.sr01);
		vgahw_writecrtc(ctx, 0x17, (vgahw_readcrtc(ctx, 0x17) & ~0x80) | modes[i].pwm.cr17);
		vgahw_writeseq(ctx, 0x00, 0x03);
		return EOK;
	}
	cfg = *modes[i].gfx.cfg;

	/* Adjust timings */
	if (cfg.flags & VGA_CLKDIV) {
		cfg.clk <<= 1;
		cfg.hres <<= 1;
		cfg.hsyncs <<= 1;
		cfg.hsynce <<= 1;
		cfg.htotal <<= 1;
		cfg.flags &= ~VGA_CLKDIV;
	}

	/* Initialize VGA state */
	vga_initstate(&cfg, &state);
	state.cmap = NULL;
	state.text = NULL;
	state.font1 = NULL;
	state.font2 = NULL;

	/* Overwrite addressing mode */
	state.cr[20] = 0x40;
	state.cr[23] = 0xa3;

	/* Program mode */
	vga_mlock(ctx);
	vga_restoremode(ctx, &state);
	vga_munlock(ctx);

	/* Update graph info */
	mutexLock(graph->lock);

	graph->width = modes[i].gfx.cfg->hres;
	graph->height = modes[i].gfx.cfg->vres;
	graph->depth = modes[i].depth;

	mutexUnlock(graph->lock);

	return EOK;
}


void vga_close(graph_t *graph)
{
	vga_dev_t *vga = (vga_dev_t *)graph->adapter;
	void *ctx = vga->ctx;

	/* Restore original video state */
	vga_mlock(ctx);
	vga_restore(ctx, &vga->state);
	vga_munlock(ctx);

	/* Lock VGA registers and destroy device */
	vga_lock(ctx);
	vgahw_done(ctx);
	free(vga);
}


int vga_open(graph_t *graph)
{
	vga_dev_t *vga;
	void *ctx;
	int err;

	if ((vga = malloc(sizeof(vga_dev_t))) == NULL)
		return -ENOMEM;

	if ((err = vgahw_init(vga->ctx)) < 0) {
		free(vga);
		return err;
	}
	ctx = vga->ctx;

	/* Check color support */
	if (!(vgahw_readmisc(ctx) & 0x01)) {
		vgahw_done(ctx);
		free(vga);
		return -ENOTSUP;
	}

	/* Unlock VGA registers and save current video state */
	vga_unlock(ctx);
	vga->state.cmap = vga->cmap;
	vga->state.font1 = vga->font1;
	vga->state.font2 = vga->font2;
	vga->state.text = vga->text;
	vga_save(ctx, &vga->state);

	/* Initialize graph info */
	graph->adapter = vga;
	graph->data = vgahw_mem(ctx);
	graph->width = 0;
	graph->height = 0;
	graph->depth = 0;

	/* Set graph functions */
	graph->close = vga_close;
	graph->mode = vga_mode;
	graph->vsync = vga_vsync;
	graph->isbusy = vga_isbusy;
	graph->trigger = vga_trigger;
	graph->commit = vga_commit;
	graph->colorset = vga_colorset;
	graph->colorget = vga_colorget;
	graph->cursorset = vga_cursorset;
	graph->cursorpos = vga_cursorpos;
	graph->cursorshow = vga_cursorshow;
	graph->cursorhide = vga_cursorhide;

	return EOK;
}


void vga_done(void)
{
	return;
}


int vga_init(void)
{
	return EOK;
}
