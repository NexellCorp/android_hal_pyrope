#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2001-2002, 2006-2010, 2012 ARM Limited
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

GLES_GB_DIR ?= .

GLES_GB_SRCS = \
	$(GLES_MALIGP2_GB_DIR)/gles_geometry_backend.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_plbu.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_vs.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_vs_attribs.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_vs_varyings.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_buffer_object.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_draw.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_rendering_state.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_cache.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_bounding_box.c \
	$(GLES_MALIGP2_GB_DIR)/gles_gb_vertex_rangescan.c

GB_LIB=$(TMP_PRODUCT_DIR)/mali_gp_geometry_common-gles_gb$(LIB_EXT)

$(call make-target-static-library,$(GB_LIB),$(GLES_GB_SRCS))
