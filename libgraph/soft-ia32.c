/*
 * Phoenix-RTOS
 *
 * Software graphics operations (IA32)
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

#include "soft.h"


static inline void *soft_data(graph_t *graph, unsigned int x, unsigned int y)
{
	return graph->data + graph->depth * (y * graph->width + x);
}


int soft_line(graph_t *graph, unsigned int x, unsigned int y, int dx, int dy, unsigned int stroke, unsigned int color)
{
	void *data, *buff;
	unsigned int a;
	int sx, sy;

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
		x = sy;
		sy = sx;
		sx = x;
		x = dx;
		dx = sx - sy;
		dy = sy;
	}
	else {
		a = dx * 0x10000 / dy * 0xffff;
		sx += sy;
		x = dy;
		dx = sy;
		dy = sx - sy;
	}

	switch (graph->depth) {
		case 1:
			__asm__ volatile(
				"movl %3, %%edi; "  /* data */
				"movl %4, %%edx; "  /* a */
				"movl %5, %%esi; "  /* sx */
				"movl %6, %%ebp; "  /* sy */
				"movl %11, %%ebx; " /* stroke */
				"movl %12, %%eax; " /* color */
				"line1: "
				"movl %%edi, %0; " /* save buff */
				"movl %%ebx, %1; " /* save y */
				"movl %7, %%ecx; " /* x */
				"movl $0x80000000, %%ebx; "
				"line2: "
				"movb %%al, (%%edi); "
				"addl %%edx, %%ebx; "
				"jc line3; "
				"addl %%ebp, %%edi; "
				"loop line2; "
				"jmp line4; "
				"line3: "
				"addl %%esi, %%edi; "
				"loop line2; "
				"line4: "
				"movl %10, %%ebx; " /* dy */
				"movl %11, %%ecx; " /* stroke */
				"line5: "
				"movb %%al, (%%edi); "
				"addl %%ebx, %%edi; "
				"loop line5; "
				"movl %2, %%edi; " /* restore buff */
				"subl %9, %%edi; " /* buff -= dx */
				"movl %8, %%ebx; " /* restore y */
				"decl %%ebx; "
				"jnz line1; "
				"addl %%esi, %%edi; "
				"movl %11, %%ebx; " /* stroke */
				"decl %%ebx; "
				"jz line10; "
				"line6: "
				"movl %%edi, %0; " /* save buff */
				"movl %%ebx, %1; " /* save y */
				"movl %7, %%ecx; " /* x */
				"movl $0x80000000, %%ebx; "
				"line7: "
				"movb %%al, (%%edi); "
				"addl %%edx, %%ebx; "
				"jc line8; "
				"addl %%ebp, %%edi; "
				"loop line7; "
				"jmp line9; "
				"line8: "
				"addl %%esi, %%edi; "
				"loop line7; "
				"line9: "
				"movl %2, %%edi; "  /* restore buff */
				"addl %10, %%edi; " /* buff += dy */
				"movl %8, %%ebx; "  /* restore y */
				"decl %%ebx; "
				"jnz line6; "
				"line10: "
				: "=m"(buff), "=m"(y)
				: "m"(buff), "m"(data), "m"(a), "m"(sx), "m"(sy), "m"(x), "m"(y), "m"(dx), "m"(dy), "m"(stroke), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
			break;

		case 2:
			__asm__ volatile(
				"movl %3, %%edi; "  /* data */
				"movl %4, %%edx; "  /* a */
				"movl %5, %%esi; "  /* sx */
				"movl %6, %%ebp; "  /* sy */
				"movl %11, %%ebx; " /* stroke */
				"movl %12, %%eax; " /* color */
				"line11: "
				"movl %%edi, %0; " /* save buff */
				"movl %%ebx, %1; " /* save y */
				"movl %7, %%ecx; " /* x */
				"movl $0x80000000, %%ebx; "
				"line12: "
				"movw %%ax, (%%edi); "
				"addl %%edx, %%ebx; "
				"jc line13; "
				"addl %%ebp, %%edi; "
				"loop line12; "
				"jmp line14; "
				"line13: "
				"addl %%esi, %%edi; "
				"loop line12; "
				"line14: "
				"movl %10, %%ebx; " /* dy */
				"movl %11, %%ecx; " /* stroke */
				"line15: "
				"movw %%ax, (%%edi); "
				"addl %%ebx, %%edi; "
				"loop line15; "
				"movl %2, %%edi; " /* restore buff */
				"subl %9, %%edi; " /* buff -= dx */
				"movl %8, %%ebx; " /* restore y */
				"decl %%ebx; "
				"jnz line11; "
				"addl %%esi, %%edi; "
				"movl %11, %%ebx; " /* stroke */
				"decl %%ebx; "
				"jz line20; "
				"line16: "
				"movl %%edi, %0; " /* save buff */
				"movl %%ebx, %1; " /* save y */
				"movl %7, %%ecx; " /* x */
				"movl $0x80000000, %%ebx; "
				"line17: "
				"movw %%ax, (%%edi); "
				"addl %%edx, %%ebx; "
				"jc line18; "
				"addl %%ebp, %%edi; "
				"loop line17; "
				"jmp line19; "
				"line18: "
				"addl %%esi, %%edi; "
				"loop line17; "
				"line19: "
				"movl %2, %%edi; "  /* restore buff */
				"addl %10, %%edi; " /* buff += dy */
				"movl %8, %%ebx; "  /* restore y */
				"decl %%ebx; "
				"jnz line16; "
				"line20: "
				: "=m"(buff), "=m"(y)
				: "m"(buff), "m"(data), "m"(a), "m"(sx), "m"(sy), "m"(x), "m"(y), "m"(dx), "m"(dy), "m"(stroke), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
			break;

		case 4:
			__asm__ volatile(
				"movl %3, %%edi; "  /* data */
				"movl %4, %%edx; "  /* a */
				"movl %5, %%esi; "  /* sx */
				"movl %6, %%ebp; "  /* sy */
				"movl %11, %%ebx; " /* stroke */
				"movl %12, %%eax; " /* color */
				"line21: "
				"movl %%edi, %0; " /* save buff */
				"movl %%ebx, %1; " /* save y */
				"movl %7, %%ecx; " /* x */
				"movl $0x80000000, %%ebx; "
				"line22: "
				"movl %%eax, (%%edi); "
				"addl %%edx, %%ebx; "
				"jc line23; "
				"addl %%ebp, %%edi; "
				"loop line22; "
				"jmp line24; "
				"line23: "
				"addl %%esi, %%edi; "
				"loop line22; "
				"line24: "
				"movl %10, %%ebx; " /* dy */
				"movl %11, %%ecx; " /* stroke */
				"line25: "
				"movl %%eax, (%%edi); "
				"addl %%ebx, %%edi; "
				"loop line25; "
				"movl %2, %%edi; " /* restore buff */
				"subl %9, %%edi; " /* buff -= dx */
				"movl %8, %%ebx; " /* restore y */
				"decl %%ebx; "
				"jnz line21; "
				"addl %%esi, %%edi; "
				"movl %11, %%ebx; " /* stroke */
				"decl %%ebx; "
				"jz line30; "
				"line26: "
				"movl %%edi, %0; " /* save buff */
				"movl %%ebx, %1; " /* save y */
				"movl %7, %%ecx; " /* x */
				"movl $0x80000000, %%ebx; "
				"line27: "
				"movl %%eax, (%%edi); "
				"addl %%edx, %%ebx; "
				"jc line28; "
				"addl %%ebp, %%edi; "
				"loop line27; "
				"jmp line29; "
				"line28: "
				"addl %%esi, %%edi; "
				"loop line27; "
				"line29: "
				"movl %2, %%edi; "  /* restore buff */
				"addl %10, %%edi; " /* buff += dy */
				"movl %8, %%ebx; "  /* restore y */
				"decl %%ebx; "
				"jnz line26; "
				"line30: "
				: "=m"(buff), "=m"(y)
				: "m"(buff), "m"(data), "m"(a), "m"(sx), "m"(sy), "m"(x), "m"(y), "m"(dx), "m"(dy), "m"(stroke), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
			break;

		default:
			return -EINVAL;
	}

	return EOK;
}


