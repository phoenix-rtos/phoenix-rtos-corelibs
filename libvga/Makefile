#
# Makefile for generic VGA library
#
# Copyright 2021 Phoenix Systems
# Author: Lukasz Kosinski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

NAME := libvga
LOCAL_HEADERS := vga.h

LOCAL_SRCS := vga.c

ifeq ($(TARGET_FAMILY), ia32)
  LOCAL_SRCS += vgahw-pc.c
else
  LOCAL_SRCS += vgahw-empty.c
endif

include $(static-lib.mk)
