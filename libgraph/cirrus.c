/*
 * Phoenix-RTOS
 *
 * Cirrus Logic GD5446 VGA driver
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 *
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

#include <sys/mman.h>
#include <sys/platform.h>
#include <sys/threads.h>

#include <phoenix/arch/ia32.h>

#include <vga.h>

#include "graph.h"


/* Default graphics mode index */
#define DEFMODE 54 /* 800x600x32 @ 60Hz */


typedef struct {
	unsigned int freq; /* VCLK frequency (kHz) */
	unsigned char num; /* VCLK numerator */
	unsigned char den; /* VCLK denominator */
} cirrus_vclk_t;


typedef struct {
	graph_mode_t mode;   /* Graphics mode */
	unsigned char depth; /* Color depth */
	union {
		/* Power management mode */
		struct {
			unsigned char sr01; /* DPMS sr01 register configuration */
			unsigned char gr0e; /* DPMS gr0e register configuration */
		} pwm;
		/* Graphics mode */
		struct {
			graph_freq_t freq;    /* Screen refresh rate */
			const vga_cfg_t *cfg; /* Mode configuration */
		} gfx;
	};
} cirrus_mode_t;


typedef struct {
	vga_state_t state; /* Base VGA state */
	/* Extended CRT controller registers */
	unsigned char cr19; /* CRT controller 0x19 register */
	unsigned char cr1a; /* CRT controller 0x1a register */
	unsigned char cr1b; /* CRT controller 0x1b register */
	unsigned char cr1d; /* CRT controller 0x1d register */
	/* Extended sequencer registers */
	unsigned char sr07; /* Sequencer 0x07 register */
	unsigned char sr0e; /* Sequencer 0x0e register */
	unsigned char sr12; /* Sequencer 0x12 register */
	unsigned char sr13; /* Sequencer 0x13 register */
	unsigned char sr1e; /* Sequencer 0x1e register */
	/* Extended DAC registers */
	unsigned char hdr; /* Hidden DAC Register */
} cirrus_state_t;


typedef struct {
	void *vmem;                      /* Mapped video memory address */
	unsigned int vmemsz;             /* Mapped video memory size */
	unsigned char ctx[VGA_CTXSZ];    /* VGA context */
	unsigned char cmap[VGA_CMAPSZ];  /* Saved color map */
	unsigned char text[VGA_TEXTSZ];  /* Saved text */
	unsigned char font1[VGA_FONTSZ]; /* Saved font1 */
	unsigned char font2[VGA_FONTSZ]; /* Saved font2 */
	cirrus_state_t state;            /* Saved video state */
} cirrus_dev_t;

/* clang-format off */
/* Graphics modes configuration table */
static const vga_cfg_t cfgs[] = {
	{ 3, 25175,   640,  656,  752,  800,  400,  412,  414,  449, VGA_VSYNCP },                              /* 640x400   @ 70Hz */
	{ 3, 25175,   640,  656,  752,  800,  480,  490,  492,  525, 0 },                                       /* 640x480   @ 60Hz */
	{ 3, 31500,   640,  664,  704,  832,  480,  489,  491,  520, 0 },                                       /* 640x480   @ 72Hz */
	{ 3, 31500,   640,  656,  720,  840,  480,  481,  484,  500, 0 },                                       /* 640x480   @ 75Hz */
	{ 3, 36000,   640,  696,  752,  832,  480,  481,  484,  509, 0 },                                       /* 640x480   @ 85Hz */
	{ 3, 40000,   800,  840,  968, 1056,  600,  601,  605,  628, VGA_HSYNCP | VGA_VSYNCP },                 /* 800x600   @ 60Hz */
	{ 3, 36000,   800,  824,  896, 1024,  600,  601,  603,  625, VGA_HSYNCP | VGA_VSYNCP },                 /* 800x600   @ 56Hz */
	{ 3, 50000,   800,  856,  976, 1040,  600,  637,  643,  666, VGA_HSYNCP | VGA_VSYNCP },                 /* 800x600   @ 72Hz */
	{ 3, 49500,   800,  816,  896, 1056,  600,  601,  604,  625, VGA_HSYNCP | VGA_VSYNCP },                 /* 800x600   @ 75Hz */
	{ 3, 56250,   800,  832,  896, 1048,  600,  601,  604,  631, VGA_HSYNCP | VGA_VSYNCP },                 /* 800x600   @ 85Hz */
	{ 3, 65000,  1024, 1048, 1184, 1344,  768,  771,  777,  806, 0 },                                       /* 1024x768  @ 60Hz */
	{ 3, 44900,  1024, 1032, 1208, 1264,  768,  768,  776,  817, VGA_HSYNCP | VGA_VSYNCP | VGA_INTERLACE }, /* 1024x768  @ 43Hz interlaced */
	{ 3, 75000,  1024, 1048, 1184, 1328,  768,  771,  777,  806, 0 },                                       /* 1024x768  @ 70Hz */
	{ 3, 78800,  1024, 1040, 1136, 1312,  768,  769,  772,  800, VGA_HSYNCP | VGA_VSYNCP },                 /* 1024x768  @ 75Hz */
	{ 3, 94500,  1024, 1072, 1168, 1376,  768,  769,  772,  808, VGA_VSYNCP | VGA_VSYNCP },                 /* 1024x768  @ 85Hz */
	{ 3, 108000, 1152, 1216, 1344, 1600,  864,  865,  868,  900, VGA_HSYNCP | VGA_VSYNCP },                 /* 1152x864  @ 75Hz */
	{ 3, 108000, 1280, 1328, 1440, 1688, 1024, 1025, 1028, 1066, VGA_HSYNCP | VGA_VSYNCP },                 /* 1280x1024 @ 60Hz */
	{ 3, 135000, 1280, 1296, 1440, 1688, 1024, 1025, 1028, 1066, VGA_HSYNCP | VGA_VSYNCP },                 /* 1280x1024 @ 75Hz */
	/* No configuration */
	{ 0 }
};


