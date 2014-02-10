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
 * Verifies the functionality of eglCreateWindowSurface
 *
 */

#include "../egl_framework.h"
#include "../egl_test_parameters.h"
#include <suite.h>
#include <stdlib.h>

static void test_eglCreateWindowSurface( suite* test_suite )
{
	EGLint i;
	EGLint num_config;
	EGLint value;
	EGLBoolean status;
	EGLConfig *configs = NULL;
	EGLConfig config_valid = (EGLConfig)NULL, config_invalid = (EGLConfig)-1;
	EGLConfig config_vg = (EGLConfig)NULL;
	EGLConfig config_gles = (EGLConfig)NULL;
	EGLConfig config_nonwindow = (EGLConfig)NULL;
	EGLDisplay dpy_valid, dpy_invalid = (EGLDisplay)-1;
	EGLSurface surface = EGL_NO_SURFACE;
	EGLSurface surface2 = EGL_NO_SURFACE;
	EGLContext context = EGL_NO_CONTEXT;
	EGLNativeWindowType win_invalid = (EGLNativeWindowType) NULL ;
	EGLNativeWindowType win = (EGLNativeWindowType)NULL;
	EGLNativeDisplayType native_dpy;

	EGLint config_attribute_nonwindow[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };

	EGLint attribute_invalid[] = {-1, -1, EGL_NONE };
	EGLint attribute_valid[] = {EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_VG_COLORSPACE, EGL_VG_COLORSPACE_sRGB, EGL_VG_ALPHA_FORMAT, EGL_ALPHA_FORMAT_NONPRE, EGL_NONE };
	EGLint attribute_unsupported[] = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_VG_COLORSPACE, EGL_VG_COLORSPACE_LINEAR, EGL_VG_ALPHA_FORMAT, EGL_ALPHA_FORMAT_PRE, EGL_NONE };
	EGLint attribute_bad1[] = { EGL_RENDER_BUFFER, EGL_RENDER_BUFFER, EGL_NONE };
	EGLint attribute_bad2[] = { EGL_VG_COLORSPACE, EGL_VG_COLORSPACE, EGL_NONE };
	EGLint attribute_bad3[] = { EGL_VG_ALPHA_FORMAT, EGL_VG_ALPHA_FORMAT, EGL_NONE };
	EGLint attribute_bad4[] = { EGL_HEIGHT, -1, EGL_NONE };
	EGLint attribute_bad5[] = { EGL_MIPMAP_TEXTURE, EGL_MIPMAP_TEXTURE, EGL_NONE };
	EGLint attribute_bad6[] = { EGL_LARGEST_PBUFFER, EGL_LARGEST_PBUFFER, EGL_NONE };
	EGLint attribute_bad7[] = { EGL_VG_ALPHA_FORMAT, EGL_ALPHA_FORMAT_PRE, EGL_NONE };
	EGLint attribute_bad8[] = { EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGB, EGL_NONE };
	EGLint attribute_bad9[] = { EGL_TEXTURE_FORMAT, EGL_WIDTH, EGL_NONE };
	EGLint attribute_bad10[] = { EGL_TEXTURE_TARGET, EGL_TEXTURE_2D, EGL_NONE };
	EGLint attribute_bad11[] = { EGL_TEXTURE_TARGET, EGL_WIDTH, EGL_NONE };
#ifdef EGL_MALI_GLES
	EGLint context_attribute_opengles[] = { EGL_CONTEXT_CLIENT_VERSION, MALI_EGL_GLES_MAJOR_VERSION, EGL_NONE };
#else
	EGLint context_attribute_opengles[] = { EGL_NONE };
#endif
	egl_backbuffer img;

#ifdef EGL_MALI_VG
	egl_backbuffer img2;
	VGfloat clear_color_vg[4];
#endif /* EGL_MALI_VG */

#ifdef EGL_MALI_GLES
	GLfloat clear_color_gles[4];
	GLfloat vertices_triangle[3*4] =
	{
		-1.0, 1.0, 0.0,
		 1.0, 1.0, 0.0,
		-1.0, 0.5, 0.0,
		 1.0, 0.5, 0.0
	};
	#if MALI_EGL_GLES_MAJOR_VERSION == 2
	GLuint prog, vs, fs;
	#endif /* MALI_EGL_GLES_MAJOR_VERSION == 2 */
