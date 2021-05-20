/*
 * Phoenix-RTOS
 *
 * Generic VGA library
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _VGA_H_
#define _VGA_H_


/* VGA common defines */
#define VGA_CTXSZ  0x80             /* VGA context size */
#define VGA_MEMSZ  0x10000          /* VGA memory size */
#define VGA_CMAPSZ 768              /* VGA color map size */
#define VGA_TEXTSZ (VGA_MEMSZ >> 1) /* VGA text size */
#define VGA_FONTSZ VGA_MEMSZ        /* VGA font size */

/* clang-format off */
/* VGA mode adjustment flags */
enum {
	VGA_HSYNCP    = (1 << 0), /* HSync positive polarity */
	VGA_VSYNCP    = (1 << 1), /* VSync positive polarity */
	VGA_CLKDIV    = (1 << 2), /* Pixel clock is divided by 2 */
	VGA_DBLSCAN   = (1 << 3), /* Double scan */
	VGA_INTERLACE = (1 << 4)  /* Interlace mode */
};
/* clang-format on */

typedef struct {
	/* Pixel clock */
	unsigned int clkidx; /* Pixel clock source index */
	unsigned int clk;    /* Pixel clock frequency (kHz) */
	/* Horizontal timings */
	unsigned int hres;   /* Horizontal resolution */
	unsigned int hsyncs; /* Horizontal sync start */
	unsigned int hsynce; /* Horizontal sync end */
	unsigned int htotal; /* Horizontal total pixels */
	/* Vertical timings */
	unsigned int vres;   /* Vertical resolution */
	unsigned int vsyncs; /* Vertical sync start */
	unsigned int vsynce; /* Vertical sync end */
	unsigned int vtotal; /* Vertical total lines */
	/* Mode adjustments */
	unsigned char flags; /* Mode adjustment flags */
} vga_cfg_t;


typedef struct {
	unsigned char mr;     /* Miscellaneous register */
	unsigned char cr[25]; /* CRT controller registers */
	unsigned char sr[5];  /* Sequencer registers */
	unsigned char gr[9];  /* Graphics controller registers */
	unsigned char ar[21]; /* Attribute controller registers */
	unsigned char *cmap;  /* Color map */
	unsigned char *text;  /* Plane 0 and 1 text */
	unsigned char *font1; /* Plane 2 font */
	unsigned char *font2; /* Plane 3 font */
} vga_state_t;


/****************************************************/
/* Low level interface (hardware abstraction layer) */
/****************************************************/


/* Returns mapped VGA memory address */
extern void *vgahw_mem(void *hwctx);


/* Reads from input status register */
extern unsigned char vgahw_status(void *hwctx);


/* Reads from feature control register */
extern unsigned char vgahw_readfcr(void *hwctx);


/* Writes to feature control register */
extern void vgahw_writefcr(void *hwctx, unsigned char val);


/* Reads from miscellaneous register */
extern unsigned char vgahw_readmisc(void *hwctx);


/* Writes to miscellaneous register */
extern void vgahw_writemisc(void *hwctx, unsigned char val);


/* Reads from CRT controller register */
extern unsigned char vgahw_readcrtc(void *hwctx, unsigned char reg);


/* Writes to CRT controller register */
extern void vgahw_writecrtc(void *hwctx, unsigned char reg, unsigned char val);


/* Reads from sequencer register */
extern unsigned char vgahw_readseq(void *hwctx, unsigned char reg);


/* Writes to sequencer register */
extern void vgahw_writeseq(void *hwctx, unsigned char reg, unsigned char val);


/* Reads from graphics controller register */
extern unsigned char vgahw_readgfx(void *hwctx, unsigned char reg);


/* Writes to graphics controller register */
extern void vgahw_writegfx(void *hwctx, unsigned char reg, unsigned char val);


/* Reads from attribute controller register */
extern unsigned char vgahw_readattr(void *hwctx, unsigned char reg);


/* Writes to attribute controller register */
extern void vgahw_writeattr(void *hwctx, unsigned char reg, unsigned char val);


/* Reads from DAC controller register */
extern unsigned char vgahw_readdac(void *hwctx, unsigned char reg);


/* Writes to DAC controller register */
extern void vgahw_writedac(void *hwctx, unsigned char reg, unsigned char val);


/* Enables color map */
extern void vgahw_enablecmap(void *hwctx);


/* Disables color map */
extern void vgahw_disablecmap(void *hwctx);


/* Destroys VGA handle */
extern void vgahw_done(void *hwctx);


/* Initializes VGA handle */
extern int vgahw_init(void *hwctx);


/************************/
/* High level interface */
/************************/


/* Locks CRTC[0-7] registers */
extern void vga_lock(void *hwctx);


/* Unlocks CRTC[0-7] registers */
extern void vga_unlock(void *hwctx);


/* Protects VGA registers and memory during mode switch */
extern void vga_mlock(void *hwctx);


/* Releases VGA mode switch protection set with vga_mlock() */
extern void vga_munlock(void *hwctx);


/* Blanks screen */
extern void vga_blank(void *hwctx);


/* Unblanks screen */
extern void vga_unblank(void *hwctx);


/* Saves VGA mode */
extern void vga_savemode(void *hwctx, vga_state_t *state);


/* Restores VGA mode */
extern void vga_restoremode(void *hwctx, vga_state_t *state);


/* Saves VGA color map */
extern void vga_savecmap(void *hwctx, vga_state_t *state);


/* Restores VGA color map */
extern void vga_restorecmap(void *hwctx, vga_state_t *state);


/* Saves VGA fonts and text */
extern void vga_savetext(void *hwctx, vga_state_t *state);


/* Restores VGA fonts and text */
extern void vga_restoretext(void *hwctx, vga_state_t *state);


/* Saves VGA settings */
extern void vga_save(void *hwctx, vga_state_t *state);


/* Restores VGA settings */
extern void vga_restore(void *hwctx, vga_state_t *state);


/* Initializes VGA state for given mode configuration */
extern void vga_initstate(vga_cfg_t *cfg, vga_state_t *state);


#endif
