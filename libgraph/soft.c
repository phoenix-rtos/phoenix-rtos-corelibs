/*
 * Phoenix-RTOS
 *
 * Software graphics operations
 *
 * Copyright 2009, 2021 Phoenix Systems
 * Copyright 2002-2007 IMMOS
 * Author: Lukasz Kosinski, Michal Slomczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "soft.h"


/* Returns pixel data address */
static inline uintptr_t soft_data(graph_t *graph, unsigned int x, unsigned int y)
{
	return (uintptr_t)graph->data + graph->depth * (y * graph->width + x);
}


/* Returns pixel color */
static inline unsigned int soft_get(graph_t *graph, uintptr_t data)
{
	switch (graph->depth) {
		case 1:
			return *(uint8_t *)data;

		case 2:
			return *(uint16_t *)data;

		case 4:
			return *(uint32_t *)data;

		default:
			return -EINVAL;
	}
}


/* Sets pixel to given color */
static inline int soft_set(graph_t *graph, uintptr_t data, unsigned int color)
{
	switch (graph->depth) {
		case 1:
			*(uint8_t *)data = color;
			break;

		case 2:
			*(uint16_t *)data = color;
			break;

		case 3:
			*(uint8_t *)data = color;
			*(uint8_t *)(data + 1) = color >> 8;
			*(uint8_t *)(data + 2) = color >> 16;
			break;

		case 4:
			*(uint32_t *)data = color;
			break;

		default:
			return -EINVAL;
	}

	return EOK;
}


int soft_line(graph_t *graph, unsigned int x, unsigned int y, int dx, int dy, unsigned int stroke, unsigned int color)
{
	uintptr_t data, buff;
	uint32_t a, acc, tmp;
	int n, sx, sy;

#ifdef GRAPH_VERIFY_ARGS
	if (!stroke || ((int)x + dx < 0) || ((int)y + dy < 0) ||
		(x + stroke > graph->width) || (x + dx + stroke > graph->width) ||
		(y + stroke > graph->height) || (y + dy + stroke > graph->height))
		return -EINVAL;
#endif

	if (!dx && !dy)
		return soft_rect(graph, x, y, stroke, stroke, color);

	data = soft_data(graph, x, y + stroke - 1);
	sy = graph->width * graph->depth;
	sx = graph->depth;

	if (dx < 0) {
		data += (stroke - 1) * sx;
		dx = -dx;
		sx = -sx;
	}

	if (dy < 0) {
		data -= (stroke - 1) * sy;
		dy = -dy;
		sy = -sy;
	}

	if (dx > dy) {
		a = dy * 0x10000 / dx * 0xffff;
		sy += sx;
		n = sy;
		sy = sx;
		sx = n;
		n = dx;
		dx = sx - sy;
		dy = sy;
	}
	else {
		a = dx * 0x10000 / dy * 0xffff;
		sx += sy;
		n = dy;
		dx = sy;
		dy = sx - sy;
	}

	switch (graph->depth) {
		case 1:
			for (x = 0; x < stroke; x++) {
				buff = data - (int)x * dx;
				acc = 0x80000000;

				for (y = 0; y < n; y++) {
					*(uint8_t *)buff = color;
					tmp = acc;
					acc += a;
					buff += (acc < tmp) ? sx : sy;
				}

				for (y = 0; y < stroke; y++) {
					*(uint8_t *)buff = color;
					buff += dy;
				}
			}

			data -= (int)(stroke - 1) * dx;
			for (x = 1; x < stroke; x++) {
				buff = data + (int)x * dy;
				acc = 0x80000000;

				for (y = 0; y < n; y++) {
					*(uint8_t *)buff = color;
					tmp = acc;
					acc += a;
					buff += (acc < tmp) ? sx : sy;
				}
			}
			break;

		case 2:
			for (x = 0; x < stroke; x++) {
				buff = data - (int)x * dx;
				acc = 0x80000000;

				for (y = 0; y < n; y++) {
					*(uint16_t *)buff = color;
					tmp = acc;
					acc += a;
					buff += (acc < tmp) ? sx : sy;
				}

				for (y = 0; y < stroke; y++) {
					*(uint16_t *)buff = color;
					buff += dy;
				}
			}

			data -= (int)(stroke - 1) * dx;
			for (x = 1; x < stroke; x++) {
				buff = data + (int)x * dy;
				acc = 0x80000000;

				for (y = 0; y < n; y++) {
					*(uint16_t *)buff = color;
					tmp = acc;
					acc += a;
					buff += (acc < tmp) ? sx : sy;
				}
			}
			break;

		case 4:
			for (x = 0; x < stroke; x++) {
				buff = data - (int)x * dx;
				acc = 0x80000000;

				for (y = 0; y < n; y++) {
					*(uint32_t *)buff = color;
					tmp = acc;
					acc += a;
					buff += (acc < tmp) ? sx : sy;
				}

				for (y = 0; y < stroke; y++) {
					*(uint32_t *)buff = color;
					buff += dy;
				}
			}

			data -= (int)(stroke - 1) * dx;
			for (x = 1; x < stroke; x++) {
				buff = data + (int)x * dy;
				acc = 0x80000000;

				for (y = 0; y < n; y++) {
					*(uint32_t *)buff = color;
					tmp = acc;
					acc += a;
					buff += (acc < tmp) ? sx : sy;
				}
			}
			break;

		default:
			return -EINVAL;
	}

	return EOK;
}


