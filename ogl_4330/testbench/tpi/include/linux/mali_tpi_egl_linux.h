/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2010-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

#ifndef _MALI_TPI_EGL_LINUX_H_
#define _MALI_TPI_EGL_LINUX_H_

#if (1 == EGL_FBDEV) || defined(TPI_BACKEND_FBDEV)
	#include "fbdev/mali_tpi_egl_fbdev.h"
#elif (1 == EGL_DUMMY) || defined(TPI_BACKEND_DUMMY)
	#include "dummy/mali_tpi_egl_dummy.h"
#elif (1 == EGL_X11) || defined(TPI_BACKEND_X11)
	#include "x11/mali_tpi_egl_x11.h"
#else
	#error "There is no TPI EGL support for your operating system"
#endif

mali_tpi_bool _mali_tpi_egl_get_display_dimensions(
		EGLNativeDisplayType native_display,
		int *width,
		int *height);

#endif /* End (_MALI_TPI_EGL_LINUX_H_) */
