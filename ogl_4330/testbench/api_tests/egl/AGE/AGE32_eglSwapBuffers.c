/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * Verifies the functionality of eglSwapBuffers
 */

#include "../egl_framework.h"
#include "../egl_test_parameters.h"
#include <suite.h>
#include <stdlib.h>

static void test_eglSwapBuffers( suite* test_suite )
{
	EGLint surface_pbuffer_attrib[] = { EGL_WIDTH, TEST_DEFAULT_EGL_WIDTH, EGL_HEIGHT, TEST_DEFAULT_EGL_HEIGHT, EGL_NONE };
	EGLDisplay dpy_valid, dpy_invalid = (EGLDisplay)-1;
	EGLContext context = EGL_NO_CONTEXT;
	EGLSurface surface_window = EGL_NO_SURFACE;
	EGLSurface surface_pbuffer = EGL_NO_SURFACE;
	EGLSurface surface_pixmap = EGL_NO_SURFACE;
	EGLSurface surface_invalid = (EGLSurface)-1;
	EGLConfig config = (EGLConfig)NULL;
	EGLint value;
	EGLBoolean status;
	EGLNativeWindowType win;
	EGLNativePixmapType pixmap = (EGLNativePixmapType) 0;
#ifdef EGL_MALI_VG
	VGfloat clear_color[4];
	EGLint context_attribs[] = { EGL_NONE };
#else
	#ifdef EGL_MALI_GLES
	EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, MALI_EGL_GLES_MAJOR_VERSION, EGL_NONE };
	#else
	EGLint context_attribs[] = { EGL_NONE };
	#endif
#endif
	EGLNativeDisplayType native_dpy;
	EGLint usage = 0;
	EGLint window_red, window_green, window_blue, window_alpha;
	display_format pixmap_display_format = DISPLAY_FORMAT_INVALID;

	egl_test_get_requested_window_pixel_format( test_suite, &window_red, &window_green, &window_blue, &window_alpha );
	if ( 8 == window_red && 8 == window_green && 8 == window_blue )
		pixmap_display_format = DISPLAY_FORMAT_ARGB8888;
	else if ( 5 == window_red && 6 == window_green && 5 == window_blue )
		pixmap_display_format = DISPLAY_FORMAT_RGB565;
	else if ( 5 == window_red && 5 == window_green && 5 == window_blue && 1 == window_alpha )
		pixmap_display_format = DISPLAY_FORMAT_ARGB1555;
	else if ( 4 == window_red && 4 == window_green && 4 == window_blue && 4 == window_alpha )
		pixmap_display_format = DISPLAY_FORMAT_ARGB4444;
	status = egl_helper_create_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT, &win );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_create_window failed" );
	if ( EGL_TRUE != status )
	{
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window failed" );
		return;
	}

	/* set up pixmap usage bits */
#ifdef EGL_MALI_VG
	usage |= EGL_OPENVG_BIT;
#endif
#ifdef EGL_MALI_GLES
#if MALI_EGL_GLES_MAJOR_VERSION == 1
	usage |= EGL_OPENGL_ES_BIT;
#else
	usage |= EGL_OPENGL_ES2_BIT;
#endif
#endif

#ifndef HAVE_ANDROID_OS
	/* set up the pixmap */
	status = egl_helper_create_nativepixmap_extended( test_suite, TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT, pixmap_display_format, usage, &pixmap);

	ASSERT_EGL_EQUAL( status, EGL_TRUE, "Pixmap creation failed" );
	if ( EGL_TRUE != status )
	{
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window failed" );
		return;
	}
#endif
	/* Case 1: Call eglSwapBuffers with invalid display */
	status = eglSwapBuffers( dpy_invalid, surface_invalid );
	ASSERT_EGL_EQUAL( EGL_FALSE, status, "eglSwapBuffers succeeded with invalid display" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_DISPLAY, "eglSwapBuffers did not set EGL_BAD_DISPLAY" );

	/* Case 2: Call eglSwapBuffers with uninitialized display */
	status = egl_helper_prepare_display( test_suite, &dpy_valid, EGL_FALSE, &native_dpy );
	if ( EGL_TRUE == status )
	{
		status = eglSwapBuffers( dpy_valid, surface_invalid );
		ASSERT_EGL_EQUAL( EGL_FALSE, status, "eglSwapBuffers succeeded with uninitialized display" );
#ifdef HAVE_ANDROID_OS
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_SURFACE, "eglSwapBuffers did not set EGL_BAD_SURFACE" );
#else
		ASSERT_EGL_ERROR( eglGetError(), EGL_NOT_INITIALIZED, "eglSwapBuffers did not set EGL_NOT_INITIALIZED" );
