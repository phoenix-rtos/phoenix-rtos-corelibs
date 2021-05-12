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

NAME := libgraph

LOCAL_HEADERS := libgraph.h
DEPS := libvga libvirtio

LOCAL_SRCS := graph.c vgadev.c virtio-gpu.c

ifeq ($(TARGET_FAMILY), ia32)
  LOCAL_SRCS += soft-ia32.c
else
  LOCAL_SRCS += soft.c
endif

include $(static-lib.mk)
