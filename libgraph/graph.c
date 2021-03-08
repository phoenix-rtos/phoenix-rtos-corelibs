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

#include <sys/threads.h>

#include "graph.h"
#include "soft.h"


/* Graph tasks */
enum {
	GRAPH_LINE,
	GRAPH_RECT,
	GRAPH_FILL,
	GRAPH_PRINT,
	GRAPH_MOVE,
	GRAPH_COPY
};


typedef struct {
	unsigned int size;
	unsigned int type;

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
			graph_fill_t type;
		} fill;

		struct {
			unsigned int x;
			unsigned int y;
			unsigned char dx;
			unsigned char dy;
			const unsigned char *bmp;
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
			const void *src;
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
static int _graph_exec(graph_t *graph, graph_task_t *task)
{
	switch (task->type) {
	case GRAPH_LINE:
		return graph->line(graph, task->line.x, task->line.y, task->line.dx, task->line.dy, task->line.stroke, task->line.color);

	case GRAPH_RECT:
		return graph->rect(graph, task->rect.x, task->rect.y, task->rect.dx, task->rect.dy, task->rect.color);

	case GRAPH_FILL:
		return graph->fill(graph, task->fill.x, task->fill.y, task->fill.color, task->fill.type);

	case GRAPH_PRINT:
		return graph->print(graph, task->print.x, task->print.y, task->print.dx, task->print.dy, task->print.bmp, task->print.width, task->print.height, task->print.span, task->print.color);

	case GRAPH_MOVE:
		return graph->move(graph, task->move.x, task->move.y, task->move.dx, task->move.dy, task->move.mx, task->move.my);

	case GRAPH_COPY:
		return graph->copy(graph, task->copy.src, task->copy.dst, task->copy.dx, task->copy.dy, task->copy.srcspan, task->copy.dstspan);

	default:
		return -EINVAL;
	}
}


/* Schedules and executes tasks */
static int _graph_schedule(graph_t *graph)
{
	graph_task_t *task;
	graph_taskq_t *q;

	while (!graph->isbusy(graph)) {
		q = &graph->hi;
		if (!q->tasks) {
			q = &graph->lo;
			if (!q->tasks)
				return EOK;
		}

		/* Wrap buffer */
		if (!(((graph_task_t *)q->used)->size))
			q->used = q->fifo;

		task = (graph_task_t *)q->used;
		_graph_exec(graph, task);
		q->used += task->size;
		q->tasks--;
	}

	return -EBUSY;
}


int graph_schedule(graph_t *graph)
{
	int ret;

	if (mutexTry(graph->lock) < 0)
		return -EAGAIN;

	ret = _graph_schedule(graph);

	mutexUnlock(graph->lock);

	return ret;
}


/* Queues up task for execution */
static int graph_queue(graph_t *graph, graph_task_t *task, graph_queue_t queue)
{
	graph_taskq_t *q;

	switch (queue) {
	case GRAPH_QUEUE_LOW:
		q = &graph->lo;
		break;

	case GRAPH_QUEUE_HIGH:
		q = &graph->hi;
		break;

	default:
		return -EINVAL;
	}

	if (q->stop)
		return -EACCES;

	mutexLock(graph->lock);

	/* Queue up task */
	if (graph->isbusy(graph) || graph->hi.tasks || q->tasks) {
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
	}
	/* Execute task */
	else {
		_graph_exec(graph, task);
	}

	/* Try to reschedule */
	_graph_schedule(graph);

	mutexUnlock(graph->lock);

	return EOK;
}


int graph_line(graph_t *graph, unsigned int x, unsigned int y, int dx, int dy, unsigned int stroke, unsigned int color, graph_queue_t queue)
{
	graph_task_t task = {
		.type = GRAPH_LINE,
		.line = { x, y, dx, dy, stroke, color },
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.line)
	};

	return graph_queue(graph, &task, queue);
}


int graph_rect(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color, graph_queue_t queue)
{
	graph_task_t task = {
		.type = GRAPH_RECT,
		.rect = { x, y, dx, dy, color },
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.rect)
	};

	return graph_queue(graph, &task, queue);
}


int graph_fill(graph_t *graph, unsigned int x, unsigned int y, unsigned int color, graph_fill_t type, graph_queue_t queue)
{
	graph_task_t task = {
		.type = GRAPH_FILL,
		.fill = { x, y, color, type },
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.fill)
	};

	return graph_queue(graph, &task, queue);
}


int graph_print(graph_t *graph, const graph_font_t *font, const char *text, unsigned int x, unsigned int y, unsigned char dx, unsigned char dy, unsigned int color, graph_queue_t queue)
{
	graph_task_t task = {
		.type = GRAPH_PRINT,
		.print = { x, y, (unsigned int)dx * font->width / font->height, dy, NULL, font->width, font->height, font->span, color },
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.print)
	};
	int err;

	for (; *text; text++, task.print.x += task.print.dx) {
		task.print.bmp = font->data + (unsigned int)font->height * font->span * (*(unsigned char *)text - font->offs);
		if ((err = graph_queue(graph, &task, queue)) < 0)
			return err;
	}

	return EOK;
}


