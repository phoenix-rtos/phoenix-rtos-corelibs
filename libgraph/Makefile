#
# Makefile for graph library
#
# Copyright 2021 Phoenix Systems
# Author: Lukasz Kosinski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

OBJS = graph.o vgadev.o virtio-gpu.o
LIBS = libvga.a libvirtio.a

ifeq ($(TARGET_FAMILY), ia32)
	OBJS += soft-ia32.o
else
	OBJS += soft.o
endif

$(PREFIX_A)libgraph.a: $(addprefix $(PREFIX_O)libgraph/, $(OBJS)) $(addprefix $(PREFIX_A), $(LIBS))
	$(ARCH)

$(PREFIX_H)libgraph.h: libgraph/graph.h
	$(HEADER)

all: $(PREFIX_A)libgraph.a $(PREFIX_H)libgraph.h
