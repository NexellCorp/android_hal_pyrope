#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2001-2002, 2007-2011 ARM Limited
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

GLES_MALIGP2_M200_SG_DIR ?= .

GLES_MALIGP2_M200_SG_SRCS = \
	$(GLES_MALIGP2_M200_SG_DIR)/gles_shader_generator.c \
	$(GLES_MALIGP2_M200_SG_DIR)/gles_vertex_state_extractor.c \
	$(GLES_MALIGP2_M200_SG_DIR)/gles_fragment_state_extractor.c \
	$(GLES_MALIGP2_M200_SG_DIR)/dump_shader_info_common.c \
	$(GLES_MALIGP2_M200_SG_DIR)/dump_vert_shader_info.c \
	$(GLES_MALIGP2_M200_SG_DIR)/dump_frag_shader_info.c

$(call set-file-flavour,fast,	$(GLES_MALIGP2_M200_SG_DIR)/gles_vertex_state_extractor.c \
				$(GLES_MALIGP2_M200_SG_DIR)/gles_fragment_state_extractor.c )


MALIGP2_M200_SG_LIB=$(TMP_PRODUCT_DIR)/shader_generator-gles_sg$(LIB_EXT)

$(call make-target-static-library,$(MALIGP2_M200_SG_LIB),$(GLES_MALIGP2_M200_SG_SRCS),)
