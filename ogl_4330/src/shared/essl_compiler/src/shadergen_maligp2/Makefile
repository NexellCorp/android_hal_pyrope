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

SHADERGEN_MALIGP2_DIR ?= .

SHADERGEN_MALIGP2_SRCS = \
	$(SHADERGEN_MALIGP2_DIR)/select_pieces.c \
	$(SHADERGEN_MALIGP2_DIR)/glue_pieces.c \
	$(SHADERGEN_MALIGP2_DIR)/shadergen_maligp2_instruction.c \
	$(SHADERGEN_MALIGP2_DIR)/shader_pieces.vertp \
	$(SHADERGEN_MALIGP2_DIR)/shadergen_maligp2.c \
	$(SHADERGEN_MALIGP2_DIR)/shadergen_maligp2_state_equivalence.c


SHADERGEN_SRCS += $(SHADERGEN_MALIGP2_SRCS)

ifneq ($(FORK_OOM),1)
%.h %.c: %.vertp  $(call source-dir-to-binary-dir,$(HOST_SYSTEM),genpieces_gles11)
	$(Q)$(call source-dir-to-binary-dir,$(HOST_SYSTEM),genpieces_gles11) $<
	$(call $(quiet)cmd-echo-build,GEN,$<)
	$(SHADERGEN_MALIGP2_DIR)/make_piece_names.py $(SHADERGEN_MALIGP2_DIR)/shader_pieces.c $(SHADERGEN_MALIGP2_DIR)/dummy_copyright.h > $(SHADERGEN_MALIGP2_DIR)/piece_names.h
endif