/* Graphics modes table */
static const cirrus_mode_t modes[] = {
	/* Power management modes */
	{ GRAPH_ON,          0, .pwm = { 0x00, 0x00 } },             /*  0, Screen: on,  HSync: on,  VSync: on  */
	{ GRAPH_OFF,         0, .pwm = { 0x20, 0x06 } },             /*  1, Screen: off, HSync: off, VSync: off */
	{ GRAPH_STANDBY,     0, .pwm = { 0x20, 0x02 } },             /*  2, Screen: off, HSync: off, VSync: on  */
	{ GRAPH_SUSPEND,     0, .pwm = { 0x20, 0x04 } },             /*  3, Screen: off, HSync: on,  VSync: off */
	/* 8-bit color palette */
	{ GRAPH_640x400x8,   1, .gfx = { GRAPH_70Hz,  cfgs } },      /*  4 */
	{ GRAPH_640x480x8,   1, .gfx = { GRAPH_60Hz,  cfgs + 1 } },  /*  5 */
	{ GRAPH_640x480x8,   1, .gfx = { GRAPH_72Hz,  cfgs + 2 } },  /*  6 */
	{ GRAPH_640x480x8,   1, .gfx = { GRAPH_75Hz,  cfgs + 3 } },  /*  7 */
	{ GRAPH_640x480x8,   1, .gfx = { GRAPH_85Hz,  cfgs + 4 } },  /*  8 */
	{ GRAPH_800x600x8,   1, .gfx = { GRAPH_60Hz,  cfgs + 5 } },  /*  9 */
	{ GRAPH_800x600x8,   1, .gfx = { GRAPH_56Hz,  cfgs + 6 } },  /* 10 */
	{ GRAPH_800x600x8,   1, .gfx = { GRAPH_72Hz,  cfgs + 7 } },  /* 11 */
	{ GRAPH_800x600x8,   1, .gfx = { GRAPH_75Hz,  cfgs + 8 } },  /* 12 */
	{ GRAPH_800x600x8,   1, .gfx = { GRAPH_85Hz,  cfgs + 9 } },  /* 13 */
	{ GRAPH_1024x768x8,  1, .gfx = { GRAPH_60Hz,  cfgs + 10 } }, /* 14 */
	{ GRAPH_1024x768x8,  1, .gfx = { GRAPH_43Hzi, cfgs + 11 } }, /* 15 */
	{ GRAPH_1024x768x8,  1, .gfx = { GRAPH_70Hz,  cfgs + 12 } }, /* 16 */
	{ GRAPH_1024x768x8,  1, .gfx = { GRAPH_75Hz,  cfgs + 13 } }, /* 17 */
	{ GRAPH_1024x768x8,  1, .gfx = { GRAPH_85Hz,  cfgs + 14 } }, /* 18 */
	{ GRAPH_1152x864x8,  1, .gfx = { GRAPH_75Hz,  cfgs + 15 } }, /* 19 */
	{ GRAPH_1280x1024x8, 1, .gfx = { GRAPH_60Hz,  cfgs + 16 } }, /* 20 */
	{ GRAPH_1280x1024x8, 1, .gfx = { GRAPH_75Hz,  cfgs + 17 } }, /* 21 */
	/* 16-bit color (5:6:5) */
	{ GRAPH_640x480x16,  2, .gfx = { GRAPH_60Hz,  cfgs + 1 } },  /* 22 */
	{ GRAPH_640x480x16,  2, .gfx = { GRAPH_72Hz,  cfgs + 2 } },  /* 23 */
	{ GRAPH_640x480x16,  2, .gfx = { GRAPH_75Hz,  cfgs + 3 } },  /* 24 */
	{ GRAPH_640x480x16,  2, .gfx = { GRAPH_85Hz,  cfgs + 4 } },  /* 25 */
	{ GRAPH_800x600x16,  2, .gfx = { GRAPH_60Hz,  cfgs + 5 } },  /* 26 */
	{ GRAPH_800x600x16,  2, .gfx = { GRAPH_56Hz,  cfgs + 6 } },  /* 27 */
	{ GRAPH_800x600x16,  2, .gfx = { GRAPH_72Hz,  cfgs + 7 } },  /* 28 */
	{ GRAPH_800x600x16,  2, .gfx = { GRAPH_75Hz,  cfgs + 8 } },  /* 29 */
	{ GRAPH_800x600x16,  2, .gfx = { GRAPH_85Hz,  cfgs + 9 } },  /* 30 */
	{ GRAPH_1024x768x16, 2, .gfx = { GRAPH_60Hz,  cfgs + 10 } }, /* 31 */
	{ GRAPH_1024x768x16, 2, .gfx = { GRAPH_43Hzi, cfgs + 11 } }, /* 32 */
	{ GRAPH_1024x768x16, 2, .gfx = { GRAPH_70Hz,  cfgs + 12 } }, /* 33 */
	{ GRAPH_1024x768x16, 2, .gfx = { GRAPH_75Hz,  cfgs + 13 } }, /* 34 */
	{ GRAPH_1024x768x16, 2, .gfx = { GRAPH_85Hz,  cfgs + 14 } }, /* 35 */
	/* 24-bit color (8:8:8) */
	{ GRAPH_640x480x24,  3, .gfx = { GRAPH_60Hz,  cfgs + 1 } },  /* 36 */
	{ GRAPH_640x480x24,  3, .gfx = { GRAPH_72Hz,  cfgs + 2 } },  /* 37 */
	{ GRAPH_640x480x24,  3, .gfx = { GRAPH_75Hz,  cfgs + 3 } },  /* 38 */
	{ GRAPH_640x480x24,  3, .gfx = { GRAPH_85Hz,  cfgs + 4 } },  /* 39 */
	{ GRAPH_800x600x24,  3, .gfx = { GRAPH_60Hz,  cfgs + 5 } },  /* 40 */
	{ GRAPH_800x600x24,  3, .gfx = { GRAPH_56Hz,  cfgs + 6 } },  /* 41 */
	{ GRAPH_800x600x24,  3, .gfx = { GRAPH_72Hz,  cfgs + 7 } },  /* 42 */
	{ GRAPH_800x600x24,  3, .gfx = { GRAPH_75Hz,  cfgs + 8 } },  /* 43 */
	{ GRAPH_800x600x24,  3, .gfx = { GRAPH_85Hz,  cfgs + 9 } },  /* 44 */
	{ GRAPH_1024x768x24, 3, .gfx = { GRAPH_60Hz,  cfgs + 10 } }, /* 45 */
	{ GRAPH_1024x768x24, 3, .gfx = { GRAPH_43Hzi, cfgs + 11 } }, /* 46 */
	{ GRAPH_1024x768x24, 3, .gfx = { GRAPH_70Hz,  cfgs + 12 } }, /* 47 */
	{ GRAPH_1024x768x24, 3, .gfx = { GRAPH_75Hz,  cfgs + 13 } }, /* 48 */
	{ GRAPH_1024x768x24, 3, .gfx = { GRAPH_85Hz,  cfgs + 14 } }, /* 49 */
	/* 32-bit color (8:8:8:8) */
	{ GRAPH_640x480x32,  4, .gfx = { GRAPH_60Hz,  cfgs + 1 } },  /* 50 */
	{ GRAPH_640x480x32,  4, .gfx = { GRAPH_72Hz,  cfgs + 2 } },  /* 51 */
	{ GRAPH_640x480x32,  4, .gfx = { GRAPH_75Hz,  cfgs + 3 } },  /* 52 */
	{ GRAPH_640x480x32,  4, .gfx = { GRAPH_85Hz,  cfgs + 4 } },  /* 53 */
	{ GRAPH_800x600x32,  4, .gfx = { GRAPH_60Hz,  cfgs + 5 } },  /* 54 */
	{ GRAPH_800x600x32,  4, .gfx = { GRAPH_56Hz,  cfgs + 6 } },  /* 55 */
	{ GRAPH_800x600x32,  4, .gfx = { GRAPH_72Hz,  cfgs + 7 } },  /* 56 */
	{ GRAPH_800x600x32,  4, .gfx = { GRAPH_75Hz,  cfgs + 8 } },  /* 57 */
	{ GRAPH_800x600x32,  4, .gfx = { GRAPH_85Hz,  cfgs + 9 } },  /* 58 */
	/* No mode */
	{ 0 }
};