int soft_rect(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color)
{
	uintptr_t data;
	unsigned int n;

#ifdef GRAPH_VERIFY_ARGS
	if ((x + dx > graph->width) || (y + dy > graph->height))
		return -EINVAL;
#endif

	if (!dx || !dy)
		return EOK;

	data = soft_data(graph, x, y);
	n = graph->depth * (graph->width - dx);

	for (y = 0; y < dy; y++) {
		for (x = 0; x < dx; x++) {
			soft_set(graph, data, color);
			data += graph->depth;
		}
		data += n;
	}

	return EOK;
}


static int cmp_flood(unsigned int data, unsigned int color)
{
	return data == color;
}


static int cmp_bound(unsigned int data, unsigned int color)
{
	return data != color;
}


int soft_fill(graph_t *graph, unsigned int x, unsigned int y, unsigned int color, graph_fill_t type)
{
	int (*cmp)(unsigned int, unsigned int);
	int *stack, *sp, lx, rx, dy;
	unsigned int cmpcolor;
	uintptr_t data, tmp;

#define PUSH(lx, rx, y, dy) \
	if ((y + dy >= 0) && (y + dy < graph->height)) { \
		*sp++ = lx; \
		*sp++ = rx; \
		*sp++ = y; \
		*sp++ = dy; \
	}

#define POP(lx, rx, y, dy) \
	dy = *--sp; \
	y = *--sp + dy; \
	rx = *--sp; \
	lx = *--sp;

#ifdef GRAPH_VERIFY_ARGS
	if ((x > graph->width) || (y > graph->height))
		return -EINVAL;
#endif

	data = soft_data(graph, x, y);
	switch (type) {
		case GRAPH_FILL_FLOOD:
			if ((cmpcolor = soft_get(graph, data)) == color)
				return EOK;
			cmp = cmp_flood;
			break;

		case GRAPH_FILL_BOUND:
			cmpcolor = color;
			cmp = cmp_bound;
			break;

		default:
			return -EINVAL;
	}

	if ((sp = stack = malloc(0x10000)) == NULL)
		return -ENOMEM;

	PUSH(x, x, y, 1);
	PUSH(x, x, y + 1, -1);

	switch (graph->depth) {
		case 1:
			while (sp > stack) {
				POP(x, rx, y, dy);
				data = soft_data(graph, x, y);
				lx = x;

				if (cmp(*(uint8_t *)data, cmpcolor)) {
					for (tmp = data - 1; lx && cmp(*(uint8_t *)tmp, cmpcolor); tmp--, lx--)
						*(uint8_t *)tmp = color;
				}

				if (lx < x) {
					PUSH(lx, x - 1, y, -dy);
				}
				else {
					for (; (x <= rx) && !cmp(*(uint8_t *)data, cmpcolor); data++, x++)
						;

					if (x > rx)
						continue;
					lx = x;
				}

				while (x <= rx) {
					for (; (x < graph->width) && cmp(*(uint8_t *)data, cmpcolor); data++, x++)
						*(uint8_t *)data = color;

					PUSH(lx, x - 1, y, dy);
					if (x > rx + 1)
						PUSH(rx + 1, x - 1, y, -dy);

					for (x++, data++; (x <= rx) && !cmp(*(uint8_t *)data, cmpcolor); data++, x++)
						;
					lx = x;
				}
			}
			break;

		case 2:
			while (sp > stack) {
				POP(x, rx, y, dy);
				data = soft_data(graph, x, y);
				lx = x;

				if (cmp(*(uint16_t *)data, cmpcolor)) {
					for (tmp = data - 2; lx && cmp(*(uint16_t *)tmp, cmpcolor); tmp -= 2, lx--)
						*(uint16_t *)tmp = color;
				}

				if (lx < x) {
					PUSH(lx, x - 1, y, -dy);
				}
				else {
					for (; (x <= rx) && !cmp(*(uint16_t *)data, cmpcolor); data += 2, x++)
						;

					if (x > rx)
						continue;
					lx = x;
				}

				while (x <= rx) {
					for (; (x < graph->width) && cmp(*(uint16_t *)data, cmpcolor); data += 2, x++)
						*(uint16_t *)data = color;

					PUSH(lx, x - 1, y, dy);
					if (x > rx + 1)
						PUSH(rx + 1, x - 1, y, -dy);

					for (x++, data += 2; (x <= rx) && !cmp(*(uint16_t *)data, cmpcolor); data += 2, x++)
						;
					lx = x;
				}
			}
			break;

		case 4:
			while (sp > stack) {
				POP(x, rx, y, dy);
				data = soft_data(graph, x, y);
				lx = x;

				if (cmp(*(uint32_t *)data, cmpcolor)) {
					for (tmp = data - 4; lx && cmp(*(uint32_t *)tmp, cmpcolor); tmp -= 4, lx--)
						*(uint32_t *)tmp = color;
				}

				if (lx < x) {
					PUSH(lx, x - 1, y, -dy);
				}
				else {
					for (; (x <= rx) && !cmp(*(uint32_t *)data, cmpcolor); data += 4, x++)
						;

					if (x > rx)
						continue;
					lx = x;
				}

				while (x <= rx) {
					for (; (x < graph->width) && cmp(*(uint32_t *)data, cmpcolor); data += 4, x++)
						*(uint32_t *)data = color;

					PUSH(lx, x - 1, y, dy);
					if (x > rx + 1)
						PUSH(rx + 1, x - 1, y, -dy);

					for (x++, data += 4; (x <= rx) && !cmp(*(uint32_t *)data, cmpcolor); data += 4, x++)
						;
					lx = x;
				}
			}
			break;

		default:
			free(stack);
			return -EINVAL;
	}

	free(stack);
	return EOK;
}


