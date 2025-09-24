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
  libmtd libptable libuuid libcache libswdg libmbr libtinyaes libalgo libm

# read out all components
ALL_MAKES := $(wildcard */Makefile) $(wildcard */*/Makefile)
include $(ALL_MAKES)

# create generic targets
.PHONY: all install clean
all: $(DEFAULT_COMPONENTS) libmath-install-headers
install: $(patsubst %,%-install,$(DEFAULT_COMPONENTS))
clean: $(patsubst %,%-clean,$(ALL_COMPONENTS))
