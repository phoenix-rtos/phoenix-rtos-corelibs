/*
 * Graph library for DPMI32
 *
 * CT69000 driver
 *
 * Copyright 2002-2007 IMMOS
 */

#include <stdlib.h>
#include <stdio.h>

#include <kernel.h>

#include "graph.h"


#define VENDOR 0x102c               // C&T
#define DEVICE 0x00c0               // CT69000

#define CMDSZ 0x100000
#define MEMSZ 0x200000
#define CURSORSZ 0x1000


unsigned int modes[] = {
  GRAPH_640x480x8,   0x04101, 640,  480,  1, 0x30,
  GRAPH_800x600x8,   0x04103, 800,  600,  1, 0x32,
  GRAPH_1024x768x8,  0x04105, 1024, 768,  1, 0x34,
  GRAPH_1280x1024x8, 0x04107, 1280, 1024, 1, 0x38,
  GRAPH_640x480x16,  0x04111, 640,  480,  2, 0x41,
  GRAPH_800x600x16,  0x04114, 800,  600,  2, 0x43,
  GRAPH_1024x768x16, 0x04117, 1024, 768,  2, 0x45,
  0
};


struct {
  u8 irq;
  u16 cmdsel;                          // command buffer selector
  u16 prev_mode;                       // previous graphic mode

  unsigned char bg;
  unsigned char fg;
} ct69000;


/* Function schedules and executes new task */
extern int graph_schedule();


int ct69000_isr(u32 n, void *d)
{
  graph_t *graph = (graph_t *)d;

  _AX = ct69000.cmdsel;
  asm {
    push ds
    mov ds, ax
    mov ecx, ds:[400604h]                 // interrupt status
    mov ds:[400604h], ecx                 // clear interrupt
    pop ds
  }

  if (!(_EDX & 0x80004000))
    return IRQ_IGNORE;

  if (_EDX & 0x4000) {
    ++graph->vsync;                    // vertical synchronization
  }

  asm sti;
  graph_schedule();

  return IRQ_HANDLED;
}


int ct69000_open(graph_t *graph, unsigned char irq)
{
  asm {
    mov eax, 4f03h
    int 10h                            // get VESA mode
  }
  if (_AX != 0x4f)
    return GRAPH_ERR_DEVICE;

  ct69000.prev_mode = _BX;

  if (irq < 16)
    ct69000.irq = irq;                 // use forced irq
  else
    irq = ct69000.irq;                 // use PCI interrupt

  if (irq > 15)
    return GRAPH_ERR_DEVICE;           // no interrupt assigned

  if (irq && irq_install(irq, ct69000_isr, NULL))
    return GRAPH_ERR_DPMI;

  _AX = ct69000.cmdsel;
  _EBX = MEMSZ - CURSORSZ;
  asm {
    push gs
    mov gs, ax
    mov eax, 220h
    mov gs:[4007ach], ax               // reset BitBLT
    mov eax, 80004000h
    mov gs:[400600h], eax              // enable BitBLT and vsync interrupt
    mov eax, 010a0h
    mov gs:[4007ach], ax               // reset cursor
    mov ax, bx
    and ax, 0f000h
    mov al, 0a2h
    mov gs:[4007ach], ax               // cursor low address
    mov eax, ebx
    shr eax, 8
    mov al, 0a3h
    mov gs:[4007ach], ax               // cursor high address
    mov eax, 0ffa4h
    mov gs:[4007ach], ax               // cursor low x position
    mov eax, 007a5h
    mov gs:[4007ach], ax               // cursor high x position
    mov eax, 0ffa6h
    mov gs:[4007ach], ax               // cursor low y position
    mov eax, 003a7h
    mov gs:[4007ach], ax               // cursor high y position
    mov al, 80h
    mov gs:[4007ach], al
    mov ah, gs:[4007adh]
    or al, 10h
    mov gs:[4007adh], al               // enable hardware cursor
    pop gs
  }

  if (irq)
    low_maskirq(irq, 0);

  return GRAPH_SUCCESS;
}


