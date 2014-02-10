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
 * @file egl_entrypoints_shim.h
 * @brief Contains helper macros for the egl_entrypoints_shim.c files
 *  The macros are used to produce files with thin shims for export  
 */

#ifndef _EGL_ENTRYPOINTS_SHIM_H_
#define _EGL_ENTRYPOINTS_SHIM_H_

/* this wraps the whole file so nothing is included without this define */
#ifdef MALI_BUILD_ANDROID_MONOLITHIC

#include <shared/mali_egl_image.h>
#include "egl_shims_common.h"
#include "egl_entrypoints.h"

typedef 		mali_egl_image * mali_eglImgPtr;

MALI_DECLARE_SHIM_EGL_APIENTRY_b4( eglGetConfigs , EGLDisplay, dpy, maliEGLConfigPtr, configs, EGLint, config_size, maliEGLIntPtr, num_config)
MALI_DECLARE_SHIM_EGL_APIENTRY_b5( eglChooseConfig , EGLDisplay, dpy, maliEGLConstIntPtr, attrib_list, maliEGLConfigPtr, configs, EGLint, config_size, maliEGLIntPtr, num_config)
MALI_DECLARE_SHIM_EGL_APIENTRY_b4( eglGetConfigAttrib , EGLDisplay, dpy, EGLConfig, config, EGLint, attribute, maliEGLIntPtr, value)
MALI_DECLARE_SHIM_EGL_APIENTRY_i0( eglGetError , void )
MALI_DECLARE_SHIM_EGL_APIENTRY_ccp2( eglQueryString , EGLDisplay, dpy, EGLint, name )
MALI_DECLARE_SHIM_EGL_APIENTRY_b0( eglWaitClient , void )
MALI_DECLARE_SHIM_EGL_APIENTRY_b0( eglWaitGL , void )
MALI_DECLARE_SHIM_EGL_APIENTRY_b1( eglWaitNative , EGLint, engine )
MALI_DECLARE_SHIM_EGL_APIENTRY_b2( eglSwapBuffers , EGLDisplay, dpy, EGLSurface, surface )
MALI_DECLARE_SHIM_EGL_APIENTRY_b3( eglCopyBuffers , EGLDisplay, dpy, EGLSurface, surface, EGLNativePixmapType, target)
MALI_DECLARE_SHIM_EGL_APIENTRY_b2( eglSwapInterval , EGLDisplay, dpy, EGLint, interval )
MALI_DECLARE_SHIM_EGL_APIENTRY_b3( eglBindTexImage , EGLDisplay, dpy, EGLSurface, surface, EGLint, buffer )
MALI_DECLARE_SHIM_EGL_APIENTRY_b3( eglReleaseTexImage , EGLDisplay, dpy, EGLSurface, surface, EGLint, buffer )
MALI_DECLARE_SHIM_EGL_APIENTRY_b1( eglBindAPI , EGLenum, api )
MALI_DECLARE_SHIM_EGL_APIENTRY_e0( eglQueryAPI , void )
MALI_DECLARE_SHIM_EGL_APIENTRY_EC4( eglCreateContext , EGLDisplay, dpy, EGLConfig, config, EGLContext, share_list, maliEGLConstIntPtr, attrib_list)
MALI_DECLARE_SHIM_EGL_APIENTRY_b2( eglDestroyContext , EGLDisplay, dpy, EGLContext, ctx )
MALI_DECLARE_SHIM_EGL_APIENTRY_b4( eglQueryContext , EGLDisplay, dpy, EGLContext, ctx, EGLint, attribute, maliEGLIntPtr, value )
MALI_DECLARE_SHIM_EGL_APIENTRY_b4( eglMakeCurrent , EGLDisplay, dpy, EGLSurface, draw, EGLSurface, read, EGLContext, ctx )
MALI_DECLARE_SHIM_EGL_APIENTRY_EC0( eglGetCurrentContext , void )
MALI_DECLARE_SHIM_EGL_APIENTRY_ED1( eglGetDisplay , EGLNativeDisplayType, display_id )
MALI_DECLARE_SHIM_EGL_APIENTRY_ED0( eglGetCurrentDisplay , void )
MALI_DECLARE_SHIM_EGL_APIENTRY_b3( eglInitialize , EGLDisplay, dpy, maliEGLIntPtr, major, maliEGLIntPtr, minor )
MALI_DECLARE_SHIM_EGL_APIENTRY_b1( eglTerminate , EGLDisplay, dpy )
MALI_DECLARE_SHIM_EGL_APIENTRY_ES4( eglCreateWindowSurface , EGLDisplay, dpy, EGLConfig, config, EGLNativeWindowType, win, maliEGLConstIntPtr, attrib_list )
MALI_DECLARE_SHIM_EGL_APIENTRY_ES3( eglCreatePbufferSurface , EGLDisplay, dpy, EGLConfig, config, maliEGLConstIntPtr, attrib_list)
MALI_DECLARE_SHIM_EGL_APIENTRY_ES5( eglCreatePbufferFromClientBuffer , EGLDisplay, dpy, EGLenum, buftype, EGLClientBuffer, buffer, EGLConfig, config, maliEGLConstIntPtr, attrib_list )
MALI_DECLARE_SHIM_EGL_APIENTRY_ES4( eglCreatePixmapSurface , EGLDisplay, dpy, EGLConfig, config, EGLNativePixmapType, pixmap, maliEGLConstIntPtr, attrib_list)
MALI_DECLARE_SHIM_EGL_APIENTRY_b2( eglDestroySurface , EGLDisplay, dpy, EGLSurface, surface )
MALI_DECLARE_SHIM_EGL_APIENTRY_b4( eglQuerySurface , EGLDisplay, dpy, EGLSurface, surface, EGLint, attribute, maliEGLIntPtr, value )
MALI_DECLARE_SHIM_EGL_APIENTRY_b4( eglSurfaceAttrib , EGLDisplay, dpy, EGLSurface, surface, EGLint, attribute, EGLint, value )
MALI_DECLARE_SHIM_EGL_APIENTRY_ES1( eglGetCurrentSurface , EGLint, readdraw )
MALI_DECLARE_SHIM_EGL_APIENTRY_b0( eglReleaseThread , void )
MALI_DECLARE_SHIM_EGL_APIENTRY_EPT1( eglGetProcAddress, const char*, procname)
#if EGL_KHR_reusable_sync_ENABLE
MALI_DECLARE_SHIM_EGL_APIENTRY_SKR3( eglCreateSyncKHR,  EGLDisplay, dpy, EGLenum, type, maliEGLConstIntPtr, attrib_list )
MALI_DECLARE_SHIM_EGL_APIENTRY_b2( eglDestroySyncKHR,  EGLDisplay, dpy, EGLSyncKHR, sync )
MALI_DECLARE_SHIM_EGL_APIENTRY_i4( eglClientWaitSyncKHR,  EGLDisplay, dpy, EGLSyncKHR, sync, EGLint, flags, EGLTimeKHR, timeout )
MALI_DECLARE_SHIM_EGL_APIENTRY_b3( eglSignalSyncKHR,  EGLDisplay, dpy, EGLSyncKHR, sync, EGLenum, mode )
MALI_DECLARE_SHIM_EGL_APIENTRY_b4( eglGetSyncAttribKHR,  EGLDisplay, dpy, EGLSyncKHR, sync, EGLint, attribute, maliEGLIntPtr, value )
#endif
#if EGL_KHR_image_ENABLE
MALI_DECLARE_SHIM_EGL_APIENTRY_IKR5( eglCreateImageKHR , EGLDisplay, dpy, EGLContext, ctx, EGLenum, target, EGLClientBuffer, buffer, maliEGLIntPtr, attr_list )
MALI_DECLARE_SHIM_EGL_APIENTRY_b2( eglDestroyImageKHR , EGLDisplay, dpy, EGLImageKHR, image )
#endif
#if EGL_KHR_lock_surface_ENABLE
MALI_DECLARE_SHIM_EGL_APIENTRY_b3( eglLockSurfaceKHR , EGLDisplay, display, EGLSurface, surface, maliEGLConstIntPtr, attrib_list )
MALI_DECLARE_SHIM_EGL_APIENTRY_b2( eglUnlockSurfaceKHR , EGLDisplay, display, EGLSurface, surface )
#endif
#if EGL_KHR_image_system_ENABLE
MALI_DECLARE_SHIM_EGL_APIENTRY_CNKR1( eglGetClientNameKHR, EGLDisplay, display)
MALI_DECLARE_SHIM_EGL_APIENTRY_INKR4( eglExportImageKHR, EGLDisplay, display, EGLImageKHR, image, EGLClientNameKHR, target_client, maliEGLConstIntPtr, attrib_list)
#endif

