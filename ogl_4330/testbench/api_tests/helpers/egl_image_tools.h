/*
 * $Copyright:
 * ----------------------------------------------------------------
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 * ----------------------------------------------------------------
 * $
 */

#ifndef EGL_IMAGE_TOOLS_H
#define EGL_IMAGE_TOOLS_H

#if EGL_MALI_GLES
#if MALI_USE_GLES_2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GLES/gl.h>
#include <GLES/glext.h>
#endif
#endif

#if EGL_MALI_VG

#ifdef __SYMBIAN32__
#define VG_VGEXT_PROTOTYPES
#define VG_MAX_ENUM 0x7FFFFFFF
#define VG_API_ENTRY VG_APIENTRY
#define VGU_API_ENTRY VGU_APIENTRY
#endif

#include <VG/openvg.h>
#include <VG/vgext.h>
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) ( sizeof(x) / sizeof(x[0]) )
#endif

extern PFNEGLCREATEIMAGEKHRPROC _eglCreateImageKHR;
extern PFNEGLDESTROYIMAGEKHRPROC _eglDestroyImageKHR;

#if EGL_MALI_GLES
extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC _glEGLImageTargetTexture2DOES;
extern PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC _glEGLImageTargetRenderbufferStorageOES;
#endif

#if EGL_MALI_VG
extern PFNVGCREATEEGLIMAGETARGETKHRPROC _vgCreateEGLImageTargetKHR;
#endif

/**
 * Initializes extensions required for EGLImage
 *
 * @return 0 on success, non-0 on failure
 */
int egl_image_tools_init_extensions( void );

#endif /* EGL_IMAGE_TOOLS_H */
