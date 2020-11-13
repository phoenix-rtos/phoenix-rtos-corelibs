/*
 * Graph library for DPMI32
 *
 * Validators
 *
 * Copyright 2001-2007 IMMOS
 */

#include <kernel.h>

#include "chk.h"


int chk_color(graph_t *graph, unsigned int color)
{
  switch (graph->depth) {
  case 1:
    if (color > 0xff)
      return GRAPH_ERR_ARG;
    break;
  case 2:
    if (color > 0xffff)
      return GRAPH_ERR_ARG;
    break;
  default:
    return GRAPH_ERR_ARG;
  }
  return GRAPH_SUCCESS;
}


int chk_point(graph_t *graph, unsigned int x, unsigned int y)
{
  if ((x < graph->width) && (y < graph->height))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}


int chk_line(graph_t *graph, char *data)
{
  unsigned int x = *(unsigned int *)data;
  unsigned int y = *(unsigned int *)(data + sizeof(int));
  int dx = *(int *)(data + 2 * sizeof(int));
  int dy = *(int *)(data + 3 * sizeof(int));
  unsigned int width = *(unsigned int *)(data + 4 * sizeof(int));
  unsigned int color = *(unsigned int *)(data + 5 * sizeof(int));

  if ((chk_color(graph, color) == GRAPH_SUCCESS) &&
      (chk_point(graph, x + width, y + width) == GRAPH_SUCCESS) &&
      (chk_point(graph, x + dx + width, y + dy + width) == GRAPH_SUCCESS))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}


int chk_fill(graph_t *graph, char *data)
{
  unsigned int x = *(unsigned int *)data;
  unsigned int y = *(unsigned int *)(data + sizeof(int));
  unsigned int color = *(unsigned int *)(data + 2 * sizeof(int));

  if ((chk_color(graph, color) == GRAPH_SUCCESS) && (chk_point(graph, x, y) == GRAPH_SUCCESS))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}


int chk_rect(graph_t *graph, char *data)
{
  unsigned int x = *(unsigned int *)data;
  unsigned int y = *(unsigned int *)(data + sizeof(int));
  unsigned int dx = *(unsigned int *)(data + 2 * sizeof(int));
  unsigned int dy = *(unsigned int *)(data + 3 * sizeof(int));
  unsigned int color = *(unsigned int *)(data + 4 * sizeof(int));

  if ((chk_color(graph, color) == GRAPH_SUCCESS) && (chk_point(graph, x, y) == GRAPH_SUCCESS) &&
      (chk_point(graph, dx - 1, dy - 1) == GRAPH_SUCCESS) &&
      (chk_point(graph, x + dx - 1, y + dy - 1) == GRAPH_SUCCESS))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}


int chk_move(graph_t *graph, char *data)
{
  unsigned int x = *(unsigned int *)data;
  unsigned int y = *(unsigned int *)(data + sizeof(int));
  unsigned int dx = *(unsigned int *)(data + 2 * sizeof(int));
  unsigned int dy = *(unsigned int *)(data + 3 * sizeof(int));
  int mx = *(int *)(data + 4 * sizeof(int));
  int my = *(int *)(data + 5 * sizeof(int));

  if ((chk_point(graph, x, y) == GRAPH_SUCCESS) &&
      (chk_point(graph, x + mx, y + my) == GRAPH_SUCCESS) &&
      (chk_point(graph, dx - 1, dy - 1) == GRAPH_SUCCESS) &&
      (chk_point(graph, x + dx - 1, y + dy - 1) == GRAPH_SUCCESS) &&
      (chk_point(graph, x + dx - 1 + mx, y + dy - 1 + my) == GRAPH_SUCCESS))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}


int chk_copyin(graph_t *graph, char *data)
{
  unsigned int src = *(unsigned int *)data;
  unsigned int dst = *(unsigned int *)(data + sizeof(int));
  unsigned int dx = *(unsigned int *)(data + 2 * sizeof(int));
  unsigned int dy = *(unsigned int *)(data + 3 * sizeof(int));
  int srcspan = *(int *)(data + 4 * sizeof(int));
  int dstspan = *(int *)(data + 5 * sizeof(int));

  if ((src < (graph->memsz - graph->cursorsz)) &&
      (dst < (graph->memsz - graph->cursorsz)) &&
      ((dx > 0) && (dy > 0)) &&
      ((src + (dy - 1) * srcspan + dx - 1) < (graph->memsz - graph->cursorsz)) &&
      ((dst + (dy - 1) * dstspan + dx - 1) < (graph->memsz - graph->cursorsz)))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}


int chk_copyto(graph_t *graph, char *data)
{
  unsigned int dst = *(unsigned int *)(data + sizeof(char *));
  unsigned int dx = *(unsigned int *)(data + sizeof(char *) + sizeof(int));
  unsigned int dy = *(unsigned int *)(data + sizeof(char *) + 2 * sizeof(int));
  int dstspan = *(int *)(data + sizeof(char *) + 4 * sizeof(int));

  if ((dst < (graph->memsz - graph->cursorsz)) &&
      ((dx > 0) && (dy > 0)) &&
      ((dst + (dy - 1) * dstspan + dx - 1) < (graph->memsz - graph->cursorsz)))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}


int chk_copyfrom(graph_t *graph, char *data)
{
  unsigned int src = *(unsigned int *)data;
  unsigned int dx = *(unsigned int *)(data + sizeof(char *) + sizeof(int));
  unsigned int dy = *(unsigned int *)(data + sizeof(char *) + 2 * sizeof(int));
  int srcspan = *(int *)(data + sizeof(char *) + 3 * sizeof(int));

  if ((src < (graph->memsz - graph->cursorsz)) &&
      ((dx > 0) && (dy > 0)) &&
      ((src + (dy - 1) * srcspan + dx - 1) < (graph->memsz - graph->cursorsz)))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}


int chk_copyout(graph_t *graph, char *data)
{
  return GRAPH_SUCCESS;
}


int chk_character(graph_t *graph, char *data)
{
  unsigned int dst = *(unsigned int *)(data + sizeof(char *));
  unsigned int sdx = *(unsigned int *)(data + sizeof(char *) + sizeof(int));
  unsigned int sdy = *(unsigned int *)(data + sizeof(char *) + 2 * sizeof(int));
  unsigned int ddx = *(unsigned int *)(data + sizeof(char *) + 3 * sizeof(int));
  unsigned int ddy = *(unsigned int *)(data + sizeof(char *) + 4 * sizeof(int));
  unsigned int color = *(unsigned int *)(data + sizeof(char *) + 6 * sizeof(int));

  if ((chk_color(graph, color) == GRAPH_SUCCESS) &&
      (dst < (graph->memsz - graph->cursorsz)) &&
      ((dst + ((ddy - 1) * graph->width * graph->depth + ddx - 1)) < (graph->memsz - graph->cursorsz)) &&
      (ddx <= sdx) && (ddy <= sdy))
    return GRAPH_SUCCESS;

  return GRAPH_ERR_ARG;
}

