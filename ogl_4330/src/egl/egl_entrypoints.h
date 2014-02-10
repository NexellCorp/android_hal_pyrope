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
 * @file egl_entrypoints.h
 * @brief Contains helper macros for the egl_entrypoints.c files
 */

#ifndef _EGL_ENTRYPOINTS_UTIL_H_
#define _EGL_ENTRYPOINTS_UTIL_H_

#include "egl_shims_common.h"
#include "base/mali_profiling.h"

#ifndef MALI_EGL_APIENTRY
#define MALI_EGL_APIENTRY(a) EGLAPIENTRY a
#endif

#ifdef MALI_MONOLITHIC
	#ifndef MALI_EGLAPI
	#define MALI_EGLAPI MALI_VISIBLE EGLAPI
	#endif
#endif

#ifndef MALI_EGLAPI
#define MALI_EGLAPI EGLAPI
#endif

#ifndef MALI_EGL_IMAGE_EXPORT
	#ifdef MALI_MONOLITHIC
		#define MALI_EGL_IMAGE_EXPORT MALI_VISIBLE MALI_EXPORT
	#else
		#define MALI_EGL_IMAGE_EXPORT MALI_EXPORT
	#endif
#endif

/* NOTE: to be used when tracing
 * NB: There are now entrypoints in other files, if certain egl features are built in
 */
#ifdef NDEBUG
#if 1 /* org */ 
#define EGL_ENTER_FUNC()
#define EGL_LEAVE_FUNC()
#else
#define EGL_ENTER_FUNC()	fprintf(stderr, "[EGL] %s enter\n", __FUNCTION__)
#define EGL_LEAVE_FUNC()	fprintf(stderr, "[EGL] %s leave\n", __FUNCTION__)
#endif
#else /* nexell add */
	#if defined(HAVE_ANDROID_OS)
		#if 0
		#include <android/log.h>
		#include <stdio.h>
		#include <stdlib.h>
		#include <unistd.h>		
		#define EGL_ENTER_FUNC() ADBG(1, "[EGL] %s enter\n", __FUNCTION__); \
								fprintf(stderr, "[EGL] %s enter\n", __FUNCTION__)
								
		#define EGL_LEAVE_FUNC() ADBG(1, "[EGL] %s leave\n", __FUNCTION__); \
								fprintf(stderr, "[EGL] %s leave\n", __FUNCTION__)
		/*
		#include <utils/Log.h>
		#define EGL_ENTER_FUNC()  ADBG(1, "EGL-enter ADBG, %s\n", __FUNCTION__);
							 ALOGE("EGL-enter LOG_ERROR, %s\n", __FUNCTION__); \
							 ALOGI("EGL-enter LOG_INFO, %s\n", __FUNCTION__); \
							 ALOGD("EGL-enter LOG_DEBUG, %s\n", __FUNCTION__)
		*/
		#else
		#define EGL_ENTER_FUNC()
		#define EGL_LEAVE_FUNC()
		#endif
	#else /* nexell add */
		#if 0		
		#include <stdio.h>
		#define EGL_ENTER_FUNC() printf("[EGL]%s enter\n", __FUNCTION__)
		#define EGL_LEAVE_FUNC() printf("[EGL]%s leave\n", __FUNCTION__)
		#else
		#define EGL_ENTER_FUNC()
		#define EGL_LEAVE_FUNC()
		#endif
	#endif
#endif

#endif /* _EGL_ENTRYPOINTS_UTIL_H_ */
