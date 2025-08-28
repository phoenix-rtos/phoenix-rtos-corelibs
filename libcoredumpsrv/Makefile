#
# Makefile for Phoenix-RTOS coredump_server
#
# Copyright 2025 Phoenix Systems
#

LOCAL_PATH := $(call my-dir)

NAME := libcoredumpsrv
SRCS := $(filter-out $(LOCAL_PATH)srv.c, $(wildcard $(LOCAL_PATH)*.c))
LOCAL_SRCS := hal/$(TARGET_SUFF)/cpu.c
LOCAL_CFLAGS := -Ilibcoredumpsrv -Ilibcoredumpsrv/hal/$(TARGET_SUFF)
LOCAL_HEADERS := coredump.h

ifneq (, $(filter imxrt117x imxrt106x imxrt105x, $(TARGET_SUBFAMILY)))
	LOCAL_CFLAGS += -DCPU_IMXRT
endif

ifeq ($(COREDUMP_DISABLE), 1)
	CPPFLAGS += -DCOREDUMP_DISABLE
endif

include $(static-lib.mk)
