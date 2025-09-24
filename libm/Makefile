#
# Makefile for math library
#
# Copyright 2025 Phoenix Systems
# Author: Mikolaj Matalowski
#
# This file is part of Phoenix-RTOS.
#
# %LICENSE%
#


NAME := libm

LOCAL_SRCS :=
LOCAL_HEADERS :=
MY_HEADERS :=
LOCAL_CFLAGS := 


LIBMCS_WANT_DAZ := y
LIBMCS_WANT_COMPLEX := y


ifeq ($(USE_LIBMCS), y)
    # Add include paths
    LOCAL_CFLAGS += -Ilibm/libmcs/sw-quality/dummy_includes
    LOCAL_CFLAGS += -Ilibm/libmcs/libm/include
    LOCAL_CFLAGS += -Ilibm/libmcs/libm/common
    LOCAL_CFLAGS += -Ilibm/libmcs/libm/mathd/internal
    LOCAL_CFLAGS += -Ilibm/libmcs/libm/mathf/internal

    # Temporary define to test compilation for IA32
    # LOCAL_CFLAGS += -DLIBMCS_LONG_IS_32BITS

    DEFINITIONS = $(shell $(CC) -dM -E - < /dev/null)
    # Setting sizes of floating point types
    TARGET_NAME := \#define __SIZEOF_DOUBLE__
    DOUBLE_SIZE = $(strip $(subst #define $(TARGET_NAME),,$(filter #define $(TARGET_NAME)%, $(DEFINITIONS))))
    ifeq ($(DOUBLE_SIZE), 4)
        LOCAL_CFLAGS += -DLIBMCS_DOUBLE_IS_64BITS
    else
        LOCAL_CFLAGS += -DLIBMCS_DOUBLE_IS_32BITS
    endif

    TARGET_NAME := \#define  __SIZEOF_LONG_DOUBLE__
    LDOUBLE_SIZE = $(strip $(subst #define $(TARGET_NAME),,$(filter #define $(TARGET_NAME)%, $(DEFINITIONS))))
    ifeq ($(LDOUBLE_SIZE), 8)
        LOCAL_CFLAGS += -DLIBMCS_LONG_DOUBLE_IS_64BITS
    else
        LOCAL_CFLAGS += -DLIBMCS_DOUBLE_IS_80BITS
    endif

    # Setting size of long
    TARGET_NAME := \#define  __SIZEOF_LONG__
    LDOUBLE_SIZE = $(strip $(subst #define $(TARGET_NAME),,$(filter #define $(TARGET_NAME)%, $(DEFINITIONS))))
    ifeq ($(LDOUBLE_SIZE), 8)
        LOCAL_CFLAGS += -DLIBMCS_LONG_IS_64BITS
    else
        LOCAL_CFLAGS += -DLIBMCS_LONG_IS_32BITS
    endif

    ifeq ($(LIBMCS_WANT_DAZ), y)
        LOCAL_CFLAGS += -DLIBMCS_FPU_DAZ
    endif

    ifeq ($(LIBMCS_WANT_COMPLEX), y)
        LOCAL_CFLAGS += -DLIBMCS_WANT_COMPLEX
        LOCAL_SRCS += libmcs/libm/complexd/cabsd.c \
        libmcs/libm/complexd/cacosd.c \
        libmcs/libm/complexd/cacoshd.c \
        libmcs/libm/complexd/cargd.c \
        libmcs/libm/complexd/casind.c \
        libmcs/libm/complexd/casinhd.c \
        libmcs/libm/complexd/catand.c \
        libmcs/libm/complexd/catanhd.c \
        libmcs/libm/complexd/ccosd.c \
        libmcs/libm/complexd/ccoshd.c \
        libmcs/libm/complexd/cexpd.c \
        libmcs/libm/complexd/cimagd.c \
        libmcs/libm/complexd/clogd.c \
        libmcs/libm/complexd/conjd.c \
        libmcs/libm/complexd/cpowd.c \
        libmcs/libm/complexd/cprojd.c \
        libmcs/libm/complexd/creald.c \
        libmcs/libm/complexd/csind.c \
        libmcs/libm/complexd/csinhd.c \
        libmcs/libm/complexd/csqrtd.c \
        libmcs/libm/complexd/ctand.c \
        libmcs/libm/complexd/ctanhd.c \
        libmcs/libm/complexd/internal/ctrigd.c \
        libmcs/libm/complexf/cabsf.c \
        libmcs/libm/complexf/cacosf.c \
        libmcs/libm/complexf/cacoshf.c \
        libmcs/libm/complexf/cargf.c \
        libmcs/libm/complexf/casinf.c \
        libmcs/libm/complexf/casinhf.c \
        libmcs/libm/complexf/catanf.c \
        libmcs/libm/complexf/catanhf.c \
        libmcs/libm/complexf/ccosf.c \
        libmcs/libm/complexf/ccoshf.c \
        libmcs/libm/complexf/cexpf.c \
        libmcs/libm/complexf/cimagf.c \
        libmcs/libm/complexf/clogf.c \
        libmcs/libm/complexf/conjf.c \
        libmcs/libm/complexf/cpowf.c \
        libmcs/libm/complexf/cprojf.c \
        libmcs/libm/complexf/crealf.c \
        libmcs/libm/complexf/csinf.c \
        libmcs/libm/complexf/csinhf.c \
        libmcs/libm/complexf/csqrtf.c \
        libmcs/libm/complexf/ctanf.c \
        libmcs/libm/complexf/ctanhf.c \
        libmcs/libm/complexf/internal/ctrigf.c
    endif

    # Add sources - copied from libmcs Makefile.in
	LOCAL_SRCS += libmcs/libm/common/signgam.c \
        libmcs/libm/common/tools.c \
        libmcs/libm/mathd/acosd.c \
        libmcs/libm/mathd/acoshd.c \
        libmcs/libm/mathd/asind.c \
        libmcs/libm/mathd/asinhd.c \
        libmcs/libm/mathd/atan2d.c \
        libmcs/libm/mathd/atand.c \
        libmcs/libm/mathd/atanhd.c \
        libmcs/libm/mathd/cbrtd.c \
        libmcs/libm/mathd/ceild.c \
        libmcs/libm/mathd/copysignd.c \
        libmcs/libm/mathd/cosd.c \
        libmcs/libm/mathd/coshd.c \
        libmcs/libm/mathd/erfcd.c \
        libmcs/libm/mathd/erfd.c \
        libmcs/libm/mathd/exp2d.c \
        libmcs/libm/mathd/expd.c \
        libmcs/libm/mathd/expm1d.c \
        libmcs/libm/mathd/fabsd.c \
        libmcs/libm/mathd/fdimd.c \
        libmcs/libm/mathd/floord.c \
        libmcs/libm/mathd/fmad.c \
        libmcs/libm/mathd/fmaxd.c \
        libmcs/libm/mathd/fmind.c \
        libmcs/libm/mathd/fmodd.c \
        libmcs/libm/mathd/frexpd.c \
        libmcs/libm/mathd/hypotd.c \
        libmcs/libm/mathd/ilogbd.c \
        libmcs/libm/mathd/internal/fpclassifyd.c \
        libmcs/libm/mathd/internal/gammad.c \
        libmcs/libm/mathd/internal/signbitd.c \
        libmcs/libm/mathd/internal/trigd.c \
        libmcs/libm/mathd/j0d.c \
        libmcs/libm/mathd/j1d.c \
        libmcs/libm/mathd/jnd.c \
        libmcs/libm/mathd/ldexpd.c \
        libmcs/libm/mathd/lgammad.c \
        libmcs/libm/mathd/llrintd.c \
        libmcs/libm/mathd/llroundd.c \
        libmcs/libm/mathd/log10d.c \
        libmcs/libm/mathd/log1pd.c \
        libmcs/libm/mathd/log2d.c \
        libmcs/libm/mathd/logbd.c \
        libmcs/libm/mathd/logd.c \
        libmcs/libm/mathd/lrintd.c \
        libmcs/libm/mathd/lroundd.c \
        libmcs/libm/mathd/modfd.c \
        libmcs/libm/mathd/nand.c \
        libmcs/libm/mathd/nearbyintd.c \
        libmcs/libm/mathd/nextafterd.c \
        libmcs/libm/mathd/nexttowardd.c \
        libmcs/libm/mathd/powd.c \
        libmcs/libm/mathd/remainderd.c \
        libmcs/libm/mathd/remquod.c \
        libmcs/libm/mathd/rintd.c \
        libmcs/libm/mathd/roundd.c \
        libmcs/libm/mathd/scalblnd.c \
        libmcs/libm/mathd/scalbnd.c \
        libmcs/libm/mathd/sind.c \
        libmcs/libm/mathd/sinhd.c \
        libmcs/libm/mathd/sqrtd.c \
        libmcs/libm/mathd/tand.c \
        libmcs/libm/mathd/tanhd.c \
        libmcs/libm/mathd/tgammad.c \
        libmcs/libm/mathd/truncd.c \
        libmcs/libm/mathd/y0d.c \
        libmcs/libm/mathd/y1d.c \
        libmcs/libm/mathd/ynd.c \
        libmcs/libm/mathf/acosf.c \
        libmcs/libm/mathf/acoshf.c \
        libmcs/libm/mathf/asinf.c \
        libmcs/libm/mathf/asinhf.c \
        libmcs/libm/mathf/atan2f.c \
        libmcs/libm/mathf/atanf.c \
        libmcs/libm/mathf/atanhf.c \
        libmcs/libm/mathf/cbrtf.c \
        libmcs/libm/mathf/ceilf.c \
        libmcs/libm/mathf/copysignf.c \
        libmcs/libm/mathf/cosf.c \
        libmcs/libm/mathf/coshf.c \
        libmcs/libm/mathf/erfcf.c \
        libmcs/libm/mathf/erff.c \
        libmcs/libm/mathf/exp2f.c \
        libmcs/libm/mathf/expf.c \
        libmcs/libm/mathf/expm1f.c \
        libmcs/libm/mathf/fabsf.c \
        libmcs/libm/mathf/fdimf.c \
        libmcs/libm/mathf/floorf.c \
        libmcs/libm/mathf/fmaf.c \
        libmcs/libm/mathf/fmaxf.c \
        libmcs/libm/mathf/fminf.c \
        libmcs/libm/mathf/fmodf.c \
        libmcs/libm/mathf/frexpf.c \
        libmcs/libm/mathf/hypotf.c \
        libmcs/libm/mathf/ilogbf.c \
        libmcs/libm/mathf/internal/fpclassifyf.c \
        libmcs/libm/mathf/internal/gammaf.c \
        libmcs/libm/mathf/internal/signbitf.c \
        libmcs/libm/mathf/internal/trigf.c \
        libmcs/libm/mathf/ldexpf.c \
        libmcs/libm/mathf/lgammaf.c \
        libmcs/libm/mathf/llrintf.c \
        libmcs/libm/mathf/llroundf.c \
        libmcs/libm/mathf/log10f.c \
        libmcs/libm/mathf/log1pf.c \
        libmcs/libm/mathf/log2f.c \
        libmcs/libm/mathf/logbf.c \
        libmcs/libm/mathf/logf.c \
        libmcs/libm/mathf/lrintf.c \
        libmcs/libm/mathf/lroundf.c \
        libmcs/libm/mathf/modff.c \
        libmcs/libm/mathf/nanf.c \
        libmcs/libm/mathf/nearbyintf.c \
        libmcs/libm/mathf/nextafterf.c \
        libmcs/libm/mathf/nexttowardf.c \
        libmcs/libm/mathf/powf.c \
        libmcs/libm/mathf/remainderf.c \
        libmcs/libm/mathf/remquof.c \
        libmcs/libm/mathf/rintf.c \
        libmcs/libm/mathf/roundf.c \
        libmcs/libm/mathf/scalblnf.c \
        libmcs/libm/mathf/scalbnf.c \
        libmcs/libm/mathf/sinf.c \
        libmcs/libm/mathf/sinhf.c \
        libmcs/libm/mathf/sqrtf.c \
        libmcs/libm/mathf/tanf.c \
        libmcs/libm/mathf/tanhf.c \
        libmcs/libm/mathf/tgammaf.c \
        libmcs/libm/mathf/truncf.c
else
    # Add srcs for libphoenix math library
	LOCAL_SRCS += math/common.c \
        math/complex.c \
        math/exp.c \
        math/hyper.c \
        math/power.c \
        math/trig.c

    # Add includes
    LOCAL_CFLAGS += -Ilibm/math
    LOCAL_CFLAGS += -Ilibm/math/include/
endif


include $(static-lib.mk)


SYSROOT := $(shell $(CC) $(CFLAGS) -print-sysroot)
HEADERS_INSTALL_DIR := $(SYSROOT)/usr/include

libmath-install-headers:
ifeq ($(USE_LIBMCS), y)
	@echo "INSTALLING HEADERS"\
	mkdir -p "$(HEADERS_INSTALL_DIR)"; \
	cp -a libm/libmcs/sw-quality/dummy_includes/*.h "$(HEADERS_INSTALL_DIR)"; \
	cp -a libm/libmcs/libm/include/*.h "$(HEADERS_INSTALL_DIR)"; \
	cp -a libm/libmcs/libm/common/*.h "$(HEADERS_INSTALL_DIR)"; \
	cp -a libm/libmcs/libm/mathd/internal/*.h "$(HEADERS_INSTALL_DIR)"; \
	cp -a libm/libmcs/libm/mathf/internal/*.h "$(HEADERS_INSTALL_DIR)"; 
else
	@echo "INSTALLING HEADERS"\
	mkdir -p "$(HEADERS_INSTALL_DIR)"; \
	cp -a libm/math/include/*.h "$(HEADERS_INSTALL_DIR)";
endif