/* VCLK table */
static const cirrus_vclk_t vclks[] = {
	/* Known stable VCLK */
	{ 12599,  0x2c, 0x33 },
	{ 18000,  0x27, 0x3e },
	{ 19600,  0x28, 0x3a },
	{ 25227,  0x4a, 0x2b },
	{ 28325,  0x5b, 0x2f },
	{ 31500,  0x42, 0x1f },
	{ 36025,  0x4e, 0x3e },
	{ 37747,  0x3a, 0x17 },
	{ 39992,  0x51, 0x3a },
	{ 41164,  0x45, 0x30 },
	{ 45076,  0x55, 0x36 },
	{ 49867,  0x65, 0x3a },
	{ 64983,  0x76, 0x34 },
	{ 72163,  0x7e, 0x32 },
	{ 75000,  0x6e, 0x2a },
	{ 80013,  0x5f, 0x22 },
	{ 85226,  0x7d, 0x2a },
	{ 89998,  0x58, 0x1c },
	{ 95019,  0x49, 0x16 },
	{ 100226, 0x46, 0x14 },
	{ 108035, 0x53, 0x16 },
	{ 109771, 0x5c, 0x18 },
	{ 120050, 0x6d, 0x1a },
	{ 125998, 0x58, 0x14 },
	{ 130055, 0x6d, 0x18 },
	{ 134998, 0x42, 0x0e },
	{ 150339, 0x69, 0x14 },
	{ 168236, 0x5e, 0x10 },
	{ 188179, 0x5c, 0x0e },
	{ 210679, 0x67, 0x0e },
	{ 229088, 0x60, 0x0c },
	/* No clock */
	{ 0 }
};


