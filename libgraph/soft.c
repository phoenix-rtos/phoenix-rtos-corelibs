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

#include "soft.h"


int soft_line(graph_t *graph, int x, int y, int dx, int dy, unsigned int stroke, unsigned int color)
{
	uintptr_t data, buff;
	uint32_t a, acc, tmp;
	int i, j, n;

	if (!stroke || (x < 0) || (x + dx < 0) || (y < 0) || (y + dy < 0) ||
		(x + stroke > graph->width) || (x + dx + stroke > graph->width) ||
		(y + stroke > graph->height) || (y + dy + stroke > graph->height))
		return -EINVAL;

	if (!dx && !dy)
		return graph_rect(x, y, stroke, stroke, color, 0);

	data = (uintptr_t)graph->data + graph->depth * ((y + stroke - 1) * graph->width + x);
	y = graph->width * graph->depth;
	x = graph->depth;

	if (dx < 0) {
		data += (stroke - 1) * x;
		dx = -dx;
		x = -x;
	}

	if (dy < 0) {
		data -= (stroke - 1) * y;
		dy = -dy;
		y = -y;
	}

	if (dx > dy) {
		a = dy * 0x10000 / dx * 0xffff;
		y += x;
		n = y;
		y = x;
		x = n;
		n = dx;
		dx = x - y;
		dy = y;
	}
	else {
		a = dx * 0x10000 / dy * 0xffff;
		x += y;
		n = dy;
		dx = y;
		dy = x - y;
	}

	switch (graph->depth) {
	case 1:
		for (i = 0; i < stroke; i++) {
			buff = data - i * dx;
			acc = 0x80000000;

			for (j = 0; j < n; j++) {
				*(uint8_t *)buff = color;
				tmp = acc;
				acc += a;
				buff += (acc < tmp) ? x : y;
			}

			for (j = 0; j < stroke; j++) {
				*(uint8_t *)buff = color;
				buff += dy;
			}
		}

		data -= (int)(stroke - 1) * dx;
		for (i = 1; i < stroke; i++) {
			buff = data + i * dy;
			acc = 0x80000000;

			for (j = 0; j < n; j++) {
				*(uint8_t *)buff = color;
				tmp = acc;
				acc += a;
				buff += (acc < tmp) ? x : y;
			}
		}
		break;

	case 2:
		for (i = 0; i < stroke; i++) {
			buff = data - i * dx;
			acc = 0x80000000;

			for (j = 0; j < n; j++) {
				*(uint16_t *)buff = color;
				tmp = acc;
				acc += a;
				buff += (acc < tmp) ? x : y;
			}

			for (j = 0; j < stroke; j++) {
				*(uint16_t *)buff = color;
				buff += dy;
			}
		}

		data -= (int)(stroke - 1) * dx;
		for (i = 1; i < stroke; i++) {
			buff = data + i * dy;
			acc = 0x80000000;

			for (j = 0; j < n; j++) {
				*(uint16_t *)buff = color;
				tmp = acc;
				acc += a;
				buff += (acc < tmp) ? x : y;
			}
		}
		break;

	case 4:
		for (i = 0; i < stroke; i++) {
			buff = data - i * dx;
			acc = 0x80000000;

			for (j = 0; j < n; j++) {
				*(uint32_t *)buff = color;
				tmp = acc;
				acc += a;
				buff += (acc < tmp) ? x : y;
			}

			for (j = 0; j < stroke; j++) {
				*(uint32_t *)buff = color;
				buff += dy;
			}
		}

		data -= (int)(stroke - 1) * dx;
		for (i = 1; i < stroke; i++) {
			buff = data + i * dy;
			acc = 0x80000000;

			for (j = 0; j < n; j++) {
				*(uint32_t *)buff = color;
				tmp = acc;
				acc += a;
				buff += (acc < tmp) ? x : y;
			}
		}
		break;

	default:
		return -EINVAL;
	}

	return EOK;
}


