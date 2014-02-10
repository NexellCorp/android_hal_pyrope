/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <mali_system.h>
#include "egl_config_extension.h"
#ifdef EGL_HAS_EXTENSIONS
#include <EGL/mali_egl.h>
#include <EGL/mali_eglext.h>
#endif
#include "egl_extensions.h"
#include "egl_display.h"
#ifdef EGL_MALI_GLES
#include "egl_gles.h"
#endif
#if EGL_HAS_EXTENSIONS
#define EGL_EXTENSION_EX(name, custom_name) {#name, (__egl_funcptr)custom_name }
#ifndef MALI_BUILD_ANDROID_MONOLITHIC
#define EGL_EXTENSION(name) EGL_EXTENSION_EX(name, name)
#define MALI_EGL_NAME_WRAP(a) a
#else
#define MALI_EGL_NAME_WRAP(a) shim_##a
#define EGL_EXTENSION(name) {#name, (__egl_funcptr)shim_##name }
#endif

/** the proc function pointer */
typedef void (*__egl_funcptr)();

/** forward declarations of the extension API functions */
#if EGL_KHR_image_ENABLE
MALI_IMPORT EGLImageKHR MALI_EGL_NAME_WRAP(eglCreateImageKHR)( EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, EGLint *attr_list );
MALI_IMPORT EGLBoolean MALI_EGL_NAME_WRAP(eglDestroyImageKHR)( EGLDisplay dpy, EGLImageKHR image );
#endif

#if EGL_KHR_reusable_sync_ENABLE
MALI_IMPORT EGLSyncKHR MALI_EGL_NAME_WRAP(eglCreateSyncKHR)( EGLDisplay dpy, EGLenum type, const EGLint *attrib_list );
MALI_IMPORT EGLBoolean MALI_EGL_NAME_WRAP(eglDestroySyncKHR)( EGLDisplay dpy, EGLSyncKHR sync );
MALI_IMPORT EGLint MALI_EGL_NAME_WRAP(eglClientWaitSyncKHR)( EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout );
MALI_IMPORT EGLBoolean MALI_EGL_NAME_WRAP(eglSignalSyncKHR)( EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode );
MALI_IMPORT EGLBoolean MALI_EGL_NAME_WRAP(eglGetSyncAttribKHR)( EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value );
#endif

#if EGL_KHR_image_system_ENABLE
MALI_IMPORT EGLClientNameKHR MALI_EGL_NAME_WRAP(eglGetClientNameKHR)(EGLDisplay dpy);
MALI_IMPORT EGLImageNameKHR MALI_EGL_NAME_WRAP(eglExportImageKHR)(EGLDisplay dpy, EGLImageKHR image, EGLClientNameKHR target_client, const EGLint *attrib_list);
#endif

#if EGL_FEATURE_SWAP_REGION_ENABLE
#include "feature/swap_region/egl_sr_entrypoint.h"
#include "feature/swap_region/egl_sr_entrypoint_name.h"
MALI_IMPORT EGLBoolean MALI_EGL_NAME_WRAP(eglSwapBuffersRegionNOK)( EGLDisplay dpy, EGLSurface surface, EGLint numrects, const EGLint* rects );
#endif

#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
#include "feature/resource_profiling/egl_rp_entrypoint.h"
#include "feature/resource_profiling/egl_rp_entrypoint_name.h"
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
#include "feature/surface_scaling/egl_ss_entrypoints.h"
#include "feature/surface_scaling/egl_ss_entrypoint_name.h"
#endif

#if EGL_KHR_lock_surface_ENABLE
EGLAPI EGLBoolean EGLAPIENTRY MALI_EGL_NAME_WRAP(eglLockSurfaceKHR)( EGLDisplay, EGLSurface, const EGLint* );
EGLAPI EGLBoolean EGLAPIENTRY MALI_EGL_NAME_WRAP(eglUnlockSurfaceKHR)( EGLDisplay, EGLSurface );
#endif

#if EGL_ANDROID_native_fencesync_ENABLE
EGLAPI EGLint EGLAPIENTRY MALI_EGL_NAME_WRAP(eglDupNativeFenceFDANDROID)(EGLDisplay dpy, EGLSyncKHR sync);
#endif
/** list of extension function addresses */
const struct
{
	char *name;
	__egl_funcptr function;
} __egl_extensions[] = {
	/* put extensions in the form EGL_EXTENSION( funcname ) here */
#if EGL_KHR_image_ENABLE
	EGL_EXTENSION(eglCreateImageKHR),
	EGL_EXTENSION(eglDestroyImageKHR),
	EGL_EXTENSION(eglDestroyImageKHR),
#ifdef EGL_MALI_GLES
	{"glEGLImageTargetTexture2DOES", __egl_gles_image_target_texture_2d},
#endif

#endif

#ifdef EXTENSION_EGL_IMAGE_OES_ENABLE
#ifdef EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
#ifdef EGL_MALI_GLES
	{"glEGLImageTargetRenderbufferStorageOES", __egl_gles_image_target_renderbuffer_storage},
#endif
#endif
#endif

#if EGL_KHR_reusable_sync_ENABLE
	EGL_EXTENSION(eglCreateSyncKHR),
	EGL_EXTENSION(eglDestroySyncKHR),
	EGL_EXTENSION(eglClientWaitSyncKHR),
	EGL_EXTENSION(eglSignalSyncKHR),
	EGL_EXTENSION(eglGetSyncAttribKHR),
#endif

#if EGL_KHR_lock_surface_ENABLE
	EGL_EXTENSION(eglLockSurfaceKHR),
	EGL_EXTENSION(eglUnlockSurfaceKHR),
#endif

#if EGL_KHR_image_system_ENABLE
	EGL_EXTENSION(eglGetClientNameKHR),
	EGL_EXTENSION(eglExportImageKHR),
#endif

#if EGL_FEATURE_SWAP_REGION_ENABLE
	EGL_EXTENSION(eglSwapBuffersRegionNOK),
#endif

#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
	EGL_EXTENSION(EGL_FEATURE_RESOURCE_PROFILING_ENT_FN),
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
	EGL_EXTENSION(EGL_FEATURE_SURFACE_SCALING_QUERY_FN),
    EGL_EXTENSION(EGL_FEATURE_SURFACE_SCALING_SET_FN),
#endif

#if EGL_ANDROID_native_fencesync_ENABLE
	EGL_EXTENSION( eglDupNativeFenceFDANDROID ),
#endif
};
#endif /* EGL_HAS_EXTENSIONS */

void (* _egl_get_proc_address_internal( const char *procname) )()
{
#if EGL_HAS_EXTENSIONS
	u32 i = 0;
	MALI_CHECK_NON_NULL( procname, NULL );

	for ( i=0; i < (sizeof(__egl_extensions) / sizeof(__egl_extensions[0])); i++ )
	{
		if ( _mali_sys_strcmp( __egl_extensions[i].name, procname ) == 0 )
		{
			return __egl_extensions[i].function;
		}
	}
#endif

	return NULL;
}