int graph_move(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my, graph_queue_t queue)
{
	graph_task_t task = {
		.type = GRAPH_MOVE,
		.move = { x, y, dx, dy, mx, my },
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.move)
	};

	return graph_queue(graph, &task, queue);
}


int graph_copy(graph_t *graph, const void *src, void *dst, unsigned int dx, unsigned int dy, unsigned int srcspan, unsigned int dstspan, graph_queue_t queue)
{
	graph_task_t task = {
		.type = GRAPH_COPY,
		.copy = { src, dst, dx, dy, srcspan, dstspan },
		.size = sizeof(task.size) + sizeof(task.type) + sizeof(task.copy)
	};

	return graph_queue(graph, &task, queue);
}


int graph_colorset(graph_t *graph, const unsigned char *colors, unsigned int first, unsigned int last)
{
	return graph->colorset(graph, colors, first, last);
}


int graph_colorget(graph_t *graph, unsigned char *colors, unsigned int first, unsigned int last)
{
	return graph->colorget(graph, colors, first, last);
}


int graph_cursorset(graph_t *graph, const unsigned char *and, const unsigned char *xor, unsigned int bg, unsigned int fg)
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


int graph_commit(graph_t *graph)
{
	return graph->commit(graph);
}


int graph_trigger(graph_t *graph)
{
	return graph->trigger(graph);
}


int graph_stop(graph_t *graph, graph_queue_t queue)
{
	if (queue != GRAPH_QUEUE_LOW)
		graph->hi.stop++;

	if (queue != GRAPH_QUEUE_HIGH)
		graph->lo.stop++;

	return EOK;
}


int graph_start(graph_t *graph, graph_queue_t queue)
{
	if (queue != GRAPH_QUEUE_LOW) {
		if (graph->hi.stop)
			graph->hi.stop--;
	}

	if (queue != GRAPH_QUEUE_HIGH) {
		if (graph->lo.stop)
			graph->lo.stop--;
	}

	return EOK;
}


int graph_tasks(graph_t *graph, graph_queue_t queue)
{
	int ret = 0;

	if (queue != GRAPH_QUEUE_LOW)
		ret += *(volatile unsigned int *)&graph->hi.tasks;

	if (queue != GRAPH_QUEUE_HIGH)
		ret += *(volatile unsigned int *)&graph->lo.tasks;

	return ret;
}


int graph_reset(graph_t *graph, graph_queue_t queue)
{
	mutexLock(graph->lock);

	if (queue != GRAPH_QUEUE_LOW) {
		graph->hi.free = graph->hi.fifo;
		graph->hi.used = graph->hi.fifo;
		graph->hi.tasks = 0;
		graph->hi.stop = 0;
	}

	if (queue != GRAPH_QUEUE_HIGH) {
		graph->lo.free = graph->lo.fifo;
		graph->lo.used = graph->lo.fifo;
		graph->lo.tasks = 0;
		graph->lo.stop = 0;
	}

	mutexUnlock(graph->lock);

	return EOK;
}


int graph_vsync(graph_t *graph)
{
	return graph->vsync(graph);
}


int graph_mode(graph_t *graph, graph_mode_t mode, graph_freq_t freq)
{
	graph_reset(graph, GRAPH_QUEUE_BOTH);
	while (graph->isbusy(graph));

	return graph->mode(graph, mode, freq);
}


void graph_close(graph_t *graph)
{
	graph->close(graph);
	resourceDestroy(graph->lock);
	free(graph->hi.fifo);
}


int graph_open(graph_t *graph, unsigned int mem, unsigned int adapter)
{
	unsigned int himem, lomem;
	int err;

	/* Check min queue size (space for 2 tasks and queue wrap value) */
	if ((himem = lomem = mem >> 1) < (sizeof(graph_task_t) << 1) + sizeof(unsigned int))
		return -EINVAL;

	if ((graph->hi.fifo = malloc(himem + lomem)) == NULL)
		return -ENOMEM;

	if ((err = mutexCreate(&graph->lock)) < 0) {
		free(graph->hi.fifo);
		return err;
	}

	/* Initialize high piority tasks queue */
	graph->hi.end = graph->hi.fifo + himem;
	graph->hi.free = graph->hi.fifo;
	graph->hi.used = graph->hi.fifo;
	graph->hi.tasks = 0;
	graph->hi.stop = 0;

	/* Initialize low priority task queue */
	graph->lo.fifo = graph->hi.end;
	graph->lo.end = graph->lo.fifo + lomem;
	graph->lo.free = graph->lo.fifo;
	graph->lo.used = graph->lo.fifo;
	graph->lo.tasks = 0;
	graph->lo.stop = 0;

	/* Set default graphics functions */
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
		err = -ENODEV;
	} while (0);

	if (err < 0) {
		resourceDestroy(graph->lock);
		free(graph->hi.fifo);
	}

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
