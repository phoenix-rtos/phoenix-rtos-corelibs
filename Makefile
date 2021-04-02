#
# Makefile for phoenix-rtos-corelibs
#
# Copyright 2019, 2020 Phoenix Systems
#
# %LICENSE%
#

SIL ?= @
MAKEFLAGS += --no-print-directory


#TARGET ?= ia32-generic
#TARGET ?= armv7m3-stm32l152xd
#TARGET ?= armv7m3-stm32l152xe
#TARGET ?= armv7m4-stm32l4x6
#TARGET ?= armv7m7-imxrt105x
#TARGET ?= armv7m7-imxrt106x
#TARGET ?= armv7m7-imxrt117x
#TARGET ?= armv7a7-imx6ull
TARGET ?= ia32-generic
#TARGET ?= riscv64-spike

include ../phoenix-rtos-build/Makefile.common
include ../phoenix-rtos-build/Makefile.$(TARGET_SUFF)


.PHONY: clean
clean:
	@echo "rm -rf $(BUILD_DIR)"

ifneq ($(filter clean,$(MAKECMDGOALS)),)
	$(shell rm -rf $(BUILD_DIR))
endif

include libcgi/Makefile
include libvirtio/Makefile
include libvga/Makefile
include libgraph/Makefile
include libgraph/test/Makefile
