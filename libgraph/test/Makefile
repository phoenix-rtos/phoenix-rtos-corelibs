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

$(PREFIX_PROG)test_libgraph: $(addprefix $(PREFIX_O)libgraph/test/, test.o) $(PREFIX_A)libgraph.a $(PREFIX_A)libvirtio.a
	$(LINK)

all: $(PREFIX_PROG_STRIPPED)test_libgraph