#endif
		egl_helper_terminate_display( test_suite, dpy_valid, &native_dpy );
	}

	/* Case 3: Call eglSwapBuffers with invalid surface */
	status = egl_helper_prepare_display( test_suite, &dpy_valid, EGL_TRUE, &native_dpy );
	if ( EGL_FALSE == status )
	{
		egl_helper_free_nativepixmap( pixmap );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
		ASSERT_FAIL( 0 == egl_helper_free_nativepixmap( pixmap ) );
		return;
	}

	status = eglSwapBuffers( dpy_valid, surface_invalid );
	ASSERT_EGL_EQUAL( EGL_FALSE, status, "eglSwapBuffers succeeded with invalid surface" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_SURFACE, "eglSwapBuffers did not set EGL_BAD_SURFACE" );

	{
		EGLint attributes[] = {
			EGL_SURFACE_TYPE, EGL_WINDOW_BIT|EGL_PBUFFER_BIT|EGL_PIXMAP_BIT,
            EGL_RENDERABLE_TYPE,
#ifdef EGL_MALI_VG
		    EGL_OPENVG_BIT |
#endif
#ifdef EGL_MALI_GLES
#if MALI_EGL_GLES_MAJOR_VERSION == 1
			EGL_OPENGL_ES_BIT,
#else
			EGL_OPENGL_ES2_BIT,
#endif
#else
            0,
#endif
			EGL_NONE
		};

		status = egl_test_get_window_config_with_attributes( test_suite, dpy_valid, attributes, &config );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_test_get_window_config_with_attributes failed" );
	}
	/* OpenVG */
#ifdef EGL_MALI_VG
	eglBindAPI( EGL_OPENVG_API );
#else
	eglBindAPI( EGL_OPENGL_ES_API );
#endif

	if ( (EGLConfig)NULL != config )
	{
		/* check if config supports window surface */
		status = eglGetConfigAttrib( dpy_valid, config, EGL_SURFACE_TYPE, &value );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglGetConfigAttrib failed to return EGL_SURFACE_TYPE" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib sat an error" );
		if ( value & EGL_WINDOW_BIT )
		{
			surface_window = eglCreateWindowSurface( dpy_valid, config, win, NULL );
			ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface_window, "eglCreateWindowSurface failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );
		}

		/* check if config supports pbuffer surface */
		status = eglGetConfigAttrib( dpy_valid, config, EGL_SURFACE_TYPE, &value );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglGetConfigAttrib failed to return EGL_SURFACE_TYPE" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib sat an error" );
		if ( value & EGL_PBUFFER_BIT )
		{
			surface_pbuffer = eglCreatePbufferSurface( dpy_valid, config, surface_pbuffer_attrib );
			ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface_pbuffer, "eglCreatePbufferSurface failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreatePbufferSurface sat an error" );
		}
#ifndef HAVE_ANDROID_OS
		/* check if config supports pixmap surface */
		status = eglGetConfigAttrib( dpy_valid, config, EGL_SURFACE_TYPE, &value );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglGetConfigAttrib failed to return EGL_SURFACE_TYPE" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib sat an error" );
		if ( value & EGL_PIXMAP_BIT )
		{
			surface_pixmap = eglCreatePixmapSurface( dpy_valid, config, pixmap, NULL );
			ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface_pixmap, "eglCreatePixmapSurface failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreatePixmapSurface sat an error" );
		}
