/*
 * Phoenix-RTOS
 *
 * Software operations (IA32)
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

#include "soft.h"


int soft_line(graph_t *graph, unsigned int x, unsigned int y, int dx, int dy, unsigned int stroke, unsigned int color)
{
	void *data, *buff;
	unsigned int i, n, a;
	int sx, sy;

	if (!stroke || ((int)x + dx < 0) || ((int)y + dy < 0) ||
		(x + stroke > graph->width) || (x + dx + stroke > graph->width) ||
		(y + stroke > graph->height) || (y + dy + stroke > graph->height))
		return -EINVAL;

	if (!dx && !dy)
		return graph_rect(x, y, stroke, stroke, color, 0);

	data = graph->data + graph->depth * ((y + stroke - 1) * graph->width + x);
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
		__asm__ volatile (
		"movl %4, %%edi; "  /* data */
		"movl %5, %%edx; "  /* a */
		"movl %7, %%esi; "  /* sx */
		"movl %8, %%ebp; "  /* sy */
		"movl %11, %%ebx; " /* stroke */
		"movl %12, %%eax; " /* color */
		"line1: "
		"movl %%edi, %0; "  /* save buff */
		"movl %%ebx, %1; "  /* save i */
		"movl %6, %%ecx; "  /* n */
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
		"movl %2, %%edi; "  /* restore buff */
		"subl %9, %%edi; "  /* buff -= dx */
		"movl %3, %%ebx; "  /* restore i */
		"decl %%ebx; "
		"jnz line1; "
		"addl %%esi, %%edi; "
		"movl %11, %%ebx; " /* stroke */
		"decl %%ebx; "
		"jz line10; "
		"line6: "
		"movl %%edi, %0; "  /* save buff */
		"movl %%ebx, %1; "  /* save i */
		"movl %6, %%ecx; "  /* n */
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
		"movl %3, %%ebx; "  /* restore i */
		"decl %%ebx; "
		"jnz line6; "
		"line10: "
		: "=m" (buff), "=m" (i)
		: "m" (buff), "m" (i), "m" (data), "m" (a), "m" (n), "m" (sx), "m" (sy), "m" (dx), "m" (dy), "m" (stroke), "m" (color)
		: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
		break;

	case 2:
		__asm__ volatile (
		"movl %4, %%edi; "  /* data */
		"movl %5, %%edx; "  /* a */
		"movl %7, %%esi; "  /* sx */
		"movl %8, %%ebp; "  /* sy */
		"movl %11, %%ebx; " /* stroke */
		"movl %12, %%eax; " /* color */
		"line11: "
		"movl %%edi, %0; "  /* save buff */
		"movl %%ebx, %1; "  /* save i */
		"movl %6, %%ecx; "  /* n */
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
		"movl %2, %%edi; "  /* restore buff */
		"subl %9, %%edi; "  /* buff -= dx */
		"movl %3, %%ebx; "  /* restore i */
		"decl %%ebx; "
		"jnz line11; "
		"addl %%esi, %%edi; "
		"movl %11, %%ebx; " /* stroke */
		"decl %%ebx; "
		"jz line20; "
		"line16: "
		"movl %%edi, %0; "  /* save buff */
		"movl %%ebx, %1; "  /* save i */
		"movl %6, %%ecx; "  /* n */
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
		"movl %3, %%ebx; "  /* restore i */
		"decl %%ebx; "
		"jnz line16; "
		"line20: "
		: "=m" (buff), "=m" (i)
		: "m" (buff), "m" (i), "m" (data), "m" (a), "m" (n), "m" (sx), "m" (sy), "m" (dx), "m" (dy), "m" (stroke), "m" (color)
		: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
		break;

	case 4:
		__asm__ volatile (
		"movl %4, %%edi; "  /* data */
		"movl %5, %%edx; "  /* a */
		"movl %7, %%esi; "  /* sx */
		"movl %8, %%ebp; "  /* sy */
		"movl %11, %%ebx; " /* stroke */
		"movl %12, %%eax; " /* color */
		"line21: "
		"movl %%edi, %0; "  /* save buff */
		"movl %%ebx, %1; "  /* save i */
		"movl %6, %%ecx; "  /* n */
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
		"movl %2, %%edi; "  /* restore buff */
		"subl %9, %%edi; "  /* buff -= dx */
		"movl %3, %%ebx; "  /* restore i */
		"decl %%ebx; "
		"jnz line21; "
		"addl %%esi, %%edi; "
		"movl %11, %%ebx; " /* stroke */
		"decl %%ebx; "
		"jz line30; "
		"line26: "
		"movl %%edi, %0; "  /* save buff */
		"movl %%ebx, %1; "  /* save i */
		"movl %6, %%ecx; "  /* n */
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
		"movl %3, %%ebx; "  /* restore i */
		"decl %%ebx; "
		"jnz line26; "
		"line30: "
		: "=m" (buff), "=m" (i)
		: "m" (buff), "m" (i), "m" (data), "m" (a), "m" (n), "m" (sx), "m" (sy), "m" (dx), "m" (dy), "m" (stroke), "m" (color)
		: "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "memory");
		break;

	default:
		return -EINVAL;
	}

	return EOK;
}