#endif /* EGL_MALI_GLES */

	configs = (EGLConfig *)MALLOC( sizeof( EGLConfig )*10 );
	ASSERT_POINTER_NOT_NULL( configs );
	if ( NULL == configs ) return;

	egl_helper_get_invalid_config(&config_invalid);

	/* Create native windows for this test */
	status = egl_helper_create_invalid_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), &win_invalid );
	ASSERT_FAIL( EGL_TRUE == status );
	if ( EGL_TRUE != status )
	{

		FREE( configs );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win_invalid );
		ASSERT_FAIL( EGL_TRUE == status );
		return;
	}


	/*create window after display is initialized. otherwise,  native window creatation will fail on Android*/
	status = egl_helper_create_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT, &win );
	ASSERT_FAIL( EGL_TRUE == status );
	if ( EGL_TRUE != status )
	{

		FREE( configs );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win_invalid );
		ASSERT_FAIL( EGL_TRUE == status );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
		ASSERT_FAIL( EGL_TRUE == status );
		return;
	}

	/* Case 1: Call eglCreateWindowSurface with invalid EGLDisplay */
	surface = eglCreateWindowSurface( dpy_invalid, config_valid, win_invalid, NULL );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface , "eglCreateWindowSurface succeeded with invalid display" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_DISPLAY, "eglCreateWindowSurface did not set EGL_BAD_DISPLAY" );

	/* Case 2: Call eglCreateWindowSurface with uninitialized EGLDisplay */
	status = egl_helper_prepare_display( test_suite, &dpy_valid, EGL_FALSE, &native_dpy );
	if ( EGL_TRUE == status )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_valid, win_invalid, NULL );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with uninitialized display" );
#if (defined(HAVE_ANDROID_OS) && MALI_ANDROID_API < 15)
		/* eglInitialize will be called when create native window in android*/
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_NATIVE_WINDOW, "eglCreateWindowSurface did not set EGL_BAD_NATIVE_WINDOW" );
#elif (defined(HAVE_ANDROID_OS) && MALI_ANDROID_API > 14)
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_CONFIG, "eglCreateWindowSurface did not set EGL_NOT_INITIALIZED" );
#else
		ASSERT_EGL_ERROR( eglGetError(), EGL_NOT_INITIALIZED, "eglCreateWindowSurface did not set EGL_NOT_INITIALIZED" );
#endif
		egl_helper_terminate_display( test_suite, dpy_valid, &native_dpy );
	}

	status = egl_helper_prepare_display( test_suite, &dpy_valid, EGL_TRUE, &native_dpy ); /* initialize the display */
	if ( EGL_FALSE == status )
	{
		FREE( configs );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win_invalid );
		ASSERT_FAIL( EGL_TRUE == status );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
		ASSERT_FAIL( EGL_TRUE == status );
		return;
	}


	status = eglGetConfigs( dpy_valid, configs, 1, &num_config );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigs did not return a valid config" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigs sat an error" );

	status = egl_test_get_window_config( test_suite, dpy_valid, EGL_OPENVG_BIT, &config_vg );

#ifdef EGL_MALI_GLES
	status = egl_test_get_window_config( test_suite, dpy_valid, EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT, &config_gles );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_test_get_window_config failed" );
