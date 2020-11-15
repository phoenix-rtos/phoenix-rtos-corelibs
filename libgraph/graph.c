/*
 * Graph library for DPMI32
 *
 * Copyright 2002-2007 IMMOS
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "graph.h"
#include "chk.h"
#include "soft.h"

extern int ct69000_init(graph_t *graph);
extern int savage4_init(graph_t *graph);
extern int geode_init(graph_t *graph);


/* Common graph structure */
static graph_t graph;


int graph_init()
{
	graph.line = soft_line;
	graph.rect = soft_rect;
	graph.fill = soft_fill;
	graph.move = soft_move;
	graph.copyin = soft_copyin;
	graph.copyto = soft_copyto;
	graph.copyfrom = soft_copyfrom;
	graph.copyout = soft_copyout;
	graph.character = soft_char;

	if (ct69000_init(&graph) == GRAPH_SUCCESS)
		return GRAPH_SUCCESS;

	if (savage4_init(&graph) == GRAPH_SUCCESS)
		return GRAPH_SUCCESS;

	if (geode_init(&graph) == GRAPH_SUCCESS)
		return GRAPH_SUCCESS;

	return GRAPH_ERR;
}


int graph_open(unsigned int mem, uint8_t irq)
{
	int ret;
	unsigned int mem_low, mem_high;

	mem_low = mem / 2;
	mem_high = mem / 2;

	if ((mem_low < GRAPH_MIN_MEM) || (mem_high < GRAPH_MIN_MEM))
		return GRAPH_ERR_MEM;

	graph.low.fifo = NULL;
	graph.high.fifo = NULL;

	/* Allocate memory for task FIFO */
	if ((graph.low.fifo = (char *)malloc(mem_low + mem_high)) == NULL)
		return GRAPH_ERR_MEM;

	/* Initialize low piority task FIFO */
	graph.low.tasks = 0;
	graph.low.stop = 0;
	graph.low.free = graph.low.fifo;
	graph.low.used = graph.low.fifo;
	graph.low.end = graph.low.fifo + mem_low;

	// init HIGH FIFO task buffer
	graph.high.tasks = 0;
	graph.high.stop = 0;
	graph.high.fifo = graph.low.end;
	graph.high.free = graph.high.fifo;
	graph.high.used = graph.high.fifo;
	graph.high.end = graph.high.fifo + mem_high;

	if ((ret = graph.open(&graph, irq)) != GRAPH_SUCCESS)
		graph.close(&graph);

	return ret;
}


/* Function closes library */
int graph_close()
{
	int ret;

	ret = graph.close(&graph);
	if (graph.low.fifo != NULL)
		free(graph.low.fifo);

	graph.low.fifo = NULL;

	return ret;
}


int graph_mode(unsigned int mode, char freq)
{
	int ret;

	if (graph.busy || graph.isbusy(&graph) || graph.low.tasks || graph.high.tasks)
		return GRAPH_ERR_BUSY;

	ret = graph.mode(&graph, mode, freq);
	return ret;
}


int graph_vsync(void)
{
	int ret;

	ret = graph.vsync;
	graph.vsync = 0;

	return ret;
}


int graph_schedule(void)
{
	graph_queue_t *q;

	if (graph.busy)
		return GRAPH_ERR_NESTED;
	graph.busy = 1;

	while (!graph.isbusy(&graph)) {

		q = &graph.high;

		if (q->tasks == 0) {

			q = &graph.low;
			if (q->tasks == 0) {
				graph.busy = 0;
				return GRAPH_SUCCESS;
			}
 		}

		if (*(int *)(q->used) == 0)
			q->used = q->fifo;               // wrap buffer


//		f = *(void **)(q->used + sizeof(int));
#if 0
    _ECX = (unsigned int)(q->used + sizeof(int) + sizeof(void *));
    _EDI = (unsigned int)&graph;
		f(&graph, arg);
    asm {
      popf
      push ecx                         // arguments
      push edi                         // &graph
      call edx                         // call function
    }
#endif
		q->used += *(int *)(q->used);      // next task
		--(q->tasks);
	}
	graph.busy = 0;

	return GRAPH_ERR_BUSY;
}


int graph_stop(int queue)
{
	if (queue != GRAPH_QUEUE_HIGH)
		++graph.low.stop;

	if (queue != GRAPH_QUEUE_LOW)
		++graph.high.stop;

	return GRAPH_SUCCESS;
}


