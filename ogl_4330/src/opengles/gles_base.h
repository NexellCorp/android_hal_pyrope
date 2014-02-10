/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_BASE_H_
#define _GLES_BASE_H_

enum gles_api_version
{
	GLES_API_VERSION_NO_VERSION,
	GLES_API_VERSION_1,
	GLES_API_VERSION_2
};

#include <mali_system.h>
#include <EGL/mali_egl.h>
#include <EGL/mali_eglext.h>
#include "gles_config_extension.h"

#if MALI_USE_GLES_1 && MALI_USE_GLES_2
	#include <GLES_GLES2/mali_gl_gl2.h>
/* Include gles2 extension before gles1 extension since we want the most recent version */
/* Inverting the order will break the compilation to extensions regarding GLSL like external image */
	#include <GLES2/mali_gl2ext.h>
	#include <GLES/mali_glext.h>
#else
	#if MALI_USE_GLES_1
		#include <GLES/mali_gl.h>
		#include <GLES/mali_glext.h>

		/* normalize blend function enum names */
		#define GL_FUNC_ADD              GL_FUNC_ADD_OES
		#define GL_FUNC_SUBTRACT         GL_FUNC_SUBTRACT_OES
		#define GL_FUNC_REVERSE_SUBTRACT GL_FUNC_REVERSE_SUBTRACT_OES
		#define GL_MIRRORED_REPEAT       GL_MIRRORED_REPEAT_OES
		#define GL_INCR_WRAP             GL_INCR_WRAP_OES
		#define GL_DECR_WRAP             GL_DECR_WRAP_OES

		#ifndef GL_APICALL
			/* If only OpenGL ES 1.x is built then GL_APICALL won't have been
			   defined so we'll define to GL_API which is defined by glplatform.h*/
			#define GL_APICALL GL_API
		#else
			#error "GL_APICALL must be defined to GL_API in order to give correct linkage of the functions"
                   "in gles_entrypoints.h and gles1_entrypoints.h when building OpenGL ES alone"
		#endif

	#elif MALI_USE_GLES_2
		#include <GLES2/mali_gl2.h>
		#include <GLES2/mali_gl2ext.h>
	#endif
#endif

#ifdef __SYMBIAN32__
#define MALI_SOFTFP __SOFTFP
#else
#define MALI_SOFTFP
#endif

#endif /* _GLES_BASE_H_ */
