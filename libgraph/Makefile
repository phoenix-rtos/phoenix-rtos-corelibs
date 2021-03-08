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

OBJS = graph.o virtio-gpu.o

ifeq ($(TARGET_FAMILY), ia32)
	OBJS += soft-ia32.o
else
	OBJS += soft.o
endif

$(PREFIX_A)libgraph.a: $(addprefix $(PREFIX_O)libgraph/, $(OBJS)) $(PREFIX_A)libvirtio.a
	$(ARCH)

$(PREFIX_H)libgraph.h: libgraph/graph.h
	$(HEADER)

all: $(PREFIX_A)libgraph.a $(PREFIX_H)libgraph.h
