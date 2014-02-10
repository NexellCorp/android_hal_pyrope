/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_config_extension.h
 * @brief EGL config extension handling
 */

#ifndef _EGL_CONFIG_EXTENSION_H_
#define _EGL_CONFIG_EXTENSION_H_

/**
 * @addtogroup egl_extension EGL Extensions
 *
 * @{
 */

#include "egl_config_extension_m200.h"

#if EGL_FEATURE_SWAP_REGION_ENABLE
#include "feature/swap_region/egl_sr_entrypoint_name.h"
#endif
#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
#include "feature/resource_profiling/egl_rp_entrypoint_name.h"
#endif
#if EGL_FEATURE_SURFACE_SCALING_ENABLE
#include "feature/surface_scaling/egl_ss_entrypoint_name.h"
#endif

static const char egl_extensions_gles1[] = {
#if EGL_KHR_image_ENABLE
	"EGL_KHR_image "
	"EGL_KHR_image_base "
	"EGL_KHR_image_pixmap "
#endif

#if EGL_KHR_vg_parent_image_ENABLE
	"EGL_KHR_vg_parent_image "
#endif

#if EGL_KHR_gl_texture_2D_image_GLES1_ENABLE
	"EGL_KHR_gl_texture_2D_image "
#endif

#if EGL_KHR_gl_texture_cubemap_image_GLES1_ENABLE
	"EGL_KHR_gl_texture_cubemap_image "
#endif

#if EGL_KHR_gl_texture_3D_image_GLES1_ENABLE
	"EGL_KHR_gl_texture_3D_image "
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES1_ENABLE
	"EGL_KHR_gl_renderbuffer_image "
#endif

#if EGL_KHR_reusable_sync_ENABLE
	"EGL_KHR_reusable_sync "
#endif

#if EGL_KHR_fence_sync_ENABLE
	"EGL_KHR_fence_sync "
#endif

#if EGL_KHR_image_system_ENABLE
	"EGL_KHR_image_system "
#endif

#if EGL_FEATURE_SWAP_REGION_ENABLE
	EGL_FEATURE_SWAP_REGION_EXT_NAME
#endif

#if EGL_ANDROID_image_native_buffer_ENABLE
	"EGL_ANDROID_image_native_buffer "
#endif

#if EGL_ANDROID_native_fence_sync_ENABLE
	"EGL_ANDROID_native_fence_sync "
#endif

#if EGL_ANDROID_framebuffer_target_ENABLE
	"EGL_ANDROID_framebuffer_target "
#endif

#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
	EGL_FEATURE_RESOURCE_PROFILING_NAME
	EGL_FEATURE_RESOURCE_PROFILING2_NAME
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
	EGL_FEATURE_SURFACE_SCALING_EXT_NAME
#endif

#ifdef EGL_KHR_lock_surface_ENABLE
	"EGL_KHR_lock_surface "
	"EGL_KHR_lock_surface2 "
#endif

#ifdef EGL_EXT_create_context_robustness_ENABLE
	"EGL_EXT_create_context_robustness "
#endif
};

