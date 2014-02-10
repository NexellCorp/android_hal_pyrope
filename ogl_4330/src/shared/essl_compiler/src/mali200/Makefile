#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2010-2012 ARM Limited
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

MALI400_COMPILER_MALI200_DIR ?= .


MALI400_MALI200_SRCS = \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_target.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_type.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_preschedule.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_driver.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_scheduler.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_instruction.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_slot.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_liveness.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_word_splitting.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_regalloc.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_register_integration.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_spilling.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_word_insertion.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_emit.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_entry_point.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_relocation.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_external_implementation.c

MALI400_MALI200_DEBUG_SRCS =

ifeq ($(DEBUGPRINT),1)
MALI400_MALI200_DEBUG_SRCS += \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_printer.c
endif

MALI400_MALI200_TEST_SRCS = \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_emit.c

MALI400_MALI200_NOT_IN_SHADERGEN = \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_entry_point.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_target.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_preschedule.c \
	$(MALI400_COMPILER_MALI200_DIR)/mali200_driver.c


SHADERGEN_SRCS += $(filter-out $MALI400_(MALI200_NOT_IN_SHADERGEN), $(MALI400_MALI200_SRCS))

MALI400_COMPILER_SRCS += $(MALI400_MALI200_SRCS)
MALI400_COMPILER_DEBUG_SRCS += $(MALI400_MALI200_DEBUG_SRCS)
TEST_SRCS += $(MALI400_MALI200_TEST_SRCS)

