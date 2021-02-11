/*
 * Phoenix-RTOS
 *
 * Software operations
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

	if (!stroke || ((int)x + dx < 0) || ((int)y + dy < 0) ||
		(x + stroke > graph->width) || (x + dx + stroke > graph->width) ||
		(y + stroke > graph->height) || (y + dy + stroke > graph->height))
		return -EINVAL;

	if (!dx && !dy)
		return graph_rect(x, y, stroke, stroke, color, 0);

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

	if ((x + dx > graph->width) || (y + dy > graph->height))
		return -EINVAL;

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


int soft_fill(graph_t *graph, unsigned int x, unsigned int y, unsigned int color, fill_t type)
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

	if ((x > graph->width) || (y > graph->height))
		return -EINVAL;

	data = soft_data(graph, x, y);
	switch (type) {
	case FILL_FLOOD:
		if ((cmpcolor = soft_get(graph, data)) == color)
			return EOK;
		cmp = cmp_flood;
		break;

	case FILL_BOUND:
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
				for (; (x <= rx) && !cmp(*(uint8_t *)data, cmpcolor); data++, x++);

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

				for (x++, data++; (x <= rx) && !cmp(*(uint8_t *)data, cmpcolor); data++, x++);
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
				for (; (x <= rx) && !cmp(*(uint16_t *)data, cmpcolor); data += 2, x++);

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

				for (x++, data += 2; (x <= rx) && !cmp(*(uint16_t *)data, cmpcolor); data += 2, x++);
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
				for (; (x <= rx) && !cmp(*(uint32_t *)data, cmpcolor); data += 4, x++);

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

				for (x++, data += 4; (x <= rx) && !cmp(*(uint32_t *)data, cmpcolor); data += 4, x++);
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


int soft_move(graph_t *graph, char *arg)
{
  static int width = graph->width;
  static int depth = graph->depth;
  static u16 fbsel = graph->fbsel;

  asm {
    push ds
    push es
    push fs
    mov ax, ds
    mov fs, ax
    mov ax, word ptr [fbsel]           // video segment
    mov ds, ax
    mov es, ax
    mov ebx, fs:[arg]                  // function arguments
    mov eax, fs:[ebx + 4]              // src y
    mul dword ptr fs:[width]           // picture width
    add eax, fs:[ebx]                  // src x
    mov ecx, dword ptr fs:[depth]      // color size
    mul ecx
    mov esi, eax                       // src address

    mov eax, fs:[ebx + 20]             // move y
    mul dword ptr fs:[width]           // picture width
    add eax, fs:[ebx + 16]             // move x
    mul ecx                            // color size
    mov edi, eax                       // move address
    add edi, esi                       // dst address

    mov eax, fs:[ebx + 8]              // src width
    mul ecx                            // color size
    mov ebp, eax
    mov eax, dword ptr fs:[width]      // picture width
    mul ecx                            // color size
    sub eax, ebp
    cmp esi, edi
    jc move5                           // copy backward

    mov edx, fs:[ebx + 12]             // src height
    cld
    cmp ebp, 8                         // small object
    jnc move2
  }
move1:
  asm {
    mov ecx, ebp
    rep
    movsb                              // copy line
    add esi, eax
    add edi, eax
    dec edx
    jnz move1
    pop fs
    pop es
    pop ds
  }
  return GRAPH_SUCCESS;

move2:
  asm {
    mov ecx, esi
    neg ecx
    and ecx, 3
    mov ebx, ecx
    jz move3
    rep
    movsb                              // align source address
  }
move3:
  asm {
    mov ecx, ebp
    sub ecx, ebx
    mov ebx, ecx
    shr ecx, 2
    rep
    movsd                              // copy line
    mov ecx, ebx
    and ecx, 3
    jz move4
    rep
    movsb                              // finish line
  }
move4:
  asm {
    add esi, eax
    add edi, eax
    dec edx
    jnz move2
    pop fs
    pop es
    pop ds
  }
  return GRAPH_SUCCESS;

move5:
  asm {
    push eax
    mov eax, fs:[ebx + 12]             // src height
    dec eax
    mul dword ptr fs:[width]           // picture width
    add eax, fs:[ebx + 8]              // src width
    mul ecx                            // color size
    dec eax
    add esi, eax                       // src last
    add edi, eax                       // dst last
    pop eax
    mov edx, fs:[ebx + 12]             // src height
    std
    cmp ebp, 8                         // small object
    jnc move7
  }
move6:
  asm {
    mov ecx, ebp
    rep
    movsb                              // copy line
    sub esi, eax
    sub edi, eax
    dec edx
    jnz move6
    pop fs
    pop es
    pop ds
    cld
  }
  return GRAPH_SUCCESS;

move7:
  asm {
    mov ecx, esi
    inc ecx
    and ecx, 3
    mov ebx, ecx
    jz move8
    rep
    movsb                              // align source address
  }
move8:
  asm {
    mov ecx, ebp
    sub ecx, ebx
    mov ebx, ecx
    shr ecx, 2
    sub esi, 3
    sub edi, 3
    rep
    movsd                              // copy line
    add esi, 3
    add edi, 3
    mov ecx, ebx
    and ecx, 3
    jz move9
    rep
    movsb                              // finish line
  }
move9:
  asm {
    sub esi, eax
    sub edi, eax
    dec edx
    jnz move7
    pop fs
    pop es
    pop ds
    cld
  }
  return GRAPH_SUCCESS;
}


int soft_copyin(graph_t *graph, char *arg)
{
  static int depth = graph->depth;
  static u16 fbsel = graph->fbsel;

  asm {
    push ds
    push es
    push fs
    mov ax, ds
    mov fs, ax
    mov ax, word ptr [fbsel]           // video segment
    mov ds, ax
    mov es, ax
    mov ebx, fs:[arg]                  // function arguments
    mov esi, fs:[ebx]                  // copy area source address
    mov edi, fs:[ebx + 4]              // copy area destination address
    mov eax, fs:[ebx + 8]              // copy area width
    mul dword ptr fs:[depth]           // color size
    mov edx, fs:[ebx + 12]             // copy area height
    sub fs:[ebx + 16], eax             // copy area source span
    sub fs:[ebx + 20], eax             // copy area destination span
    cld
    cmp eax, 8                         // small object
    jnc copyin2
  }
copyin1:
  asm {
    mov ecx, eax
    rep
    movsb                              // copy line
    add esi, fs:[ebx + 16]
    add edi, fs:[ebx + 20]
    dec edx
    jnz copyin1
    pop fs
    pop es
    pop ds
  }
  return GRAPH_SUCCESS;

copyin2:
  asm {
    mov ecx, esi
    neg ecx
    and ecx, 3
    mov ebp, ecx
    jz copyin3
    rep
    movsb                              // align source address
  }
copyin3:
  asm {
    mov ecx, eax
    sub ecx, ebp
    mov ebp, ecx
    shr ecx, 2
    rep
    movsd                              // copy line
    mov ecx, ebp
    and ecx, 3
    jz copyin4
    rep
    movsb                              // finish line
  }
copyin4:
  asm {
    add esi, fs:[ebx + 16]             // copy area source span
    add edi, fs:[ebx + 20]             // copy area destination span
    dec edx
    jnz copyin2
    pop fs
    pop es
    pop ds
  }
  return GRAPH_SUCCESS;
}


int soft_copyto(graph_t *graph, char *arg)
{
  static int depth = graph->depth;
  static u16 fbsel = graph->fbsel;

  asm {
    push es
    mov ax, word ptr [fbsel]           // video segment
    mov es, ax
    mov ebx, [arg]                     // function arguments
    mov esi, [ebx]                     // copy area source address
    mov edi, [ebx + 4]                 // copy area destination address
    mov eax, [ebx + 8]                 // copy area width
    mul dword ptr [depth]              // color size
    mov edx, [ebx + 12]                // copy area height
    sub [ebx + 16], eax                // copy area source span
    sub [ebx + 20], eax                // copy area destination span
    cld
    cmp eax, 8                         // small object
    jnc copyto2
  }
copyto1:
  asm {
    mov ecx, eax
    rep
    movsb                              // copy line
    add esi, [ebx + 16]                // copy area source span
    add edi, [ebx + 20]                // copy area destination span
    dec edx
    jnz copyto1
    pop es
  }
  return GRAPH_SUCCESS;

copyto2:
  asm {
    mov ecx, edi
    neg ecx
    and ecx, 3
    mov ebp, ecx
    jz copyto3
    rep
    movsb                              // align destination address
  }
copyto3:
  asm {
    mov ecx, eax
    sub ecx, ebp
    mov ebp, ecx
    shr ecx, 2
    rep
    movsd                              // copy line
    mov ecx, ebp
    and ecx, 3
    jz copyto4
    rep
    movsb                              // finish line
  }
copyto4:
  asm {
    add esi, [ebx + 16]                // copy area source span
    add edi, [ebx + 20]                // copy area destination span
    dec edx
    jnz copyto2
    pop es
  }
  return GRAPH_SUCCESS;
}


int soft_copyfrom(graph_t *graph, char *arg)
{
  static int depth = graph->depth;
  static u16 fbsel = graph->fbsel;

  asm {
    push ds
    mov ax, word ptr [fbsel]           // video segment
    mov ds, ax
    mov ebx, [arg]                     // function arguments
    mov esi, es:[ebx]                  // copy area source address
    mov edi, es:[ebx + 4]              // copy area destination address
    mov eax, es:[ebx + 8]              // copy area width
    mul dword ptr es:[depth]           // color size
    mov edx, es:[ebx + 12]             // copy area height
    sub es:[ebx + 16], eax             // copy area source span
    sub es:[ebx + 20], eax             // copy area destination span
    cld
    cmp eax, 8                         // small object
    jnc copyfrom2
  }
copyfrom1:
  asm {
    mov ecx, eax
    rep
    movsb                              // copy line
    add esi, es:[ebx + 16]             // copy area source span
    add edi, es:[ebx + 20]             // copy area destination span
    dec edx
    jnz copyfrom1
    pop ds
  }
  return GRAPH_SUCCESS;

copyfrom2:
  asm {
    mov ecx, esi
    neg ecx
    and ecx, 3
    mov ebp, ecx
    jz copyfrom3
    rep
    movsb                              // align source address
  }
copyfrom3:
  asm {
    mov ecx, eax
    sub ecx, ebp
    mov ebp, ecx
    shr ecx, 2
    rep
    movsd                              // copy line
    mov ecx, ebp
    and ecx, 3
    jz copyfrom4
    rep
    movsb                              // finish line
  }
copyfrom4:
  asm {
    add esi, es:[ebx + 16]             // copy area source span
    add edi, es:[ebx + 20]             // copy area destination span
    dec edx
    jnz copyfrom2
    pop ds
  }
  return GRAPH_SUCCESS;
}


int soft_copyout(graph_t *graph, char *arg)
{
  static int depth = graph->depth;

  asm {
    mov ebx, [arg]                     // function arguments
    mov esi, [ebx]                     // copy area source address
    mov edi, [ebx + 4]                 // copy area destination address
    mov eax, [ebx + 8]                 // copy area width
    mul dword ptr [depth]              // color size
    mov edx, [ebx + 12]                // copy area height
    sub [ebx + 16], eax                // copy area source span
    sub [ebx + 20], eax                // copy area destination span
    cld
    cmp eax, 8                         // small object
    jnc copyout2
  }
copyout1:
  asm {
    mov ecx, eax
    rep
    movsb                              // copy line
    add esi, [ebx + 16]
    add edi, [ebx + 20]
    dec edx
    jnz copyout1
  }
  return GRAPH_SUCCESS;

copyout2:
  asm {
    mov ecx, edi
    neg ecx
    and ecx, 3
    mov ebp, ecx
    jz copyout3
    rep
    movsb                              // align destination address
  }
copyout3:
  asm {
    mov ecx, eax
    sub ecx, ebp
    mov ebp, ecx
    shr ecx, 2
    rep
    movsd                              // copy line
    mov ecx, ebp
    and ecx, 3
    jz copyout4
    rep
    movsb                              // finish line
  }
copyout4:
  asm {
    add esi, [ebx + 16]                // copy area source span
    add edi, [ebx + 20]                // copy area destination span
    dec edx
    jnz copyout2
  }
  return GRAPH_SUCCESS;
}


int soft_char(graph_t *graph, char *arg)
{
  static int width = graph->width;
  static int depth = graph->depth;
  static u16 fbsel = graph->fbsel;

  static char line[4096];

  asm {
    push es
    mov ax, word ptr [fbsel]           // video segment
    mov es, ax
    mov ebx, [arg]                     // function arguments
    mov esi, [ebx]                     // source address
    mov edi, [ebx + 4]                 // destination address
    mov edx, [ebx + 20]                // destination dy
    push edx                           // lines
    push dword ptr [ebx + 28]          // color
    mov eax, [ebx + 8]                 // source dx
    add eax, 31
    shr eax, 3                         // div by 8 bits
    and eax, 0fffffffch
    neg eax
    add eax, [ebx + 24]                // span
    push eax                           // corrected source span
    dec edx
    mov eax, 0ffffffffh
    mov ecx, [ebx + 12]                // source dy
    div ecx
    shr eax, 24
    mov ah, cl
    push eax                           // vertical subpixel
    mov eax, dword ptr [width]         // picture witdh
    mov ecx, [ebx + 16]                // destination dx
    sub eax, ecx
    mul dword ptr [depth]              // color size
    push eax                           // corrected destination span
    mov edx, ecx
    dec edx
    mov eax, 0ffffffffh
    mov ebx, [ebx + 8]
    div ebx                            // source dx
    shr eax, 24
    mov ah, bl
    cld
    push eax                           // horizontal subpixel
    push ecx                           // destination dx
  }
char1:
  asm {
    lea ebp, dword ptr [line]          // line buffer
    mov ch, cl                         // destination width
    xor edx, edx
  }
char2:
  asm {
    mov [ebp], edx                     // clear line buffer
    add ebp, 4
    dec ch
    jnz char2
  }
char3:
  asm {
    lea ebp, dword ptr [line]          // line buffer
    lodsd                              // first double word
    mov ch, 32
    mov ebx, [esp + 4]                 // horizontal subpixel
  }
char4:
  asm {
    shr eax, 1                         // shift bit
    adc edx, 10000h                    // count bits and sets
    add bh, bl                         // subpixel
    jc char5
    dec ch
    jnz char4                          // next bit
    lodsd                              // next double word
    mov ch, 32
    jmp char4
  }
char5:
  asm {
    add [ebp], edx                     // add bits and sets
    add ebp, 4
    xor edx, edx                       // clear bits and sets
    dec cl
    jz char6                           // next source line
    dec ch
    jnz char4                          // next bit
    lodsd                              // next double word
    mov ch, 32
    jmp char4
  }
char6:
  asm {
    mov ecx, [esp]                     // destination dx
    add esi, [esp + 16]                // source span
    mov eax, [esp + 12]                // vertical subpixel
    add ah, al
    mov [esp + 12], eax
    jnc char3                          // next source line
    mov eax, [esp + 20]                // color
    lea ebp, dword ptr [line]          // line buffer
    mov ch, cl
    cmp dword ptr [depth], 2
    jz char9                           // 16bit color
  }
char7:
  asm {
    mov ebx, [ebp]                     // line buffer
    add ebp, 4
    lea edx, [ebx * 2]
    shr ebx, 16
    cmp dx, bx
    jc char8                           // transparent
    stosb
    dec ch
    jnz char7
    add edi, [esp + 8]                 // destination span
    dec dword ptr [esp + 24]           // lines
    jnz char1
    add esp, 28
    pop es
  }
  return GRAPH_SUCCESS;

char8:
  asm {
    inc edi
    dec ch
    jnz char7
    mov ebx, [esp + 4]                 // horizontal subpixel
    add edi, [esp + 8]                 // destination span
    dec dword ptr [esp + 24]           // lines
    jnz char1
    add esp, 28
    pop es
  }
  return GRAPH_SUCCESS;

char9:
  asm {
    mov ebx, [ebp]                     // line buffer
    add ebp, 4
    lea edx, [ebx * 2]
    shr ebx, 16
    cmp dx, bx
    jc char10                          // transparent
    stosb
    dec ch
    jnz char9
    mov ebx, [esp + 4]                 // horizontal subpixel
    add edi, [esp + 8]                 // destination span
    dec dword ptr [esp + 24]           // lines
    jnz char1
    add esp, 28
    pop es
  }
  return GRAPH_SUCCESS;

char10:
  asm {
    inc edi
    dec ch
    jnz char9
    mov ebx, [esp + 4]                 // horizontal subpixel
    add edi, [esp + 8]                 // destination span
    dec dword ptr [esp + 24]           // lines
    jnz char1
    add esp, 28
    pop es
  }
  return GRAPH_SUCCESS;
}
#endif