#if EGL_FEATURE_SWAP_REGION_ENABLE
MALI_DECLARE_SHIM_EGL_APIENTRY_b4(eglSwapBuffersRegionNOK, EGLDisplay, dpy, EGLSurface, surface, EGLint, numrects, maliEGLConstIntPtr, rects)
#endif


MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b1( mali_egl_image_init , mali_egl_image_version, version )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_i0( mali_egl_image_get_error , void )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_megli1( mali_egl_image_lock_ptr , EGLImageKHR, image )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b1( mali_egl_image_unlock_ptr , EGLImageKHR, image )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b3( mali_egl_image_set_data , mali_eglImgPtr, image, maliEGLIntPtr, attribs, maliVoidPtr, data )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b2( mali_egl_image_get_width , mali_eglImgPtr, image, maliEGLIntPtr, width )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b2( mali_egl_image_get_height , mali_eglImgPtr, image, maliEGLIntPtr, height )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b2( mali_egl_image_get_format , mali_eglImgPtr, image, maliEGLIntPtr, format )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b2( mali_egl_image_get_miplevels , mali_eglImgPtr, image, maliEGLIntPtr, miplevels )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_pv2( mali_egl_image_map_buffer , mali_eglImgPtr, image, maliEGLIntPtr, attribs )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b2( mali_egl_image_unmap_buffer , mali_eglImgPtr, image, maliEGLIntPtr, attribs )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b3( mali_egl_image_get_buffer_width , mali_eglImgPtr, image, maliEGLIntPtr, attribs, maliEGLIntPtr, width )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b3( mali_egl_image_get_buffer_height , mali_eglImgPtr, image, maliEGLIntPtr, attribs, maliEGLIntPtr, height )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b3( mali_egl_image_get_buffer_secure_id , mali_eglImgPtr, image, maliEGLIntPtr, attribs, maliEGLIntPtr, secure_id )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b3( mali_egl_image_get_buffer_layout , mali_eglImgPtr, image, maliEGLIntPtr, attribs, maliEGLIntPtr, layout )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b1( mali_egl_image_create_sync , mali_eglImgPtr, image )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b1( mali_egl_image_set_sync , mali_eglImgPtr, image )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b1( mali_egl_image_unset_sync , mali_eglImgPtr, image )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b2( mali_egl_image_wait_sync , mali_eglImgPtr, image, EGLint, timeout )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_IKR2( mali_egl_image_create , EGLDisplay, display, maliEGLIntPtr, attribs )
MALI_EGL_IMAGE_EXPORT MALI_DECLARE_SHIM_EGL_APIENTRY_b1( mali_egl_image_destroy , EGLImageKHR, image )


#endif /* MALI_BUILD_ANDROID_MONOLITHIC */

#endif /* _EGL_ENTRYPOINTS_SHIM_H */
