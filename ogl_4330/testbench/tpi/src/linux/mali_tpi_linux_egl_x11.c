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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <EGL/eglext.h>

#include "xcb/dri2.h"
#include <tpi/include/common/mali_tpi_egl_fb_common.h>

#include <string.h>
#include <assert.h>
#include <stdlib.h>

static Display* x11_default_display = NULL;

#define FB_PATH "/dev/fb0"

typedef struct colormap_cache_entry
{
	VisualID visual_id;
	Colormap color_map;
	struct colormap_cache_entry* next;
} colormap_cache_entry;

/* We need to maintain a cache of color maps so that we can free all the color maps during tpi_egl_term() */
static colormap_cache_entry *colormap_cache = NULL;

static Colormap get_colormap(Display* dpy, Window root_window, VisualID visual_id)
{
	Colormap result;
	mali_tpi_bool map_found = MALI_TPI_FALSE;
	colormap_cache_entry *entry = colormap_cache;

	while (entry != NULL)
	{
		if (entry->visual_id == visual_id)
		{
			result = entry->color_map;
			map_found = MALI_TPI_TRUE;
			break;
		}
		entry = entry->next;
	}

	/* If the colormap's not already in the cache, create it. Note: These stay in the cache until tpi shutdown */
	if (map_found == MALI_TPI_FALSE)
	{
		XVisualInfo visual_info_template;
		XVisualInfo *visual_info = NULL;
		int matching_count;

		visual_info_template.visualid = visual_id;
		visual_info = XGetVisualInfo(dpy, VisualIDMask, &visual_info_template, &matching_count);
		assert(matching_count > 0);
		assert(visual_info);

		result = XCreateColormap(dpy, root_window, visual_info->visual, AllocNone);

		XFree(visual_info);
		visual_info = NULL;

		/* Create cache entry */
		entry = (colormap_cache_entry*) mali_tpi_alloc(sizeof(colormap_cache_entry));
		assert(entry);

		entry->color_map = result;
		entry->visual_id = visual_id;

		/* Add the new colormap at the front of the cache */
		entry->next = colormap_cache;
		colormap_cache = entry;
	}

	return result;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_init()
{
	mali_tpi_bool retval = MALI_TPI_TRUE;

	if (x11_default_display == NULL)
	{
		x11_default_display = XOpenDisplay( NULL );
		if( NULL == x11_default_display )
		{
			retval = MALI_TPI_FALSE;
		}
	}

	if( MALI_TPI_TRUE == retval )
	{
		retval = _mali_tpi_egl_init_internal();
		if( MALI_TPI_FALSE == retval )
		{
			XCloseDisplay( x11_default_display );
			x11_default_display = NULL;
		}
	}
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_shutdown()
{
	colormap_cache_entry *entry = colormap_cache;

	if (x11_default_display != NULL)
	{
		/* Clear the colormap cache */
		while (entry != NULL)
		{
			colormap_cache_entry *next_entry = entry->next;
			XFreeColormap(x11_default_display, entry->color_map);
			mali_tpi_free(entry);
			entry = next_entry;
		}
		colormap_cache = NULL;

		XCloseDisplay(x11_default_display);
		x11_default_display = NULL;
	}

	return _mali_tpi_egl_shutdown_internal();
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_get_invalid_native_display(EGLNativeDisplayType *display)
{
	*display = (EGLNativeDisplayType)1;

	return MALI_TPI_FALSE;
}

#if defined(EGL_KHR_image) && EGL_KHR_image
MALI_TPI_IMPL EGLint mali_tpi_egl_get_pixmap_type(void)
{
	return EGL_NATIVE_PIXMAP_KHR;
}
#endif

MALI_TPI_IMPL EGLNativeDisplayType mali_tpi_egl_get_default_native_display( void )
{
	assert(x11_default_display != NULL);

	return (EGLNativeDisplayType)x11_default_display;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_window(
	EGLNativeDisplayType native_display,
	int width, int height,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window )
{
	EGLint visual_id;
	EGLBoolean res;
	Window root_window;
	XVisualInfo *visual_info = NULL;
	XVisualInfo visual_info_template;
	XSetWindowAttributes window_attributes;
	unsigned long window_attribute_mask = CWBackPixel | CWBorderPixel;
	int matching_count;

	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );

	root_window = XRootWindow(native_display, 0 /* screen number */);

	/* Get the visual info for the given EGL config */
	res = eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visual_id);
	assert( res == EGL_TRUE );

	visual_info_template.visualid = visual_id;
	visual_info = XGetVisualInfo((Display*)native_display, VisualIDMask, &visual_info_template, &matching_count);
	assert(matching_count > 0);
	assert(visual_info);

	/* We need a colormap if there is alpha channel or the visual is not the same as root window */
	window_attributes.colormap = get_colormap(native_display, root_window, visual_id);
	window_attribute_mask |= CWColormap;

	window_attributes.background_pixel = XWhitePixel((Display*)native_display, 0 /* screen number */);
	window_attributes.border_pixel = XWhitePixel((Display*)native_display, 0 /* screen number */);


	*window = XCreateWindow((Display*)native_display, /* display */
							root_window, /* parent */
							0, /* x-coordinate */
							0, /* y-coordinate */
							width,
							height,
							0, /* border width */
							visual_info->depth, /* depth */
							InputOutput, /* class */
							visual_info->visual, /* visual */
							window_attribute_mask,
							&window_attributes);

	XFree(visual_info);
	visual_info = NULL;

	XStoreName((Display*)native_display, *window, "Mali TPI Test Window");
	XMapWindow((Display*)native_display, (Window)*window);

	return MALI_TPI_TRUE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_resize_window(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType *window,
		int width, int height,
		EGLDisplay egl_display,
		EGLConfig config)
{
	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );

	XResizeWindow( (Display*) native_display, *window, width, height );
	return MALI_TPI_TRUE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_fullscreen_window(
	EGLNativeDisplayType native_display,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window )
{
	mali_tpi_bool success;
	Atom wm_state;
	Atom fullscreen;
	int display_width;
	int display_height;
	
	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );

	display_width = DisplayWidth(native_display, 0 /* screen */);
	display_height = DisplayHeight(native_display, 0 /* screen */);

	success = mali_tpi_egl_create_window(native_display, display_width, display_height, display, config, window);
	/* If we created the window, set the fullscreen property so any window manager running knows the 
	 * window is
	   supposed to be full-screen: */
	if (success)
	{
		/* Get the atom IDs for the property and the value we want to set that property to */
		wm_state = XInternAtom(native_display, "_NET_WM_STATE", False);
		fullscreen = XInternAtom (native_display, "_NET_WM_STATE_FULLSCREEN", False);

		XChangeProperty(native_display /*display*/,
		                *window /*window*/,
		                wm_state /*property*/,
		                XA_ATOM /*property datatype*/,
		                8 * sizeof(Atom) /* format of the data (8, 16 or 32-bit): this is *not* the depth of the window. */,
		                PropModeReplace /*mode*/,
		                (unsigned char*) &fullscreen /*data*/,
		                1 /*value count*/);
	}

	return success;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_window(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType window)
{
	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );
	XDestroyWindow(native_display, window);

	return MALI_TPI_TRUE;
}

static mali_tpi_bool get_window_dimensions(
		Display *native_display,
		Window window,
		int *width, int *height,
		int *x, int *y)
{
	XWindowAttributes window_attribs;
	Status success;
	mali_tpi_bool result = MALI_TPI_FALSE;

	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );
	success = XGetWindowAttributes(native_display, window, &window_attribs);

	if (success > 0)
	{
		result = MALI_TPI_TRUE;
		*width = window_attribs.width;
		*height = window_attribs.height;

		if (x != NULL)
		{
			*x = window_attribs.x;
		}
		if (y != NULL)
		{
			*y = window_attribs.y;
		}
	}

	return result;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_get_window_dimensions(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType window,
		int *width, int *height)
{
	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );
	return get_window_dimensions(native_display, window, width, height, NULL, NULL);
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_pixmap(
		EGLNativeDisplayType native_display,
		int width, int height,
		EGLDisplay display,
		EGLConfig config,
		EGLNativePixmapType *pixmap)
{
	Window root_window;
	EGLint visual_id;
	XVisualInfo visual_info_template;
	XVisualInfo *visual_info = NULL;
	int matching_count;

	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );
	root_window = XRootWindow(native_display, 0 /* screen */);

	/* Get the visual info for the given EGL config so we can find out the config's visual's color depth */
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visual_id);
	visual_info_template.visualid = visual_id;
	visual_info = XGetVisualInfo((Display*)native_display, VisualIDMask, &visual_info_template, &matching_count);
	assert(matching_count > 0);
	assert(visual_info);

	*pixmap = XCreatePixmap(native_display, root_window, width, height, visual_info->depth);

	return MALI_TPI_TRUE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_pixmap(
		EGLNativeDisplayType native_display,
		EGLNativePixmapType pixmap)
{
	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );
	XFreePixmap(native_display, pixmap);
	return MALI_TPI_TRUE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_fill_pixmap(
		EGLNativeDisplayType native_display,
		EGLNativePixmapType pixmap,
		mali_tpi_egl_simple_color color,
		int to_fill)
{
	XGCValues gc_values;
	Colormap colormap;
	XColor xcolor;
	char* color_string = NULL;
	GC gc;
	Status success;
	mali_tpi_bool retval;
	int width, height;

	CSTD_UNUSED( to_fill );

	switch (color)
	{
	case MALI_TPI_EGL_SIMPLE_COLOR_RED:
		color_string = "#FF0000";
		break;
	case MALI_TPI_EGL_SIMPLE_COLOR_GREEN:
		color_string = "#00FF00";
		break;
	case MALI_TPI_EGL_SIMPLE_COLOR_BLUE:
		color_string = "#0000FF";
		break;
	case MALI_TPI_EGL_SIMPLE_COLOR_BLACK:
		color_string = "#000000";
		break;
	default: /* Fall-through to white */
	case MALI_TPI_EGL_SIMPLE_COLOR_WHITE:
			color_string = "#FFFFFF";
			break;
	}

	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );
	/* Assume the pixmap is going to be drawn onto a window using a default colormap: */
	colormap = DefaultColormap(native_display, 0 /*screen*/);

	success = XParseColor(native_display, colormap, color_string, &xcolor);

	gc_values.fill_style = FillSolid;
	gc_values.foreground = xcolor.pixel;

	gc = XCreateGC(native_display, pixmap, GCFillStyle|GCForeground, &gc_values);

	retval = get_window_dimensions(native_display, pixmap, &width, &height, NULL, NULL);
	if(MALI_TPI_FALSE != retval )
	{
		if (MALI_TPI_FALSE != (MALI_TPI_EGL_TOP_LEFT & to_fill))
		{
			XFillRectangle(native_display, pixmap, gc, 0, 0, width/2, height/2);
		}
		if (MALI_TPI_FALSE != (MALI_TPI_EGL_TOP_RIGHT & to_fill))
		{
			XFillRectangle(native_display, pixmap, gc, width/2, 0, width, height/2);
		}
		if (MALI_TPI_FALSE != (MALI_TPI_EGL_BOTTOM_LEFT & to_fill))
		{
			XFillRectangle(native_display, pixmap, gc, 0, height/2, width/2, height);
		}
		if (MALI_TPI_FALSE != (MALI_TPI_EGL_BOTTOM_RIGHT & to_fill))
		{
			XFillRectangle(native_display, pixmap, gc, width/2, height/2, width, height);
		}
	}

	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_check_pixmap_pixel(
		EGLNativeDisplayType native_display,
		EGLNativePixmapType pixmap,
		int x, int y,
		mali_tpi_egl_simple_color color,
		mali_tpi_bool *result)
{
	/*MIDEGL-930: Implement this function */
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_check_window_pixel(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType window,
		int pixel_x_pos, int pixel_y_pos,
		mali_tpi_egl_simple_color color,
		mali_tpi_bool *result)
{
	mali_tpi_bool retval;
	int width, height, win_x_pos, win_y_pos;

	/* EGL_DEFAULT_DISPLAY must not be used , use mali_tpi_egl_get_default_native_display() instead */
	assert( EGL_DEFAULT_DISPLAY != native_display );

	retval = get_window_dimensions(native_display, window, &width, &height, &win_x_pos, &win_y_pos);
	if (retval == MALI_TPI_FALSE)
	{
		goto finish;
	}

	retval = check_fbdev_pixel(
			FB_PATH,
			win_x_pos, win_y_pos,
			width, height,
			pixel_x_pos, pixel_y_pos,
			color,
			result);

finish:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_external_pixmap(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	mali_tpi_egl_pixmap_format format,
	EGLNativePixmapType *pixmap)
{
	/* MIDEGL-931: Implement this function */
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_load_pixmap(
	EGLNativePixmapType pixmap,
	unsigned int num_planes,
	void const * const data[])
{
	/* MIDEGL-932: Implement this function */
	return MALI_TPI_FALSE;
}

