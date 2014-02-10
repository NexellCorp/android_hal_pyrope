/*
 * Copyright:
 * ----------------------------------------------------------------------------
 * This confidential and proprietary software may be used only as authorized
 * by a licensing agreement from ARM Limited.
 *      (C) COPYRIGHT 2009-2010, 2012 ARM Limited, ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized copies and
 * copies may only be made to the extent permitted by a licensing agreement
 * from ARM Limited.
 * ----------------------------------------------------------------------------
 */

/* ============================================================================
	Includes
============================================================================ */
#define MALI_TPI_EGL_API MALI_TPI_EXPORT
#include "mali_tpi.h"
#include "mali_tpi_egl.h"
#include "EGL/egl.h"

#include <assert.h>
#include <tchar.h>

/* ============================================================================
	Data Structures
============================================================================ */
static ATOM window_class_close = 0;
static ATOM window_class_noclose = 0;

/* ============================================================================
	Private Functions
============================================================================ */
static LRESULT CALLBACK windowproc(
	HWND hwnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam )
{
	switch( uMsg )
	{
	case WM_DESTROY:
		/* DefWindowProc does not handle WM_DESTROY */
		PostQuitMessage( 0 );
		return 0L;
	}

	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

/* ------------------------------------------------------------------------- */
static HINSTANCE get_instance( void )
{
	/* See docs for GetCurrentProcess() for casting to HANDLE of
	 * GetCurrentProcessId() */
	return (HANDLE)GetCurrentProcessId();
}

/* ------------------------------------------------------------------------- */
/* Register a window class for use with mali_tpi_egl_create_window.
 * We use two classes internally, one with CS_NOCLOSE and one without,
 * so this function takes a parameter to distinguish.
 *
 * Returns an ATOM for the window class.
 */
static ATOM register_class( const TCHAR *name, DWORD style )
{
	WNDCLASS cls;

	memset( &cls, 0, sizeof(cls) );

	cls.style = style;
	cls.lpfnWndProc = windowproc;
	cls.hInstance = get_instance();
	cls.lpszClassName = name;
	return RegisterClass( &cls );
}

/* ============================================================================
	Public Functions
============================================================================ */
MALI_TPI_IMPL EGLDisplay mali_tpi_egl_get_display(void)
{
	return eglGetDisplay( EGL_DEFAULT_DISPLAY );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_init(void)
{
	if( !_mali_tpi_egl_init_internal() )
	{
		return MALI_TPI_FALSE;
	}

	if( 0 == window_class_close )
	{
		window_class_close =
		    register_class( _T("TPI_C"), CS_VREDRAW|CS_HREDRAW );
		if( 0 == window_class_close )
		{
			return MALI_TPI_FALSE;
		}
	}

	if( 0 == window_class_noclose )
	{
		window_class_noclose =
		    register_class( _T("TPI_NC"), CS_NOCLOSE|CS_VREDRAW|CS_HREDRAW );
		if( 0 == window_class_noclose )
		{
			UnregisterClass( MAKEINTATOM(window_class_close), get_instance() );
			return MALI_TPI_FALSE;
		}
	}
	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_shutdown( void )
{
	if( window_class_close != 0 )
	{
		if( !UnregisterClass(MAKEINTATOM(window_class_close), get_instance()) )
		{
			return MALI_TPI_FALSE;
		}
		window_class_close = 0;
	}

	if( window_class_noclose != 0 )
	{
		if(!UnregisterClass(MAKEINTATOM(window_class_noclose), get_instance()))
		{
			return MALI_TPI_FALSE;
		}
		window_class_noclose = 0;
	}

	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_window(
	EGLDisplay display,
	int width,
	int height,
	const mali_tpi_egl_window_hints *hints,
	mali_tpi_egl_window_placement **placement,
	EGLNativeWindowType *window )
{
	RECT rect;
	DWORD style = WS_THICKFRAME | WS_OVERLAPPED | WS_VISIBLE |
	              WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	DWORD exstyle = 0;
	TCHAR *title;
	mali_tpi_bool close;
	mali_tpi_bool result;
	int x = 0, y = 0;
	int display_width, display_height;
	mali_tpi_egl_window_placement *place = NULL;
	HWND hwnd;

	/* First check the requested size against the display size. If it's an
	 * exact match, create a full-screen window since that will have better
	 * performance, and will ensure that all pixels end up on-screen. */
	display_width  = GetSystemMetrics(SM_CXSCREEN);
	display_height = GetSystemMetrics(SM_CYSCREEN);
	if( (width == display_width) && (height == display_height) )
	{
		return mali_tpi_egl_create_fullscreen_window(
			display, hints, placement, window );
	}

	result = _mali_tpi_egl_window_hints_get_decoration(
	    hints, MALI_TPI_EGL_WINDOW_DECORATION_BORDER );
	if( result )
	{
		style |= WS_BORDER;
	}

	result = _mali_tpi_egl_window_hints_get_decoration(
	    hints, MALI_TPI_EGL_WINDOW_DECORATION_TITLE );
	if( result )
	{
		style |= WS_CAPTION;
	}
	else
	{
		/* If we don't specify POPUP, Windows gives it a caption anyway */
		style |= WS_POPUP;
	}

	result = _mali_tpi_egl_window_hints_get_decoration(
	    hints, MALI_TPI_EGL_WINDOW_DECORATION_MINIMIZE );
	if( result )
	{
		/* WS_MINIMIZEBOX requires WS_SYSMENU, which requires WS_CAPTION */
		style |= WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION;
	}

	result = _mali_tpi_egl_window_hints_get_decoration(
	    hints, MALI_TPI_EGL_WINDOW_DECORATION_MAXIMIZE );
	if( rseult )
	{
		/* WS_MAXIMIZEBOX requires WS_SYSMENU, which requires WS_CAPTION */
		style |= WS_MAXIMIZEBOX | WS_SYSMENU | WS_CAPTION;
	}

	close = _mali_tpi_egl_window_hints_get_decoration(
	    hints, MALI_TPI_EGL_WINDOW_DECORATION_CLOSE );
	{
#ifdef _UNICODE
		int size;
		const char *title_mb = _mali_tpi_egl_window_hints_get_title( hints );

		size = MultiByteToWideChar( CP_UTF8, 0, title_mb, -1, NULL, 0 );
		if( 0 == size )
		{
			/* 0 indicates a failure - the return value otherwise includes
			 * space for a NULL */
			return MALI_TPI_FALSE;
		}

		title = mali_tpi_alloc( size * sizeof(TCHAR) );
		MultiByteToWideChar( CP_UTF8, 0, title_mb, -1, title, size );
#else
		title = _mali_tpi_egl_window_hints_get_title( hints );
#endif
	}

	/* Create struct for window placement ahead of time, since it may fail */
	if( NULL != placement )
	{
		place = _mali_tpi_egl_create_window_placement();
		if( NULL == place )
		{
#ifdef _UNICODE
			mali_tpi_free( title );
#endif
			return MALI_TPI_FALSE;
		}
	}

	/* Calculate window size required for desired client area */
	SetRect( &rect, 0, 0, width, height );
	if( !AdjustWindowRectEx( &rect, style, FALSE, exstyle ) )
	{
#ifdef _UNICODE
		mali_tpi_free( title );
#endif
		return MALI_TPI_FALSE;
	}

	/* Get coordinates for edge of window area */
	if ( !_mali_tpi_egl_window_hints_get_position( hints, &x, &y ) )
	{
		x = CW_USEDEFAULT;
		y = CW_USEDEFAULT;
	}

	hwnd = CreateWindowEx(
		exstyle,
		MAKEINTATOM( close ? window_class_close : window_class_noclose ),
		title,
		style,
		x,
		y,
		rect.right - rect.left,
		rect.bottom - rect.top,
		NULL,                         /* parent */
		NULL,                         /* menu */
		get_instance(),
		NULL );                       /* lParam for WM_CREATE */

#ifdef _UNICODE
	mali_tpi_free( title );
#endif

	if( NULL == hwnd )
	{
		return MALI_TPI_FALSE;
	}

	/* Update the placement */
	if( NULL != placement )
	{
		if( GetWindowRect(hwnd, &rect) )
		{
			_mali_tpi_egl_window_placement_area(
			    place, rect.left, rect.top,
			    rect.right - rect.left, rect.bottom - rect.top );
		}
		*placement = place;
	}

	*window = hwnd;
	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_fullscreen_window(
	EGLDisplay display,
	const mali_tpi_egl_window_hints *hints,
	mali_tpi_egl_window_placement **placement,
	EGLNativeWindowType *window )
{
	mali_tpi_egl_window_placement *place;

	/* Allocate the pointer in temporary memory, in case it fails */
	if( NULL != placement )
	{
		place = _mali_tpi_egl_create_window_placement();
		if( NULL == place )
		{
			return MALI_TPI_FALSE;
		}

		/* We don't know big the underlying driver will make the window,
		 * so we can't pass any information in the placement.
		 */
		*placement = place;
	}
	*window = NULL;
	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_window(
	EGLNativeWindowType window )
{
	if( NULL == window )
	{
		/* NULL window used for full-screen rendering - nothing to destroy */
		return MALI_TPI_TRUE;
	}

	assert(IsWindow( window ));
	return DestroyWindow( window ) ? MALI_TPI_TRUE : MALI_TPI_FALSE;
}
