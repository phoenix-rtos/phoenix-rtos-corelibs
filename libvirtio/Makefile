#
# Makefile for VirtIO library
#
# Copyright 2020 Phoenix Systems
# Author: Lukasz Kosinski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

OBJS = virtio.o virtiopci.o virtqueue.o

ifeq ($(TARGET_FAMILY), ia32)
	OBJS += virtio-ia32.o virtiopci-ia32.o
else ifeq ($(TARGET_FAMILY),riscv64)
	OBJS += virtio-riscv64.o virtiopci-riscv64.o
endif

$(PREFIX_A)libvirtio.a: $(addprefix $(PREFIX_O)libvirtio/, $OBJS)
	$(ARCH)

$(PREFIX_H)libvirtio.h: libvirtio/libvirtio.h
	$(HEADER)

all: $(PREFIX_A)libvirtio.a $(PREFIX_H)libvirtio.h