/* Max VCLK for given color depth */
static const unsigned int maxvclks[] = { 0, 135300, 86000, 86000, 60000 };
/* clang-format on */

/* Schedules and executes tasks */
extern int graph_schedule(graph_t *graph);


int cirrus_vsync(graph_t *graph)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;

	/* Wait for current vertical retrace end */
	while (vgahw_status(ctx) & 0x08)
		;
	/* Wait for next vertical retrace start */
	while (!(vgahw_status(ctx) & 0x08))
		;

	return 1;
}


int cirrus_isbusy(graph_t *graph)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;

	return vgahw_readgfx(ctx, 0x31) & 0x01;
}


int cirrus_trigger(graph_t *graph)
{
	if (cirrus_isbusy(graph))
		return -EBUSY;

	return graph_schedule(graph);
}


int cirrus_commit(graph_t *graph)
{
	return EOK;
}


int cirrus_colorset(graph_t *graph, const unsigned char *colors, unsigned char first, unsigned char last)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;
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


int cirrus_colorget(graph_t *graph, unsigned char *colors, unsigned char first, unsigned char last)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;
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


int cirrus_cursorset(graph_t *graph, const unsigned char *amask, const unsigned char *xmask, unsigned int bg, unsigned int fg)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;
	unsigned char *cur, sr12;
	unsigned int i, j;

	if (cdev->vmemsz < graph->height * graph->width * graph->depth + 0x1000)
		return -ENOSPC;

	cur = (unsigned char *)cdev->vmem + cdev->vmemsz - 0x1000;
	for (i = 0; i < 64; i++) {
		for (j = 0; j < 8; j++)
			*cur++ = *xmask++;
		for (j = 0; j < 8; j++)
			*cur++ = ~(*amask++);
	}
	vgahw_writeseq(ctx, 0x13, 0x30);

	sr12 = vgahw_readseq(ctx, 0x12);
	vgahw_writeseq(ctx, 0x12, sr12 | 0x82);
	vgahw_writedac(ctx, 0x02, 0x00);
	vgahw_status(ctx);
	vgahw_writedac(ctx, 0x03, bg >> 18);
	vgahw_status(ctx);
	vgahw_writedac(ctx, 0x03, bg >> 10);
	vgahw_status(ctx);
	vgahw_writedac(ctx, 0x03, bg >> 2);
	vgahw_writedac(ctx, 0x02, 0x0f);
	vgahw_status(ctx);
	vgahw_writedac(ctx, 0x03, fg >> 18);
	vgahw_status(ctx);
	vgahw_writedac(ctx, 0x03, fg >> 10);
	vgahw_status(ctx);
	vgahw_writedac(ctx, 0x03, fg >> 2);
	vgahw_writeseq(ctx, 0x12, sr12 & ~0x02);

	return EOK;
}


