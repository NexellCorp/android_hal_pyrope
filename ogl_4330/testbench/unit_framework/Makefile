#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2006-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

# -*- mode: makefile -*-

#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2001-2002, 2006-2009 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

#Magic to find the right directory to build from
ifndef FIND_BUILD_DIR_MAGIC
FIND_BUILD_DIR_MAGIC := 1
ifeq ($(wildcard topleveldir),)
.PHONY: $(MAKECMDGOALS) recurse_to_parent_and_build
$(MAKECMDGOALS): recurse_to_parent_and_build
recurse_to_parent_and_build:
	$(MAKE) -C .. $(MAKECMDGOALS)
endif
endif

UNIT_FRAMEWORK_DIR ?= .
TMP_PRODUCT_DIR ?= .

ifeq ($(call is-feature-enabled,android),)
ENABLE_BACKTRACE ?= TRUE
endif

UNIT_FRAMEWORK_SRCS = \
	$(UNIT_FRAMEWORK_DIR)/framework_main.c \
	$(UNIT_FRAMEWORK_DIR)/suite.c \
	$(UNIT_FRAMEWORK_DIR)/mem.c \
	$(UNIT_FRAMEWORK_DIR)/resultset.c \
	$(UNIT_FRAMEWORK_DIR)/test_helpers.c

ifeq ($(ENABLE_BACKTRACE),TRUE)
CPPFLAGS += -DSUITE_ENABLE_BACKTRACE
UNIT_FRAMEWORK_SRCS += \
	$(UNIT_FRAMEWORK_DIR)/backtrace.c
endif

DEP_SRCS += $(UNIT_FRAMEWORK_SRCS)

UNIT_FRAMEWORK_LIB = $(TMP_PRODUCT_DIR)/libunit_framework$(LIB_EXT)


CPPFLAGS += -I$(UNIT_FRAMEWORK_DIR)
ifneq ($(wildcard $(TESTBENCH_DIR)/tpi),)
	CPPFLAGS += -I$(TESTBENCH_DIR)/tpi
endif

$(call make-static-library,$(UNIT_FRAMEWORK_LIB),$(UNIT_FRAMEWORK_SRCS) $(TMP_PRODUCT_DIR)/libtpi$(LIB_EXT),)