int graph_start(int queue)
{
	int ret = GRAPH_ERR_STOP;

	if (queue != GRAPH_QUEUE_HIGH)
		if (graph.low.stop) {
			--graph.low.stop;
			ret = GRAPH_SUCCESS;
		}

	if (queue != GRAPH_QUEUE_LOW)
		if (graph.high.stop) {
			--graph.high.stop;
		ret = GRAPH_SUCCESS;
	}

	return ret;
}


int graph_tasks(int queue)
{
	int ret = 0;

	if (queue != GRAPH_QUEUE_HIGH)
		ret += graph.low.tasks;

	if (queue != GRAPH_QUEUE_LOW)
		ret += graph.high.tasks;

	return ret;
}


int graph_reset(int queue)
{
	if (graph.busy)
		return GRAPH_ERR_NESTED;

	if (queue != GRAPH_QUEUE_HIGH) {
		graph.low.stop = 0;
		graph.low.tasks = 0;
		graph.low.free = graph.low.fifo;
		graph.low.used = graph.low.fifo;
	}
	if (queue != GRAPH_QUEUE_LOW) {
		graph.high.stop = 0;
		graph.high.tasks = 0;
		graph.high.free = graph.high.fifo;
		graph.high.used = graph.high.fifo;
	}
	return GRAPH_SUCCESS;
}


/* Execute taks or put it into the queue */
static int graph_exec(int (*func)(graph_t *, char *), int (*chk)(graph_t *, char *), char *arg, unsigned int bytes, int queue)
{
	graph_queue_t *q;

	switch (queue) {
	case GRAPH_QUEUE_LOW:
		q = &graph.low;
		break;
	case GRAPH_QUEUE_HIGH:
		q = &graph.high;
		break;
	default:
		return GRAPH_ERR_ARG;
	}

	if (q->stop != 0)
		return GRAPH_ERR_STOP;

#ifdef GRAPH_VERIFY_ARG
	if ((chk != NULL) && (chk(&graph, arg) != GRAPH_SUCCESS)) {
		return GRAPH_ERR_ARG;
	}
#endif

	if (graph.isbusy(&graph) || graph.busy || graph.high.tasks || q->tasks) {

		/* Free space is between free and used */
		if (q->free < q->used) {  
			if ((q->free + sizeof(int) + sizeof(void *) + bytes) >= q->used) {
				return GRAPH_ERR_QUEUE;
			}
 		}

		/* No free space at the end */
		else if ((q->free + sizeof(int) + sizeof(void *) + bytes + sizeof(int)) > q->end) {

			if ((q->fifo + sizeof(int) + sizeof(void *) + bytes) >= q->used) {
				return GRAPH_ERR_QUEUE;
			}
			*(int *)(q->free) = 0;
			q->free = q->fifo;
		}

		*(int *)(q->free) = sizeof(int) + sizeof(void *) + bytes;
		*(void **)(q->free + sizeof(int)) = (void *)func;

		memcpy(q->free + sizeof(int) + sizeof(void *), arg, bytes);
		q->free += sizeof(int) + sizeof(void *) + bytes;

		++(q->tasks);
		graph_schedule();                  // try to reschedule
		return GRAPH_SUCCESS;
	}
	graph.busy = 1;

	func(&graph, arg);                   // execute function

	graph.busy = 0;
	graph_schedule();                    // try to reschedule

	return GRAPH_SUCCESS;
}


int graph_trigger()
{
	int ret;

	ret = graph.trigger(&graph);

	return ret;
}


/*
 * Graphics functions
 */


int graph_line(unsigned int x, unsigned int y, int dx, int dy, unsigned int width, unsigned int color, int queue)
{
	if ((dx == 0) && (dy == 0))
		return graph_rect(x, y, width, width, color, queue);

	return graph_exec(graph.line, chk_line, (char *)&x, 6 * sizeof(int), queue);
}


int graph_fill(unsigned int x, unsigned int y, unsigned int color, int queue)
{
	return graph_exec(graph.fill, chk_fill, (char *)&x, 4 * sizeof(int), queue);
}


int graph_rect(unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color, int queue)
{
	return graph_exec(graph.rect, chk_rect, (char *)&x, 5 * sizeof(int), queue);
}


