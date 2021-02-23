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


/* Graphics adapters */
// #define GRAPH_CT69000   (1 << 0)
// #define GRAPH_SAVAGE4   (1 << 1)
// #define GRAPH_GEODELX   (1 << 2)
#define GRAPH_VIRTIOGPU (1 << 3)
#define GRAPH_ANY       -1


/* Check graphics functions arguments */
// #define GRAPH_VERIFY_ARGS


/* Graphics modes */
enum {
	GRAPH_640x480x8    = 0,
	GRAPH_800x600x8    = 1,
	GRAPH_1024x768x8   = 2,
	GRAPH_1280x1024x8  = 3,
	GRAPH_640x480x16   = 100,
	GRAPH_800x600x16   = 101,
	GRAPH_1024x768x16  = 102,
	GRAPH_640x480x32   = 200,
	GRAPH_800x600x32   = 201,
	GRAPH_832x624x32   = 202,
	GRAPH_896x672x32   = 203,
	GRAPH_928x696x32   = 204,
	GRAPH_960x540x32   = 205,
	GRAPH_960x600x32   = 206,
	GRAPH_960x720x32   = 207,
	GRAPH_1024x576x32  = 208,
	GRAPH_1024x768x32  = 209,
	GRAPH_1152x864x32  = 210,
	GRAPH_1280x720x32  = 211,
	GRAPH_1280x800x32  = 212,
	GRAPH_1280x960x32  = 213,
	GRAPH_1280x1024x32 = 214,
	GRAPH_1360x768x32  = 215,
	GRAPH_1368x768x32  = 216,
	GRAPH_1400x900x32  = 217,
	GRAPH_1400x1050x32 = 218,
	GRAPH_1440x810x32  = 219,
	GRAPH_1440x900x32  = 220,
	GRAPH_1600x900x32  = 221,
	GRAPH_1600x1024x32 = 222,
	GRAPH_1680x1050x32 = 223,
	GRAPH_1920x1080x32 = 224
};


/* Screen refresh rates */
enum {
	GRAPH_87Hzi = (1 << 0),
	GRAPH_56Hz  = (1 << 1),
	GRAPH_60Hz  = (1 << 2),
	GRAPH_70Hz  = (1 << 3),
	GRAPH_72Hz  = (1 << 4),
	GRAPH_75Hz  = (1 << 5),
	GRAPH_80Hz  = (1 << 6)
};


/* Graphics task queues */
enum {
	GRAPH_QUEUE_HI,
	GRAPH_QUEUE_LO,
	GRAPH_QUEUE_BOTH,
	GRAPH_QUEUE_DEFAULT = GRAPH_QUEUE_LO
};


/* Polygon fill type */
enum {
	FILL_FLOOD,
	FILL_BOUND
};


typedef struct {
	unsigned char width;  /* Glyph width in pixels */
	unsigned char height; /* Glyph height in pixels */
	unsigned char span;   /* Glyph row span in bytes */
	unsigned char offs;   /* First character (ASCII offset) */
	unsigned char *data;  /* Font data */
} graph_font_t;


typedef struct {
	unsigned int tasks;  /* Number of tasks to process */
	unsigned int stop;   /* Stop counter */
	unsigned char *fifo; /* Task buffer start address */
	unsigned char *free; /* Free position */
	unsigned char *used; /* Used position */
	unsigned char *end;  /* Task buffer end address */
} graph_queue_t;


typedef struct _graph_t graph_t;


struct _graph_t {
	/* Graph info */
	unsigned char busy;  /* Is graph busy? */
	void *adapter;       /* Graphics adapter */

	/* Screen info */
	void *data;          /* Screen buffer */
	unsigned int width;  /* Screen width */
	unsigned int height; /* Screen height */
	unsigned int vsync;  /* Screen vertical synchronizations */
	unsigned char depth; /* Screen color depth */

	/* Task queues */
	graph_queue_t hi;    /* High priority tasks queue */
	graph_queue_t lo;    /* Low priority tasks queue */

