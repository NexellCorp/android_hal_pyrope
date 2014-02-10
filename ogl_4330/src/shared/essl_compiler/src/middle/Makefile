#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2010, 2012 ARM Limited
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

MALI400_COMPILER_MIDDLE_DIR ?= .


MALI400_COMPILER_MIDDLE_TEST_SRCS = \
	$(MALI400_COMPILER_MIDDLE_DIR)/dominator.c

MALI400_COMPILER_MIDDLE_NOTEST_SRCS = \
	$(MALI400_COMPILER_MIDDLE_DIR)/middle.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/control_deps.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/optimise_cse.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/optimise_basic_blocks.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/optimise_constant_fold.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/remove_dead_code.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/conditional_select.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/rewrite_sampler_accesses.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/expand_builtin_functions.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/optimise_vector_ops.c \
	$(MALI400_COMPILER_MIDDLE_DIR)/proactive_calculations.c \



MALI400_COMPILER_SRCS += $(MALI400_COMPILER_MIDDLE_TEST_SRCS) $(MALI400_COMPILER_MIDDLE_NOTEST_SRCS)
TEST_SRCS += $(MALI400_COMPILER_MIDDLE_TEST_SRCS)
SHADERGEN_SRCS += $(MALI400_COMPILER_MIDDLE_DIR)/remove_dead_code.c
