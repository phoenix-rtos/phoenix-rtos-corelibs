/*
 * Phoenix-RTOS
 *
 * Graph library
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


/* Generic graphics adapters */
#define GRAPH_NONE       0
#define GRAPH_ANY       -1
#define GRAPH_VGADEV    (1 << 0)
#define GRAPH_VIRTIOGPU (1 << 1)


/* Graphics adapters supported on IA32 platform */
#ifdef TARGET_IA32
// #define GRAPH_CIRRUS    (1 << 2)
// #define GRAPH_CT69000   (1 << 3)
// #define GRAPH_SAVAGE4   (1 << 4)
// #define GRAPH_GEODELX   (1 << 5)
#endif


/* Graphics modes */
typedef enum {
	/* Control modes */
	GRAPH_NOMODE,
	GRAPH_ON,
	GRAPH_OFF,
	GRAPH_STANDBY,
	GRAPH_SUSPEND,
	/* 1-byte color */
	GRAPH_320x200x8,
	GRAPH_640x400x8,
	GRAPH_640x480x8,
	GRAPH_800x600x8,
	GRAPH_1024x768x8,
	GRAPH_1152x864x8,
	GRAPH_1280x1024x8,
	GRAPH_1600x1200x8,
	/* 2-byte color */
	GRAPH_640x480x16,
	GRAPH_800x600x16,
	GRAPH_1024x768x16,
	GRAPH_1280x1024x16,
	/* 4-byte color */
	GRAPH_640x480x32,
	GRAPH_720x480x32,
	GRAPH_720x576x32,
	GRAPH_800x600x32,
	GRAPH_832x624x32,
	GRAPH_896x672x32,
	GRAPH_928x696x32,
	GRAPH_960x540x32,
	GRAPH_960x600x32,
	GRAPH_960x720x32,
	GRAPH_1024x576x32,
	GRAPH_1024x768x32,
	GRAPH_1152x864x32,
	GRAPH_1280x720x32,
	GRAPH_1280x800x32,
	GRAPH_1280x960x32,
	GRAPH_1280x1024x32,
	GRAPH_1360x768x32,
	GRAPH_1368x768x32,
	GRAPH_1400x900x32,
	GRAPH_1400x1050x32,
	GRAPH_1440x240x32,
	GRAPH_1440x288x32,
	GRAPH_1440x576x32,
	GRAPH_1440x810x32,
	GRAPH_1440x900x32,
	GRAPH_1600x900x32,
	GRAPH_1600x1024x32,
	GRAPH_1650x750x32,
	GRAPH_1680x720x32,
	GRAPH_1680x1050x32,
	GRAPH_1920x540x32,
	GRAPH_1920x1080x32
} graph_mode_t;


/* Screen refresh rates */
typedef enum {
	GRAPH_NOFREQ,
	GRAPH_24Hz,
	GRAPH_30Hz,
	GRAPH_43Hz,
	GRAPH_56Hz,
	GRAPH_60Hz,
	GRAPH_70Hz,
	GRAPH_72Hz,
	GRAPH_75Hz,
	GRAPH_80Hz,
	GRAPH_85Hz,
	GRAPH_87Hz,
	GRAPH_90Hz,
	GRAPH_120Hz,
	GRAPH_144HZ,
	GRAPH_165Hz,
	GRAPH_240Hz,
	GRAPH_300Hz,
	GRAPH_360Hz
} graph_freq_t;


/* Graph queues */
typedef enum {
	GRAPH_QUEUE_HIGH,
	GRAPH_QUEUE_LOW,
	GRAPH_QUEUE_BOTH,
	GRAPH_QUEUE_DEFAULT = GRAPH_QUEUE_LOW
} graph_queue_t;


/* Graph fill type */
typedef enum {
	GRAPH_FILL_FLOOD,
	GRAPH_FILL_BOUND
} graph_fill_t;


typedef struct {
	unsigned char width;       /* Glyph width in pixels */
	unsigned char height;      /* Glyph height in pixels */
	unsigned char span;        /* Glyph row span in bytes */
	unsigned char offs;        /* First character (ASCII offset) */
	const unsigned char *data; /* Font bitmap */
} graph_font_t;


typedef struct {
	unsigned int stop;         /* Stop counter */
	unsigned int tasks;        /* Number of tasks to process */
	unsigned char *fifo;       /* Task buffer start address */
	unsigned char *end;        /* Task buffer end address */
	unsigned char *free;       /* Free position */
	unsigned char *used;       /* Used position */
} graph_taskq_t;


typedef struct _graph_t graph_t;


struct _graph_t {
	/* Graph info */
	void *adapter;             /* Graphics adapter */

	/* Screen info */
	void *data;                /* Framebuffer */
	unsigned int width;        /* Screen width */
	unsigned int height;       /* Screen height */
	unsigned char depth;       /* Screen color depth */

	/* Task queues */
	graph_taskq_t hi;          /* High priority tasks queue */
	graph_taskq_t lo;          /* Low priority tasks queue */

	/* Synchronization */
	handle_t lock;             /* Graph synchronization mutex */

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
	int (*colorset)(graph_t *, const unsigned char *, unsigned int, unsigned int);
	int (*colorget)(graph_t *, unsigned char *, unsigned int, unsigned int);

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
extern int graph_colorset(graph_t *graph, const unsigned char *colors, unsigned int first, unsigned int last);


/* Retrieves color palette */
extern int graph_colorget(graph_t *graph, unsigned char *colors, unsigned int first, unsigned int last);


/* Sets cursor icon */
extern int graph_cursorset(graph_t *graph, const unsigned char *and, const unsigned char *xor, unsigned int bg, unsigned int fg);


/* Updates cursor position */
extern int graph_cursorpos(graph_t *graph, unsigned int x, unsigned int y);


/* Enables cursor */
extern int graph_cursorshow(graph_t *graph);


/* Disables cursor */
extern int graph_cursorhide(graph_t *graph);


/* Commits framebuffer changes (flushes framebuffer to screen) */
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
extern int graph_open(graph_t *graph, unsigned int mem, unsigned int adapter);


/* Destroys graph library */
extern void graph_done(void);


/* Initializes graph library */
extern int graph_init(void);


#endif
