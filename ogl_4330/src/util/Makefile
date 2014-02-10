#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2007-2011 ARM Limited
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

UTIL_DIR ?= .

UTIL_MATH_DIR=$(UTIL_DIR)/math
-include $(UTIL_MATH_DIR)/Makefile

UTIL_CSTRUCT2ASM_DIR=$(UTIL_DIR)/cstruct2asm
-include $(UTIL_CSTRUCT2ASM_DIR)/Makefile

UTIL_TRACETOC_DIR=$(UTIL_DIR)/tracetoc
-include $(UTIL_TRACETOC_DIR)/Makefile
