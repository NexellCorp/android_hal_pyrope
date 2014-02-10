#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2005, 2007-2010 ARM Limited
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

UTIL_MATH_DIR ?= .

MATH_SRCS = $(UTIL_MATH_DIR)/mali_math.c \
	$(UTIL_MATH_DIR)/mali_float_matrix4x4.c

MATH_LIB = $(TMP_PRODUCT_DIR)/libmath$(LIB_EXT)

$(call set-file-flavour,fast,$(MATH_SRCS))

$(call make-target-static-library,$(MATH_LIB),$(MATH_SRCS),)