int soft_rect(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, unsigned int color)
{
	void *data;

#ifdef GRAPH_VERIFY_ARGS
	if ((x + dx > graph->width) || (y + dy > graph->height))
		return -EINVAL;
#endif

	if (!dx || !dy)
		return EOK;

	data = soft_data(graph, x, y);
	x = graph->depth * (graph->width - dx);

	switch (graph->depth) {
		case 1:
			__asm__ volatile(
				"movl %0, %%edi; " /* data */
				"movl %1, %%esi; " /* x */
				"movl %2, %%ebx; " /* dx */
				"movl %3, %%edx; " /* dy */
				"movl %4, %%eax; " /* color */
				"rect1: "
				"movl %%ebx, %%ecx; "
				"rep stosb; "
				"addl %%esi, %%edi; "
				"decl %%edx; "
				"jnz rect1; "
				:
				: "m"(data), "m"(x), "m"(dx), "m"(dy), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory");
			break;

		case 2:
			__asm__ volatile(
				"movl %0, %%edi; " /* data */
				"movl %1, %%esi; " /* x */
				"movl %2, %%ebx; " /* dx */
				"movl %3, %%edx; " /* dy */
				"movl %4, %%eax; " /* color */
				"rect2: "
				"movl %%ebx, %%ecx; "
				"rep stosw; "
				"addl %%esi, %%edi; "
				"decl %%edx; "
				"jnz rect2; "
				:
				: "m"(data), "m"(x), "m"(dx), "m"(dy), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory");
			break;

		case 4:
			__asm__ volatile(
				"movl %0, %%edi; " /* data */
				"movl %1, %%esi; " /* x */
				"movl %2, %%ebx; " /* dx */
				"movl %3, %%edx; " /* dy */
				"movl %4, %%eax; " /* color */
				"rect3: "
				"movl %%ebx, %%ecx; "
				"rep stosl; "
				"addl %%esi, %%edi; "
				"decl %%edx; "
				"jnz rect3; "
				:
				: "m"(data), "m"(x), "m"(dx), "m"(dy), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory");
			break;

		default:
			return -EINVAL;
	}

	return EOK;
}


int soft_fill(graph_t *graph, unsigned int x, unsigned int y, unsigned int color, graph_fill_t type)
{
	int *stack, *sp, rx, y1, y2, ret = EOK;
	unsigned int gh, gw;
	void *gd, *data;

#ifdef GRAPH_VERIFY_ARGS
	if ((x > graph->width) || (y > graph->height))
		return -EINVAL;
#endif

	if ((sp = stack = malloc(0x10000)) == NULL)
		return -ENOMEM;

	/* Push (x, x, y + 1, 1) */
	if (y + 1 < graph->height) {
		*sp++ = x;
		*sp++ = x;
		*sp++ = y + 1;
		*sp++ = 1;
	}

	/* Push (x, x, y, -1) */
	*sp++ = x;
	*sp++ = x;
	*sp++ = y;
	*sp++ = -1;

	/* Save graph data on stack */
	data = soft_data(graph, x, y);
	gh = graph->height;
	gw = graph->width;
	gd = graph->data;

	switch (graph->depth) {
		case 1:
			switch (type) {
				case GRAPH_FILL_FLOOD:
					__asm__ volatile(
						"movl %17, %%edx; "    /* color */
						"movl %6, %%esi; "     /* data */
						"movb (%%esi), %%cl; " /* cmpcolor */
						"cmpb %%cl, %%dl; "
						"jz fill15; "
						"fill1: "
						"movl %11, %%ebp; " /* sp */
						"cmpl %10, %%ebp; " /* sp == stack */
						"jz fill15; "
						"movl -4(%%ebp), %%esi; "  /* pop dy */
						"movl -8(%%ebp), %%eax; "  /* pop y */
						"movl -12(%%ebp), %%edi; " /* pop rx */
						"movl -16(%%ebp), %%ebx; " /* pop x */
						"subl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %%eax, %%ebp; "
						"addl %%esi, %%ebp; "
						"js fill2; "
						"cmpl %9, %%ebp; "
						"jge fill2; "
						"jmp fill3; "
						"fill2: "
						"movl $-1, %%ebp; "
						"fill3: "
						"movl %%ebp, %5; " /* y + dy */
						"movl %%eax, %%ebp; "
						"subl %%esi, %%ebp; "
						"js fill4; "
						"cmpl %9, %%ebp; "
						"jge fill4; "
						"jmp fill5; "
						"fill4: "
						"movl $-1, %%ebp; "
						"fill5: "
						"movl %%ebp, %4; " /* y - dy */
						"movl %%esi, %3; " /* dy */
						"negl %%esi; "
						"movl %%esi, %2; " /* -dy */
						"movl %7, %%esi; "
						"movl %%edx, %%ebp; "
						"mull %8; "
						"movl %%ebp, %%edx; "
						"addl %%ebx, %%eax; "
						"leal (%%esi, %%eax), %%esi; "
						"movl %%ebx, %%eax; "
						"orl %%eax, %%eax; "
						"jz fill13; "
						"cmpb %%cl, (%%esi); "
						"jnz fill13; "
						"movl %%esi, %%ebp; "
						"fill6: "
						"decl %%esi; "
						"cmpb %%cl, (%%esi); "
						"jnz fill7; "
						"movb %%dl, (%%esi); "
						"decl %%eax; "
						"jnz fill6; "
						"fill7: "
						"movl %%ebp, %%esi; "
						"cmpl %%eax, %%ebx; "
						"jz fill13; "
						"movl %15, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill8; "
						"decl %%ebx; "
						"movl %%edi, %1; "
						"movl %11, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %15, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y - dy */
						"movl %13, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"incl %%ebx; "
						"fill8: "
						"cmpl %%ebx, %%edi; "
						"jc fill1; "
						"fill9: "
						"cmpl %8, %%ebx; "
						"jge fill10; "
						"cmpb %%cl, (%%esi); "
						"jnz fill10; "
						"movb %%dl, (%%esi); "
						"incl %%esi; "
						"incl %%ebx; "
						"jmp fill9; "
						"fill10: "
						"decl %%ebx; "
						"movl %16, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill11; "
						"movl %%edi, %1; "
						"movl %11, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %16, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y + dy */
						"movl %14, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"fill11: "
						"cmpl %%ebx, %%edi; "
						"jnc fill12; "
						"movl %15, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill12; "
						"movl %%edi, %1 ; "
						"incl %%edi; "
						"movl %11, %%ebp; "
						"movl %%edi, (%%ebp); "  /* push rx + 1 */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %15, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y - dy */
						"movl %13, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"fill12: "
						"incl %%esi; "
						"addl $2, %%ebx; "
						"fill13: "
						"cmpl %%ebx, %%edi; "
						"jc fill14; "
						"cmpb %%cl, (%%esi); "
						"jz fill14; "
						"incl %%esi; "
						"incl %%ebx; "
						"jmp fill13; "
						"fill14: "
						"movl %%ebx, %%eax; "
						"jmp fill8; "
						"fill15: "
						: "=m"(sp), "=m"(rx), "=m"(y1), "=m"(y2), "=m"(x), "=m"(y)
						: "m"(data), "m"(gd), "m"(gw), "m"(gh), "m"(stack), "m"(sp), "m"(rx), "m"(y1), "m"(y2), "m"(x), "m"(y), "m"(color)
						: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
					break;

				case GRAPH_FILL_BOUND:
					__asm__ volatile(
						"movl %14, %%edx; " /* color */
						"fill16: "
						"movl %9, %%ebp; " /* sp */
						"cmpl %8, %%ebp; " /* sp == stack */
						"jz fill30; "
						"movl -4(%%ebp), %%esi; "  /* pop dy */
						"movl -8(%%ebp), %%eax; "  /* pop y */
						"movl -12(%%ebp), %%edi; " /* pop rx */
						"movl -16(%%ebp), %%ebx; " /* pop x */
						"subl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %%eax, %%ebp; "
						"addl %%esi, %%ebp; "
						"js fill17; "
						"cmpl %8, %%ebp; "
						"jge fill17; "
						"jmp fill18; "
						"fill17: "
						"movl $-1, %%ebp; "
						"fill18: "
						"movl %%ebp, %4; " /* y + dy */
						"movl %%eax, %%ebp; "
						"subl %%esi, %%ebp; "
						"js fill19; "
						"cmpl %7, %%ebp; "
						"jge fill19; "
						"jmp fill20; "
						"fill19: "
						"movl $-1, %%ebp; "
						"fill20: "
						"movl %%ebp, %3; " /* y - dy */
						"movl %%esi, %2; " /* dy */
						"negl %%esi; "
						"movl %%esi, %1; " /* -dy */
						"movl %5, %%esi; "
						"movl %%edx, %%ebp; "
						"mull %6; "
						"movl %%ebp, %%edx; "
						"addl %%ebx, %%eax; "
						"leal (%%esi, %%eax), %%esi; "
						"movl %%ebx, %%eax; "
						"orl %%eax, %%eax; "
						"jz fill28; "
						"cmpb %%dl, (%%esi); "
						"jz fill28; "
						"movl %%esi, %%ebp; "
						"fill21: "
						"decl %%esi; "
						"cmpb %%dl, (%%esi); "
						"jz fill22; "
						"movb %%dl, (%%esi); "
						"decl %%eax; "
						"jnz fill21; "
						"fill22: "
						"movl %%ebp, %%esi; "
						"cmpl %%eax, %%ebx; "
						"jz fill28; "
						"movl %13, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill23; "
						"decl %%ebx; "
						"movl %9, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y - dy */
						"movl %10, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"incl %%ebx; "
						"fill23: "
						"cmpl %%ebx, %%edi; "
						"jc fill16; "
						"fill24: "
						"cmpl %6, %%ebx; "
						"jge fill25; "
						"cmpb %%dl, (%%esi); "
						"jz fill25; "
						"movb %%dl, (%%esi); "
						"incl %%esi; "
						"incl %%ebx; "
						"jmp fill24; "
						"fill25: "
						"decl %%ebx; "
						"movl %13, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill26; "
						"movl %9, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y + dy */
						"movl %11, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"fill26: "
						"cmpl %%ebx, %%edi; "
						"jnc fill27; "
						"movl %12, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill27; "
						"incl %%edi; "
						"movl %9, %%ebp; "
						"movl %%edi, (%%ebp); "  /* push rx + 1 */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y - dy */
						"movl %10, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"decl %%edi; "
						"fill27: "
						"incl %%esi; "
						"addl $2, %%ebx; "
						"fill28: "
						"cmpl %%ebx, %%edi; "
						"jc fill29; "
						"cmpb %%dl, (%%esi); "
						"jnz fill29; "
						"incl %%esi; "
						"incl %%ebx; "
						"jmp fill28; "
						"fill29: "
						"movl %%ebx, %%eax; "
						"jmp fill23; "
						"fill30: "
						: "=m"(sp), "=m"(y1), "=m"(y2), "=m"(x), "=m"(y)
						: "m"(gd), "m"(gw), "m"(gh), "m"(stack), "m"(sp), "m"(y1), "m"(y2), "m"(x), "m"(y), "m"(color)
						: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
					break;

				default:
					ret = -EINVAL;
			}
			break;

		case 2:
			switch (type) {
				case GRAPH_FILL_FLOOD:
					__asm__ volatile(
						"movl %17, %%edx; "    /* color */
						"movl %6, %%esi; "     /* data */
						"movw (%%esi), %%cx; " /* cmpcolor */
						"cmpw %%cx, %%dx; "
						"jz fill45; "
						"fill31: "
						"movl %11, %%ebp; " /* sp */
						"cmpl %10, %%ebp; " /* sp == stack */
						"jz fill45; "
						"movl -4(%%ebp), %%esi; "  /* pop dy */
						"movl -8(%%ebp), %%eax; "  /* pop y */
						"movl -12(%%ebp), %%edi; " /* pop rx */
						"movl -16(%%ebp), %%ebx; " /* pop x */
						"subl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %%eax, %%ebp; "
						"addl %%esi, %%ebp; "
						"js fill32; "
						"cmpl %9, %%ebp; "
						"jge fill32; "
						"jmp fill33; "
						"fill32: "
						"movl $-1, %%ebp; "
						"fill33: "
						"movl %%ebp, %5; " /* y + dy */
						"movl %%eax, %%ebp; "
						"subl %%esi, %%ebp; "
						"js fill34; "
						"cmpl %9, %%ebp; "
						"jge fill34; "
						"jmp fill35; "
						"fill34: "
						"movl $-1, %%ebp; "
						"fill35: "
						"movl %%ebp, %4; " /* y - dy */
						"movl %%esi, %3; " /* dy */
						"negl %%esi; "
						"movl %%esi, %2; " /* -dy */
						"movl %7, %%esi; "
						"movl %%edx, %%ebp; "
						"mull %8; "
						"movl %%ebp, %%edx; "
						"addl %%ebx, %%eax; "
						"leal (%%esi, %%eax, 2), %%esi; "
						"movl %%ebx, %%eax; "
						"orl %%eax, %%eax; "
						"jz fill43; "
						"cmpw %%cx, (%%esi); "
						"jnz fill43; "
						"movl %%esi, %%ebp; "
						"fill36: "
						"subl $2, %%esi; "
						"cmpw %%cx, (%%esi); "
						"jnz fill37; "
						"movw %%dx, (%%esi); "
						"decl %%eax; "
						"jnz fill36; "
						"fill37: "
						"movl %%ebp, %%esi; "
						"cmpl %%eax, %%ebx; "
						"jz fill43; "
						"movl %15, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill38; "
						"decl %%ebx; "
						"movl %%edi, %1; "
						"movl %11, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %15, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y - dy */
						"movl %13, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"incl %%ebx; "
						"fill38: "
						"cmpl %%ebx, %%edi; "
						"jc fill31; "
						"fill39: "
						"cmpl %8, %%ebx; "
						"jge fill40; "
						"cmpw %%cx, (%%esi); "
						"jnz fill40; "
						"movw %%dx, (%%esi); "
						"addl $2, %%esi; "
						"incl %%ebx; "
						"jmp fill39; "
						"fill40: "
						"decl %%ebx; "
						"movl %16, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill41; "
						"movl %%edi, %1; "
						"movl %11, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %16, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y + dy */
						"movl %14, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"fill41: "
						"cmpl %%ebx, %%edi; "
						"jnc fill42; "
						"movl %15, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill42; "
						"movl %%edi, %1 ; "
						"incl %%edi; "
						"movl %11, %%ebp; "
						"movl %%edi, (%%ebp); "  /* push rx + 1 */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %15, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y - dy */
						"movl %13, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"fill42: "
						"addl $2, %%esi; "
						"addl $2, %%ebx; "
						"fill43: "
						"cmpl %%ebx, %%edi; "
						"jc fill44; "
						"cmpw %%cx, (%%esi); "
						"jz fill44; "
						"addl $2, %%esi; "
						"incl %%ebx; "
						"jmp fill43; "
						"fill44: "
						"movl %%ebx, %%eax; "
						"jmp fill38; "
						"fill45: "
						: "=m"(sp), "=m"(rx), "=m"(y1), "=m"(y2), "=m"(x), "=m"(y)
						: "m"(data), "m"(gd), "m"(gw), "m"(gh), "m"(stack), "m"(sp), "m"(rx), "m"(y1), "m"(y2), "m"(x), "m"(y), "m"(color)
						: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
					break;

				case GRAPH_FILL_BOUND:
					__asm__ volatile(
						"movl %14, %%edx; " /* color */
						"fill46: "
						"movl %9, %%ebp; " /* sp */
						"cmpl %8, %%ebp; " /* sp == stack */
						"jz fill60; "
						"movl -4(%%ebp), %%esi; "  /* pop dy */
						"movl -8(%%ebp), %%eax; "  /* pop y */
						"movl -12(%%ebp), %%edi; " /* pop rx */
						"movl -16(%%ebp), %%ebx; " /* pop x */
						"subl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %%eax, %%ebp; "
						"addl %%esi, %%ebp; "
						"js fill47; "
						"cmpl %8, %%ebp; "
						"jge fill47; "
						"jmp fill48; "
						"fill47: "
						"movl $-1, %%ebp; "
						"fill48: "
						"movl %%ebp, %4; " /* y + dy */
						"movl %%eax, %%ebp; "
						"subl %%esi, %%ebp; "
						"js fill49; "
						"cmpl %7, %%ebp; "
						"jge fill49; "
						"jmp fill50; "
						"fill49: "
						"movl $-1, %%ebp; "
						"fill50: "
						"movl %%ebp, %3; " /* y - dy */
						"movl %%esi, %2; " /* dy */
						"negl %%esi; "
						"movl %%esi, %1; " /* -dy */
						"movl %5, %%esi; "
						"movl %%edx, %%ebp; "
						"mull %6; "
						"movl %%ebp, %%edx; "
						"addl %%ebx, %%eax; "
						"leal (%%esi, %%eax, 2), %%esi; "
						"movl %%ebx, %%eax; "
						"orl %%eax, %%eax; "
						"jz fill58; "
						"cmpw %%dx, (%%esi); "
						"jz fill58; "
						"movl %%esi, %%ebp; "
						"fill51: "
						"subl $2, %%esi; "
						"cmpw %%dx, (%%esi); "
						"jz fill52; "
						"movw %%dx, (%%esi); "
						"decl %%eax; "
						"jnz fill51; "
						"fill52: "
						"movl %%ebp, %%esi; "
						"cmpl %%eax, %%ebx; "
						"jz fill58; "
						"movl %13, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill53; "
						"decl %%ebx; "
						"movl %9, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y - dy */
						"movl %10, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"incl %%ebx; "
						"fill53: "
						"cmpl %%ebx, %%edi; "
						"jc fill46; "
						"fill54: "
						"cmpl %6, %%ebx; "
						"jge fill55; "
						"cmpw %%dx, (%%esi); "
						"jz fill55; "
						"movw %%dx, (%%esi); "
						"addl $2, %%esi; "
						"incl %%ebx; "
						"jmp fill54; "
						"fill55: "
						"decl %%ebx; "
						"movl %13, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill56; "
						"movl %9, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y + dy */
						"movl %11, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"fill56: "
						"cmpl %%ebx, %%edi; "
						"jnc fill57; "
						"movl %12, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill57; "
						"incl %%edi; "
						"movl %9, %%ebp; "
						"movl %%edi, (%%ebp); "  /* push rx + 1 */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y - dy */
						"movl %10, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"decl %%edi; "
						"fill57: "
						"addl $2, %%esi; "
						"addl $2, %%ebx; "
						"fill58: "
						"cmpl %%ebx, %%edi; "
						"jc fill59; "
						"cmpw %%dx, (%%esi); "
						"jnz fill59; "
						"addl $2, %%esi; "
						"incl %%ebx; "
						"jmp fill58; "
						"fill59: "
						"movl %%ebx, %%eax; "
						"jmp fill53; "
						"fill60: "
						: "=m"(sp), "=m"(y1), "=m"(y2), "=m"(x), "=m"(y)
						: "m"(gd), "m"(gw), "m"(gh), "m"(stack), "m"(sp), "m"(y1), "m"(y2), "m"(x), "m"(y), "m"(color)
						: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
					break;

				default:
					ret = -EINVAL;
			}
			break;

		case 4:
			switch (type) {
				case GRAPH_FILL_FLOOD:
					__asm__ volatile(
						"movl %17, %%edx; "     /* color */
						"movl %6, %%esi; "      /* data */
						"movl (%%esi), %%ecx; " /* cmpcolor */
						"cmpl %%ecx, %%edx; "
						"jz fill75; "
						"fill61: "
						"movl %11, %%ebp; " /* sp */
						"cmpl %10, %%ebp; " /* sp == stack */
						"jz fill75; "
						"movl -4(%%ebp), %%esi; "  /* pop dy */
						"movl -8(%%ebp), %%eax; "  /* pop y */
						"movl -12(%%ebp), %%edi; " /* pop rx */
						"movl -16(%%ebp), %%ebx; " /* pop x */
						"subl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %%eax, %%ebp; "
						"addl %%esi, %%ebp; "
						"js fill62; "
						"cmpl %9, %%ebp; "
						"jge fill62; "
						"jmp fill63; "
						"fill62: "
						"movl $-1, %%ebp; "
						"fill63: "
						"movl %%ebp, %5; " /* y + dy */
						"movl %%eax, %%ebp; "
						"subl %%esi, %%ebp; "
						"js fill64; "
						"cmpl %9, %%ebp; "
						"jge fill64; "
						"jmp fill65; "
						"fill64: "
						"movl $-1, %%ebp; "
						"fill65: "
						"movl %%ebp, %4; " /* y - dy */
						"movl %%esi, %3; " /* dy */
						"negl %%esi; "
						"movl %%esi, %2; " /* -dy */
						"movl %7, %%esi; "
						"movl %%edx, %%ebp; "
						"mull %8; "
						"movl %%ebp, %%edx; "
						"addl %%ebx, %%eax; "
						"leal (%%esi, %%eax, 4), %%esi; "
						"movl %%ebx, %%eax; "
						"orl %%eax, %%eax; "
						"jz fill73; "
						"cmpl %%ecx, (%%esi); "
						"jnz fill73; "
						"movl %%esi, %%ebp; "
						"fill66: "
						"subl $4, %%esi; "
						"cmpl %%ecx, (%%esi); "
						"jnz fill67; "
						"movl %%edx, (%%esi); "
						"decl %%eax; "
						"jnz fill66; "
						"fill67: "
						"movl %%ebp, %%esi; "
						"cmpl %%eax, %%ebx; "
						"jz fill73; "
						"movl %15, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill68; "
						"decl %%ebx; "
						"movl %%edi, %1; "
						"movl %11, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %15, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y - dy */
						"movl %13, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"incl %%ebx; "
						"fill68: "
						"cmpl %%ebx, %%edi; "
						"jc fill61; "
						"fill69: "
						"cmpl %8, %%ebx; "
						"jge fill70; "
						"cmpl %%ecx, (%%esi); "
						"jnz fill70; "
						"movl %%edx, (%%esi); "
						"addl $4, %%esi; "
						"incl %%ebx; "
						"jmp fill69; "
						"fill70: "
						"decl %%ebx; "
						"movl %16, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill71; "
						"movl %%edi, %1; "
						"movl %11, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %16, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y + dy */
						"movl %14, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"fill71: "
						"cmpl %%ebx, %%edi; "
						"jnc fill72; "
						"movl %15, %%ebp; "
						"cmpl $-1, %%ebp; "
						"je fill72; "
						"movl %%edi, %1 ; "
						"incl %%edi; "
						"movl %11, %%ebp; "
						"movl %%edi, (%%ebp); "  /* push rx + 1 */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %15, %%edi; "
						"movl %%edi, 8(%%ebp); " /* push y - dy */
						"movl %13, %%edi; "
						"movl %%edi, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %12, %%edi; "
						"fill72: "
						"addl $4, %%esi; "
						"addl $2, %%ebx; "
						"fill73: "
						"cmpl %%ebx, %%edi; "
						"jc fill74; "
						"cmpl %%ecx, (%%esi); "
						"jz fill74; "
						"addl $4, %%esi; "
						"incl %%ebx; "
						"jmp fill73; "
						"fill74: "
						"movl %%ebx, %%eax; "
						"jmp fill68; "
						"fill75: "
						: "=m"(sp), "=m"(rx), "=m"(y1), "=m"(y2), "=m"(x), "=m"(y)
						: "m"(data), "m"(gd), "m"(gw), "m"(gh), "m"(stack), "m"(sp), "m"(rx), "m"(y1), "m"(y2), "m"(x), "m"(y), "m"(color)
						: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
					break;

				case GRAPH_FILL_BOUND:
					__asm__ volatile(
						"movl %14, %%edx; " /* color */
						"fill76: "
						"movl %9, %%ebp; " /* sp */
						"cmpl %8, %%ebp; " /* sp == stack */
						"jz fill90; "
						"movl -4(%%ebp), %%esi; "  /* pop dy */
						"movl -8(%%ebp), %%eax; "  /* pop y */
						"movl -12(%%ebp), %%edi; " /* pop rx */
						"movl -16(%%ebp), %%ebx; " /* pop x */
						"subl $16, %%ebp; "
						"movl %%ebp, %0; "
						"movl %%eax, %%ebp; "
						"addl %%esi, %%ebp; "
						"js fill77; "
						"cmpl %8, %%ebp; "
						"jge fill77; "
						"jmp fill78; "
						"fill77: "
						"movl $-1, %%ebp; "
						"fill78: "
						"movl %%ebp, %4; " /* y + dy */
						"movl %%eax, %%ebp; "
						"subl %%esi, %%ebp; "
						"js fill79; "
						"cmpl %7, %%ebp; "
						"jge fill79; "
						"jmp fill80; "
						"fill79: "
						"movl $-1, %%ebp; "
						"fill80: "
						"movl %%ebp, %3; " /* y - dy */
						"movl %%esi, %2; " /* dy */
						"negl %%esi; "
						"movl %%esi, %1; " /* -dy */
						"movl %5, %%esi; "
						"movl %%edx, %%ebp; "
						"mull %6; "
						"movl %%ebp, %%edx; "
						"addl %%ebx, %%eax; "
						"leal (%%esi, %%eax, 4), %%esi; "
						"movl %%ebx, %%eax; "
						"orl %%eax, %%eax; "
						"jz fill88; "
						"cmpl %%edx, (%%esi); "
						"jz fill88; "
						"movl %%esi, %%ebp; "
						"fill81: "
						"subl $4, %%esi; "
						"cmpl %%edx, (%%esi); "
						"jz fill82; "
						"movl %%edx, (%%esi); "
						"decl %%eax; "
						"jnz fill81; "
						"fill82: "
						"movl %%ebp, %%esi; "
						"cmpl %%eax, %%ebx; "
						"jz fill88; "
						"movl %13, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill83; "
						"decl %%ebx; "
						"movl %9, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y - dy */
						"movl %10, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"incl %%ebx; "
						"fill83: "
						"cmpl %%ebx, %%edi; "
						"jc fill76; "
						"fill84: "
						"cmpl %6, %%ebx; "
						"jge fill85; "
						"cmpl %%edx, (%%esi); "
						"jz fill85; "
						"movl %%edx, (%%esi); "
						"addl $4, %%esi; "
						"incl %%ebx; "
						"jmp fill84; "
						"fill85: "
						"decl %%ebx; "
						"movl %13, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill86; "
						"movl %9, %%ebp; "
						"movl %%eax, (%%ebp); "  /* push lx */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y + dy */
						"movl %11, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"fill86: "
						"cmpl %%ebx, %%edi; "
						"jnc fill87; "
						"movl %12, %%ecx; "
						"cmpl $-1, %%ecx; "
						"je fill87; "
						"incl %%edi; "
						"movl %9, %%ebp; "
						"movl %%edi, (%%ebp); "  /* push rx + 1 */
						"movl %%ebx, 4(%%ebp); " /* push x - 1 */
						"movl %%ecx, 8(%%ebp); " /* push y - dy */
						"movl %10, %%ecx; "
						"movl %%ecx, 12(%%ebp); " /* push -dy */
						"addl $16, %%ebp; "
						"movl %%ebp, %0; "
						"decl %%edi; "
						"fill87: "
						"addl $4, %%esi; "
						"addl $2, %%ebx; "
						"fill88: "
						"cmpl %%ebx, %%edi; "
						"jc fill89; "
						"cmpl %%edx, (%%esi); "
						"jnz fill89; "
						"addl $4, %%esi; "
						"incl %%ebx; "
						"jmp fill88; "
						"fill89: "
						"movl %%ebx, %%eax; "
						"jmp fill83; "
						"fill90: "
						: "=m"(sp), "=m"(y1), "=m"(y2), "=m"(x), "=m"(y)
						: "m"(gd), "m"(gw), "m"(gh), "m"(stack), "m"(sp), "m"(y1), "m"(y2), "m"(x), "m"(y), "m"(color)
						: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
					break;

				default:
					ret = -EINVAL;
			}
			break;

		default:
			ret = -EINVAL;
	}

	free(stack);
	return ret;
}


int soft_print(graph_t *graph, unsigned int x, unsigned int y, unsigned char dx, unsigned char dy, const unsigned char *bmp, unsigned char width, unsigned char height, unsigned char span, unsigned int color)
{
	unsigned int line[0x100];
	int sl, dl;
	void *data;

#ifdef GRAPH_VERIFY_ARGS
	if (!dx || !dy || (x + dx > graph->width) || (y + dy > graph->height) || (dx > width) || (dy > height) || ((span << 3) < width))
		return -EINVAL;
#endif

	data = soft_data(graph, x, y);
	x = ((unsigned int)dx * 0x10000 / (unsigned int)width * 0xffff) >> 24;
	y = ((unsigned int)dy * 0x10000 / (unsigned int)height * 0xffff) >> 24;
	sl = (int)span - ((((int)width + 31) >> 3) & 0xfc);
	dl = graph->depth * (graph->width - dx);

	switch (graph->depth) {
		case 1:
			__asm__ volatile(
				"movl %9, %%esi; " /* bmp */
				"movl %1, %%edi; " /* data */
				"movb %7, %%cl; "  /* dx */
				"cld; "
				"char1: "
				"leal %2, %%ebp; " /* line */
				"movb %%cl, %%ch; "
				"xorl %%edx, %%edx; "
				"char2: "
				"movl %%edx, (%%ebp); "
				"addl $4, %%ebp; "
				"decb %%ch; "
				"jnz char2; "
				"char3: "
				"leal %2, %%ebp; " /* line */
				"lodsl; "
				"movb $32, %%ch; "
				"movb %5, %%bl; "  /* x */
				"movb %10, %%bh; " /* width */
				"char4: "
				"shrl $1, %%eax; "
				"adcl $0x10000, %%edx; "
				"addb %%bl, %%bh; "
				"jc char5; "
				"decb %%ch; "
				"jnz char4; "
				"lodsl; "
				"movb $32, %%ch; "
				"jmp char4; "
				"char5: "
				"addl %%edx, (%%ebp); "
				"addl $4, %%ebp; "
				"xorl %%edx, %%edx; "
				"decb %%cl; "
				"jz char6; "
				"decb %%ch; "
				"jnz char4; "
				"lodsl; "
				"movb $32, %%ch; "
				"jmp char4; "
				"char6: "
				"movb %7, %%cl; "  /* dx */
				"addl %3, %%esi; " /* bmp += sl */
				"movb %6, %%al; "  /* y */
				"addb %%al, %0; "
				"jnc char3; "
				"movl %12, %%eax; " /* color */
				"movb %%cl, %%ch; " /* dx */
				"char7: "
				"subl $4, %%ebp; "
				"movl (%%ebp), %%ebx; "
				"leal (, %%ebx, 2), %%edx; "
				"shrl $16, %%ebx; "
				"cmpw %%bx, %%dx; "
				"jc char8; "
				"stosb; "
				"jmp char9; "
				"char8: "
				"incl %%edi; "
				"char9: "
				"decb %%ch; "
				"jnz char7; "
				"movb %5, %%bl; "  /* x */
				"movb %10, %%bh; " /* width */
				"addl %4, %%edi; " /* data += dl */
				"decb %8; "        /* dy-- */
				"jnz char1; "
				: "=m"(height)
				: "m"(data), "m"(line), "m"(sl), "m"(dl), "m"(x), "m"(y), "m"(dx), "m"(dy), "m"(bmp), "m"(width), "m"(height), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
			break;

		case 2:
			__asm__ volatile(
				"movl %9, %%esi; " /* bmp */
				"movl %1, %%edi; " /* data */
				"movb %7, %%cl; "  /* dx */
				"cld; "
				"char10: "
				"leal %2, %%ebp; " /* line */
				"movb %%cl, %%ch; "
				"xorl %%edx, %%edx; "
				"char11: "
				"movl %%edx, (%%ebp); "
				"addl $4, %%ebp; "
				"decb %%ch; "
				"jnz char11; "
				"char12: "
				"leal %2, %%ebp; " /* line */
				"lodsl; "
				"movb $32, %%ch; "
				"movb %5, %%bl; "  /* x */
				"movb %10, %%bh; " /* width */
				"char13: "
				"shrl $1, %%eax; "
				"adcl $0x10000, %%edx; "
				"addb %%bl, %%bh; "
				"jc char14; "
				"decb %%ch; "
				"jnz char13; "
				"lodsl; "
				"movb $32, %%ch; "
				"jmp char13; "
				"char14: "
				"addl %%edx, (%%ebp); "
				"addl $4, %%ebp; "
				"xorl %%edx, %%edx; "
				"decb %%cl; "
				"jz char15; "
				"decb %%ch; "
				"jnz char13; "
				"lodsl; "
				"movb $32, %%ch; "
				"jmp char13; "
				"char15: "
				"movb %7, %%cl; "  /* dx */
				"addl %3, %%esi; " /* bmp += sl */
				"movb %6, %%al; "  /* y */
				"addb %%al, %0; "
				"jnc char12; "
				"movl %12, %%eax; " /* color */
				"movb %%cl, %%ch; " /* dx */
				"char16: "
				"subl $4, %%ebp; "
				"movl (%%ebp), %%ebx; "
				"leal (, %%ebx, 2), %%edx; "
				"shrl $16, %%ebx; "
				"cmpw %%bx, %%dx; "
				"jc char17; "
				"stosw; "
				"jmp char18; "
				"char17: "
				"addl $2, %%edi; "
				"char18: "
				"decb %%ch; "
				"jnz char16; "
				"movb %5, %%bl; "  /* x */
				"movb %10, %%bh; " /* width */
				"addl %4, %%edi; " /* data += dl */
				"decb %8; "        /* dy-- */
				"jnz char10; "
				: "=m"(height)
				: "m"(data), "m"(line), "m"(sl), "m"(dl), "m"(x), "m"(y), "m"(dx), "m"(dy), "m"(bmp), "m"(width), "m"(height), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
			break;

		case 4:
			__asm__ volatile(
				"movl %9, %%esi; " /* bmp */
				"movl %1, %%edi; " /* data */
				"movb %7, %%cl; "  /* dx */
				"cld; "
				"char19: "
				"leal %2, %%ebp; " /* line */
				"movb %%cl, %%ch; "
				"xorl %%edx, %%edx; "
				"char20: "
				"movl %%edx, (%%ebp); "
				"addl $4, %%ebp; "
				"decb %%ch; "
				"jnz char20; "
				"char21: "
				"leal %2, %%ebp; " /* line */
				"lodsl; "
				"movb $32, %%ch; "
				"movb %5, %%bl; "  /* x */
				"movb %10, %%bh; " /* width */
				"char22: "
				"shrl $1, %%eax; "
				"adcl $0x10000, %%edx; "
				"addb %%bl, %%bh; "
				"jc char23; "
				"decb %%ch; "
				"jnz char22; "
				"lodsl; "
				"movb $32, %%ch; "
				"jmp char22; "
				"char23: "
				"addl %%edx, (%%ebp); "
				"addl $4, %%ebp; "
				"xorl %%edx, %%edx; "
				"decb %%cl; "
				"jz char24; "
				"decb %%ch; "
				"jnz char22; "
				"lodsl; "
				"movb $32, %%ch; "
				"jmp char22; "
				"char24: "
				"movb %7, %%cl; "  /* dx */
				"addl %3, %%esi; " /* bmp += sl */
				"movb %6, %%al; "  /* y */
				"addb %%al, %0; "
				"jnc char21; "
				"movl %12, %%eax; " /* color */
				"movb %%cl, %%ch; " /* dx */
				"char25: "
				"subl $4, %%ebp; "
				"movl (%%ebp), %%ebx; "
				"leal (, %%ebx, 2), %%edx; "
				"shrl $16, %%ebx; "
				"cmpw %%bx, %%dx; "
				"jc char26; "
				"stosl; "
				"jmp char27; "
				"char26: "
				"addl $4, %%edi; "
				"char27: "
				"decb %%ch; "
				"jnz char25; "
				"movb %5, %%bl; "  /* x */
				"movb %10, %%bh; " /* width */
				"addl %4, %%edi; " /* data += dl */
				"decb %8; "        /* dy-- */
				"jnz char19; "
				: "=m"(height)
				: "m"(data), "m"(line), "m"(sl), "m"(dl), "m"(x), "m"(y), "m"(dx), "m"(dy), "m"(bmp), "m"(width), "m"(height), "m"(color)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
			break;
	}

	return EOK;
}


int soft_move(graph_t *graph, unsigned int x, unsigned int y, unsigned int dx, unsigned int dy, int mx, int my)
{
	void *src, *dst;

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
	x = graph->depth * dx;
	y = graph->depth * graph->width - x;

	if (dst < src) {
		if (x > 8) {
			__asm__ volatile(
				"movl %0, %%esi; " /* src */
				"movl %1, %%edi; " /* dst */
				"movl %2, %%ebp; " /* x */
				"movl %3, %%eax; " /* y */
				"movl %4, %%edx; " /* dy */
				"cld; "
				"move1: "
				"movl %%esi, %%ecx; "
				"negl %%ecx; "
				"andl $3, %%ecx; "
				"movl %%ecx, %%ebx; "
				"jz move2; "
				"rep movsb; "
				"move2: "
				"movl %%ebp, %%ecx; "
				"subl %%ebx, %%ecx; "
				"movl %%ecx, %%ebx; "
				"shrl $2, %%ecx; "
				"rep movsl; "
				"movl %%ebx, %%ecx; "
				"andl $3, %%ecx; "
				"jz move3; "
				"rep movsb; "
				"move3: "
				"addl %%eax, %%esi; "
				"addl %%eax, %%edi; "
				"decl %%edx; "
				"jnz move1; "
				:
				: "m"(src), "m"(dst), "m"(x), "m"(y), "m"(dy)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
		}
		else {
			__asm__ volatile(
				"movl %0, %%esi; " /* src */
				"movl %1, %%edi; " /* dst */
				"movl %2, %%ebx; " /* x */
				"movl %3, %%eax; " /* y */
				"movl %4, %%edx; " /* dy */
				"cld; "
				"move4: "
				"movl %%ebx, %%ecx; "
				"rep movsb; "
				"addl %%eax, %%esi; "
				"addl %%eax, %%edi; "
				"decl %%edx; "
				"jnz move4; "
				:
				: "m"(src), "m"(dst), "m"(x), "m"(y), "m"(dy)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory");
		}
	}
	else {
		dx = graph->depth * ((dy - 1) * graph->width + dx) - 1;
		src += dx;
		dst += dx;

		if (x > 8) {
			__asm__ volatile(
				"movl %0, %%esi; " /* src */
				"movl %1, %%edi; " /* dst */
				"movl %2, %%ebp; " /* x */
				"movl %3, %%eax; " /* y */
				"movl %4, %%edx; " /* dy */
				"std; "
				"move5: "
				"movl %%esi, %%ecx; "
				"incl %%ecx; "
				"andl $3, %%ecx; "
				"movl %%ecx, %%ebx; "
				"jz move6; "
				"rep movsb; "
				"move6: "
				"movl %%ebp, %%ecx; "
				"subl %%ebx, %%ecx; "
				"movl %%ecx, %%ebx; "
				"shrl $2, %%ecx; "
				"subl $3, %%esi; "
				"subl $3, %%edi; "
				"rep movsl; "
				"addl $3, %%esi; "
				"addl $3, %%edi; "
				"movl %%ebx, %%ecx; "
				"andl $3, %%ecx; "
				"jz move7; "
				"rep movsb; "
				"move7: "
				"subl %%eax, %%esi; "
				"subl %%eax, %%edi; "
				"decl %%edx; "
				"jnz move5; "
				"cld; "
				:
				: "m"(src), "m"(dst), "m"(x), "m"(y), "m"(dy)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
		}
		else {
			__asm__ volatile(
				"movl %0, %%esi; " /* src */
				"movl %1, %%edi; " /* dst */
				"movl %2, %%ebx; " /* x */
				"movl %3, %%eax; " /* y */
				"movl %4, %%edx; " /* dy */
				"std; "
				"move8: "
				"movl %%ebx, %%ecx; "
				"rep movsb; "
				"subl %%eax, %%esi; "
				"subl %%eax, %%edi; "
				"decl %%edx; "
				"jnz move8; "
				"cld; "
				:
				: "m"(src), "m"(dst), "m"(x), "m"(y), "m"(dy)
				: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory");
		}
	}

	return EOK;
}


int soft_copy(graph_t *graph, const void *src, void *dst, unsigned int dx, unsigned int dy, unsigned int srcspan, unsigned int dstspan)
{
#ifdef GRAPH_VERIFY_ARGS
	if ((srcspan < graph->depth * dx) || (dstspan < graph->depth * dx))
		return -EINVAL;
#endif

	if (!dx || !dy)
		return EOK;

	dx *= graph->depth;
	srcspan -= dx;
	dstspan -= dx;

	if (dx > 8) {
		__asm__ volatile(
			"movl %0, %%esi; " /* src */
			"movl %1, %%edi; " /* dst */
			"movl %2, %%eax; " /* dx */
			"movl %3, %%edx; " /* dy */
			"cld; "
			"copy1: "
			"movl %%esi, %%ecx; "
			"negl %%ecx; "
			"andl $3, %%ecx; "
			"movl %%ecx, %%ebx; "
			"jz copy2; "
			"rep movsb; "
			"copy2: "
			"movl %%eax, %%ecx; "
			"subl %%ebx, %%ecx; "
			"shrl $2, %%ecx; "
			"rep movsl; "
			"movl %%ebx, %%ecx; "
			"andl $3, %%ecx; "
			"jz copy3; "
			"rep movsb; "
			"copy3: "
			"addl %4, %%esi; " /* src += srcspan */
			"addl %5, %%edi; " /* dst += dstspan */
			"decl %%edx; "
			"jnz copy1; "
			:
			: "m"(src), "m"(dst), "m"(dx), "m"(dy), "m"(srcspan), "m"(dstspan)
			: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory");
	}
	else {
		__asm__ volatile(
			"movl %0, %%esi; " /* src */
			"movl %1, %%edi; " /* dst */
			"movl %2, %%eax; " /* dx */
			"movl %3, %%edx; " /* dy */
			"movl %4, %%ebx; " /* srcspan */
			"movl %5, %%ebp; " /* dstspan */
			"cld; "
			"copy4: "
			"movl %%eax, %%ecx; "
			"rep movsb; "
			"addl %%ebx, %%esi; "
			"addl %%ebp, %%edi; "
			"decl %%edx; "
			"jnz copy4; "
			:
			: "m"(src), "m"(dst), "m"(dx), "m"(dy), "m"(srcspan), "m"(dstspan)
			: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
	}

	return EOK;
}
