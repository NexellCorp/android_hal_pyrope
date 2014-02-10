/*
 * $Copyright:
 * ----------------------------------------------------------------
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 * ----------------------------------------------------------------
 * $
 */
#include "suite.h"
#include "egl_image_tools.h"


PFNEGLCREATEIMAGEKHRPROC _eglCreateImageKHR = NULL;
PFNEGLDESTROYIMAGEKHRPROC _eglDestroyImageKHR = NULL;

#if EGL_MALI_GLES 
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC _glEGLImageTargetTexture2DOES = NULL;
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC _glEGLImageTargetRenderbufferStorageOES = NULL;
#endif

#if EGL_MALI_VG
PFNVGCREATEEGLIMAGETARGETKHRPROC _vgCreateEGLImageTargetKHR = NULL;
#endif

int egl_image_tools_init_extensions( void  )
{
	_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
	if ( NULL == _eglCreateImageKHR ) return 1;

	_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
	if ( NULL == _eglDestroyImageKHR ) return 1;
#if EGL_MALI_GLES
	_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if ( NULL == _glEGLImageTargetTexture2DOES ) return 1;

	_glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	if ( NULL == _glEGLImageTargetRenderbufferStorageOES ) return 1;
#endif

#if EGL_MALI_VG
	_vgCreateEGLImageTargetKHR = (PFNVGCREATEEGLIMAGETARGETKHRPROC) eglGetProcAddress("vgCreateEGLImageTargetKHR");
	if ( NULL == _vgCreateEGLImageTargetKHR ) return 1;
#endif

	return 0;
}