static const char egl_extensions_gles2[] = {
#if EGL_KHR_image_ENABLE
	"EGL_KHR_image "
	"EGL_KHR_image_base "
	"EGL_KHR_image_pixmap "
#endif

#if EGL_KHR_vg_parent_image_ENABLE
	"EGL_KHR_vg_parent_image "
#endif

#if EGL_KHR_gl_texture_2D_image_GLES2_ENABLE
	"EGL_KHR_gl_texture_2D_image "
#endif

#if EGL_KHR_gl_texture_cubemap_image_GLES2_ENABLE
	"EGL_KHR_gl_texture_cubemap_image "
#endif

#if EGL_KHR_gl_texture_3D_image_GLES2_ENABLE
	"EGL_KHR_gl_texture_3D_image "
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES2_ENABLE
	"EGL_KHR_gl_renderbuffer_image "
#endif

#if EGL_KHR_reusable_sync_ENABLE
	"EGL_KHR_reusable_sync "
#endif

#if EGL_KHR_fence_sync_ENABLE
	"EGL_KHR_fence_sync "
#endif

#if EGL_KHR_image_system_ENABLE
	"EGL_KHR_image_system "
#endif

#if EGL_FEATURE_SWAP_REGION_ENABLE
	EGL_FEATURE_SWAP_REGION_EXT_NAME
#endif

#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
	EGL_FEATURE_RESOURCE_PROFILING_NAME
	EGL_FEATURE_RESOURCE_PROFILING2_NAME
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
	EGL_FEATURE_SURFACE_SCALING_EXT_NAME
#endif

#ifdef EGL_KHR_lock_surface_ENABLE
	"EGL_KHR_lock_surface "
	"EGL_KHR_lock_surface2 "
#endif

#ifdef EGL_EXT_create_context_robustness_ENABLE
	"EGL_EXT_create_context_robustness "
#endif

#if EGL_ANDROID_native_fence_sync_ENABLE
	"EGL_ANDROID_native_fence_sync "
#endif

#if EGL_ANDROID_framebuffer_target_ENABLE
	"EGL_ANDROID_framebuffer_target "
#endif
};

static const char egl_extensions_gles1_gles2[] = {
#if EGL_KHR_image_ENABLE
	"EGL_KHR_image "
	"EGL_KHR_image_base "
	"EGL_KHR_image_pixmap "
#endif

#if EGL_KHR_vg_parent_image_ENABLE
	"EGL_KHR_vg_parent_image "
#endif

#if EGL_KHR_gl_texture_2D_image_GLES1_ENABLE || EGL_KHR_gl_texture_2D_image_GLES2_ENABLE
	"EGL_KHR_gl_texture_2D_image "
#endif

#if EGL_KHR_gl_texture_cubemap_image_GLES1_ENABLE || EGL_KHR_gl_texture_cubemap_image_GLES2_ENABLE
	"EGL_KHR_gl_texture_cubemap_image "
#endif

#if EGL_KHR_gl_texture_3D_image_GLES1_ENABLE || EGL_KHR_gl_texture_3D_image_GLES2_ENABLE
	"EGL_KHR_gl_texture_3D_image "
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES1_ENABLE || EGL_KHR_gl_renderbuffer_image_GLES2_ENABLE
	"EGL_KHR_gl_renderbuffer_image "
#endif

#if EGL_KHR_reusable_sync_ENABLE
	"EGL_KHR_reusable_sync "
#endif

#if EGL_KHR_fence_sync_ENABLE
	"EGL_KHR_fence_sync "
#endif

#if EGL_KHR_image_system_ENABLE
	"EGL_KHR_image_system "
#endif

#if EGL_FEATURE_SWAP_REGION_ENABLE
	EGL_FEATURE_SWAP_REGION_EXT_NAME
#endif

#if EGL_ANDROID_image_native_buffer_ENABLE
	"EGL_ANDROID_image_native_buffer "
#endif

#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
	EGL_FEATURE_RESOURCE_PROFILING_NAME
	EGL_FEATURE_RESOURCE_PROFILING2_NAME
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
	EGL_FEATURE_SURFACE_SCALING_EXT_NAME
#endif

#ifdef EGL_KHR_lock_surface_ENABLE
	"EGL_KHR_lock_surface "
	"EGL_KHR_lock_surface2 "
#endif

#if EGL_ANDROID_recordable_ENABLE
	"EGL_ANDROID_recordable "
#endif

#if EGL_ANDROID_native_fence_sync_ENABLE
	"EGL_ANDROID_native_fence_sync "
#endif

#if EGL_ANDROID_framebuffer_target_ENABLE
	"EGL_ANDROID_framebuffer_target "
#endif

#ifdef EGL_EXT_create_context_robustness_ENABLE
	"EGL_EXT_create_context_robustness "
#endif
};

/** @} */ /* end group egl_extension */

#endif /* _EGL_CONFIG_EXTENSION_H_ */
