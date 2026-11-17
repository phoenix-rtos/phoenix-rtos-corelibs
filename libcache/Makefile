#
# Makefile for graphics library
#
# Copyright 2022 Phoenix Systems
# Author: Malgorzata Wrobel
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

LOCAL_DIR := $(call my-dir)
NAME := libcache
LOCAL_HEADERS := cache.h
LOCAL_SRCS := cache.c

# the build host has no libphoenix: the unit tests bring stand-ins for its headers
ifeq ($(TARGET_FAMILY), host)
LOCAL_CFLAGS := -I$(LOCAL_DIR)tests/host
endif

include $(static-lib.mk)
