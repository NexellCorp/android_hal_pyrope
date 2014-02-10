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

MALI400_COMPILER_BACKEND_DIR ?= .

MALI400_BACKEND_SRCS = \
	$(MALI400_COMPILER_BACKEND_DIR)/arithmetic.c \
	$(MALI400_COMPILER_BACKEND_DIR)/abstract_scheduler.c \
	$(MALI400_COMPILER_BACKEND_DIR)/address_allocator.c \
	$(MALI400_COMPILER_BACKEND_DIR)/serializer.c \
	$(MALI400_COMPILER_BACKEND_DIR)/liveness.c \
	$(MALI400_COMPILER_BACKEND_DIR)/eliminate_phi_nodes.c \
	$(MALI400_COMPILER_BACKEND_DIR)/graph_coloring.c \
	$(MALI400_COMPILER_BACKEND_DIR)/relocation.c \
	$(MALI400_COMPILER_BACKEND_DIR)/reservation.c \
	$(MALI400_COMPILER_BACKEND_DIR)/extra_info.c \
	$(MALI400_COMPILER_BACKEND_DIR)/instruction.c \
	$(MALI400_COMPILER_BACKEND_DIR)/interference_graph.c \
	$(MALI400_COMPILER_BACKEND_DIR)/static_cycle_count.c

MALI400_BACKEND_DEBUG_SRCS =

ifeq ($(DEBUGPRINT),1)
MALI400_BACKEND_DEBUG_SRCS += \
	$(MALI400_COMPILER_BACKEND_DIR)/liveness_printer.c
endif

MALI400_BACKEND_NOT_IN_SHADERGEN = $(MALI400_COMPILER_BACKEND_DIR)/static_cycle_count.c

TEST_SRCS += $(MALI400_COMPILER_BACKEND_DIR)/address_allocator.c $(MALI400_COMPILER_BACKEND_DIR)/arithmetic.c

SHADERGEN_SRCS += $(filter-out $(MALI400_BACKEND_NOT_IN_SHADERGEN), $(MALI400_BACKEND_SRCS))

MALI400_COMPILER_SRCS += $(MALI400_BACKEND_SRCS)
MALI400_COMPILER_DEBUG_SRCS += $(MALI400_BACKEND_DEBUG_SRCS)
