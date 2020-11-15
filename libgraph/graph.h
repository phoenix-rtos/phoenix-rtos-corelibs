/*
 * Graph library for DPMI32
 *
 * Copyright 2002-2007 IMMOS
 */

#ifndef _LIBGRAPH_GRAPH_H_
#define _LIBGRAPH_GRAPH_H_


#define GRAPH_VERSION   "3.0"


/* Supported modes */
#define GRAPH_640x480x8      1
#define GRAPH_800x600x8      2
#define GRAPH_1024x768x8     3
#define GRAPH_1280x1024x8    4
#define GRAPH_640x480x16     11
#define GRAPH_800x600x16     12
#define GRAPH_1024x768x16    13


/* Supported frequencies */
#define GRAPH_87Hzi 1
#define GRAPH_56Hz  2
#define GRAPH_60Hz  4
#define GRAPH_70Hz  8
#define GRAPH_72Hz  16
#define GRAPH_75Hz  32
#define GRAPH_80Hz  64


/* Error codes */
#define GRAPH_ABSENT        1
#define GRAPH_SUCCESS       0
#define GRAPH_ERR          -1
#define GRAPH_ERR_DEVICE   -2
#define GRAPH_ERR_PCI      -3
#define GRAPH_ERR_DPMI     -4
#define GRAPH_ERR_MEM      -5
#define GRAPH_ERR_STOP     -6
#define GRAPH_ERR_QUEUE    -7
#define GRAPH_ERR_ARG      -8
#define GRAPH_ERR_BUSY     -9
#define GRAPH_ERR_NESTED  -10


/* Queue parameters */
#define GRAPH_QUEUE_HIGH      0
#define GRAPH_QUEUE_LOW       1
#define GRAPH_QUEUE_BOTH     -1
#define GRAPH_QUEUE_DEFAULT  GRAPH_QUEUE_LOW


/* Task buffer size */
#define GRAPH_MIN_MEM       128


/* Arguments validation */
// #define GRAPH_VERIFY_ARG


/* Task queue descriptor */
typedef struct _graph_queue_t {
	unsigned int stop;                  // stop counter
	unsigned int tasks;                 // tasks to process
	char *fifo;                         // task buffer address
	char *free;                         // task buffer free position pointer
	char *used;                         // task buffer used position pointer
	char *end;                          // end of task buffer
} graph_queue_t;


typedef struct _graph_t {
	char busy;
	graph_queue_t low;
	graph_queue_t high;

	void *data;

	unsigned int width;
	unsigned int height;
	unsigned char depth;
	unsigned int memsz;
	unsigned int cursorsz;
	unsigned int vsync;
	
	int (*open)(struct _graph_t *graph, uint8_t irq);
	int (*close)(struct _graph_t *graph);
	int (*mode)(struct _graph_t *graph, int mode, char freq);
	int (*isbusy)(struct _graph_t *graph);
	int (*trigger)(struct _graph_t *graph);

	int (*line)(struct _graph_t *graph, int,  int,  int,  int,  unsigned int,  unsigned int);
	int (*rect)(struct _graph_t *graph, char *arg);
	int (*fill)(struct _graph_t *graph, char *arg);

	int (*move)(struct _graph_t *graph, char *arg);
	int (*copyin)(struct _graph_t *graph, char *arg);
	int (*copyto)(struct _graph_t *graph, char *arg);
	int (*copyfrom)(struct _graph_t *graph, char *arg);
	int (*copyout)(struct _graph_t *graph, char *arg);
	int (*character)(struct _graph_t *graph, char *arg);

	int (*colorset)(struct _graph_t *graph, char *colors, unsigned char first, unsigned char last);
	int (*colorget)(struct _graph_t *graph, char *colors, unsigned char first, unsigned char last);

	int (*cursorset)(struct _graph_t *graph, char *and, char *xor, unsigned char bg, unsigned char fg);
	int (*cursorpos)(struct _graph_t *graph, unsigned int x, unsigned int y);
	int (*cursorshow)(struct _graph_t *graph);
	int (*cursorhide)(struct _graph_t *graph);
} graph_t;


/* Function initializes graph library and associated drivers */
extern int graph_init(void);


/* Function prepares library for use */
extern int graph_open(unsigned int mem, uint8_t irq);


/* Function closes library */
extern int graph_close(void);


/* Function sets graphics modes */
extern int graph_mode(unsigned int mode, char freq);


/* Function returns number of vertical synchronizations since last call */
extern int graph_vsync(void);


/* Function stops graphics task queue processing */
extern int graph_stop(int queue);


/* Function starts graphics task queue processing */
extern int graph_start(int queue);


/* Function returns number of tasks in queue */
extern int graph_tasks(int queue);


/* Function resets task queue */
extern int graph_reset(int queue);


/* Function triggers next task execution */
extern int graph_trigger(void);


extern int graph_line(unsigned int x, unsigned int y, int dx, int dy, unsigned int width, unsigned int color, int queue);

extern int graph_fill(unsigned int x, unsigned int y, unsigned int color, int queue);

extern int graph_rect(unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color, int queue);

extern int graph_move(unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my, int queue);

extern int graph_copyin(unsigned int src, unsigned int dst, unsigned int dx, unsigned int dy, int srcspan, int dstspan, int queue);

extern int graph_copyto(char *src, unsigned int dst, unsigned int dx, unsigned int dy, int srcspan, int dstspan, int queue);

extern int graph_copyfrom(unsigned int src, char *dst, unsigned int dx, unsigned int dy, int srcspan, int dstspan, int queue);

extern int graph_copyout(char *src, char *dst, unsigned int dx, unsigned int dy, int srcspan, int dstspan, int queue);

extern int graph_print(char *text, unsigned int x, unsigned int y, char *fonts, unsigned int dx, unsigned int dy, unsigned int color, int queue);


extern int graph_colorset(char *colors, unsigned char first, unsigned char last);

extern int graph_colorget(char *colors, unsigned char first, unsigned char last);


extern int graph_cursorset(char *and, char *xor, unsigned char bg, unsigned char fg);

extern int graph_cursorpos(unsigned int x, unsigned int y);

extern int graph_cursorshow(void);

extern int graph_cursorhide(void);


extern char *graph_version(void);

extern int graph_addr(unsigned int x, unsigned int y);

extern int graph_span(void);

/* Function returns free frame buffer memory offset */
extern int graph_offset(void);

/* Function returns size of free frame buffer memory */
extern int graph_size(void);

/* Function calculates text width in pixels */
extern int graph_width(char *text, char *fonts, unsigned int dx);


#endif
