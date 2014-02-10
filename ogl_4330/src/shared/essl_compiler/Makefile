#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2001-2002, 2007-2010 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

# makefile for the compiler integration library.
#
# The src folder is a svn copy of S0011-compiler/trunk/src .
# This folder can be replaced at any time with the newest copy from the compiler -
# Do not alter the contents of said folder.


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

# Inform the makefile system that the essl compiler makefile and src are present
ESSL_COMPILER_PRESENT := 1

COMPILER_DIR=$(ESSL_COMPILER_DIR)/src
include $(COMPILER_DIR)/Makefile

