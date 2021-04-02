#
# Makefile for graph library test
#
# Copyright 2021 Phoenix Systems
# Author: Lukasz Kosinski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#

LIBS = libgraph.a libvga.a libvirtio.a

$(PREFIX_PROG)test_libgraph: $(addprefix $(PREFIX_O)libgraph/test/, test.o) $(addprefix $(PREFIX_A), $(LIBS))
	$(LINK)

all: $(PREFIX_PROG_STRIPPED)test_libgraph