#endif
	eglChooseConfig( dpy_valid, config_attribute_nonwindow, configs, 10, &num_config );
	config_nonwindow = (EGLConfig)NULL;
	for ( i=0; i<num_config; i++ )
	{
		eglGetConfigAttrib( dpy_valid, configs[i], EGL_SURFACE_TYPE, &value );
		if ( !(value & EGL_WINDOW_BIT) )
		{
			config_nonwindow = configs[i];
		}
	}

	config_valid = configs[0];
	/* Case 3: Call eglCreateWindowSurface with invalid config */
	surface = eglCreateWindowSurface( dpy_valid, config_invalid, win_invalid, NULL );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with invalid config" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_CONFIG, "eglCreateWindowSurface did not set EGL_BAD_CONFIG" );

	/* Case 4: Call eglCreateWindowSurface with invalid native window */
	surface = eglCreateWindowSurface( dpy_valid, config_valid, win_invalid, NULL );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with invalid native window" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_NATIVE_WINDOW , "eglCreateWindowSurface did not set EGL_BAD_NATIVE_WINDOW" );

	/* Case 5: Call eglCreateWindowSurface with invalid attribute */
	surface = eglCreateWindowSurface( dpy_valid, config_valid, win, attribute_invalid );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with invalid attribute" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );

	surface = eglCreateWindowSurface( dpy_valid, config_valid, win, attribute_bad1 );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with bad attribute" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );

	surface = eglCreateWindowSurface( dpy_valid, config_valid, win, attribute_bad2 );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with bad attribute" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );

	surface = eglCreateWindowSurface( dpy_valid, config_valid, win, attribute_bad3 );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with bad attribute" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );

	surface = eglCreateWindowSurface( dpy_valid, config_valid, win, attribute_bad4 );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with bad attribute" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );

	eglGetConfigAttrib( dpy_valid, config_valid, EGL_RENDERABLE_TYPE, &value );
	surface = eglCreateWindowSurface( dpy_valid, config_valid, win, attribute_bad5 );

	if ( (value & EGL_OPENGL_ES_BIT) || (value&EGL_OPENGL_ES2_BIT) )
	{
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with bad parameter" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_PARAMETER" );
	}
	else
	{
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with bad attribute" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );
	}

	surface = eglCreateWindowSurface( dpy_valid, config_valid, win, attribute_bad6 );
	ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with bad attribute" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );

	if ( (EGLConfig)NULL != config_gles )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_gles, win, attribute_bad7 );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with with bad attribute" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglCreateWindowSurface did not set EGL_BAD_MATCH" );

		surface = eglCreateWindowSurface( dpy_valid, config_gles, win, attribute_bad9 );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with with bad attribute" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );

		surface = eglCreateWindowSurface( dpy_valid, config_gles, win, attribute_bad11 );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with with bad parameter" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );
	}

	if ( (EGLConfig)NULL != config_vg )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, attribute_bad8 );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with with bad attribute" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );

		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, attribute_bad10 );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with with bad attribute" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ATTRIBUTE, "eglCreateWindowSurface did not set EGL_BAD_ATTRIBUTE" );
	}

	/* Case 6: Call eglCreateWindowSurface with attrib_list containing EGL_RENDER_BUFFER, EGL_VG_COLORSPACE, EG_VG_ALPHA_FORMAT */
	if ( (EGLConfig)NULL != config_vg )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, attribute_valid );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed with valid attributes" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		/* check that we have the wanted attributes */
		egl_helper_query_check_surface_value( test_suite, dpy_valid, surface, EGL_RENDER_BUFFER, &value, EGL_BACK_BUFFER );
		egl_helper_query_check_surface_value( test_suite, dpy_valid, surface, EGL_VG_COLORSPACE, &value, EGL_VG_COLORSPACE_sRGB );
		egl_helper_query_check_surface_value( test_suite, dpy_valid, surface, EGL_VG_ALPHA_FORMAT, &value, EGL_ALPHA_FORMAT_NONPRE );
		eglDestroySurface( dpy_valid, surface );
	}

	/* Case 7: Call eglCreateWindowSurface with attrib_list=NULL or attrlib_list=EGL_NONE */
	if ( (EGLConfig)NULL != config_vg )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, attribute_valid );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed with valid attributes" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		eglDestroySurface( dpy_valid, surface );
	}
	if ( (EGLConfig)NULL != config_gles )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_gles, win, attribute_valid );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed with valid attributes" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		eglDestroySurface( dpy_valid, surface );
	}

	/* Case 8: Call eglCreateWindowSurface with a window having attributes not corresponding to the config */
	/* the window does not currently have any attributes that can't be matched in EGL */
	if ( (EGLConfig)NULL != config_vg )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_vg, win_invalid, NULL );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with invalid window attributes" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_NATIVE_WINDOW, "eglCreateWindowSurface did not set EGL_BAD_MATCH" );
	}
	if ( (EGLConfig)NULL != config_gles )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_gles, win_invalid, NULL );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with invalid window attributes" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_NATIVE_WINDOW, "eglCreateWindowSurface did not set EGL_BAD_MATCH" );
	}

	/* Case 9: Call eglCreateWindowSurface with a config not supporting window surface (EGL_WINDOW_BIT missing) */
	/* go through the configs looking for a OpenVG config that doesn't support window surface */
	if ( (EGLConfig)NULL != config_nonwindow )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_nonwindow, win, NULL );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with pbuffer config" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH , "eglCreateWindowSurface did not set EGL_BAD_MATCH" );

		eglDestroySurface( dpy_valid, surface );
	}

	/* Case 10: Call eglCreateWindowSurface with a config not supporting colorspace or alpha format attributes in attrib_list */
	if ( (EGLConfig)NULL != config_vg )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, attribute_unsupported );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with unsupported attribute values" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglCreateWindowSurface did not set EGL_BAD_MATCH" );
	}
	if ( (EGLConfig)NULL != config_gles )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_gles, win, attribute_unsupported );
		ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface succeeded with unsupported attribute values" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglCreateWindowSurface did not set EGL_BAD_MATCH" );
	}


	/* Case 11: Call eglCreateWindowSurface with a window previously used with eglCreateWindowSurface */
	/* create a window using the win struct, then create another one using the same struct */
	if ( (EGLConfig)NULL != config_vg )
	{
		eglBindAPI( EGL_OPENVG_API );

		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		if ( EGL_NO_SURFACE != surface )
		{
			/* should not succeed creating the second surface */
			surface2 = eglCreateWindowSurface( dpy_valid, config_vg, win, NULL );
			ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface2, "eglCreateWindowSurface succeeded using a native window twice" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ALLOC, "eglCreateWindowSurface did not set EGL_BAD_ALLOC" );
		}

		eglDestroySurface( dpy_valid, surface );
	}
	if ( (EGLConfig)NULL != config_gles )
	{
		eglBindAPI( EGL_OPENGL_ES_API );

		surface = eglCreateWindowSurface( dpy_valid, config_gles, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		if ( EGL_NO_SURFACE != surface )
		{
			/* should not succeed creating the second surface */
			surface2 = eglCreateWindowSurface( dpy_valid, config_gles, win, NULL );
			ASSERT_EGL_EQUAL( EGL_NO_SURFACE, surface2, "eglCreateWindowSurface succeeded using a native window twice" );
			#ifdef HAVE_ANDROID_OS
			/*android libEGL will set EGL_BAD_NATIVE_WINDOW if attempt to create a second surface for a native window*/
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_NATIVE_WINDOW, "eglCreateWindowSurface did not set EGL_BAD_ALLOC" );
			#else
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_ALLOC, "eglCreateWindowSurface did not set EGL_BAD_ALLOC" );
			#endif
		}

		eglDestroySurface( dpy_valid, surface );
	}


	/* Case 12: Render a square with red, blue, green into the surface, verify buffer */
	if ( (EGLConfig)NULL != config_vg )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed with supported attribute values" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		eglBindAPI( EGL_OPENVG_API );

		/* create a context supporting openvg */
		context = eglCreateContext( dpy_valid, config_vg, EGL_NO_CONTEXT, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );


		if ( (EGL_NO_SURFACE != surface) && (EGL_NO_CONTEXT != context) )
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );
			if ( EGL_TRUE == status )
			{
				/* render an image with white, red, blue, green lines (4 pixel high) */
				/* white */
#ifdef EGL_MALI_VG
				clear_color_vg[0] = clear_color_vg[1] = clear_color_vg[2] = clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 0, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), egl_helper_get_window_height( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ) );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				/* red */
				clear_color_vg[0] = 1.0f;
				clear_color_vg[1] = 0.0f;
				clear_color_vg[2] = 0.0f;
				clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 8, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), 4 );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				/* green */
				clear_color_vg[0] = 0.0f;
				clear_color_vg[1] = 1.0f;
				clear_color_vg[2] = 0.0f;
				clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 4, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), 4 );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				/* blue */
				clear_color_vg[0] = 0.0f;
				clear_color_vg[1] = 0.0f;
				clear_color_vg[2] = 1.0f;
				clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 0, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), 4 );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				get_backbuffer_vg(&img, TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT);

				assert_pixel_color_and_alpha(&img, 0, 12, 1.0f, 1.0f, 1.0f, 1.0f, 1, "not white");
				assert_pixel_color_and_alpha(&img, 0,  8, 1.0f, 0.0f, 0.0f, 1.0f, 1, "not red");
				assert_pixel_color_and_alpha(&img, 0,  4, 0.0f, 1.0f, 0.0f, 1.0f, 1, "not green");
				assert_pixel_color_and_alpha(&img, 0,  0, 0.0f, 0.0f, 1.0f, 1.0f, 1, "not blue");

				/*vg_save_backbuffer( "windowsurface_vg.ppm", 0, 0, TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT );*/