int ct69000_close(graph_t *graph)
{
  irq_uninstall(ct69000.irq, ct69000_isr, NULL);

  if(ct69000.cmdsel) {
    _AX = ct69000.cmdsel;
    asm {
      push gs
      mov gs, ax
      mov eax, 220h
      mov gs:[4007ach], ax             // reset BitBLT
      xor eax, eax
      mov gs:[4007adh], al
      mov gs:[400600h], eax            // disable BitBLT interrupts
      mov al, 80h
      mov gs:[4007ach], al
      mov al, gs:[4007adh]
      and al, 0efh
      mov gs:[4007adh], al             // disable hardware cursor
      pop gs
    }
  }

  if (ct69000.prev_mode) {
    _EBX = ct69000.prev_mode;          // previous graphic mode
    asm {
      mov eax, 4f02h
      int 10h                          // set VESA mode
    }
    if (_AX != 0x4f)
      return GRAPH_ERR_DEVICE;
  }

  return GRAPH_SUCCESS;
}


int ct69000_mode(graph_t *graph, int mode, char freq)
{
  int index;

  for (index = 0; modes[index] != mode; index += 6)
    if (!modes[index])
      return GRAPH_ERR_ARG;

  graph->width = modes[index + 2];
  graph->height = modes[index + 3];
  graph->depth = modes[index + 4];

  // set vertical refresh rate */
  _EBX = modes[index + 5];
  _BH = freq;
  asm {
    mov ax, 5f05h
    int 10h
  }
  if (_AX != 0x15f)
    return GRAPH_ERR_DEVICE;

  // set screen parameters
  _EBX = modes[index + 1];
  asm {
    mov eax, 4f02h
    int 10h                            // set VESA mode
  }
  if (_AX != 0x4f)
    return GRAPH_ERR_DEVICE;

  _EDX = ((graph->depth - 1) << 12) | 0x20;
  _AX = ct69000.cmdsel;
  asm {
    push gs
    mov gs, ax
    mov al, 80h
    mov gs:[4007ach], al
    mov al,gs:[4007adh]
    or al, 80h
    mov gs:[4007adh], al               // set 8 bit DAC
    mov gs:[4007ach], dx               // set BitBLT color mode
    pop gs
  }

  return GRAPH_SUCCESS;
}


int ct69000_isbusy(graph_t *graph)
{
  return (low_getfar(ct69000.cmdsel, 0x400010) & 0x80000000);
}


int ct69000_trigger(graph_t *graph)
{
  u32 v;

  // Verify interrupt and clear interrupt source
  v = low_getfar(ct69000.cmdsel, 0x400604);
  low_setfar(ct69000.cmdsel, 0x400604, v);

  if (v & 0x4000)
    ++graph->vsync;

  if (ct69000_isbusy(graph))
    return GRAPH_ERR_BUSY;

  return graph_schedule();
}


// Draw rectangle
int ct69000_rect(graph_t *graph, char *arg)
{
  static int width = graph->width;
  static int depth = graph->depth;

  _AX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push gs
    mov gs, ax
    mov ebx, [arg]                     // function arguments
    mov eax, dword ptr [width]         // picture width
    mul dword ptr [depth]              // color size
    mov ecx, eax
    shl eax, 16
    or eax, ecx
    mov gs:[400000h], eax              // source and destination span
    mov eax, [ebx + 16]                // rectangle color
    mov gs:[400004h], eax              // background color
    mov gs:[400008h], eax              // foreground color
    mov eax, 1000000h
    mov gs:[40000ch], eax              // monochrome source
    mov eax, 10cch
    mov gs:[400010h], eax              // fill rectangle
    mov eax, [ebx + 4]                 // rectangle y position
    mul dword ptr [width]              // picture width
    add eax, [ebx]                     // rectangle x position
    mul dword ptr [depth]              // color size
    mov gs:[400018h], eax              // source address
    mov gs:[40001ch], eax              // destination address
    mov eax, [ebx + 8]                 // rectangle width
    mul dword ptr [depth]              // color size
    mov ecx, eax
    mov eax, [ebx + 12]                // rectangle height
    shl eax, 16
    or eax, ecx
    mov gs:[400020h], eax              // width and height, start BitBLT
    pop gs
    popf
  }

  return GRAPH_SUCCESS;
}


// Move window
int ct69000_move(graph_t *graph, char *arg)
{
  static int width = graph->width;
  static int depth = graph->depth;

  _AX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push gs
    mov gs, ax
    mov ebx, [arg]                     // function arguments
    mov eax, dword ptr [width]         // picture width
    mul dword ptr [depth]              // color size
    mov ecx, eax
    shl eax, 16
    or eax, ecx
    mov gs:[400000h], eax              // source and destination span
    xor esi, esi
    xor ecx, ecx
    mov eax, [ebx + 16]                // move horizontal
    dec eax
    js move1
    or ecx, 100h
    mov eax, [ebx + 8]                 // move area width
    mul dword ptr [depth]              // color size
    lea esi, [eax - 1]
  }
