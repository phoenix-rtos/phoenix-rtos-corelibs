#
# Makefile for VirtIO core interface library
#
# Copyright 2020 Phoenix Systems
# Author: Lukasz Kosinski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

ifeq ($(TARGET_FAMILY), ia32)
	VIRTIO_OBJS := virtqueue.o virtio-pci.o
else ifeq ($(TARGET_FAMILY),riscv64)
	VIRTIO_OBJS := virtqueue.o virtio-mmio.o
endif

$(PREFIX_A)libvirtio.a: $(addprefix $(PREFIX_O)libvirtio/, $(VIRTIO_OBJS))
	$(ARCH)

$(PREFIX_H)libvirtio.h: libvirtio/libvirtio.h
	$(HEADER)

all: $(PREFIX_A)libvirtio.a $(PREFIX_H)libvirtio.h
