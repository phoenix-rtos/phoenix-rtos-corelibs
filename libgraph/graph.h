/*
 * Phoenix-RTOS
 *
 * Graphics library
 *
 * Copyright 2009, 2021 Phoenix Systems
 * Copyright 2002-2007 IMMOS
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <sys/types.h>

/* clang-format off */
/* Graphics adapters */
typedef enum {
	GRAPH_NONE      =  0,       /* No graphics adapter */
	GRAPH_VIRTIOGPU = (1 << 0), /* Generic VirtIO GPU graphics adapter */
	GRAPH_VGA       = (1 << 1), /* Generic VGA graphics adapter */
	GRAPH_CIRRUS    = (1 << 2), /* Cirrus Logic graphics adapter */
	GRAPH_ANY       = -1        /* Any graphics adapter */
} graph_adapter_t;
/* clang-format on */

/* Graphics modes */
typedef enum {
	GRAPH_DEFMODE, /* Default graphics mode */
	/* Power management modes */
	GRAPH_ON,      /* Display enabled mode */
	GRAPH_OFF,     /* Display disabled mode */
	GRAPH_STANDBY, /* Display standby mode */
	GRAPH_SUSPEND, /* Display suspend mode */
	/* 8-bit color palette */
	GRAPH_320x200x8,   /* 320x200   8-bit color */
	GRAPH_640x400x8,   /* 640x400   8-bit color */
	GRAPH_640x480x8,   /* 640x480   8-bit color */
	GRAPH_800x500x8,   /* 800x500   8-bit color */
	GRAPH_800x600x8,   /* 800x600   8-bit color */
	GRAPH_896x672x8,   /* 896x672   8-bit color */
	GRAPH_1024x640x8,  /* 1024x640  8-bit color */
	GRAPH_1024x768x8,  /* 1024x768  8-bit color */
	GRAPH_1152x720x8,  /* 1024x720  8-bit color */
	GRAPH_1152x864x8,  /* 1152x864  8-bit color */
	GRAPH_1280x1024x8, /* 1280x1024 8-bit color */
	GRAPH_1440x900x8,  /* 1440x900  8-bit color */
	GRAPH_1600x1200x8, /* 1600x1200 8-bit color */
	/* 16-bit color (5:6:5) */
	GRAPH_320x200x16,   /* 320x200   16-bit color */
	GRAPH_640x400x16,   /* 640x400   16-bit color */
	GRAPH_640x480x16,   /* 640x480   16-bit color */
	GRAPH_800x500x16,   /* 800x500   16-bit color */
	GRAPH_800x600x16,   /* 800x600   16-bit color */
	GRAPH_896x672x16,   /* 896x672   16-bit color */
	GRAPH_1024x640x16,  /* 1024x640  16-bit color */
	GRAPH_1024x768x16,  /* 1024x768  16-bit color */
	GRAPH_1152x720x16,  /* 1152x720  16-bit color */
	GRAPH_1280x1024x16, /* 1280x1024 16-bit color */
	GRAPH_1360x768x16,  /* 1360x768  16-bit color */
	GRAPH_1440x900x16,  /* 1440x900  16-bit color */
	GRAPH_1600x1200x16, /* 1600x1200 16-bit color */
	/* 24-bit color (8:8:8) */
	GRAPH_640x480x24,  /* 640x480   24-bit color */
	GRAPH_800x600x24,  /* 800x600   24-bit color */
	GRAPH_1024x768x24, /* 1024x768  24-bit color */
	/* 32-bit color (8:8:8:8) */
	GRAPH_640x400x32,   /* 640x400   32-bit color */
	GRAPH_640x480x32,   /* 640x480   32-bit color */
	GRAPH_720x480x32,   /* 720x480   32-bit color */
	GRAPH_720x576x32,   /* 720x576   32-bit color */
	GRAPH_800x500x32,   /* 800x500   32-bit color */
	GRAPH_800x600x32,   /* 800x600   32-bit color */
	GRAPH_832x624x32,   /* 832x624   32-bit color */
	GRAPH_896x672x32,   /* 896x672   32-bit color */
	GRAPH_928x696x32,   /* 928x696   32-bit color */
	GRAPH_960x540x32,   /* 960x540   32-bit color */
	GRAPH_960x600x32,   /* 960x600   32-bit color */
	GRAPH_960x720x32,   /* 960x720   32-bit color */
	GRAPH_1024x576x32,  /* 1024x576  32-bit color */
	GRAPH_1024x640x32,  /* 1024x640  32-bit color */
	GRAPH_1024x768x32,  /* 1024x768  32-bit color */
	GRAPH_1152x720x32,  /* 1152x720  32-bit color */
	GRAPH_1152x864x32,  /* 1152x864  32-bit color */
	GRAPH_1280x720x32,  /* 1280x720  32-bit color */
	GRAPH_1280x800x32,  /* 1280x800  32-bit color */
	GRAPH_1280x960x32,  /* 1280x960  32-bit color */
	GRAPH_1280x1024x32, /* 1280x1024 32-bit color */
	GRAPH_1360x768x32,  /* 1360x768  32-bit color */
	GRAPH_1368x768x32,  /* 1368x768  32-bit color */
	GRAPH_1400x900x32,  /* 1400x900  32-bit color */
	GRAPH_1400x1050x32, /* 1400x1050 32-bit color */
	GRAPH_1440x240x32,  /* 1440x240  32-bit color */
	GRAPH_1440x288x32,  /* 1440x288  32-bit color */
	GRAPH_1440x576x32,  /* 1440x576  32-bit color */
	GRAPH_1440x810x32,  /* 1440x810  32-bit color */
	GRAPH_1440x900x32,  /* 1440x900  32-bit color */
	GRAPH_1600x900x32,  /* 1600x900  32-bit color */
	GRAPH_1600x1024x32, /* 1600x1024 32-bit color */
	GRAPH_1600x1200x32, /* 1600x1200 32-bit color */
	GRAPH_1650x750x32,  /* 1650x750  32-bit color */
	GRAPH_1680x720x32,  /* 1680x720  32-bit color */
	GRAPH_1680x1050x32, /* 1680x1050 32-bit color */
	GRAPH_1920x540x32,  /* 1920x540  32-bit color */
	GRAPH_1920x1080x32  /* 1920x1080 32-bit color */
} graph_mode_t;