int cirrus_cursorpos(graph_t *graph, unsigned int x, unsigned int y)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;

	vgahw_writeseq(ctx, (x << 5) | 0x10, x >> 3);
	vgahw_writeseq(ctx, (y << 5) | 0x11, y >> 3);

	return EOK;
}


int cirrus_cursorhide(graph_t *graph)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;

	vgahw_writeseq(ctx, 0x12, vgahw_readseq(ctx, 0x12) & ~0x01);

	return EOK;
}


int cirrus_cursorshow(graph_t *graph)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;

	vgahw_writeseq(ctx, 0x12, vgahw_readseq(ctx, 0x12) | 0x01);

	return EOK;
}


/* Performs screen-to-screen BitBLT operation */
static int cirrus_blt(cirrus_dev_t *cdev, unsigned int src, unsigned int dst, unsigned int dx, unsigned int dy, unsigned int srcspan, unsigned int dstspan, unsigned char mode)
{
	void *ctx = cdev->ctx;

	/* BLT width and height */
	vgahw_writegfx(ctx, 0x20, dx - 1);
	vgahw_writegfx(ctx, 0x21, (dx - 1) >> 8);
	vgahw_writegfx(ctx, 0x22, dy - 1);
	vgahw_writegfx(ctx, 0x23, (dy - 1) >> 8);

	/* BLT pitch */
	vgahw_writegfx(ctx, 0x24, dstspan);
	vgahw_writegfx(ctx, 0x25, dstspan >> 8);
	vgahw_writegfx(ctx, 0x26, srcspan);
	vgahw_writegfx(ctx, 0x27, srcspan >> 8);

	/* BLT destination offset */
	vgahw_writegfx(ctx, 0x28, dst);
	vgahw_writegfx(ctx, 0x29, dst >> 8);
	vgahw_writegfx(ctx, 0x2a, dst >> 16);

	/* BLT source offset */
	vgahw_writegfx(ctx, 0x2c, src);
	vgahw_writegfx(ctx, 0x2d, src >> 8);
	vgahw_writegfx(ctx, 0x2e, src >> 16);

	/* Set BLT mode, BLT ROP and go! */
	vgahw_writegfx(ctx, 0x30, mode);
	vgahw_writegfx(ctx, 0x32, 0x0d);
	vgahw_writegfx(ctx, 0x31, 0x02);

	return EOK;
}


int cirrus_rect(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;
	unsigned int span = graph->width * graph->depth;
	unsigned char mode;

#ifdef GRAPH_VERIFY_ARGS
	if ((x + dx > graph->width) || (y + dy > graph->height))
		return -EINVAL;
#endif

	vgahw_writegfx(ctx, 0x33, 0x04);
	vgahw_writegfx(ctx, 0x00, color);
	vgahw_writegfx(ctx, 0x01, color);
	mode = 0xc0;

	if (graph->depth >= 2) {
		vgahw_writegfx(ctx, 0x10, color >> 8);
		vgahw_writegfx(ctx, 0x11, color >> 8);
		mode = 0xd0;
	}

	if (graph->depth >= 3) {
		vgahw_writegfx(ctx, 0x12, color >> 16);
		vgahw_writegfx(ctx, 0x13, color >> 16);
		mode = 0xe0;
	}

	if (graph->depth == 4) {
		vgahw_writegfx(ctx, 0x14, color >> 24);
		vgahw_writegfx(ctx, 0x15, color >> 24);
		mode = 0xf0;
	}

	return cirrus_blt(cdev, 0, y * span + x * graph->depth, dx * graph->depth, dy, span, span, mode);
}


