#
# Makefile for graphics library
#
# Copyright 2021 Phoenix Systems
# Author: Lukasz Kosinski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

NAME := libgraph

LOCAL_HEADERS := graph.h
DEPS := libvga libvirtio

LOCAL_SRCS := graph.c vga.c virtio-gpu.c

ifeq ($(TARGET_FAMILY), ia32)
  LOCAL_SRCS += cirrus.c soft-ia32.c
else
  LOCAL_SRCS += soft.c
endif

include $(static-lib.mk)
