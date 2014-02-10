/*
 * Copyright:
 * ----------------------------------------------------------------------------
 * This confidential and proprietary software may be used only as authorized
 * by a licensing agreement from ARM Limited.
 *      (C) COPYRIGHT 2009-2012 ARM Limited, ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized copies and
 * copies may only be made to the extent permitted by a licensing agreement
 * from ARM Limited.
 * ----------------------------------------------------------------------------
 */

#define MALI_TPI_EGL_API MALI_TPI_EXPORT
#include <mali_tpi.h>
#include <mali_tpi_egl.h>
#include <tpi/include/common/mali_tpi_egl_fb_common.h>

#include <assert.h>

#define FB_PATH "/dev/fb0"

MALI_TPI_IMPL mali_tpi_bool _mali_tpi_egl_get_display_dimensions(EGLNativeDisplayType native_display, int *width, int *height)
{
	return get_fbdev_dimensions(FB_PATH, width, height);
}

MALI_TPI_IMPL EGLNativeDisplayType mali_tpi_egl_get_default_native_display()
{
	return EGL_DEFAULT_DISPLAY;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_check_window_pixel(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType window,
		int x, int y,
		mali_tpi_egl_simple_color color,
		mali_tpi_bool *result)
{
	mali_tpi_bool retval;
	int window_width, window_height;
	fbdev_window *win = window;

	if (win == NULL)
	{
		window_width = 0;
		window_height = 0;
	}
	else
	{
		window_width = win->width;
		window_height = win->height;
	}

	retval = check_fbdev_pixel(
			FB_PATH,
			0, 0,
			window_width, window_height,
			x, y,
			color,
			result);

	return retval;
}