#endif /* EGL_MALI_VG */

				eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			}
		}
		if ( EGL_NO_CONTEXT != context ) eglDestroyContext( dpy_valid, context );
		if ( EGL_NO_SURFACE != surface ) eglDestroySurface( dpy_valid, surface );
	}

	if ( (EGLConfig)NULL != config_gles )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_gles, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed with supported attribute values" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		/* create a context supporting gles */
		eglBindAPI( EGL_OPENGL_ES_API );

		context = eglCreateContext( dpy_valid, config_gles, EGL_NO_CONTEXT, context_attribute_opengles );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		if ( (EGL_NO_SURFACE != surface) && (EGL_NO_CONTEXT != context) )
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

			if ( EGL_TRUE == status )
			{

#ifdef EGL_MALI_GLES
	#if MALI_EGL_GLES_MAJOR_VERSION == 1
				glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glClear( GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glMatrixMode(GL_PROJECTION);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glLoadIdentity();
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glOrthof(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glDisable(GL_DEPTH_TEST);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glDisable(GL_CULL_FACE);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );
	#elif MALI_EGL_GLES_MAJOR_VERSION == 2
				glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glClear( GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glDisable(GL_DEPTH_TEST);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				glDisable(GL_CULL_FACE);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				prog = egl_load_shader2("egl-pass_two_attribs.vert", "egl-color.frag", &vs, &fs);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );
	#endif /* MALI_EGL_GLES_MAJOR_VERSION */

				clear_color_gles[0] = clear_color_gles[1] = clear_color_gles[2] = clear_color_gles[3] = 1.0f;
				egl_draw_custom_square_offset( vertices_triangle, clear_color_gles, 0, 0 );
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				clear_color_gles[0] = 1.0f; clear_color_gles[1] = 0.0f; clear_color_gles[2] = 0.0f; clear_color_gles[3] = 1.0f;
				egl_draw_custom_square_offset( vertices_triangle, clear_color_gles, 0, -0.5 );
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				clear_color_gles[0] = 0.0f; clear_color_gles[1] = 1.0f; clear_color_gles[2] = 0.0f; clear_color_gles[3] = 1.0f;
				egl_draw_custom_square_offset( vertices_triangle, clear_color_gles, 0, -1.0 );
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				clear_color_gles[0] = 0.0f; clear_color_gles[1] = 0.0f; clear_color_gles[2] = 1.0f; clear_color_gles[3] = 1.0f;
				egl_draw_custom_square_offset( vertices_triangle, clear_color_gles, 0, -1.5 );
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				get_backbuffer_gles(&img, TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

				assert_pixel_color_and_alpha(&img, 0, 12, 1.0f, 1.0f, 1.0f, 1.0f, 1, "not white");
				assert_pixel_color_and_alpha(&img, 0,  8, 1.0f, 0.0f, 0.0f, 1.0f, 1, "not red");
				assert_pixel_color_and_alpha(&img, 0,  4, 0.0f, 1.0f, 0.0f, 1.0f, 1, "not green");
				assert_pixel_color_and_alpha(&img, 0,  0, 0.0f, 0.0f, 1.0f, 1.0f, 1, "not blue");

	#if MALI_EGL_GLES_MAJOR_VERSION == 2
				egl_unload_shader2( prog, vs, fs);
				ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );
	#endif /* MALI_EGL_GLES_MAJOR_VERSION == 2 */
				/*gl_save_backbuffer( "windowsurface_gles.ppm", 0, 0, TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT );*/
#endif /* EGL_MALI_GLES */

				eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			}
		}
		if ( EGL_NO_SURFACE != surface ) eglDestroySurface( dpy_valid, surface );
		if ( EGL_NO_CONTEXT != context ) eglDestroyContext( dpy_valid, context );
	}

	/* Case 13: Resizing a native window */
	if ( (EGLConfig)NULL != config_vg )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed with supported attribute values" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		/* create a context supporting openvg */
		eglBindAPI( EGL_OPENVG_API );

		context = eglCreateContext( dpy_valid, config_vg, EGL_NO_CONTEXT, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		if ( (EGL_NO_SURFACE != surface) && (EGL_NO_CONTEXT != context) )
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

			if ( EGL_TRUE == status )
			{
				eglSwapBuffers( dpy_valid, surface );

				status = egl_helper_resize_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), TEST_DEFAULT_EGL_WIDTH*2, TEST_DEFAULT_EGL_HEIGHT*2, win );

				eglSwapBuffers( dpy_valid, surface ); /* should trigger a render target resize */

				/* render an image with white, red, blue, green lines (8 pixel high) */
				/* white */