int soft_print(graph_t *graph, unsigned int x, unsigned int y, unsigned char dx, unsigned char dy, const unsigned char *bmp, unsigned char width, unsigned char height, unsigned char span, unsigned int color)
{
	uint32_t n, val, line[0x100];
	uint8_t sx, sy, ax, ay, tmp;
	int sl, dl;
	uintptr_t data;

#ifdef GRAPH_VERIFY_ARGS
	if (!dx || !dy || (x + dx > graph->width) || (y + dy > graph->height) || (dx > width) || (dy > height) || ((span << 3) < width))
		return -EINVAL;
#endif

	data = soft_data(graph, x, y);
	sx = ((unsigned int)dx * 0x10000 / (unsigned int)width * 0xffff) >> 24;
	sy = ((unsigned int)dy * 0x10000 / (unsigned int)height * 0xffff) >> 24;
	sl = (int)span - ((((int)width + 31) >> 3) & 0xfc);
	dl = graph->depth * (graph->width - dx);
	ay = height;

	switch (graph->depth) {
		case 1:
			for (y = 0; y < dy; y++) {
				memset(line, 0, dx * sizeof(line[0]));

				do {
					ax = width;
					n = val = 0;

					for (x = 0; x < dx; x++) {
						do {
							if (!(n++ % 32)) {
								val = *(uint32_t *)bmp;
								bmp += 4;
							}
							line[x] += 0x10000 + (val & 0x1);
							val >>= 1;
							tmp = ax;
							ax += sx;
						} while (ax > tmp);
					}

					bmp += sl;
					tmp = ay;
					ay += sy;
				} while (ay > tmp);

				for (; x--; data++) {
					if ((line[x] << 1 & 0xffff) >= (line[x] >> 16))
						*(uint8_t *)data = color;
				}
				data += dl;
			}
			break;

		case 2:
			for (y = 0; y < dy; y++) {
				memset(line, 0, dx * sizeof(line[0]));

				do {
					ax = width;
					n = val = 0;

					for (x = 0; x < dx; x++) {
						do {
							if (!(n++ % 32)) {
								val = *(uint32_t *)bmp;
								bmp += 4;
							}
							line[x] += 0x10000 + (val & 0x1);
							val >>= 1;
							tmp = ax;
							ax += sx;
						} while (ax > tmp);
					}

					bmp += sl;
					tmp = ay;
					ay += sy;
				} while (ay > tmp);

				for (; x--; data += 2) {
					if ((line[x] << 1 & 0xffff) >= (line[x] >> 16))
						*(uint16_t *)data = color;
				}
				data += dl;
			}
			break;

		case 4:
			for (y = 0; y < dy; y++) {
				memset(line, 0, dx * sizeof(line[0]));

				do {
					ax = width;
					n = val = 0;

					for (x = 0; x < dx; x++) {
						do {
							if (!(n++ % 32)) {
								val = *(uint32_t *)bmp;
								bmp += 4;
							}
							line[x] += 0x10000 + (val & 0x1);
							val >>= 1;
							tmp = ax;
							ax += sx;
						} while (ax > tmp);
					}

					bmp += sl;
					tmp = ay;
					ay += sy;
				} while (ay > tmp);

				for (; x--; data += 4) {
					if ((line[x] << 1 & 0xffff) >= (line[x] >> 16))
						*(uint32_t *)data = color;
				}
				data += dl;
			}
			break;

		default:
			return -EINVAL;
	}

	return EOK;
}