#endif
		context = eglCreateContext( dpy_valid, config, (EGLConfig)NULL, context_attribs );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );
	}

	/* Case 4: Call eglSwapBuffers with pixmap, current context */
	if ( (EGL_NO_SURFACE != surface_pixmap) && (EGL_NO_CONTEXT != context) )
	{
		status = eglMakeCurrent( dpy_valid, surface_pixmap, surface_pixmap, context );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed making pixmap surface current" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

		if ( EGL_TRUE == status )
		{
			status = eglSwapBuffers( dpy_valid, surface_pixmap );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglSwapBuffers  failed with current pixmap surface" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglSwapBuffers  sat an error for current pixmap surface" );

			status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed resetting current context" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );
		}
	}

	/* Case 5: Call eglSwapBuffers with pbuffer, current context */
	if ( (EGL_NO_SURFACE != surface_pbuffer) && (EGL_NO_CONTEXT != context) )
	{
		status = eglMakeCurrent( dpy_valid, surface_pbuffer, surface_pbuffer, context );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed making pbuffer surface current" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

		if ( EGL_TRUE == status )
		{
			status = eglSwapBuffers( dpy_valid, surface_pbuffer );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglSwapBuffers  failed with current pbuffer surface" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglSwapBuffers sat an error for current pbuffer surface" );

			status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed resetting current context" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );
		}
	}

	/* Case 6: Call eglSwapBuffers with window, not on current context */
	if ( (EGL_NO_SURFACE != surface_pbuffer) && (EGL_NO_SURFACE != surface_window) && (EGL_NO_CONTEXT != context) )
	{
		status = eglMakeCurrent( dpy_valid, surface_pbuffer, surface_pbuffer, context );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed making pbuffer surface current" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

		if ( EGL_TRUE == status )
		{
			status = eglSwapBuffers( dpy_valid, surface_window );
			ASSERT_EGL_EQUAL( EGL_FALSE, status, "eglSwapBuffers succeeded with window surface not being current" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_SURFACE, "eglSwapBuffers did not set EGL_BAD_SURFACE ");

			status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed resetting current context" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );
		}
	}

	/* Case 7: Call eglSwapBuffers with valid window surface, but no current context for current API */
	if ( EGL_NO_SURFACE != surface_window )
	{
		status = eglSwapBuffers( dpy_valid, surface_window );
		ASSERT_EGL_EQUAL( EGL_FALSE, status, "eglSwapBuffers succeeded with window surface and no current context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_CONTEXT, "eglSwapBuffers did not set EGL_BAD_CONTEXT");
	}

	/* Case 8: Call eglSwapBuffers with valid window surface, on current context */
	if ( (EGL_NO_SURFACE != surface_window) && (EGL_NO_CONTEXT != context) )
	{
		status = eglMakeCurrent( dpy_valid, surface_window, surface_window, context );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed making window surface current" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

		if ( EGL_TRUE == status )
		{
#ifdef EGL_MALI_VG
			clear_color[0] = clear_color[1] = clear_color[2] = clear_color[3] = 1.0f;
			vgSetfv( VG_CLEAR_COLOR, 4, clear_color );
			vgClear( 0, 0, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), egl_helper_get_window_height( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ) );
#endif /* EGL_MALI_VG */

			status = eglSwapBuffers( dpy_valid, surface_window );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglSwapBuffers failed with window surface being current" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglSwapBuffers sat an error");

			status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed resetting current context" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );
		}
	}

	if ( EGL_NO_SURFACE != surface_window )
	{
		status = eglDestroySurface( dpy_valid, surface_window );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface did not succeed deleting the window surface" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
	}

	if ( EGL_NO_SURFACE != surface_pbuffer )
	{
		status = eglDestroySurface( dpy_valid, surface_pbuffer );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface did not succeed deleting the pbuffer surface" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
	}

	if ( EGL_NO_SURFACE != surface_pixmap )
	{
		status = eglDestroySurface( dpy_valid, surface_pixmap );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface did not succeed deleting the pixmap surface" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
	}

	if ( EGL_NO_CONTEXT != context )
	{
		status = eglDestroyContext( dpy_valid, context );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
	}

	egl_helper_terminate_display( test_suite, dpy_valid, &native_dpy );

	status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window failed" );
	ASSERT_FAIL( 0 == egl_helper_free_nativepixmap( pixmap ) );
}

/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_eglSwapBuffers(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "eglSwapBuffers", egl_create_fixture,
	                                egl_remove_fixture, results );
	add_test_with_flags( new_suite, "AGE32_eglSwapBuffers", test_eglSwapBuffers,
	                     FLAG_TEST_ALL | FLAG_TEST_SMOKETEST );
	return new_suite;
}
