#
# Makefile for Tiny AES library
#
# Copyright 2024 Phoenix Systems
# Author: Aleksander Kaminski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

NAME := libtinyaes
LOCAL_PATH := $(call my-dir)
ALL_SRCS := $(wildcard $(LOCAL_PATH)*.c)

SRCS := $(filter-out $(LOCAL_PATH)aes_hw_stm32l4.c, $(ALL_SRCS))

include $(static-lib.mk)

ifeq ($(TARGET_SUBFAMILY), stm32l4x6)
  NAME := libtinyaes-stm32l4
  SRCS := $(filter-out $(LOCAL_PATH)aes.c, $(ALL_SRCS))
  LOCAL_HEADERS_DIR := __none # prevent headers re-installation

  include $(static-lib.mk)
endif