move1:
  asm {
    mov eax, [ebx + 20]                // move vertical
    dec eax
    js move2
    or ecx, 200h
    mov eax, [ebx + 12]                // move area height
    dec eax
    mul dword ptr [width]              // picture width
    mul dword ptr [depth]              // color size
    add esi, eax
  }
move2:
  asm {
    lea eax, [ecx + 0cch]
    mov gs:[400010h], eax              // move block
    mov eax, [ebx + 4]                 // move area y position
    mul dword ptr [width]              // picture width
    add eax, [ebx]                     // move area x position
    mul dword ptr [depth]              // color size
    add eax, esi
    mov gs:[400018h], eax              // source address
    mov ecx, eax
    mov eax, [ebx + 20]                // move horizontal
    mul dword ptr [width]              // picture width
    add eax, [ebx + 16]                // move vertical
    mul dword ptr [depth]              // color size
    add eax, ecx
    mov gs:[40001ch], eax              // destination address
    mov eax, [ebx + 8]                 // move area width
    mul dword ptr [depth]              // color size
    mov ecx, eax
    mov eax, [ebx + 12]                // move area height
    shl eax, 16
    or eax, ecx
    mov gs:[400020h], eax              // width and height, start BitBLT
    pop gs
    popf
  }

  return GRAPH_SUCCESS;
}


// Copy window in frame buffer
int ct69000_copyin(graph_t * graph, char *arg)
{
  static int depth = graph->depth;

  _AX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push gs
    mov gs, ax
    mov ebx, [arg]                     // function arguments
    mov eax, [ebx + 20]                // copy area destination span
    shl eax, 16
    or eax, [ebx + 16]                 // copy area source span
    mov gs:[400000h], eax              // source and destination span
    mov eax, 0cch
    mov gs:[400010h], eax              // move block
    mov eax, [ebx]                     // copy area source address
    mov gs:[400018h], eax              // source address
    mov eax, [ebx + 4]                 // copy area destination address
    mov gs:[40001ch], eax              // destination address
    mov eax, [ebx + 8]                 // copy area width
    mul dword ptr [depth]              // color size
    mov ecx, eax
    mov eax, [ebx + 12]                // copy area height
    shl eax, 16
    or eax, ecx
    mov gs:[400020h], eax              // width and height, start BitBLT
    pop gs
    popf
  }

  return GRAPH_SUCCESS;
}


int ct69000_cursorcol(char *buf, unsigned char color)
{
  color = (color & 1) + 4;

  _AX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push gs
    mov gs, ax
    mov al, 80h
    mov gs:[4007ach], al
    mov ah, gs:[4007adh]
    or ah, 1
    mov gs:[4007adh], ah
    mov al, [color]
    mov gs:[400790h], al               // color write index
    mov ecx, 3
    mov esi, [buf]
 }
cursorcol1:
  asm {
    lodsb
    mov gs:[400791h], al               // color write data
    loop cursorcol1
    mov al, 80h
    mov gs:[4007ach], al
    mov ah, gs:[4007adh]
    and ah, 0feh
    mov gs:[4007adh], ah
    pop gs
    popf
  }

  return GRAPH_SUCCESS;
}


int ct69000_colorset(graph_t *graph, char *colors, unsigned char first, unsigned char last)
{
  _AX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push gs
    mov gs, ax
    xor eax, eax
    xor ebx, ebx
    mov al, [first]
    mov gs:[400790h], al               // color write index
    mov bl, [last]
    sub ebx, eax
    lea ecx, [ebx * 2 + ebx + 3]       // (EBX + 1) * 3
    mov esi, [colors]
  }
colorset1:
  asm {
    lodsb
    mov gs:[400791h], al               // color write data
    loop colorset1
    pop gs
    popf
  }

  if ((first <= ct69000.bg) && (last >= ct69000.bg))
    ct69000_cursorcol(colors + (ct69000.bg - first) * 3, 0);

  if ((first <= ct69000.fg) && (last >= ct69000.fg))
    ct69000_cursorcol(colors + (ct69000.fg - first) * 3, 1);

  return GRAPH_SUCCESS;
}


int ct69000_colorget(graph_t *graph, char *colors, unsigned char first, unsigned char last)
{
  _AX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push gs
    mov gs, ax
    xor eax, eax
    xor ebx, ebx
    mov al, [first]
    mov gs:[40078dh], al               // color read index
    mov bl, [last]
    sub ebx, eax
    lea ecx, [ebx * 2 + ebx + 3]       // (EBX + 1) * 3
    mov edi, [colors]
  }