/* Screen refresh rates */
typedef enum {
	GRAPH_DEFFREQ, /* Default refresh rate */
	GRAPH_24Hz,    /* 24Hz  refresh rate */
	GRAPH_30Hz,    /* 30Hz  refresh rate */
	GRAPH_43Hzi,   /* 43Hz  refresh rate interlaced */
	GRAPH_56Hz,    /* 56Hz  refresh rate */
	GRAPH_60Hz,    /* 60Hz  refresh rate */
	GRAPH_70Hz,    /* 70Hz  refresh rate */
	GRAPH_72Hz,    /* 72Hz  refresh rate */
	GRAPH_75Hz,    /* 75Hz  refresh rate */
	GRAPH_80Hz,    /* 80Hz  refresh rate */
	GRAPH_85Hz,    /* 85Hz  refresh rate */
	GRAPH_87Hzi,   /* 87Hz  refresh rate interlaced */
	GRAPH_90Hz,    /* 90Hz  refresh rate */
	GRAPH_120Hz,   /* 120Hz refresh rate */
	GRAPH_144HZ,   /* 144Hz refresh rate */
	GRAPH_165Hz,   /* 165Hz refresh rate */
	GRAPH_240Hz,   /* 240Hz refresh rate */
	GRAPH_300Hz,   /* 300Hz refresh rate */
	GRAPH_360Hz    /* 360Hz refresh rate */
} graph_freq_t;


/* Graphics task queues */
typedef enum {
	GRAPH_QUEUE_HIGH, /* High priority queue */
	GRAPH_QUEUE_LOW,  /* Low priority queue */
	GRAPH_QUEUE_BOTH, /* Both queues */
	GRAPH_QUEUE_DEFAULT = GRAPH_QUEUE_LOW
} graph_queue_t;


/* Polygon fill types */
typedef enum {
	GRAPH_FILL_FLOOD, /* Bucket flood fill */
	GRAPH_FILL_BOUND  /* Boundary fill */
} graph_fill_t;


typedef struct {
	unsigned char width;       /* Glyph width in pixels */
	unsigned char height;      /* Glyph height in pixels */
	unsigned char span;        /* Glyph row span in bytes */
	unsigned char offs;        /* First character (ASCII offset) */
	const unsigned char *data; /* Font data */
} graph_font_t;


typedef struct {
	unsigned int stop;   /* Stop counter */
	unsigned int tasks;  /* Number of tasks to process */
	unsigned char *fifo; /* Task buffer start address */
	unsigned char *end;  /* Task buffer end address */
	unsigned char *free; /* Free position */
	unsigned char *used; /* Used position */
} graph_taskq_t;


typedef struct _graph_t graph_t;


struct _graph_t {
	/* Graph info */
	void *adapter;       /* Graphics adapter */
	void *data;          /* Framebuffer */
	unsigned int width;  /* Horizontal resolution */
	unsigned int height; /* Vertical resolution */
	unsigned char depth; /* Color depth */

