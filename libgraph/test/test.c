/*
 * Phoenix-RTOS
 *
 * Graph library test
 *
 * Copyright 2021 Phoenix Systems
 * Copyright 2002-2007 IMMOS
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>

#include <libgraph.h>

#include "cursor.h"
#include "font.h"
#include "logo.h"


int test_lines1(graph_t *graph, unsigned int dx, unsigned int dy, int step)
{
	unsigned int i;
	int err;

	/* Slow lines */
	for (i = 0; i < 500; i++) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_line(graph, rand() % (graph->width - dx - 2 * step) + step, rand() % (graph->height - dx - 2 * step) + step, rand() % dx, rand() % dy, 1, rand() % (1ULL << 8 * graph->depth), GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	/* Fast lines */
	for (i = 0; i < 100000; i++) {
		while ((err = graph_trigger(graph)) && (err != -EAGAIN));
		if ((err = graph_line(graph, rand() % (graph->width - 2 * dx - 2 * step) + step + dx, rand() % (graph->height - 2 * dy - 2 * step) + step + dy, rand() % (2 * dx) - dx, rand() % (2 * dy) - dy, 1, rand() % (1ULL << 8 * graph->depth), GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}
	if ((err = graph_commit(graph)) < 0)
		return err;

	/* Move */
	for (i = 0; i < graph->height; i += step) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_move(graph, 0, step, graph->width, graph->height - step, 0, -step, GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	return EOK;
}


int test_lines2(graph_t *graph, unsigned int dx, unsigned int dy, int step)
{
	unsigned int i, pal = graph_colorget(graph, NULL, 1, 0) != -ENOTSUP;
	int err;

	if ((err = graph_rect(graph, 100, 100, graph->width - 199, graph->height - 199, (pal) ? 2 : 0x0000ffff, GRAPH_QUEUE_HIGH)) < 0)
		return err;

	for (i = 0; i < graph->height - 199; i += step) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_line(graph, 100, 100 + i, graph->width - 200, graph->height - 200 - i * step, 1, (pal) ? 100 : 0x00ff00ff, GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	for (i = 0; i < graph->width - 199; i += step) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_line(graph, 100 + i, graph->height - 100, graph->width - 200 - i * step, 200 - graph->height, 1, (pal) ? 100 : 0x00ff00ff, GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}
	while (graph_trigger(graph));
	if ((err = graph_commit(graph)) < 0)
		return err;

	return EOK;
}


int test_rectangles(graph_t *graph, unsigned int dx, unsigned int dy, int step)
{
	unsigned int i;
	int err;

	/* Slow rectangles */
	for (i = 0; i < 300; i++) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_rect(graph, rand() % (graph->width - dx - 2 * step) + step, rand() % (graph->height - dy - 2 * step) + step, dx, dy, rand() % (1ULL << 8 * graph->depth), GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	/* Fast rectangles */
	for (i = 0; i < 10000; i++) {
		while ((err = graph_trigger(graph)) && (err != -EAGAIN));
		if ((err = graph_rect(graph, rand() % (graph->width - dx - 2 * step) + step, rand() % (graph->height - dy - 2 * step) + step, dx, dy, rand() % (1ULL << 8 * graph->depth), GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}
	if ((err = graph_commit(graph)) < 0)
		return err;

	/* Move */
	for (i = 0; i < graph->width; i += step) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_move(graph, 0, 0, graph->width - step, graph->height, step, 0, GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	return EOK;
}


int test_logo(graph_t *graph, int step)
{
	static const char text[] = "Phoenix-RTOS";                      /* Text under logo */
	static const unsigned int fx = (sizeof(text) - 1) * font.width; /* Text width */
	static const unsigned int fy = font.height;                     /* Text height */
	static const unsigned int lx = 300;                             /* Logo width */
	static const unsigned int ly = 225;                             /* Logo height */
	static const unsigned int dy = ly + (3 * fy) / 2;               /* Total logo height */
	unsigned int i, x, y;
	int err, sy, ay;
	void *data;

	if ((graph_colorget(graph, NULL, 1, 0) != -ENOTSUP) || (graph->depth != 4))
		return -ENOTSUP;

	x = graph->width - lx - 2 * step;
	y = graph->height - dy - 2 * step;

	/* Compose logo at bottom left corner */
	data = (void *)((uintptr_t)graph->data + graph->depth * ((graph->height - dy - step) * graph->width + step));
	while (graph_trigger(graph), !graph_vsync(graph));
	if ((err = graph_rect(graph, 0, 0, graph->width, graph->height, *(uint32_t *)logo[0], GRAPH_QUEUE_HIGH)) < 0)
		return err;
	if ((err = graph_copy(graph, logo[0], data, lx, ly, graph->depth * lx, graph->depth * graph->width, GRAPH_QUEUE_HIGH)) < 0)
		return err;
	if ((err = graph_print(graph, &font, text, step + (lx - fx) / 2 + 1, graph->height - fy - step, font.height, font.height, 0xffffffff, GRAPH_QUEUE_HIGH)) < 0)
		return err;

	/* Move right */
	for (i = 0; i < x; i += step) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_move(graph, 0, graph->height - dy - step, graph->width - step, dy, step, 0, GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	/* Move diagonal */
	for (i = 0, ay = 0; i < x; i += step, ay += sy) {
		sy = i * y / x;
		sy = (ay < sy) ? sy - ay : 0;
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_move(graph, step, step, graph->width - step, graph->height - step, -step, -sy, GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	/* Move right */
	for (i = 0; i < x; i += step) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_move(graph, 0, 0, graph->width - step, dy, step, 0, GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	/* Move diagonal to center */
	for (i = 0, ay = 0, x >>= 1, y >>= 1; i < x; i += step, ay += sy) {
		sy = i * y / x;
		sy = (ay < sy) ? sy - ay : 0;
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_commit(graph)) < 0)
			return err;
		if ((err = graph_move(graph, step, 0, graph->width - step, graph->height - step, -step, sy, GRAPH_QUEUE_HIGH)) < 0)
			return err;
	}

	return EOK;
}


int test_cursor(graph_t *graph)
{
	unsigned int i, pal = graph_colorget(graph, NULL, 1, 0) != -ENOTSUP;
	int err;

	if ((err = graph_cursorset(graph, cand[0], cxor[0], (pal) ? 0 : 0xff000000, (pal) ? 1 : 0xffffffff)) < 0)
		return err;

	if ((err = graph_cursorshow(graph)) < 0)
		return err;

	for (i = 0; i < graph->height; i++) {
		while (graph_trigger(graph), !graph_vsync(graph));
		if ((err = graph_cursorpos(graph, i * graph->width / graph->height, i)) < 0)
			return err;
	}

	if ((err = graph_cursorhide(graph)) < 0)
		return err;

	return EOK;
}


int main(void)
{
	unsigned int mode = GRAPH_1024x768x32, freq = GRAPH_60Hz;
	graph_t graph;
	int ret;

	if ((ret = graph_init()) < 0) {
		fprintf(stderr, "test_libgraph: failed to initialize library\n");
		return ret;
	}

	if ((ret = graph_open(&graph, 0x2000, GRAPH_ANY)) < 0) {
		fprintf(stderr, "test_libgraph: failed to initialize graphics adapter\n");
		graph_done();
		return ret;
	}

	printf("test_libgraph: starting test in 1024x768x32 60Hz mode\n");
	srand(time(NULL));

	do {
		if ((ret = graph_mode(&graph, mode, freq)) < 0) {
			fprintf(stderr, "test_libgraph: failed to set graphics mode\n");
			break;
		}

		printf("test_libgraph: starting lines1 test...\n");
		if ((ret = test_lines1(&graph, 100, 100, 2)) < 0) {
			fprintf(stderr, "test_libgraph: lines1 test failed\n");
			break;
		}

		printf("test_libgraph: starting lines2 test...\n");
		if ((ret = test_lines2(&graph, 100, 100, 2)) < 0) {
			fprintf(stderr, "test_libgraph: lines2 test failed\n");
			break;
		}

		printf("test_libgraph: starting rectangles test...\n");
		if ((ret = test_rectangles(&graph, 100, 100, 2)) < 0) {
			fprintf(stderr, "test_libgraph: rectangles test failed\n");
			break;
		}

		printf("test_libgraph: starting logo test...\n");
		if ((ret = test_logo(&graph, 2)) < 0) {
			fprintf(stderr, "test_libgraph: logo test failed\n");
			break;
		}

		printf("test_libgraph: starting cursor test...\n");
		if ((ret = test_cursor(&graph)) < 0) {
			fprintf(stderr, "test_libgraph: cursor test failed\n");
			break;
		}
	} while (0);

	graph_close(&graph);
	graph_done();

	if (!ret)
		printf("test_libgraph: test finished successfully\n");

	return ret;
}