int sys_graph_rect(graph_t *graph, int x, int y, unsigned int dx, unsigned int dy, unsigned int color)
{
        int fx, fy, lx, ly;
        
        fx = (x < 0) ? 0 : x;
        fy = (y < 0) ? 0 : y;
        lx = (x + (int)dx) > (int)graph->width ? (int)graph->width : x + dx;
        ly = (y + (int)dy) > (int)graph->height ? (int)graph->height : y + dy;
        
        for (y = fy; y < ly; y++)
                for (x = fx; x < lx; x++) {
        
                        switch (graph->depth) {
                        case 1:
                                *(uint8_t *)(graph->data + y * graph->width * graph->depth + x * graph->depth) = (uint8_t)color;
                                break;
                        case 2:
                                *(uint16_t *)(graph->data + y * graph->width * graph->depth + x * graph->depth) = (uint16_t)color;
                                break;
                        case 4:
                                *(uint32_t *)(graph->data + y * graph->width * graph->depth + x * graph->depth) = color;
                                break;
                        }
                }
        return EOK;
}


int soft_fill(graph_t *graph, char *arg)
{
	int a, b, c, d; /* min, max, address, pitch */
        
	d = graph->depth * graph->width;
	a = (int)graph->data + d;
	b = (int)graph->data + (img->height - 1) * d;
        c = (int)img->data + d * y + img->graph->pixelsize * x;
        if ((c < a) || (c > b))
                return ERR_ARG;
        switch (img->graph->pixelsize) {
        case 1:
                __asm__ volatile (
                        "movl %0, %%esi; "   /* min */
                        "movl %1, %%edi; "   /* max */
                        "movl %2, %%edx; "   /* address */
                        "movl %3, %%ecx; "   /* pitch */
                        "movl %4, %%eax; "   /* color */
                        "fill10: "
                        "movl %%edx, %%ebx; "
                        "xorl %%edx, %%edx; "
                        "fill11: "
                        "decl %%ebx; "
                        "cmpl %%esi, %%ebx; "
                        "jc fill15; "
                        "cmpb %%al, (%%ebx); "
                        "jnz fill11; "
                        "fill12: "
                        "incl %%ebx; "
                        "cmpl %%edi, %%ebx; "
                        "jnc fill15; "
                        "cmpb %%al, (%%ebx); "
                        "jz fill13; "
                        "movb %%al, (%%ebx); "
                        "orl %%edx, %%edx; "
                        "jnz fill12; "
                        "cmpb %%al, (%%ebx, %%ecx); "
                        "jz fill12; "
                        "leal (%%ebx, %%ecx), %%edx; "
                        "jmp fill12; "
                        "fill13: "
                        "orl %%edx, %%edx; "
                        "jnz fill10; "
                        "negl %%ecx; "
                        "jns fill14; "
                        "movl %2, %%edx; "   /* address */
                        "addl %%ecx, %%edx; "
                        "cmpb %%al, (%%edx); "
                        "jnz fill10; "
                        "fill14: "
                        "movl $0, %2; "   /* return */
                        "fill15: "
                :
                : "m" (a), "m" (b), "m" (c), "m" (d), "m" (color)
                : "eax", "ebx", "ecx", "edx", "esi", "edi");
                return c ? ERR_ARG : 0;
        case 2:
                __asm__ volatile (
                        "movl %0, %%esi; "
                        "movl %1, %%edi; "
                        "movl %2, %%edx; "
                        "movl %3, %%ecx; "
                        "movl %4, %%eax; "
                        "fill20: "
                        "movl %%edx, %%ebx; "
                        "xorl %%edx, %%edx; "
                        "fill21: "
                        "subl $2, %%ebx; "
                        "cmpl %%esi, %%ebx; "
                        "jc fill25; "
                        "cmpw %%ax, (%%ebx); "
                        "jnz fill21; "
                        "fill22: "
                        "addl $2, %%ebx; "
                        "cmpl %%edi, %%ebx; "
                        "jnc fill25; "
                        "cmpw %%ax, (%%ebx); "
                        "jz fill23; "
                        "movw %%ax, (%%ebx); "
                        "orl %%edx, %%edx; "
                        "jnz fill22; "
                        "cmpw %%ax, (%%ebx, %%ecx); "
                        "jz fill22; "
                        "leal (%%ebx, %%ecx), %%edx; "
                        "jmp fill22; "
                        "fill23: "
                        "orl %%edx, %%edx; "
                        "jnz fill20; "
                        "negl %%ecx; "
                        "jns fill24; "
                        "movl %2, %%edx; "
                        "addl %%ecx, %%edx; "
                        "cmpw %%ax, (%%edx); "
                        "jnz fill20; "
                        "fill24: "
                        "movl $0, %2; "
                        "fill25: "
                :
                : "m" (a), "m" (b), "m" (c), "m" (d), "m" (color)
                : "eax", "ebx", "ecx", "edx", "esi", "edi");
                return c ? ERR_ARG : 0;
        case 4:
                __asm__ volatile (
                        "movl %0, %%esi; "
                        "movl %1, %%edi; "
                        "movl %2, %%edx; "
                        "movl %3, %%ecx; "
                        "movl %4, %%eax; "
                        "fill40: "
                        "movl %%edx, %%ebx; "
                        "xorl %%edx, %%edx; "
                        "fill41: "
                        "subl $4, %%ebx; "
                        "cmpl %%esi, %%ebx; "
                        "jc fill45; "
                        "cmpl %%eax, (%%ebx); "
                        "jnz fill41; "
                        "fill42: "
                        "addl $4, %%ebx; "
                        "cmpl %%edi, %%ebx; "
                        "jnc fill45; "
                        "cmpl %%eax, (%%ebx); "
                        "jz fill43; "
                        "movl %%eax, (%%ebx); "
                        "orl %%edx, %%edx; "
                        "jnz fill42; "
                        "cmpl %%eax, (%%ebx, %%ecx); "
                        "jz fill42; "
                        "leal (%%ebx, %%ecx), %%edx; "
                        "jmp fill42; "
                        "fill43: "
                        "orl %%edx, %%edx; "
                        "jnz fill40; "
                        "negl %%ecx; "
                        "jns fill44; "
                        "movl %2, %%edx; "
                        "addl %%ecx, %%edx; "
                        "cmpl %%eax, (%%edx); "
                        "jnz fill40; "
                        "fill44: "
                        "movl $0, %2; "
                        "fill45: "
                :
                : "m" (a), "m" (b), "m" (c), "m" (d), "m" (color)
                : "eax", "ebx", "ecx", "edx", "esi", "edi");
                return c ? ERR_ARG : 0;
        }
        return ERR_ARG;


  static int width = graph->width;
  static int depth = graph->depth;
  static int size = graph->memsz - graph->cursorsz;
  static u16 fbsel = graph->fbsel;

  asm {
    push es
    mov ax, word ptr [fbsel]           // video segment
    mov es, ax
    mov ebx, [arg]                     // function arguments
    mov eax, [ebx + 4]                 // fill y
    mul dword ptr [width]              // picture width
    add eax, [ebx]                     // fill x
    mov ecx, dword ptr [depth]         // color size
    mul ecx
    mov esi, eax
    push eax                           // save fill address
    mov eax, dword ptr [width]         // picture width
    mul ecx                            // color size
    mov edi, eax                       // vertical step
    mov ebp, edi
    neg ebp                            // revers vertical step
    mov eax, [ebx + 8]                 // color
    mov edx, -1
    cmp ecx, 2
    jz fill7                           // 16 bit color
  }
fill1:
  asm {
    dec esi
    cmp esi, dword ptr [size]
    jnc fill13                         // filled out
    cmp es:[esi], al
    jnz fill1
    add ebp, esi
  }
fill2:
  asm {
    inc esi
    inc ebp
    cmp ebp, dword ptr [size]
    jnc fill13                         // filled out
    cmp es:[esi], al
    jz fill3
    mov es:[esi], al
    cmp es:[ebp], al
    jz fill2
    cmp edx, -1
    jnz fill2
    mov edx, ebp
    jmp fill2
  }
fill3:
  asm {
    mov ebp, edi
    neg ebp
    mov esi, edx
    mov edx, -1
    cmp esi, edx
    jnz fill1
    pop esi
    add esi, edi
  }
fill4:
  asm {
    dec esi
    cmp esi, dword ptr [size]
    jnc fill14                         // filled out
    cmp es:[esi], al
    jnz fill4
    add edi, esi
  }
fill5:
  asm {
    inc esi
    inc edi
    cmp edi, dword ptr [size]
    jnc fill14                         // filled out
    cmp es:[esi], al
    jz fill6
    mov es:[esi], al
    cmp es:[edi], al
    jz fill5
    cmp edx, -1
    jnz fill5
    mov edx, edi
    jmp fill5
  }
fill6:
  asm {
    mov edi, ebp
    neg edi
    mov esi, edx
    mov edx, -1
    cmp esi, edx
    jnz fill4
    pop es
  }
  return GRAPH_SUCCESS;

fill7:
  asm {
    sub esi, 2
    cmp esi, dword ptr [size]
    jnc fill13                         // filled out
    cmp es:[esi], ax
    jnz fill7
    add ebp, esi
  }
fill8:
  asm {
    add esi, 2
    add ebp, 2
    cmp ebp, dword ptr [size]
    jnc fill13                         // filled out
    cmp es:[esi], ax
    jz fill9
    mov es:[esi], ax
    cmp es:[ebp], ax
    jz fill8
    cmp edx, -1
    jnz fill8
    mov edx, ebp
    jmp fill8
  }
fill9:
  asm {
    mov ebp, edi
    neg ebp
    mov esi, edx
    mov edx, -1
    cmp esi, edx
    jnz fill7
    pop esi
    add esi, edi
  }
fill10:
  asm {
    sub esi, 2
    cmp esi, dword ptr [size]
    jnc fill14                         // filled out
    cmp es:[esi], ax
    jnz fill10
    add edi, esi
  }
fill11:
  asm {
    add esi, 2
    add edi, 2
    cmp edi, dword ptr [size]
    jnc fill14                         // filled out
    cmp es:[esi], ax
    jz fill12
    mov es:[esi], ax
    cmp es:[edi], ax
    jz fill11
    cmp edx, -1
    jnz fill11
    mov edx, edi
    jmp fill11
  }
fill12:
  asm {
    mov edi, ebp
    neg edi
    mov esi, edx
    mov edx, -1
    cmp esi, edx
    jnz fill10
  }
fill13:
  asm pop eax
fill14:
  asm pop es;

  return GRAPH_SUCCESS;
}