	/* Task queues */
	graph_taskq_t hi; /* High priority tasks queue */
	graph_taskq_t lo; /* Low priority tasks queue */

	/* Synchronization */
	handle_t lock; /* Graph mutex */

	/* Control functions */
	void (*close)(graph_t *);
	int (*mode)(graph_t *, graph_mode_t, graph_freq_t);
	int (*vsync)(graph_t *);
	int (*isbusy)(graph_t *);
	int (*trigger)(graph_t *);
	int (*commit)(graph_t *);

	/* Draw functions */
	int (*line)(graph_t *, unsigned int, unsigned int, int, int, unsigned int, unsigned int);
	int (*rect)(graph_t *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
	int (*fill)(graph_t *, unsigned int, unsigned int, unsigned int, graph_fill_t);
	int (*print)(graph_t *, unsigned int, unsigned int, unsigned char, unsigned char, const unsigned char *, unsigned char, unsigned char, unsigned char, unsigned int);

	/* Copy functions */
	int (*move)(graph_t *, unsigned int, unsigned int, unsigned int, unsigned int, int, int);
	int (*copy)(graph_t *, const void *, void *, unsigned int, unsigned int, unsigned int, unsigned int);

	/* Color palette functions */
	int (*colorset)(graph_t *, const unsigned char *, unsigned char, unsigned char);
	int (*colorget)(graph_t *, unsigned char *, unsigned char, unsigned char);

	/* Cursor functions */
	int (*cursorset)(graph_t *, const unsigned char *, const unsigned char *, unsigned int, unsigned int);
	int (*cursorpos)(graph_t *, unsigned int, unsigned int);
	int (*cursorshow)(graph_t *);
	int (*cursorhide)(graph_t *);
};


/* Draws line */
extern int graph_line(graph_t *graph, unsigned int x, unsigned int y, int dx, int dy, unsigned int stroke, unsigned int color, graph_queue_t queue);


/* Draws rectangle */
extern int graph_rect(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color, graph_queue_t queue);


/* Fills polygon */
extern int graph_fill(graph_t *graph, unsigned int x, unsigned int y, unsigned int color, graph_fill_t type, graph_queue_t queue);


/* Prints text */
extern int graph_print(graph_t *graph, const graph_font_t *font, const char *text, unsigned int x, unsigned int y, unsigned char dx, unsigned char dy, unsigned int color, graph_queue_t queue);


/* Moves data */
extern int graph_move(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my, graph_queue_t queue);


/* Copies data */
extern int graph_copy(graph_t *graph, const void *src, void *dst, unsigned int dx, unsigned int dy, unsigned int srcspan, unsigned int dstspan, graph_queue_t queue);


/* Sets color palette */
extern int graph_colorset(graph_t *graph, const unsigned char *colors, unsigned char first, unsigned char last);


/* Retrieves color palette */
extern int graph_colorget(graph_t *graph, unsigned char *colors, unsigned char first, unsigned char last);


/* Sets cursor icon */
extern int graph_cursorset(graph_t *graph, const unsigned char *amask, const unsigned char *xmask, unsigned int bg, unsigned int fg);


/* Updates cursor position */
extern int graph_cursorpos(graph_t *graph, unsigned int x, unsigned int y);


/* Enables cursor */
extern int graph_cursorshow(graph_t *graph);


/* Disables cursor */
extern int graph_cursorhide(graph_t *graph);


/* Commits framebuffer changes (flushes framebuffer) */
extern int graph_commit(graph_t *graph);


/* Triggers next task execution */
extern int graph_trigger(graph_t *graph);


/* Disables queueing up new tasks */
extern int graph_stop(graph_t *graph, graph_queue_t queue);


/* Enables queueing up new tasks */
extern int graph_start(graph_t *graph, graph_queue_t queue);


/* Returns number of tasks in queue */
extern int graph_tasks(graph_t *graph, graph_queue_t queue);


/* Resets task queue */
extern int graph_reset(graph_t *graph, graph_queue_t queue);


/* Returns number of vertical synchronizations since last call */
extern int graph_vsync(graph_t *graph);


/* Sets graphics mode */
extern int graph_mode(graph_t *graph, graph_mode_t mode, graph_freq_t freq);


/* Closes graph context */
extern void graph_close(graph_t *graph);


/* Opens graph context */
extern int graph_open(graph_t *graph, graph_adapter_t adapter, unsigned int mem);


/* Destroys graph library */
extern void graph_done(void);


/* Initializes graph library */
extern int graph_init(void);


#endif
