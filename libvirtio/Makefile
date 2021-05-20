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

NAME := libvirtio
LOCAL_HEADERS := virtio.h

LOCAL_SRCS := virtio.c virtiopci.c virtqueue.c

ifeq ($(TARGET_FAMILY), ia32)
  LOCAL_SRCS += virtiopci-ia32.c
else
  LOCAL_SRCS += virtiopci-empty.c
endif

include $(static-lib.mk)