#ifdef EGL_MALI_VG
				clear_color_vg[0] = clear_color_vg[1] = clear_color_vg[2] = clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 0, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), egl_helper_get_window_height( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ) );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				/* red */
				clear_color_vg[0] = 1.0f;
				clear_color_vg[1] = 0.0f;
				clear_color_vg[2] = 0.0f;
				clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 8*2, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), 4*2 );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				/* green */
				clear_color_vg[0] = 0.0f;
				clear_color_vg[1] = 1.0f;
				clear_color_vg[2] = 0.0f;
				clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 4*2, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), 4*2 );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				/* blue */
				clear_color_vg[0] = 0.0f;
				clear_color_vg[1] = 0.0f;
				clear_color_vg[2] = 1.0f;
				clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 0, egl_helper_get_window_width( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win ), 4*2 );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				get_backbuffer_vg_doublesize(&img2);

				assert_pixel_color_and_alpha(&img2, 0, 32-8, 1.0f, 1.0f, 1.0f, 1.0f, 1, "not white");
				assert_pixel_color_and_alpha(&img2, 0,  32-16, 1.0f, 0.0f, 0.0f, 1.0f, 1, "not red");
				assert_pixel_color_and_alpha(&img2, 0,  32-24, 0.0f, 1.0f, 0.0f, 1.0f, 1, "not green");
				assert_pixel_color_and_alpha(&img2, 0,  0, 0.0f, 0.0f, 1.0f, 1.0f, 1, "not blue");

				/*vg_save_backbuffer( "windowsurface_vg_resize.ppm", 0, 0, TEST_DEFAULT_EGL_WIDTH*2, TEST_DEFAULT_EGL_HEIGHT*2 );*/

