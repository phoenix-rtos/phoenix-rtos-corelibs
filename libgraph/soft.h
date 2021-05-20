/*
 * Phoenix-RTOS
 *
 * Software graphics operations
 *
 * Copyright 2009, 2021 Phoenix Systems
 * Copyright 2002-2007 IMMOS
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _SOFT_H_
#define _SOFT_H_

#include "graph.h"


extern int soft_line(graph_t *graph, unsigned int x, unsigned int y, int dx, int dy, unsigned int stroke, unsigned int color);


extern int soft_rect(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color);


extern int soft_fill(graph_t *graph, unsigned int x, unsigned int y, unsigned int color, graph_fill_t type);


extern int soft_print(graph_t *graph, unsigned int x, unsigned int y, unsigned char dx, unsigned char dy, const unsigned char *bmp, unsigned char width, unsigned char height, unsigned char span, unsigned int color);


extern int soft_move(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my);


extern int soft_copy(graph_t *graph, const void *src, void *dst, unsigned int dx, unsigned int dy, unsigned int srcspan, unsigned int dstspan);


#endif
