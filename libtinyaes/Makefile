ifeq ($(OUTPUT_DIR),)
  $(error Please define OUTPUT_DIR)
endif

include $(OUTPUT_DIR)/mk/pre.mk

# NOTICE: to provide libtinyaes compact in size (the original form)
# additional operation modes need to be enabled, e.g. to build into
# the AES library CCM* functionality compile your project with
# defined WITH_AES_CCM_S, using CFLAGS/CPPFLAGS

# original tiny AES provided with (unlicense.txt)
$(eval $(call ADD_HDRS_ALL,libtinyaes-orig,,tiny-aes))
ORIG_SRC_DIR := tiny-aes/

# ours extensions
EXT_SRC_DIR := src/
$(eval $(call ADD_HDRS_ALL,libtinyaes-ext,,include))

AES_IMPL_SRC := $(addprefix $(ORIG_SRC_DIR),aes.c)
AES_EXT_SRC := $(addprefix $(EXT_SRC_DIR),aes_ccm_s.c cmac.c aes_kw.c aes_eax.c aes_gcm.c)

$(eval $(call ADD_LIB_FROM_FILES,libtinyaes,$(AES_IMPL_SRC) $(AES_EXT_SRC)))

ifneq ($(LIBTINYAES_USE_PHRTOS_DEVICES_LIBAES),)
  $(eval $(call ADD_LIB_FROM_FILES,libtinyaes-stm32l4,$(addprefix $(EXT_SRC_DIR),aes_hw_stm32l4.c) $(AES_EXT_SRC)))
endif

ifeq ($(PS_LIBTINYAES_TESTS),yes)
  TEST_LIBS = libtinyaes libunity libtinyaes_tests
  $(eval $(call ADD_LIB,libtinyaes_tests,test/))
  $(eval $(call ADD_PROG,libtinyaes_test_runner,tools/test_runner.c, $(TEST_LIBS)))

  $(eval $(call ADD_UNIT_TEST,libtinyaes_test_runner))
endif

include $(OUTPUT_DIR)/mk/post.mk
