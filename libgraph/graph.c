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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "soft.h"


/* Graphics tasks */
enum {
	TASK_LINE,
	TASK_RECT,
	TASK_FILL,
	TASK_PRINT,
	TASK_MOVE,
	TASK_COPY
};


typedef struct {
	unsigned int size; /* Task size */
	unsigned int type; /* Task type */

	union {
		struct {
			unsigned int x;
			unsigned int y;
			int dx;
			int dy;
			unsigned int stroke;
			unsigned int color;
		} line;

		struct {
			unsigned int x;
			unsigned int y;
			unsigned int dx;
			unsigned int dy;
			unsigned int color;
		} rect;

		struct {
			unsigned int x;
			unsigned int y;
			unsigned int color;
			unsigned char type;
		} fill;

		struct {
			unsigned int x;
			unsigned int y;
			unsigned char dx;
			unsigned char dy;
			unsigned char *bmp;
			unsigned char width;
			unsigned char height;
			unsigned char span;
			unsigned int color;
		} print;

		struct {
			unsigned int x;
			unsigned int y;
			unsigned int dx;
			unsigned int dy;
			int mx;
			int my;
		} move;

		struct {
			void *src;
			void *dst;
			unsigned int dx;
			unsigned int dy;
			unsigned int srcspan;
			unsigned int dstspan;
		} copy;
	};
} __attribute__((packed)) graph_task_t;


#ifdef GRAPH_CT69000
extern int ct69000_open(graph_t *);
extern void ct69000_done(void);
extern int ct69000_init(void);
#endif


#ifdef GRAPH_SAVAGE4
extern int savage4_open(graph_t *);
extern void savage4_done(void);
extern int savage4_init(void);
#endif


#ifdef GRAPH_GEODELX
extern int geode_open(graph_t *);
extern void geode_done(void);
extern int geode_init(void);
#endif


#ifdef GRAPH_VIRTIOGPU
extern int virtiogpu_open(graph_t *);
extern void virtiogpu_done(void);
extern int virtiogpu_init(void);
#endif


/* Executes task */
static int graph_exec(graph_t *graph, graph_task_t *task)
{
	switch (task->type) {
	case TASK_LINE:
		return graph->line(graph, task->line.x, task->line.y, task->line.dx, task->line.dy, task->line.stroke, task->line.color);

	case TASK_RECT:
		return graph->rect(graph, task->rect.x, task->rect.y, task->rect.dx, task->rect.dy, task->rect.color);

	case TASK_FILL:
		return graph->fill(graph, task->fill.x, task->fill.y, task->fill.color, task->fill.type);

	case TASK_PRINT:
		return graph->print(graph, task->print.x, task->print.y, task->print.dx, task->print.dy, task->print.bmp, task->print.width, task->print.height, task->print.span, task->print.color);

	case TASK_MOVE:
		return graph->move(graph, task->move.x, task->move.y, task->move.dx, task->move.dy, task->move.mx, task->move.my);

	case TASK_COPY:
		return graph->copy(graph, task->copy.src, task->copy.dst, task->copy.dx, task->copy.dy, task->copy.srcspan, task->copy.dstspan);

	default:
		return -EINVAL;
	}
}


int graph_schedule(graph_t *graph)
{
	graph_task_t *task;
	graph_queue_t *q;

	if (graph->busy)
		return -EBUSY;

	graph->busy = 1;
	while (!graph->isbusy(graph)) {
		q = &graph->hi;

		if (!q->tasks) {
			q = &graph->lo;

			if (!q->tasks) {
				graph->busy = 0;
				return EOK;
			}
		}

		if (!(((graph_task_t *)q->used)->size))
			q->used = q->fifo;

		task = (graph_task_t *)q->used;
		graph_exec(graph, task);
		q->used += task->size;
		q->tasks--;
	}
	graph->busy = 0;

	return -EBUSY;
}


