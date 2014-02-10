/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_config_extension_m200.h
 * @brief EGL config extension handling for Mali-200 / Mali-400
 */

#ifndef _EGL_CONFIG_EXTENSION_M200_H_
#define _EGL_CONFIG_EXTENSION_M200_H_

#if !defined(USING_MALI200) && !defined(USING_MALI400) && !defined(USING_MALI450)
#error "Included incorrect EGL extension-file for Mali200/300/400/450\n"
#endif

#define EGL_HAS_EXTENSIONS                      1
#define EGL_KHR_image_ENABLE                    1

#if EGL_PLATFORM_LOCKSURFACE_SUPPORTED
#define EGL_KHR_lock_surface_ENABLE             1
#endif

#if EGL_PLATFORM_PIXMAP_LOCKSURFACE_SUPPORTED
#define EGL_KHR_pixmap_lock_surface_ENABLE      1
#endif

#ifdef EGL_MALI_VG
#define EGL_KHR_vg_parent_image_ENABLE          1
#endif

#ifdef EGL_MALI_GLES
#define EGL_KHR_gl_texture_2D_image_GLES1_ENABLE      1
#define EGL_KHR_gl_texture_2D_image_GLES2_ENABLE      1
#define EGL_KHR_gl_texture_cubemap_image_GLES1_ENABLE 0
#define EGL_KHR_gl_texture_cubemap_image_GLES2_ENABLE 1
#define EGL_KHR_gl_texture_3D_image_GLES1_ENABLE      0
#define EGL_KHR_gl_texture_3D_image_GLES2_ENABLE      0
#define EGL_KHR_gl_renderbuffer_image_GLES1_ENABLE    0
#define EGL_KHR_gl_renderbuffer_image_GLES2_ENABLE    1
#ifndef HAVE_ANDROID_OS
#define EGL_EXT_create_context_robustness_ENABLE      1
#endif
#endif /* EGL_MALI_GLES */

#define EGL_KHR_reusable_sync_ENABLE           1
#define EGL_KHR_fence_sync_ENABLE              1

/* Android specific extensions */
#ifdef HAVE_ANDROID_OS
#define EGL_ANDROID_image_native_buffer_ENABLE 1
#else
#define EGL_ANDROID_image_native_buffer_ENABLE 0
#endif

#if defined(HAVE_ANDROID_OS) && (MALI_ANDROID_API > 13)
#define EGL_ANDROID_recordable_ENABLE 1
#else
#define EGL_ANDROID_recordable_ENABLE 0
#endif

#if defined(HAVE_ANDROID_OS) && (MALI_ANDROID_API > 16)
#define EGL_ANDROID_native_fencesync_ENABLE 1
#define EGL_ANDROID_framebuffer_target_ENABLE 1 
#else
#define EGL_ANDROID_native_fencesync_ENABLE 0
#define EGL_ANDROID_framebuffer_target_ENABLE 0
#endif

#if 0 && !defined( EGL_KHR_image_system_ENABLE )
/* This extension will work only with UMP support */
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
#if !defined(HAVE_ANDROID_OS) && !defined(EGL_BACKEND_X11)
#define EGL_KHR_image_system_ENABLE	1
#endif
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER */
#endif /* EGL_KHR_image_system_ENABLE */

#endif /* _EGL_CONFIG_EXTENSION_M200_H_ */