int cirrus_move(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	unsigned int span, src, dst;
	unsigned char mode;

#ifdef GRAPH_VERIFY_ARGS
	if ((x + dx > graph->width) || (y + dy > graph->height) ||
		((int)x + mx < 0) || ((int)y + my < 0) ||
		((int)x + mx > graph->width) || ((int)y + my > graph->height) ||
		(x + dx + mx > graph->width) || (y + dy + my > graph->height))
		return -EINVAL;
#endif

	span = graph->depth * graph->width;
	src = y * span + x * graph->depth;
	dst = (y + my) * span + (x + mx) * graph->depth;
	dx *= graph->depth;
	mode = 0x00;

	/* BLT backwards */
	if (((int)dy > my) && ((my > 0) || (!my && (mx > 0)))) {
		src += (dy - 1) * span + dx - 1;
		dst += (dy - 1) * span + dx - 1;
		mode = 0x01;
	}

	return cirrus_blt(cdev, src, dst, dx, dy, span, span, mode);
}


/* Finds best numerator and denominator values for given VCLK frequency */
static int cirrus_vclk(unsigned int maxvclk, cirrus_vclk_t *vclk)
{
	unsigned int n, d, f, diff, mindiff, freq = vclk->freq;

	/* Prefer tested clock if it matches within 0.1% */
	for (n = 0; vclks[n].freq; n++) {
		if (abs((int)vclks[n].freq - (int)freq) < freq / 1000) {
			*vclk = vclks[n];
			return EOK;
		}
	}

	/* Max stable VCLK */
	if (maxvclk < 111000)
		maxvclk = 111000;

	/* Find VCLK */
	vclk->freq = 0;
	mindiff = freq;
	for (n = 0x10; n < 0x7f; n++) {
		for (d = 0x14; d < 0x3f; d++) {
			/* Skip unstable combinations */
			f = (n & 0x7f) * 28636 / (d & 0x3e);
			if ((f < 28636) || (f > maxvclk))
				continue;
			f >>= (d & 0x01);

			if ((diff = abs((int)f - (int)freq)) < mindiff) {
				vclk->freq = f;
				vclk->num = n;
				vclk->den = d;
				mindiff = diff;
			}
		}
	}

	return (vclk->freq) ? EOK : -EINVAL;
}


/* Saves video state */
static void cirrus_save(cirrus_dev_t *cdev, cirrus_state_t *state)
{
	void *ctx = cdev->ctx;

	/* Save base VGA state */
	vga_save(ctx, &state->state);

	/* Save extended VGA state */
	state->cr19 = vgahw_readcrtc(ctx, 0x19);
	state->cr1a = vgahw_readcrtc(ctx, 0x1a);
	state->cr1b = vgahw_readcrtc(ctx, 0x1b);
	state->cr1d = vgahw_readcrtc(ctx, 0x1d);
	state->sr07 = vgahw_readseq(ctx, 0x07);
	state->sr0e = vgahw_readseq(ctx, 0x0e);
	state->sr12 = vgahw_readseq(ctx, 0x12);
	state->sr13 = vgahw_readseq(ctx, 0x13);
	state->sr1e = vgahw_readseq(ctx, 0x1e);
	/* Read DAC pixel mask before HDR access */
	vgahw_readdac(ctx, 0x00);
	vgahw_readdac(ctx, 0x00);
	vgahw_readdac(ctx, 0x00);
	vgahw_readdac(ctx, 0x00);
	state->hdr = vgahw_readdac(ctx, 0x00);
}


/* Restores video state */
static void cirrus_restore(cirrus_dev_t *cdev, cirrus_state_t *state)
{
	void *ctx = cdev->ctx;

	/* Restore extended VGA state */
	vgahw_writecrtc(ctx, 0x19, state->cr19);
	vgahw_writecrtc(ctx, 0x1a, state->cr1a);
	vgahw_writecrtc(ctx, 0x1b, state->cr1b);
	vgahw_writecrtc(ctx, 0x1d, state->cr1d);
	vgahw_writeseq(ctx, 0x07, state->sr07);
	vgahw_writeseq(ctx, 0x0e, state->sr0e);
	vgahw_writeseq(ctx, 0x12, state->sr12);
	vgahw_writeseq(ctx, 0x13, state->sr13);
	vgahw_writeseq(ctx, 0x1e, state->sr1e);
	/* Read DAC pixel mask before HDR access */
	vgahw_readdac(ctx, 0x00);
	vgahw_readdac(ctx, 0x00);
	vgahw_readdac(ctx, 0x00);
	vgahw_readdac(ctx, 0x00);
	vgahw_writedac(ctx, 0x00, state->hdr);

	/* Restore base VGA state */
	vga_restore(ctx, &state->state);
}


