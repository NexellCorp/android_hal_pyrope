#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2005, 2007-2012 ARM Limited
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

OPENGLES_DIR ?= .

SHADER_GENERATOR=

# Override these to set different back ends if necessary
MALI200_GP_BACKEND ?= mali_gp_geometry_common

# Override this to use FIXED or FLOAT as FTYPE in the driver
GLES_FTYPE=FLOAT

# These objects are gles2 driver only
OPENGLES_2_SRCS = \
	$(OPENGLES_DIR)/gles2_entrypoints.c \
	$(OPENGLES_DIR)/gles2_vtable.c \
	$(OPENGLES_DIR)/gles2_context.c \
	$(OPENGLES_DIR)/gles2_extensions.c \
	$(OPENGLES_DIR)/gles2_draw.c \
	$(OPENGLES_DIR)/gles2_texture_object.c \
	$(OPENGLES_DIR)/gles2_state/gles2_hint.c \
	$(OPENGLES_DIR)/gles2_state/gles2_program_object.c \
	$(OPENGLES_DIR)/gles2_state/gles2_program_object_uniform.c \
	$(OPENGLES_DIR)/gles2_state/gles2_program_object_attribute.c \
	$(OPENGLES_DIR)/gles2_state/gles2_shader_object.c \
	$(OPENGLES_DIR)/gles2_state/gles2_enable.c \
	$(OPENGLES_DIR)/gles2_state/gles2_pixel.c \
	$(OPENGLES_DIR)/gles2_state/gles2_pixel_operations.c \
	$(OPENGLES_DIR)/gles2_state/gles2_state.c \
	$(OPENGLES_DIR)/gles2_state/gles2_tex_state.c \
	$(OPENGLES_DIR)/gles2_state/gles2_vertex_array.c \
	$(OPENGLES_DIR)/gles2_state/gles2_vertex_shader.c \
	$(OPENGLES_DIR)/gles2_state/gles2_viewport.c \
	$(OPENGLES_DIR)/gles2_state/gles2_rasterization.c \
	$(OPENGLES_DIR)/gles2_state/gles2_error.c \
	$(OPENGLES_DIR)/gles2_state/gles2_get.c \
	$(OPENGLES_DIR)/_gles2_build_info.c

# These objects are gles1 driver only
OPENGLES_1_SRCS = \
	$(OPENGLES_DIR)/gles1_entrypoints.c \
	$(OPENGLES_DIR)/gles1_vtable.c \
	$(OPENGLES_DIR)/gles1_context.c \
	$(OPENGLES_DIR)/gles1_extensions.c \
	$(OPENGLES_DIR)/gles1_draw.c \
	$(OPENGLES_DIR)/gles1_draw_tex.c \
	$(OPENGLES_DIR)/gles1_matrix.c \
	$(OPENGLES_DIR)/gles1_texture_object.c \
	$(OPENGLES_DIR)/gles1_state/gles1_enable.c \
	$(OPENGLES_DIR)/gles1_state/gles1_get.c \
	$(OPENGLES_DIR)/gles1_state/gles1_coloring.c \
	$(OPENGLES_DIR)/gles1_state/gles1_current.c \
	$(OPENGLES_DIR)/gles1_state/gles1_hint.c \
	$(OPENGLES_DIR)/gles1_state/gles1_lighting.c \
	$(OPENGLES_DIR)/gles1_state/gles1_pixel.c \
	$(OPENGLES_DIR)/gles1_state/gles1_pixel_operations.c \
	$(OPENGLES_DIR)/gles1_state/gles1_rasterization.c \
	$(OPENGLES_DIR)/gles1_state/gles1_viewport.c \
	$(OPENGLES_DIR)/gles1_state/gles1_transform.c \
	$(OPENGLES_DIR)/gles1_state/gles1_tex_state.c \
	$(OPENGLES_DIR)/gles1_state/gles1_vertex_array.c \
	$(OPENGLES_DIR)/gles1_state/gles1_state.c \
	$(OPENGLES_DIR)/gles1_state/gles1_error.c \
	$(OPENGLES_DIR)/_gles1_build_info.c

# These are M200 only
OPENGLES_M200_SRCS = \
	$(OPENGLES_DIR)/m200_backend/gles_m200_texel_format.c \
	$(OPENGLES_DIR)/m200_backend/gles_fb_swappers.c \
	$(OPENGLES_DIR)/m200_backend/gles_fb_texture_object.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_td.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_uniforms.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_fragment_backend.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_rsw_setup.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_shaders.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_clear.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_rsw.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_rsw_map.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_mipmap.c \
	$(OPENGLES_DIR)/m200_backend/gles_fb_rendering_state.c \
	$(OPENGLES_DIR)/m200_backend/gles_fb_texture_memory.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_dummy_quad.c \
	$(OPENGLES_DIR)/m200_backend/gles_m200_draw_common.c 