/* Queues up task for execution */
static int graph_queue(graph_t *graph, graph_task_t *task, unsigned char queue)
{
	graph_queue_t *q;

	switch (queue) {
	case GRAPH_QUEUE_LO:
		q = &graph->lo;
		break;

	case GRAPH_QUEUE_HI:
		q = &graph->hi;
		break;

	default:
		return -EINVAL;
	}

	if (q->stop)
		return -EFAULT;

	if (graph->isbusy(graph) || graph->busy || graph->hi.tasks || q->tasks) {
		if (q->free < q->used) {
			if (q->free + task->size > q->used)
				return -ENOSPC;
		}
		else if (q->free + task->size + sizeof(task->size) > q->end) {
			if (q->fifo + task->size > q->used)
				return -ENOSPC;

			((graph_task_t *)q->free)->size = 0;
			q->free = q->fifo;
		}

		memcpy(q->free, task, task->size);
		q->free += task->size;
		q->tasks++;

		/* Try to reschedule */
		graph_schedule(graph);

		return EOK;
	}

	/* Execute task */
	graph->busy = 1;
	graph_exec(graph, task);
	graph->busy = 0;

	/* Try to reschedule */
	graph_schedule(graph);

	return EOK;
}


int graph_line(graph_t *graph, unsigned int x, unsigned int y, int dx, int dy, unsigned int stroke, unsigned int color, unsigned char queue)
{
	graph_task_t task = {
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.line),
		.type = TASK_LINE,
		.line = { x, y, dx, dy, stroke, color }
	};

	return graph_queue(graph, &task, queue);
}


int graph_rect(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color, unsigned char queue)
{
	graph_task_t task = {
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.rect),
		.type = TASK_RECT,
		.rect = { x, y, dx, dy, color }
	};

	return graph_queue(graph, &task, queue);
}


int graph_fill(graph_t *graph, unsigned int x, unsigned int y, unsigned int color, unsigned char type, unsigned char queue)
{
	graph_task_t task = {
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.fill),
		.type = TASK_FILL,
		.fill = { x, y, color, type }
	};

	return graph_queue(graph, &task, queue);
}


int graph_print(graph_t *graph, graph_font_t *font, const char *text, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color, unsigned char queue)
{
	graph_task_t task = {
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.print),
		.type = TASK_PRINT,
		.print = { x, y, dx / font->width, dy, NULL, font->width, font->height, font->span, color }
	};
	int err;

	for (; *text; text++, task.print.x += task.print.dx) {
		task.print.bmp = font->data + font->height * font->span * (*(unsigned char *)text - font->offs);
		if ((err = graph_queue(graph, &task, queue)) < 0)
			return err;
	}

	return EOK;
}


int graph_move(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my, unsigned char queue)
{
	graph_task_t task = {
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.move),
		.type = TASK_MOVE,
		.move = { x, y, dx, dy, mx, my }
	};

	return graph_queue(graph, &task, queue);
}


int graph_copy(graph_t *graph, void *src, void *dst, unsigned int dx, unsigned int dy, unsigned int srcspan, unsigned int dstspan, unsigned char queue)
{
	graph_task_t task = {
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.copy),
		.type = TASK_COPY,
		.copy = { src, dst, dx, dy, srcspan, dstspan }
	};

	return graph_queue(graph, &task, queue);
}


int graph_colorset(graph_t *graph, char *colors, unsigned char first, unsigned char last)
{
	return graph->colorset(graph, colors, first, last);
}


int graph_colorget(graph_t *graph, char *colors, unsigned char first, unsigned char last)
{
	return graph->colorget(graph, colors, first, last);
}


int graph_cursorset(graph_t *graph, char *and, char *xor, unsigned char bg, unsigned char fg)
{
	return graph->cursorset(graph, and, xor, bg, fg);
}


int graph_cursorpos(graph_t *graph, unsigned int x, unsigned int y)
{
	return graph->cursorpos(graph, x, y);
}


int graph_cursorshow(graph_t *graph)
{
	return graph->cursorshow(graph);
}


int graph_cursorhide(graph_t *graph)
{
	return graph->cursorhide(graph);
}


int graph_trigger(graph_t *graph)
{
	return graph->trigger(graph);
}


int graph_stop(graph_t *graph, unsigned int queue)
{
	if (queue != GRAPH_QUEUE_LO)
		graph->hi.stop++;

	if (queue != GRAPH_QUEUE_HI)
		graph->lo.stop++;

	return EOK;
}


int graph_start(graph_t *graph, unsigned int queue)
{
	if (queue != GRAPH_QUEUE_LO) {
		if (graph->hi.stop)
			graph->hi.stop--;
	}

	if (queue != GRAPH_QUEUE_HI) {
		if (graph->lo.stop)
			graph->lo.stop--;
	}

	return EOK;
}