int cirrus_mode(graph_t *graph, graph_mode_t mode, graph_freq_t freq)
{
	unsigned int hdiv = 0, vdiv = 0, i = DEFMODE;
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;
	cirrus_state_t state;
	cirrus_vclk_t vclk;
	vga_cfg_t cfg;

	if (mode != GRAPH_DEFMODE) {
		for (i = 0; (modes[i].mode != mode) || (modes[i].depth && (freq != GRAPH_DEFFREQ) && (modes[i].gfx.freq != freq)); i++) {
			if (!modes[i].mode)
				return -ENOTSUP;
		}
	}

	/* Power management mode (DPMS) */
	if (!modes[i].depth) {
		vgahw_writeseq(ctx, 0x01, (vgahw_readseq(ctx, 0x01) & ~0x20) | modes[i].pwm.sr01);
		vgahw_writegfx(ctx, 0x0e, (vgahw_readgfx(ctx, 0x0e) & ~0x06) | modes[i].pwm.gr0e);
		return EOK;
	}

	if (cdev->vmemsz < modes[i].gfx.cfg->vres * modes[i].gfx.cfg->hres * modes[i].depth)
		return -ENOSPC;
	cfg = *modes[i].gfx.cfg;

	/* Adjust horizontal timings */
	if (cfg.clk > 85500) {
		cfg.hres >>= 1;
		cfg.hsyncs >>= 1;
		cfg.hsynce >>= 1;
		cfg.htotal >>= 1;
		cfg.clk >>= 1;
		hdiv = 1;
	}

	/* Adjust vertical timings */
	if (cfg.vtotal >= 1024 && !(cfg.flags & VGA_INTERLACE)) {
		cfg.vres >>= 1;
		cfg.vsyncs >>= 1;
		cfg.vsynce >>= 1;
		cfg.vtotal >>= 1;
		vdiv = 1;
	}

	/* Find pixel clock */
	vclk.freq = cfg.clk;
	if (cirrus_vclk(maxvclks[modes[i].depth], &vclk) < 0)
		return -EINVAL;

	/* Initialize VGA state */
	vga_initstate(&cfg, &state.state);
	state.state.cmap = NULL;
	state.state.text = NULL;
	state.state.font1 = NULL;
	state.state.font2 = NULL;
	state.state.cr[19] = (modes[i].gfx.cfg->hres * modes[i].depth) >> 3;
	state.state.cr[23] |= vdiv * 0x04;

	/* Initialize extended VGA registers */
	state.cr19 = 0x00;
	state.cr1a = (((cfg.vsyncs + 1) & 0x300) >> 2) | ((cfg.hsynce & 0xc0) >> 2);
	state.cr1b = ((((modes[i].gfx.cfg->hres * modes[i].depth) >> 3) & 0x100) >> 4) | 0x22;
	state.cr1d = 0x00;
	state.sr07 = 0xe0;
	state.sr0e = vclk.num;
	state.sr12 = 0x04;
	state.sr13 = 0x00;
	state.sr1e = vclk.den;

	if (cfg.flags & VGA_INTERLACE) {
		state.cr19 = ((cfg.htotal >> 3) - 5) >> 1;
		state.cr1a |= 0x01;
	}

	switch (modes[i].depth) {
		case 1:
			state.sr07 |= (hdiv) ? 0x17 : 0x11;
			state.hdr = (hdiv) ? 0x4a : 0x00;
			break;

		case 2:
			state.sr07 |= (hdiv) ? 0x19 : 0x17;
			state.hdr = 0xc1;
			break;

		case 3:
			state.sr07 |= 0x15;
			state.hdr = 0xc5;
			break;

		case 4:
			state.sr07 |= 0x19;
			state.hdr = 0xc5;
			break;

		default:
			return -EINVAL;
	}

	/* Program mode */
	vga_mlock(ctx);
	cirrus_restore(cdev, &state);
	vga_munlock(ctx);

	/* Update graph info */
	mutexLock(graph->lock);

	graph->depth = modes[i].depth;
	graph->width = modes[i].gfx.cfg->hres;
	graph->height = modes[i].gfx.cfg->vres;

	mutexUnlock(graph->lock);

	return EOK;
}