	/* Control functions */
	int (*close)(graph_t *graph);
	int (*mode)(graph_t *graph, int mode, char freq);
	int (*isbusy)(graph_t *graph);
	int (*trigger)(graph_t *graph);

	/* Draw functions */
	int (*line)(graph_t *graph, unsigned int, unsigned int, int, int, unsigned int, unsigned int);
	int (*rect)(graph_t *graph, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
	int (*fill)(graph_t *graph, unsigned int, unsigned int, unsigned int, unsigned char);
	int (*print)(graph_t *graph, unsigned int, unsigned int, unsigned char, unsigned char, unsigned char *, unsigned char, unsigned char, unsigned char, unsigned int);

	/* Copy functions */
	int (*move)(graph_t *graph, unsigned int, unsigned int, unsigned int, unsigned int, int, int);
	int (*copy)(graph_t *graph, void *, void *, unsigned int, unsigned int, unsigned int, unsigned int);

	/* Color palette functions */
	int (*colorset)(graph_t *graph, char *colors, unsigned char first, unsigned char last);
	int (*colorget)(graph_t *graph, char *colors, unsigned char first, unsigned char last);

	/* Cursor functions */
	int (*cursorset)(graph_t *graph, char *and, char *xor, unsigned char bg, unsigned char fg);
	int (*cursorpos)(graph_t *graph, unsigned int x, unsigned int y);
	int (*cursorshow)(graph_t *graph);
	int (*cursorhide)(graph_t *graph);
};


/* Draws line */
extern int graph_line(graph_t *graph, unsigned int x, unsigned int y, int dx, int dy, unsigned int stroke, unsigned int color, unsigned char queue);


/* Draws rectangle */
extern int graph_rect(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color, unsigned char queue);


/* Fills polygon */
extern int graph_fill(graph_t *graph, unsigned int x, unsigned int y, unsigned int color, unsigned char type, unsigned char queue);


/* Prints text */
extern int graph_print(graph_t *graph, graph_font_t *font, const char *text, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color, unsigned char queue);


/* Moves data */
extern int graph_move(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my, unsigned char queue);


/* Copies data */
extern int graph_copy(graph_t *graph, void *src, void *dst, unsigned int dx, unsigned int dy, unsigned int srcspan, unsigned int dstspan, unsigned char queue);


/* Sets color palette */
extern int graph_colorset(graph_t *graph, char *colors, unsigned char first, unsigned char last);


/* Retrieves color palette */
extern int graph_colorget(graph_t *graph, char *colors, unsigned char first, unsigned char last);


/* Sets cursor icon */
extern int graph_cursorset(graph_t *graph, char *and, char *xor, unsigned char bg, unsigned char fg);


/* Updates cursor position */
extern int graph_cursorpos(graph_t *graph, unsigned int x, unsigned int y);


/* Enables cursor */
extern int graph_cursorshow(graph_t *graph);


/* Disables cursor */
extern int graph_cursorhide(graph_t *graph);


/* Triggers next task execution */
extern int graph_trigger(graph_t *graph);


/* Stops graphics task queue processing */
extern int graph_stop(graph_t *graph, unsigned int queue);


/* Starts graphics task queue processing */
extern int graph_start(graph_t *graph, unsigned int queue);


/* Returns number of tasks in queue */
extern int graph_tasks(graph_t *graph, unsigned int queue);


/* Resets task queue */
extern int graph_reset(graph_t *graph, unsigned int queue);


/* Returns number of vertical synchronizations since last call */
extern int graph_vsync(graph_t *graph);


/* Sets graphics mode */
extern int graph_mode(graph_t *graph, unsigned int mode, unsigned int freq);


/* Closes graphics adapter context */
extern void graph_close(graph_t *graph);


/* Opens graphics adapter context */
extern int graph_open(graph_t *graph, unsigned int mem, unsigned int adapter);


/* Destroys graph library */
extern void graph_done(void);


/* Initializes graph library */
extern int graph_init(void);


#endif