int graph_tasks(graph_t *graph, unsigned int queue)
{
	int ret = 0;

	if (queue != GRAPH_QUEUE_LO)
		ret += graph->hi.tasks;

	if (queue != GRAPH_QUEUE_HI)
		ret += graph->lo.tasks;

	return ret;
}


int graph_reset(graph_t *graph, unsigned int queue)
{
	if (graph->busy)
		return -EBUSY;

	if (queue != GRAPH_QUEUE_LO) {
		graph->hi.stop = 0;
		graph->hi.tasks = 0;
		graph->hi.free = graph->hi.fifo;
		graph->hi.used = graph->hi.fifo;
	}

	if (queue != GRAPH_QUEUE_HI) {
		graph->lo.stop = 0;
		graph->lo.tasks = 0;
		graph->lo.free = graph->lo.fifo;
		graph->lo.used = graph->lo.fifo;
	}

	return EOK;
}


int graph_vsync(graph_t *graph)
{
	int ret;

	ret = graph->vsync;
	graph->vsync = 0;

	return ret;
}


int graph_mode(graph_t *graph, unsigned int mode, unsigned int freq)
{
	if (graph->busy || graph->isbusy(graph) || graph->hi.tasks || graph->lo.tasks)
		return -EBUSY;

	return graph->mode(graph, mode, freq);
}


void graph_close(graph_t *graph)
{
	graph->close(graph);
	free(graph->hi.fifo);
}


int graph_open(graph_t *graph, unsigned int mem, unsigned int adapter)
{
	unsigned int himem, lomem;
	int err;

	/* Check min queue size */
	if ((himem = lomem = mem >> 1) < 2 * sizeof(graph_task_t) + sizeof(unsigned int))
		return -EINVAL;

	graph->lo.fifo = NULL;
	graph->hi.fifo = NULL;

	if ((graph->hi.fifo = malloc(himem + lomem)) == NULL)
		return -ENOMEM;

	/* Initialize high piority task FIFO */
	graph->hi.tasks = 0;
	graph->hi.stop = 0;
	graph->hi.free = graph->hi.fifo;
	graph->hi.used = graph->hi.fifo;
	graph->hi.end = graph->hi.fifo + himem;

	/* Initialize low priority task FIFO */
	graph->lo.tasks = 0;
	graph->lo.stop = 0;
	graph->lo.fifo = graph->hi.end;
	graph->lo.free = graph->lo.fifo;
	graph->lo.used = graph->lo.fifo;
	graph->lo.end = graph->lo.fifo + lomem;

	/* Set default graphics tasks functions */
	graph->line = soft_line;
	graph->rect = soft_rect;
	graph->fill = soft_fill;
	graph->print = soft_print;
	graph->move = soft_move;
	graph->copy = soft_copy;

	/* Initialize graphics adapter context */
	do {
#ifdef GRAPH_CT69000
		if ((adapter & GRAPH_CT69000) && ((err = ct69000_open(graph)) != -ENODEV))
			break;
#endif

#ifdef GRAPH_SAVAGE4
		if ((adapter & GRAPH_SAVAGE4) && ((err = savage4_open(graph)) != -ENODEV))
			break;
#endif

#ifdef GRAPH_GEODELX
		if ((adapter & GRAPH_GEODELX) && ((err = geode_open(graph)) != -ENODEV))
			break;
#endif

#ifdef GRAPH_VIRTIOGPU
		if ((adapter & GRAPH_VIRTIOGPU) && ((err = virtiogpu_open(graph)) != -ENODEV))
			break;
#endif
	} while (0);

	if (err < 0)
		free(graph->hi.fifo);

	return err;
}


void graph_done(void)
{
#ifdef GRAPH_CT69000
	ct69000_done();
#endif

#ifdef GRAPH_SAVAGE4
	savage4_done();
#endif

#ifdef GRAPH_GEODELX
	geode_done();
#endif

#ifdef GRAPH_VIRTIOGPU
	virtiogpu_done();
#endif
}


int graph_init(void)
{
	int err;

#ifdef GRAPH_CT69000
	if ((err = ct69000_init()) < 0)
		return err;
#endif

#ifdef GRAPH_SAVAGE4
	if ((err = savage4_init()) < 0)
		return err;
#endif

#ifdef GRAPH_GEODELX
	if ((err = geode_init()) < 0)
		return err;
#endif

#ifdef GRAPH_VIRTIOGPU
	if ((err = virtiogpu_init()) < 0)
		return err;
#endif

	return EOK;
}