#endif /* EGL_MALI_VG */

				eglSwapBuffers( dpy_valid, surface ); /* for visual inspection */
				eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			}
		}
		if ( EGL_NO_CONTEXT != context ) eglDestroyContext( dpy_valid, context );
		if ( EGL_NO_SURFACE != surface ) eglDestroySurface( dpy_valid, surface );
	}

	/* Case 14: Resizing a native window to zero-size */
	if ( (EGLConfig)NULL != config_vg )
	{
		surface = eglCreateWindowSurface( dpy_valid, config_vg, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed with supported attribute values" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		/* create a context supporting openvg */
		eglBindAPI( EGL_OPENVG_API );

		context = eglCreateContext( dpy_valid, config_vg, EGL_NO_CONTEXT, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		if ( (EGL_NO_SURFACE != surface) && (EGL_NO_CONTEXT != context) )
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

			if ( EGL_TRUE == status )
			{
				eglSwapBuffers( dpy_valid, surface );
				status = egl_helper_resize_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), 0, 0, win );

				eglSwapBuffers( dpy_valid, surface ); /* should trigger a render target resize */
#ifdef EGL_MALI_VG
				clear_color_vg[0] = clear_color_vg[1] = clear_color_vg[2] = clear_color_vg[3] = 1.0f;
				vgSetfv( VG_CLEAR_COLOR, 4, clear_color_vg );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				vgClear( 0, 0, 1, 1 );
				ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

				get_backbuffer_vg_doublesize(&img2);
				assert_pixel_color_and_alpha(&img2, 0, 0, 1.0f, 1.0f, 1.0f, 1.0f, 1, "not white");

#endif /* EGL_MALI_VG */

				eglSwapBuffers( dpy_valid, surface ); /* for visual inspection */
				eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			}
		}
		if ( EGL_NO_CONTEXT != context ) eglDestroyContext( dpy_valid, context );
	}

	/* Resizing a gles surface is no different from resizing a vg surface,
	 * but we should at some time update this to also include a gles test.
	 * We should also have separate tests for upsize and downsize
	 */
	if ( (EGLConfig)NULL != config_gles )
	{
#if MALI_EGL_GLES_MAJOR_VERSION == 2
	/* OpenGL ES 2.x */
#else /* MALI_EGL_GLES_MAJOR_VERSION == 2 */
	/* OpenGL ES 1.x */
#endif /* MALI_EGL_GLES_MAJOR_VERSION */
	}

	/** Surface still valid **/
	if ( EGL_NO_SURFACE != surface )
	{
		eglDestroySurface( dpy_valid, surface );
	}
	egl_helper_terminate_display( test_suite, dpy_valid, &native_dpy );

	FREE( configs );
	status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
	ASSERT_FAIL( EGL_TRUE == status );

	status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win_invalid );
	ASSERT_FAIL( EGL_TRUE == status );
}

/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_eglCreateWindowSurface(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "eglCreateWindowSurface", egl_create_fixture,
	                                egl_remove_fixture, results );
	add_test_with_flags( new_suite, "AGE09_eglCreateWindowSurface", test_eglCreateWindowSurface,
	                     FLAG_TEST_ALL | FLAG_TEST_SMOKETEST );
	return new_suite;
}

