/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_image_android.h
 * @brief EGLImage extension functionality for Android native buffer
 */

#ifndef _EGL_IMAGE_ANDROID_H_
#define _EGL_IMAGE_ANDROID_H_

/**
 * @addtogroup eglcore_image EGL core EGLImageKHR API
 *
 * @{
 */

/**
 * @defgroup eglcore_image_android EGLImageKHR Android native buffer API
 *
 * @note EGLImageKHR Android native buffer API
 *
 * @{
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <shared/mali_image.h>
#include <shared/m200_td.h>
#include "egl_image.h"

#ifdef NDEBUG
/* org */
#define ANDROID_ENABLE_DEBUG 0
#define ANDROID_DEBUG_LEVEL 0
#define EGL_ANDROID_LOG_TIME 0
#else
#define ANDROID_ENABLE_DEBUG 1
#define ANDROID_DEBUG_LEVEL 1
#define EGL_ANDROID_LOG_TIME 1
#endif


#if ANDROID_ENABLE_DEBUG
#if 1 /* org */
#define ADBG(level, fmt, args...) if ( level <= ANDROID_DEBUG_LEVEL ) __android_log_print(ANDROID_LOG_DEBUG, "[EGL]", "%s:%d: " fmt,__func__,__LINE__,args)
#else /* nexell add */
#include <utils/Log.h>
#include <stdio.h>
#include <fcntl.h>		/* for O_RDWR */
#define __KERNEL__
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <pthread.h>

#define  ERRSTR  strerror(errno)
extern int __g_is_pid_system_surfaceflinger_exist;
extern int __g_system_surfaceflinger_pid;
MALI_STATIC_INLINE int find_task_by_id(int pid, char *name)
{
    int  fd, ret;
    char task[64];

   sprintf(task, "/proc/%d/cmdline", pid);

    fd = open(task, O_RDONLY);
    if (0 > fd) {
          ALOGE("[EGL] fail, open %s, %s ...\n", task, ERRSTR);
          return -1;
    }

    ret = read(fd, name, 1024);
           if (0 > ret){
          ALOGE("[EGL] fail, read %s, %s ...\n", task, ERRSTR);
          return -1;
    }
    return 0;
}

MALI_STATIC_INLINE int android_dbg_check(void)
{
    unsigned int pid = getpid();
    char name[1024];	
	if(__g_is_pid_system_surfaceflinger_exist == -1)
	{
		if(pid >= 90 && pid < 100)
		{
			/* surfaceflinger pid */
			find_task_by_id(pid, name);
			if( ! strcmp( "/system/bin/surfaceflinger", name ) )
			{
				__g_system_surfaceflinger_pid = pid;	
				__g_is_pid_system_surfaceflinger_exist = 1;
				return 1;
			}
			else 
				return 0;
		}
		else 
			return 0;
	}	
	else /* surfaceflinger exist */
	{
		if(pid == __g_system_surfaceflinger_pid)
			return 1;
		else
			return 0;
	}		
}
#define ADBG(level, fmt, args...) if(android_dbg_check() && level <= ANDROID_DEBUG_LEVEL ) __android_log_print(ANDROID_LOG_DEBUG, "[EGL]", "%s:%d: " fmt,__func__,__LINE__,args)
#endif
#else
#define ADBG(level, fmt, args...)
#endif

#define AERR(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, "[EGL-ERROR]", "%s:%d: " fmt,__func__,__LINE__,args)
#define AINF(fmt, args...) __android_log_print(ANDROID_LOG_INFO, "", "%s:%d: " fmt, MALI_FUNCTION, __LINE__,##args)


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates an EGLImage from a native Android buffer
 * @param display display to use
 * @param context context to use
 * @param buffer the native buffer resource
 * @param attr_list attribute list
 * @param thread_state current thread state
 * @return pointer to egl_image, NULL on failure
 */
egl_image* _egl_create_image_ANDROID_native_buffer( egl_display *display,
                                                    egl_context *context,
                                                    EGLClientBuffer buffer,
                                                    EGLint *attr_list,
                                                    void *thread_state );

void _egl_image_unmap_ANDROID_native_buffer( EGLClientBuffer buffer );

#ifdef __cplusplus
}
#endif

/** @} */ /* end group eglcore_image */

/** @} */ /* end group eglcore_image_android */

#endif /* _EGL_IMAGE_ANDROID_H_ */