# These objects are common for both drivers
OPENGLES_CORE_SRCS = \
	$(OPENGLES_DIR)/gles_entrypoints.c \
	$(OPENGLES_DIR)/gles_texture_object_surfaces.c \
	$(OPENGLES_DIR)/gles_texture_object.c \
	$(OPENGLES_DIR)/gles_buffer_object.c \
	$(OPENGLES_DIR)/gles_context.c \
	$(OPENGLES_DIR)/gles_share_lists.c \
	$(OPENGLES_DIR)/gles_extensions.c \
	$(OPENGLES_DIR)/gles_read_pixels.c \
	$(OPENGLES_DIR)/gles_util.c \
	$(OPENGLES_DIR)/gles_convert.c \
	$(OPENGLES_DIR)/gles_flush.c \
	$(OPENGLES_DIR)/gles_clear.c \
	$(OPENGLES_DIR)/gles_object.c \
	$(OPENGLES_DIR)/gles_mipmap.c \
	$(OPENGLES_DIR)/gles_mipmap_filter.c \
	$(OPENGLES_DIR)/gles_draw.c \
	$(OPENGLES_DIR)/gles_incremental_rendering.c \
	$(OPENGLES_DIR)/gles_common_state/gles_tex_state.c \
	$(OPENGLES_DIR)/gles_common_state/gles_vertex_array.c \
	$(OPENGLES_DIR)/gles_common_state/gles_pixel_operations.c \
	$(OPENGLES_DIR)/gles_common_state/gles_rasterization.c \
	$(OPENGLES_DIR)/gles_common_state/gles_framebuffer_control.c \
	$(OPENGLES_DIR)/gles_common_state/gles_framebuffer_state.c \
	$(OPENGLES_DIR)/gles_common_state/gles_program_rendering_state.c \
	$(OPENGLES_DIR)/gles_common_state/gles_multisampling.c \
	$(OPENGLES_DIR)/gles_common_state/gles_viewport.c \
	$(OPENGLES_DIR)/gles_renderbuffer_object.c \
	$(OPENGLES_DIR)/gles_fbo_bindings.c \
	$(OPENGLES_DIR)/gles_framebuffer_object.c

OPENGLES_1_SRCS_SHIM = \
	$(OPENGLES_DIR)/gles_entrypoints_shim.c \
	$(OPENGLES_DIR)/gles1_entrypoints_shim.c

OPENGLES_2_SRCS_SHIM = \
        $(OPENGLES_DIR)/gles_entrypoints_shim.c \
        $(OPENGLES_DIR)/gles2_entrypoints_shim.c

ifneq ($(MALI_BUILD_ANDROID_MONOLITHIC), 1)
OPENGLES_CORE_SRCS += \
        $(OPENGLES_DIR)/gles_entrypoints_shim.c

OPENGLES_1_SRCS += \
        $(OPENGLES_DIR)/gles1_entrypoints_shim.c

OPENGLES_2_SRCS += \
        $(OPENGLES_DIR)/gles2_entrypoints_shim.c
endif

# If we are building MONOLITHIC drivers with OpenGL ES 2.x included
# then OpenGL ES 1.x must use the 2.x share lists backend so that
# it can delete OpenGL ES 2.x objects as well
ifdef MALI_MONOLITHIC
ifdef USE_OPENGLES_2
OPENGLES_CORE_SRCS += $(OPENGLES_DIR)/gles2_share_lists_backend.c
else
OPENGLES_1_SRCS += $(OPENGLES_DIR)/gles1_share_lists_backend.c
endif
else
OPENGLES_1_SRCS += $(OPENGLES_DIR)/gles1_share_lists_backend.c
OPENGLES_2_SRCS += $(OPENGLES_DIR)/gles2_share_lists_backend.c
endif

# Conditional setting of GLES2_SRCS and GLES1_SRCS
ifdef USE_OPENGLES_2
	GLES2_SRCS = $(OPENGLES_2_SRCS)
endif

ifdef USE_OPENGLES_1
	GLES1_SRCS = $(OPENGLES_1_SRCS)
endif

#need to be in fast mode to support non-interworking callees.
$(call set-file-flavour,fast, 	$(OPENGLES_DIR)/gles_entrypoints.c \
						$(OPENGLES_DIR)/gles1_entrypoints.c \
						$(OPENGLES_DIR)/gles2_entrypoints.c \
						$(OPENGLES_DIR)/gles2_vtable.c \
						$(OPENGLES_DIR)/gles1_vtable.c )

# Core specific additional includes

## NOTE: version_info.mak relies on MALI200_GP_BACKEND to set the 'GEOM' version info correctly.

GEOMETRY_BACKEND = $(MALI200_GP_BACKEND)
FRAGMENT_BACKEND_SRCS = $(OPENGLES_M200_SRCS)

ifdef USE_OPENGLES_1
		SHADER_GENERATOR = shader_generator
endif

# if no backend found, abort and terminate
ifndef GEOMETRY_BACKEND
$(error NO Geometry Backend specified, abort)
endif

# Other External libraries on which the drivers depend

