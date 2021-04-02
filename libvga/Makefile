#
# Makefile for VGA library
#
# Copyright 2021 Phoenix Systems
# Author: Lukasz Kosinski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

OBJS = vga.o

ifeq ($(TARGET_FAMILY), ia32)
	OBJS += vgahw-pc.o
else
	OBJS += vgahw-empty.o
endif

$(PREFIX_A)libvga.a: $(addprefix $(PREFIX_O)libvga/, $(OBJS))
	$(ARCH)

$(PREFIX_H)libvga.h: libvga/vga.h
	$(HEADER)

all: $(PREFIX_A)libvga.a $(PREFIX_H)libvga.h
