/*
 * Graph library for DPMI32
 *
 * Software operations
 *
 * Copyright 2002-2007 IMMOS
 */

#ifndef _SOFT_H_
#define _SOFT_H_

#include "graph.h"


extern int soft_line(graph_t *graph, int x, int y, int dx, int dy, unsigned int stroke, unsigned int color);


extern int soft_fill(graph_t *graph, char *arg);

extern int soft_rect(graph_t *graph, char *arg);

extern int soft_move(graph_t *graph, char *arg);

extern int soft_copyin(graph_t *graph, char *arg);

extern int soft_copyto(graph_t *graph, char *arg);

extern int soft_copyfrom(graph_t *graph, char *arg);

extern int soft_copyout(graph_t *graph, char *arg);

extern int soft_char(graph_t *graph, char *arg);


#endif // _SOFT_H_