colorget1:
  asm {
    mov al, gs:[400791h]               // color read data
    stosb
    loop colorget1
    pop gs
    popf
  }

  return GRAPH_SUCCESS;
}


int ct69000_cursorset(graph_t *graph, char *and, char *xor, unsigned char bg, unsigned char fg)
{
  int n;
  u32 offs;
  char colors[3];

  ct69000.bg = bg;
  ct69000.fg = fg;

  ct69000_colorget(graph, colors, bg, bg);
  ct69000_cursorcol(colors, 0);
  ct69000_colorget(graph, colors, fg, fg);
  ct69000_cursorcol(colors, 1);

  offs = MEMSZ - CURSORSZ;

  for (n = 0; n < 64; ++n) {
    low_setfar(graph->fbsel, offs + n * 16, ((int *)and)[n * 2]);
    low_setfar(graph->fbsel, offs + n * 16 + 4, ((int *)and)[n * 2 + 1]);
    low_setfar(graph->fbsel, offs + n * 16 + 8, ((int *)xor)[n * 2]);
    low_setfar(graph->fbsel, offs + n * 16 + 12, ((int *)xor)[n * 2 + 1]);
  }

  return GRAPH_SUCCESS;
}


int ct69000_cursorpos(graph_t *graph, unsigned int x, unsigned int y)
{
  _AX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push gs
    mov gs, ax
    mov ebx, [x]
    mov al, 0a4h
    mov ah, bl
    mov gs:[4007ach], ax               // cursor low x position
    mov al, 0a5h
    mov ah, bh
    and ah, 7
    mov gs:[4007ach], ax               // cursor high x position
    mov ebx, [y]
    mov al, 0a6h
    mov ah, bl
    mov gs:[4007ach], ax               // cursor low y position
    mov al, 0a7h
    mov ah, bh
    and ah, 7
    mov gs:[4007ach], ax               // cursor high y position
    pop gs
    popf
  }

  return GRAPH_SUCCESS;
}


int ct69000_cursorshow(graph_t *graph)
{
  _BX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push ds
    mov ds, bx
    mov ax, 015a0h
    mov ds:[4007ach], ax               // cursor control register
    pop ds
    popf
  }

  return GRAPH_SUCCESS;
}


int ct69000_cursorhide(graph_t *graph)
{
  _BX = ct69000.cmdsel;
  asm {
    pushf
    cli
    push ds
    mov ds, bx
    mov ax, 010a0h
    mov ds:[4007ach], ax               // cursor control register
    pop ds
    popf
  }

  return GRAPH_SUCCESS;
}


int ct69000_init(graph_t *graph)
{
  u32 fb;
  pci_id_t id = { VENDOR, DEVICE, 0 };
  pci_dev_t pdev;

  if (!pci_find(&id, 0, PCI_FIND_DEVICE, &pdev))
    return GRAPH_ABSENT;

  fb = pdev.base[0] & ~0xf;

  ct69000.irq = pdev.irq;

  graph->fbsel = low_dpmi_ldtalloc(1);
  low_dpmi_copydescr(low_getds(), graph->fbsel);
  low_dpmi_setbase(graph->fbsel, low_dpmi_phmap(fb, MEMSZ));
  low_dpmi_setlimit(graph->fbsel, MEMSZ - 1);

  ct69000.cmdsel = low_dpmi_ldtalloc(1);
  low_dpmi_copydescr(low_getds(), ct69000.cmdsel);
  low_dpmi_setbase(ct69000.cmdsel, low_dpmi_phmap(fb, 0x500000));
  low_dpmi_setlimit(ct69000.cmdsel, 0x4fffff);

  graph->open = ct69000_open;
  graph->close = ct69000_close;
  graph->mode = ct69000_mode;
  graph->isbusy = ct69000_isbusy;
  graph->trigger = ct69000_trigger;
  graph->rect = ct69000_rect;
  graph->move = ct69000_move;
  graph->copyin = ct69000_copyin;
  graph->colorset = ct69000_colorset;
  graph->colorget = ct69000_colorget;
  graph->cursorset = ct69000_cursorset;
  graph->cursorpos = ct69000_cursorpos;
  graph->cursorshow = ct69000_cursorshow;
  graph->cursorhide = ct69000_cursorhide;

  graph->memsz = MEMSZ;
  graph->cursorsz = CURSORSZ;

  ct69000.bg = 0;
  ct69000.fg = 1;

  return GRAPH_SUCCESS;
}
