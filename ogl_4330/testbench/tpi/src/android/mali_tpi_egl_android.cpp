/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2011-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

#if MALI_ANDROID_API >= 15
#include <ui/ANativeObjectBase.h>
#else
 #include <ui/android_native_buffer.h>
#endif

#include <ui/FramebufferNativeWindow.h>

#define MALI_TPI_EGL_API MALI_TPI_EXPORT
extern "C" {
#include <mali_tpi.h>
}
#include <mali_tpi_egl.h>

#include <tpi/include/common/mali_tpi_egl_fb_common.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "test_framework/mali_tpi_android_framework.h"

#define FB_PATH "/dev/graphics/fb0"

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_init()
{
	/* This variable will be read from the EGL winsys, therefore use the real "setenv" instead of 
	 * "cdbg_env_set"
	 */
	setenv("EGLP_WAIT_FOR_COMPOSITION", "1", 1);

	return _mali_tpi_egl_init_internal();
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_shutdown()
{
	return _mali_tpi_egl_shutdown_internal();
}

MALI_TPI_IMPL EGLNativeDisplayType mali_tpi_egl_get_default_native_display( void )
{
	/* Android only supports the default display */
	return EGL_DEFAULT_DISPLAY;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_get_invalid_native_display(EGLNativeDisplayType *native_display)
{
	*native_display= (EGLNativeDisplayType)-1;

	return MALI_TPI_TRUE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_pixmap(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	EGLConfig config,
	EGLNativePixmapType *pixmap)
{
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_pixmap(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap)
{
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_fill_pixmap(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap,
	mali_tpi_egl_simple_color color,
	int to_fill)
{
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_check_pixmap_pixel(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap,
	int x,
	int y,
	mali_tpi_egl_simple_color color,
	mali_tpi_bool *result)
{
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_window(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window )
{
	EGLBoolean egl_res;
	int pixel_format;
	mali_tpi_bool retval = MALI_TPI_FALSE;
	EGLNativeWindowType new_win;

	/* The EGL_NATIVE_VISUAL_ID should be the Android enum for the pixel format of the config */
	egl_res = eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &pixel_format);
	if(EGL_FALSE == egl_res)
	{
		/* We've not taken a reference on the Window so nothing to free */
		goto out;
	}

	new_win = mali_test_new_native_window(width, height, pixel_format);
	if (NULL != new_win)
	{
		retval = MALI_TPI_TRUE;
		*window = new_win;
	}

out:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_resize_window(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType *window,
	int width,
	int height,
	EGLDisplay display,
	EGLConfig config)
{
	EGLBoolean egl_res;
	int pixel_format;
	mali_tpi_bool retval = MALI_TPI_FALSE;
	mali_tpi_bool res;
	ANativeWindow *w = reinterpret_cast<ANativeWindow*>(window);

	/* The EGL_NATIVE_VISUAL_ID should be the Android enum for the pixel format of the config */
	egl_res = eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &pixel_format);
	if(EGL_FALSE == egl_res)
	{
		/* We've not taken a reference on the Window so nothing to free */
		goto out;
	}

	res = mali_test_resize_native_window(w, width, height, pixel_format);
	if (MALI_TPI_FALSE != res)
	{
		retval = MALI_TPI_TRUE;
	}

out:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_fullscreen_window(
	EGLNativeDisplayType native_display,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window )
{
	return mali_tpi_egl_create_window(native_display, 0, 0, display, config, window);
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_window(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType window )
{
	/* Bodge until MIDEGL-944, to be removed as part of that bug */ 
	EGLDisplay display = eglGetDisplay(native_display);
	
	if (EGL_NO_DISPLAY == display)
	{
		return MALI_TPI_FALSE;
	}

	if (EGL_FALSE == eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
	{
		return MALI_TPI_FALSE;
	}

	/* we need to handle window == NULL like in the Linux implementation, for consistency */
	if (NULL == window)
	{
		return MALI_TPI_TRUE;
	}
	ANativeWindow *w = reinterpret_cast<ANativeWindow*>(window);
	return mali_test_delete_native_window(w);
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_get_window_dimensions(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType window,
	int *width,
	int *height)
{
	ANativeWindow *w = reinterpret_cast<ANativeWindow*>(window);

	w->query(w, NATIVE_WINDOW_WIDTH, width);
	w->query(w, NATIVE_WINDOW_HEIGHT, height);

	return MALI_TPI_TRUE;
}


static mali_tpi_bool _mali_tpi_egl_check_color_component(unsigned char *pixel_address, int bytes_per_pixel,
		struct fb_bitfield *bf, int expected)
{
	mali_tpi_bool retval;
	mali_tpi_uint32 pixel;
	mali_tpi_uint32 component;
	mali_tpi_uint32 component_mask = 0;

	/* Make sure we don't get an unaligned access */
	memcpy(&pixel, pixel_address, sizeof(pixel));

	/* Shift to the least significant end if necessary */
	/* TODO: MIDEGL-724 */
	/*pixel >>= sizeof(pixel) - bytes_per_pixel;*/

	component_mask = (1 << bf->length) - 1;

	component = pixel >> bf->offset;
	component &= component_mask;

	if (0 == expected)
	{
		if (0 == component)
		{
			retval = MALI_TPI_TRUE;
		}
		else
		{
			retval = MALI_TPI_FALSE;
		}
	}
	else
	{
		if (component_mask == component)
		{
			retval = MALI_TPI_TRUE;
		}
		else
		{
			retval = MALI_TPI_FALSE;
		}
	}

	return retval;
}

/** The current state of checking window pixels:
 *  We will be reading directly from the front buffer by changing the permissions of fbdev 
 *  to give global access (for testing purposes only). The windows we are creating in the application
 *  take up the entire screen and therefore we *should* be able to correctly read back test output.
 *  Once we are testing on an implementation that supports GLES1.1 with FBOs we *should* be able to use
 *  a feature of the surface composition to take screenshots meaning we won't need direct access to fbdev
 */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_check_window_pixel(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType window,
		int x, int y,
		mali_tpi_egl_simple_color color,
		mali_tpi_bool *result)
{
	mali_tpi_bool retval = MALI_TPI_TRUE;

	retval = check_fbdev_pixel(
			FB_PATH,
			0, 0, /* window position */
			0, 0, /* window dimensions (zero is fullscreen) */
			x, y,
			color,
			result);

finish:
	return retval;
}

MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_create_external_pixmap(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	mali_tpi_egl_pixmap_format format,
	EGLNativePixmapType *pixmap)
{
	/* TODO: Implement */
	return MALI_FALSE;
}

MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_load_pixmap(
	EGLNativePixmapType pixmap,
	unsigned int num_planes,
	void const * const data[])
{
	/* TODO: Implement */
	return MALI_FALSE;
}

MALI_TPI_EGL_API EGLint mali_tpi_egl_get_pixmap_type(void)
{
	/* TODO: Implement */
	return 0;
}



