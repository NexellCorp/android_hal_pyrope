/*
 * Copyright:
 * ----------------------------------------------------------------------------
 * This confidential and proprietary software may be used only as authorized
 * by a licensing agreement from ARM Limited.
 *      (C) COPYRIGHT 2010-2012 ARM Limited, ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized copies and
 * copies may only be made to the extent permitted by a licensing agreement
 * from ARM Limited.
 * ----------------------------------------------------------------------------
 */

#define MALI_TPI_EGL_API MALI_TPI_EXPORT
#include <mali_tpi.h>
#include <mali_tpi_egl.h>

#include <assert.h>
#include <string.h>

typedef struct dummy_display
{
	int width;
	int height;
	int bytes_per_pixel;
	int red_mask;
	int green_mask;
	int blue_mask;
	int alpha_mask;
	unsigned char *front_buffer;
} dummy_display;

dummy_display default_dummy_display;


MALI_TPI_IMPL mali_tpi_bool _mali_tpi_egl_get_display_dimensions(EGLNativeDisplayType native_display, int *width, int *height)
{
	dummy_display *dpy = &default_dummy_display;

	assert(EGL_DEFAULT_DISPLAY == native_display);

	*width = dpy->width;
	*height = dpy->height;

	return MALI_TPI_TRUE;
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
	mali_tpi_bool retval = MALI_TPI_TRUE;
	mali_tpi_bool tpi_res;
	dummy_display *dpy = &default_dummy_display;
	fbdev_window *win = window;
	int window_width, window_height;

	assert(EGL_DEFAULT_DISPLAY == native_display);

	if (NULL == win)
	{
		tpi_res = _mali_tpi_egl_get_display_dimensions(native_display, &window_width, &window_height);
		if (MALI_TPI_FALSE == tpi_res)
		{
			retval = MALI_TPI_FALSE;
			goto finish;
		}
	}
	else
	{
		window_width = win->width;
		window_height = win->height;
	}

	retval = check_pixel_within_window(x, y, window_width, window_height);
	if (retval == MALI_TPI_FALSE)
	{
		goto finish;
	}

	*result = check_pixel(
			dpy->front_buffer,
			dpy->bytes_per_pixel,
			0, 0, /* window position */
			x, y,
			dpy->width,
			dpy->height,
			dpy->red_mask,
			dpy->green_mask,
			dpy->blue_mask,
			dpy->alpha_mask,
			color);

finish:
	return retval;
}