void cirrus_close(graph_t *graph)
{
	cirrus_dev_t *cdev = (cirrus_dev_t *)graph->adapter;
	void *ctx = cdev->ctx;

	/* Restore original video state */
	vga_mlock(ctx);
	cirrus_restore(cdev, &cdev->state);
	vga_munlock(ctx);

	/* Lock VGA registers and destroy device */
	vga_lock(ctx);
	vgahw_done(ctx);
	munmap(cdev->vmem, (cdev->vmemsz + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1));
	free(cdev);
}


/* Returns video memory size */
static unsigned int cirrus_vmemsz(cirrus_dev_t *cdev)
{
	void *ctx = cdev->ctx;
	unsigned char sr0f, sr17;

	sr0f = vgahw_readseq(ctx, 0x0f);
	sr17 = vgahw_readseq(ctx, 0x17);

	if ((sr0f & 0x18) == 0x18) {
		if (sr0f & 0x80) {
			if (sr17 & 0x80)
				return 0x200000;

			if (sr17 & 0x02)
				return 0x300000;

			return 0x400000;
		}

		if (!(sr17 & 0x80))
			return 0x200000;
	}

	return 0x100000;
}


int cirrus_open(graph_t *graph)
{
	platformctl_t pctl = { .action = pctl_get, .type = pctl_pci };
	cirrus_dev_t *cdev;
	void *ctx;
	int err;

	pctl.pci.id.vendor = 0x1013;
	pctl.pci.id.device = 0x00b8;
	pctl.pci.id.subvendor = PCI_ANY;
	pctl.pci.id.subdevice = PCI_ANY;
	pctl.pci.id.cl = PCI_ANY;
	pctl.pci.dev.bus = 0;
	pctl.pci.dev.dev = 0;
	pctl.pci.dev.func = 0;
	pctl.pci.caps = NULL;

	if ((err = platformctl(&pctl)) < 0)
		return err;

	/* Check PCI BAR0 for video memory space */
	if (!pctl.pci.dev.resources[0].base || !pctl.pci.dev.resources[0].limit || (pctl.pci.dev.resources[0].flags & 0x01))
		return -EFAULT;

	/* Allocate device memory */
	if ((cdev = malloc(sizeof(cirrus_dev_t))) == NULL)
		return -ENOMEM;
	ctx = cdev->ctx;

	/* Initialize VGA chip */
	if ((err = vgahw_init(ctx)) < 0) {
		free(cdev);
		return err;
	}

	/* Check color support */
	if (!(vgahw_readmisc(ctx) & 0x01)) {
		vgahw_done(ctx);
		free(cdev);
		return -ENOTSUP;
	}

	/* Map video memory */
	cdev->vmemsz = cirrus_vmemsz(cdev);
	if ((cdev->vmem = mmap(NULL, (cdev->vmemsz + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1), PROT_READ | PROT_WRITE, MAP_DEVICE | MAP_UNCACHED | MAP_ANONYMOUS | MAP_PHYSMEM, -1, pctl.pci.dev.resources[0].base)) == MAP_FAILED) {
		vgahw_done(ctx);
		free(cdev);
		return -ENOMEM;
	}

	/* Unlock VGA registers and save current video state */
	vga_unlock(ctx);
	cdev->state.state.cmap = cdev->cmap;
	cdev->state.state.font1 = cdev->font1;
	cdev->state.state.font2 = cdev->font2;
	cdev->state.state.text = cdev->text;
	cirrus_save(cdev, &cdev->state);

	/* Initialize graph info */
	graph->adapter = cdev;
	graph->data = cdev->vmem;
	graph->width = 0;
	graph->height = 0;
	graph->depth = 0;

	/* Set graph functions */
	graph->close = cirrus_close;
	graph->mode = cirrus_mode;
	graph->vsync = cirrus_vsync;
	graph->isbusy = cirrus_isbusy;
	graph->trigger = cirrus_trigger;
	graph->commit = cirrus_commit;
	graph->colorset = cirrus_colorset;
	graph->colorget = cirrus_colorget;
	graph->cursorset = cirrus_cursorset;
	graph->cursorpos = cirrus_cursorpos;
	graph->cursorshow = cirrus_cursorshow;
	graph->cursorhide = cirrus_cursorhide;

	/* Accelerated graphics operations */
	graph->rect = cirrus_rect;
	graph->move = cirrus_move;

	return EOK;
}


void cirrus_done(void)
{
	return;
}


int cirrus_init(void)
{
	return EOK;
}
