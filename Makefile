#
# Makefile for phoenix-rtos-corelibs
#
# Copyright 2019, 2020 Phoenix Systems
#
# %LICENSE%
#

include ../phoenix-rtos-build/Makefile.common

.DEFAULT_GOAL := all

# DEFAULT_COMPONENTS are shared between all targets
DEFAULT_COMPONENTS := libcgi libvirtio libvga libgraph libstorage \
  libmtd libptable libuuid libcache libswdg libmbr libtinyaes libalgo

ifeq ($(TARGET_SUBFAMILY), stm32n6)
  DEFAULT_COMPONENTS := $(filter-out libtinyaes, $(DEFAULT_COMPONENTS))
  DEFAULT_COMPONENTS += libtinyaes-stm32n6
endif

# read out all components
ALL_MAKES := $(wildcard */Makefile) $(wildcard */*/Makefile)
include $(ALL_MAKES)

# create generic targets
.PHONY: all install clean
all: $(DEFAULT_COMPONENTS)
install: $(patsubst %,%-install,$(DEFAULT_COMPONENTS))
clean: $(patsubst %,%-clean,$(ALL_COMPONENTS))
