/*
 * Graph library for DPMI32
 *
 * Validators
 *
 * Copyright 2002-2007 IMMOS
 */

#ifndef _CHK_H_
#define _CHK_H_

#include "graph.h"


extern int chk_line(graph_t *graph, char *data);

extern int chk_fill(graph_t *graph, char *data);

extern int chk_rect(graph_t *graph, char *data);

extern int chk_move(graph_t *graph, char *data);

extern int chk_copyin(graph_t *graph, char *data);

extern int chk_copyto(graph_t *graph, char *data);

extern int chk_copyfrom(graph_t *graph, char *data);

extern int chk_copyout(graph_t *graph, char *data);

extern int chk_character(graph_t *graph, char *data);


#endif // _CHK_H_