int soft_rect(graph_t *graph, char *arg)
{
  static int width = graph->width;
  static int depth = graph->depth;
  static u16 fbsel = graph->fbsel;

  asm {
    push es
    mov ax, word ptr [fbsel]           // video segment
    mov es, ax
    mov ebx, [arg]                     // function arguments
    mov eax, [ebx + 4]                 // rect y
    mul dword ptr [width]              // picture width
    add eax, [ebx]                     // rect x
    mov ecx, dword ptr [depth]         // color size
    mul ecx
    mov edi, eax                       // rect address
    mov eax, dword ptr [width]         // picture width
    sub eax, [ebx + 8]                 // rect width
    mul ecx
    mov esi, eax
    mov eax, [ebx + 16]                // rect color
    mov edx, [ebx + 12]                // rect height
    mov ebx, [ebx + 8]                 // rect width
    cld
    cmp ecx, 2
    jz rect2                           // 16 bit color
  }
rect1:
  asm {
    mov ecx, ebx
    rep
    stosb                              // rect line
    add edi, esi
    dec edx
    jnz rect1
    pop es
  }
  return GRAPH_SUCCESS;

rect2:
  asm {
    mov ecx, ebx
    rep
    stosw                              // rect line
    add edi, esi
    dec edx
    jnz rect2
    pop es
  }
  return GRAPH_SUCCESS;
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
