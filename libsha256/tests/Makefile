#
# Makefile for libsha256 unit tests
#
# Copyright 2026 Phoenix Systems
# Author: Marek Bialowas
#
# SPDX-License-Identifier: BSD-3-Clause
#

# tests are written with unity, which lives in phoenix-rtos-tests - a component
# of another repository, so it is linked (LIBS) rather than depended on
# (DEP_LIBS, which only resolves components of this repository)

NAME := test-libsha256
LOCAL_SRCS := test_libsha256.c
LOCAL_HEADERS_DIR := nothing
DEP_LIBS := libsha256
LIBS := unity

include $(test-binary.mk)


# reaches static functions by #including the implementation, so the library is
# only needed for its header - DEPS instead of DEP_LIBS keeps it out of the link
NAME := test-libsha256-internal
LOCAL_SRCS := test_libsha256_internal.c
LOCAL_HEADERS_DIR := nothing
DEPS := libsha256
LIBS := unity

include $(test-binary.mk)