SHADER_GENERATOR_LIB :=
ifneq ($(SHADER_GENERATOR),)
	SHADER_GENERATOR_LIB := $(TMP_PRODUCT_DIR)/$(SHADER_GENERATOR)-gles_sg$(LIB_EXT)
endif

# Default makeflags
CPPFLAGS += -DGLES_FTYPE=$(GLES_FTYPE) -I$(DRIVER_DIR)/src/opengles
ifneq ($(SHADER_GENERATOR),)
	CPPFLAGS += -DUSING_SHADER_GENERATOR
endif

ifeq ($(USE_TRACING),1)
	CPPFLAGS += -DGL_TRACE_WRAPPER
	GLES1_SRCS += $(SHARED_DIR)/trace/gl1trace.c
endif

ifeq ($(MALI_API_TRACE),TRUE)
	OPENGLES_CORE_SRCS += $(OPENGLES_DIR)/gles_api_trace.c
	OPENGLES_1_SRCS += $(OPENGLES_DIR)/gles1_api_trace.c
	OPENGLES_2_SRCS += $(OPENGLES_DIR)/gles2_api_trace.c
endif

GLES_MALIGP2_GB_DIR=$(OPENGLES_DIR)/$(MALI200_GP_BACKEND)
include $(GLES_MALIGP2_GB_DIR)/Makefile
GLES_MALIGP2_M200_SG_DIR=$(OPENGLES_DIR)/shader_generator
include $(GLES_MALIGP2_M200_SG_DIR)/Makefile

# We are building GLES 1.x
ifdef USE_OPENGLES_1

# Ensure we set _DLL_EXPORTS but just for the gles source files when they themselves are compiled
$(call source-to-object,$(TARGET_SYSTEM),$(GLES1_SRCS) $(OPENGLES_CORE_SRCS) $(FRAGMENT_BACKEND_SRCS) $(GB_LIB)): CPPFLAGS+=-D_DLL_EXPORTS
TOOLCHAIN_SPECIFIC_GLES1_SRCS=

# Create libGLESv1_CM aliases to versioned library links, if required.
$(call make-library-aliases, \
	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_GLES11_LIB_VERSIONED)) \
	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_GLES11_LIB_ALIASES)) \
	)

ifndef MALI_MONOLITHIC
$(call make-target-dynamic-library,$(MALI_GLES11_LIB_VERSIONED), \
				$(GLES1_SRCS) $(OPENGLES_CORE_SRCS) $(FRAGMENT_BACKEND_SRCS) \
				$(MATH_LIB) $(MALI_LIB) $(GB_LIB) $(SHADER_GENERATOR_LIB) $(TOOLCHAIN_SPECIFIC_GLES1_SRCS), MULTITHREADED)
endif

# autogenerated build version info
$(call make-build-info-file,$(OPENGLES_DIR)/_gles1_build_info.c,gles,$(OPENGLES_DIR),,USING_MALI200=${USING_MALI200} USING_MALI400=${USING_MALI400} USING_MALI450=${USING_MALI450} GLES1_MAJOR_VERSION=${MALI_GLES_1_MAJOR_VERSION} GLES1_MINOR_VERSION=${MALI_GLES_1_MINOR_VERSION} GEOMETRY_BACKEND=${GEOMETRY_BACKEND})
endif # USE_OPENGLES_1


# We are building GLES 2.x
ifdef USE_OPENGLES_2
# Ensure we set _DLL_EXPORTS but just for the gles source files when they themselves are compiled
$(call source-to-object,$(TARGET_SYSTEM),$(GLES2_SRCS) $(OPENGLES_CORE_SRCS) $(FRAGMENT_BACKEND_SRCS) $(GB_LIB)): CPPFLAGS+=-D_DLL_EXPORTS
TOOLCHAIN_SPECIFIC_GLES2_SRCS=

# Create libGLESv2 aliases to versioned library links, if required.
$(call make-library-aliases, \
	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_GLES20_LIB_VERSIONED)) \
	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_GLES20_LIB_ALIASES)) \
	)

ifndef MALI_MONOLITHIC
# Create actual versioned library for libGLESv2
$(call make-target-dynamic-library,$(MALI_GLES20_LIB_VERSIONED), \
				$(GLES2_SRCS) $(OPENGLES_CORE_SRCS) $(FRAGMENT_BACKEND_SRCS) \
				$(MATH_LIB) $(MALI_LIB) $(GB_LIB) $(TOOLCHAIN_SPECIFIC_GLES2_SRCS), MULTITHREADED)
endif

# autogenerated build version info
$(call make-build-info-file,$(OPENGLES_DIR)/_gles2_build_info.c,gles20,$(OPENGLES_DIR),,USING_MALI200=${USING_MALI200} USING_MALI400=${USING_MALI400} USING_MALI450=${USING_MALI450} GLES2_MAJOR_VERSION=${MALI_GLES_2_MAJOR_VERSION} GLES2_MINOR_VERSION=${MALI_GLES_2_MINOR_VERSION} GEOMETRY_BACKEND=${GEOMETRY_BACKEND})
endif # USE_OPENGLES_2