int soft_move(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my)
{
	uintptr_t src, dst;
	int span;

#ifdef GRAPH_VERIFY_ARGS
	if ((x + dx > graph->width) || (y + dy > graph->height) ||
		((int)x + mx < 0) || ((int)y + my < 0) ||
		((int)x + mx > graph->width) || ((int)y + my > graph->height) ||
		(x + dx + mx > graph->width) || (y + dy + my > graph->height))
		return -EINVAL;
#endif

	if (!dx || !dy || (!mx && !my))
		return EOK;

	src = soft_data(graph, x, y);
	dst = soft_data(graph, x + mx, y + my);
	span = graph->depth * graph->width;
	dx *= graph->depth;

	if (dst > src) {
		src += (dy - 1) * span;
		dst += (dy - 1) * span;
		span = -span;
	}

	for (y = 0; y < dy; y++, src += span, dst += span)
		memmove((void *)dst, (void *)src, dx);

	return EOK;
}


int soft_copy(graph_t *graph, const void *src, void *dst, unsigned int dx, unsigned int dy, unsigned int srcspan, unsigned int dstspan)
{
	unsigned int y;

#ifdef GRAPH_VERIFY_ARGS
	if ((srcspan < graph->depth * dx) || (dstspan < graph->depth * dx))
		return -EINVAL;
#endif

	if (!dx || !dy)
		return EOK;

	dx *= graph->depth;

	for (y = 0; y < dy; y++, src += srcspan, dst += dstspan)
		memcpy(dst, src, dx);

	return EOK;
}