int graph_move(unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my, int queue)
{
	return graph_exec(graph.move, chk_move, (char *)&x, 6 * sizeof(int), queue);
}


int graph_copyin(unsigned int src, unsigned int dst, unsigned int dx, unsigned int dy, int srcspan, int dstspan, int queue)
{
	return graph_exec(graph.copyin, chk_copyin, (char *)&src, 6 * sizeof(int), queue);
}


int graph_copyto(char *src, unsigned int dst, unsigned int dx, unsigned int dy, int srcspan, int dstspan, int queue)
{
	return graph_exec(graph.copyto, chk_copyto, (char *)&src, 6 * sizeof(int), queue);
}


int graph_copyfrom(unsigned int src, char *dst, unsigned int dx, unsigned int dy, int srcspan, int dstspan, int queue)
{
	return graph_exec(graph.copyfrom, chk_copyfrom, (char *)&src, 6 * sizeof(int), queue);
}


int graph_copyout(char *src, char *dst, unsigned int dx, unsigned int dy, int srcspan, int dstspan, int queue)
{
	return graph_exec(graph.copyout, chk_copyout, (char *)&src, 6 * sizeof(int), queue);
}


int graph_colorset(char *colors, unsigned char first, unsigned char last)
{
	int ret;

	ret = graph.colorset(&graph, colors, first, last);

	return ret;
}


int graph_colorget(char *colors, unsigned char first, unsigned char last)
{
	int ret;

	ret = graph.colorget(&graph, colors, first, last);

	return ret;
}


int graph_cursorset(char *and, char *xor, unsigned char bg, unsigned char fg)
{
	int ret;

	ret = graph.cursorset(&graph, and, xor, bg, fg);
	return ret;
}


int graph_cursorpos(unsigned int x, unsigned int y)
{
	int ret;

	ret = graph.cursorpos(&graph, x, y);
	return ret;
}


int graph_cursorshow()
{
	int ret;

	ret = graph.cursorshow(&graph);
	return ret;
}


int graph_cursorhide()
{
	int ret;

	ret = graph.cursorhide(&graph);

	return ret;
}


int graph_print(char *text, unsigned int x, unsigned int y, char *fonts, unsigned int dx, unsigned int dy, unsigned int color, int queue)
{
	int ret;
	unsigned int arg[8];
	unsigned int *l;

	arg[5] = dy;
	arg[7] = color;

	while (*text) {
		l = (unsigned int *)(fonts + 16 * (*(unsigned char*)(text++)));

	arg[0] = l[0];                     // character address
	arg[1] = graph_addr(x, y);         // print address
	arg[2] = l[1];                     // character width
	arg[3] = l[2];                     // character height
	arg[4] = l[1] * dx / l[2];         // print width
	arg[6] = l[3];                     // character span
	x += arg[4];

	ret = graph_exec(graph.character, chk_character, (char *)arg, 8 * sizeof(int), queue);

	if (ret != GRAPH_SUCCESS)
		return ret;
	}

	return GRAPH_SUCCESS;
}


/*
 * Helper functions
 */


char *graph_version()
{
	return GRAPH_VERSION;
}


int graph_addr(unsigned int x, unsigned int y)
{
	int ret;

#ifdef GRAPH_VERIFY_ARG
	if ((x >= graph.width) || (y >= graph.height)) {
		return GRAPH_ERR_ARG;
	}
#endif

	ret = (graph.width * y + x) * graph.depth;
	return ret;
}


int graph_span()
{
	int ret;

	ret = graph.width * graph.depth;
	return ret;
}


int graph_offset()
{
	int ret;

	ret = graph.width * graph.height * graph.depth;
	return ret;
}


int graph_size()
{
	int ret;

	ret = graph.memsz - graph.cursorsz - graph.width * graph.height * graph.depth;
	return ret;
}


int graph_width(char *text, char *fonts, unsigned int dx)
{
	unsigned int *l;
	unsigned int width;
	unsigned int ret = 0;

	while (*text) {
		l = (unsigned int *)(fonts + 16 * (*(unsigned char*)(text++)));
		width = l[1] * dx / l[2];          // character width

		if (width > l[1])
			return GRAPH_ERR_ARG;            // improper stretch
		ret += width;
	}
	return ret;
}
