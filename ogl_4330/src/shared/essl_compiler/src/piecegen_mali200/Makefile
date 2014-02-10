#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2009-2010 ARM Limited
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

PIECEGEN_COMPILER_MALI200_DIR ?= .

PIECEGEN_MALI200_SRCS = \
	$(PIECEGEN_COMPILER_MALI200_DIR)/piecegen_mali200_target.c \
	$(PIECEGEN_COMPILER_MALI200_DIR)/piecegen_mali200_driver.c \
	$(PIECEGEN_COMPILER_MALI200_DIR)/piecegen_mali200_output.c
