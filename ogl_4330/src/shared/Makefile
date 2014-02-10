#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2006-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#
#Magic to find the right directory to build from
ifndef FIND_BUILD_DIR_MAGIC
FIND_BUILD_DIR_MAGIC := 1
ifeq ($(wildcard topleveldir),)
.PHONY: $(MAKECMDGOALS) recurse_to_parent_and_build libMaliShared
$(MAKECMDGOALS): recurse_to_parent_and_build
recurse_to_parent_and_build:
	$(MAKE) -C .. $(MAKECMDGOALS)
endif
endif

SHARED_DIR ?= .

SHARED_SRCS = \
	$(SHARED_DIR)/named_list/mali_named_list.c \
	$(SHARED_DIR)/linked_list/mali_linked_list.c \
	$(SHARED_DIR)/mali_pixel_format.c  \
	$(SHARED_DIR)/mem_ref/mali_mem_ref.c \
	$(SHARED_DIR)/mali_shared_mem_ref.c \
	$(SHARED_DIR)/mali_surface.c \
	$(SHARED_DIR)/mali_surface_specifier.c \
	$(SHARED_DIR)/mali_image.c \
	$(SHARED_DIR)/mem_pool/mali_mem_pool.c

SHARED_SRCS +=\
	$(SHARED_DIR)/binary_shader/bs_loader.c \
	$(SHARED_DIR)/binary_shader/bs_loader_uniform.c \
	$(SHARED_DIR)/binary_shader/bs_loader_attribute.c \
	$(SHARED_DIR)/binary_shader/bs_loader_varying.c \
	$(SHARED_DIR)/binary_shader/bs_error.c \
	$(SHARED_DIR)/binary_shader/bs_symbol.c \
	$(SHARED_DIR)/binary_shader/bs_object.c \
	$(SHARED_DIR)/binary_shader/link_gp2.c \
	$(SHARED_DIR)/binary_shader/online_compiler_integration.c \
	$(SHARED_DIR)/m200_regs.c \
	$(SHARED_DIR)/m200_td.c \
	$(SHARED_DIR)/m200_tilelists.c \
	$(SHARED_DIR)/m200_gp_frame_builder.c \
	$(SHARED_DIR)/m200_frame.c \
	$(SHARED_DIR)/m200_hw_workarounds.c \
	$(SHARED_DIR)/m200_incremental_rendering.c \
	$(SHARED_DIR)/m200_readback_util.c \
	$(SHARED_DIR)/m200_quad.c \
	$(SHARED_DIR)/m200_texel_format.c \
	$(SHARED_DIR)/m200_texture.c \
	$(SHARED_DIR)/m200_projob.c \
	$(SHARED_DIR)/m200_surfacetracking.c \
	$(SHARED_DIR)/convert/m200_texture_convert.c \
	$(SHARED_DIR)/convert/mali_convert.c 

# Optional Tracing
ifeq ($(USE_TRACING),1)
	SHARED_SRCS += $(SHARED_DIR)/trace/oglestrace.c	\
		$(SHARED_DIR)/trace/systrace.c
endif

ifeq ($(MALI_SW_COUNTERS_ENABLED),1)
	SHARED_SRCS += $(SHARED_DIR)/mali_sw_counters.c 
endif

# Frame buffer dumping
ifeq ($(MALI_FRAMEBUFFER_DUMP_ENABLED),1)
	SHARED_SRCS += $(SHARED_DIR)/m200_fbdump.c
endif

ESSL_COMPILER_DIR=$(SHARED_DIR)/essl_compiler
-include $(ESSL_COMPILER_DIR)/Makefile

ifdef NO_ESSL_SUPPORT_FEATURE
	NO_ESSL_SUPPORT := 1
endif
ifdef NO_SHADERGEN_SUPPORT_FEATURE
	NO_SHADERGEN_SUPPORT := 1
endif

ifndef ESSL_COMPILER_PRESENT
	NO_ESSL_SUPPORT :=1
	NO_SHADERGEN_SUPPORT :=1
endif

ifdef NO_ESSL_SUPPORT
	CPPFLAGS += -DNO_SHADERGEN_SUPPORT
endif

ifdef NO_ESSL_SUPPORT
	CPPFLAGS += -DNO_ESSL_SUPPORT
endif

ifndef NO_SHADERGEN_SUPPORT
	SHARED_SRCS += $(SHARED_DIR)/shadergen_interface.c
endif

ifndef NO_ESSL_SUPPORT
	ifndef NO_SHADERGEN_SUPPORT
		COMP_LIB = compiler_shadergen$(LIB_EXT)
	else
		COMP_LIB = compiler$(LIB_EXT)
	endif
else
	ifndef NO_SHADERGEN_SUPPORT
		COMP_LIB = shadergen$(LIB_EXT)
	else
		COMP_LIB =
	endif
endif

DEP_SRCS += $(SHARED_SRCS)

