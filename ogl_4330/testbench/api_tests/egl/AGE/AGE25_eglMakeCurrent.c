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
 * Verifies the functionality of eglMakeCurrent
 */

#include "../egl_framework.h"
#include "../egl_test_parameters.h"
#include <suite.h>
#include <stdlib.h>

static void test_eglMakeCurrent( suite* test_suite )
{
	EGLDisplay dpy_valid, dpy_invalid = (EGLDisplay)-1;
	EGLSurface surface = EGL_NO_SURFACE;
	EGLSurface surface_draw = EGL_NO_SURFACE;
	EGLint config_attribute_gles[] = { EGL_CONFORMANT, 0, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT|EGL_OPENGL_ES_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
	EGLint config_attribute_vg[] = { EGL_CONFORMANT, 0, EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
#ifdef EGL_MALI_GLES
	EGLint context_attribute_gles[] = { EGL_CONTEXT_CLIENT_VERSION, MALI_EGL_GLES_MAJOR_VERSION, EGL_NONE };
#else
	EGLint context_attribute_gles[] = { EGL_NONE };
#endif
	EGLConfig config_gles = (EGLConfig)NULL;
	EGLConfig config_vg = (EGLConfig)NULL;
	EGLConfig conf1 = (EGLConfig)NULL, conf2 = (EGLConfig)NULL; /* uncompatible configs */
	EGLConfig curr_config = (EGLConfig)NULL;
	EGLConfig *configs_vg = NULL;
	EGLConfig *configs_gles = NULL;
	EGLContext context = EGL_NO_CONTEXT;
	EGLContext context_vg = EGL_NO_CONTEXT;
#if defined(EGL_MALI_VG) && defined(EGL_MALI_GLES)
	EGLContext context_gles = EGL_NO_CONTEXT;
#endif
#ifdef EGL_MALI_VG
	EGLSurface surface_read = EGL_NO_SURFACE;
#endif
	EGLBoolean status;
	EGLint num_config_gles = 0;
	EGLint num_config_vg = 0;
	EGLint i, j;
	EGLint depth_size, stencil_size;
	EGLNativeWindowType win, win2;
	EGLNativeDisplayType native_dpy;

	configs_gles = (EGLConfig *)MALLOC( sizeof( EGLConfig )*10 );
	ASSERT_POINTER_NOT_NULL( configs_gles );

	configs_vg   = (EGLConfig *)MALLOC( sizeof( EGLConfig )*10 );
	ASSERT_POINTER_NOT_NULL( configs_vg );
	if ( (NULL == configs_vg) || (NULL == configs_gles) )
	{
		if ( NULL != configs_vg ) FREE( configs_vg );
		if ( NULL != configs_gles ) FREE( configs_gles );
		return;
	}

	status = egl_helper_create_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT, &win );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_create_window failed" );
	if ( EGL_TRUE != status )
	{
		FREE( configs_vg );
		FREE( configs_gles );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window failed" );
		return;
	}  

	status = egl_helper_create_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), TEST_DEFAULT_EGL_WIDTH, TEST_DEFAULT_EGL_HEIGHT, &win2 );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_create_window failed" );
	if ( EGL_TRUE != status )
	{
		free( configs_vg );
		free( configs_gles );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window failed" );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win2 );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window2 failed" );
		return;
	}  
    
	/* Case 1: Call eglMakeCurrent with invalid display */
	status = eglMakeCurrent( dpy_invalid, surface, surface, context );
	ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for invalid display" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_DISPLAY, "eglMakeCurrent did not set EGL_BAD_DISPLAY" );

	/* Case 2: Call eglMakeCurrent with uninitialized display */
	status = egl_helper_prepare_display( test_suite, &dpy_valid, EGL_FALSE, &native_dpy );
	if ( EGL_TRUE == status )
	{
#ifndef HAVE_ANDROID_OS
		status = eglMakeCurrent( dpy_valid, surface, surface, context );
		ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for uninitialized display" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_NOT_INITIALIZED, "eglMakeCurrent did not set EGL_NOT_INITIALIZED" );
		egl_helper_terminate_display( test_suite, dpy_valid, &native_dpy );
#endif
	}

	/* Case 3: Call eglMakeCurrent with invalid draw surface */
	status = egl_helper_prepare_display( test_suite, &dpy_valid, EGL_TRUE, &native_dpy );
	if ( EGL_FALSE == status )
	{
		FREE( configs_vg );
		FREE( configs_gles );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window win1 failed" );
		status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win2 );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window win2 failed" );
		return;
	}
/*read / draw surface should not be -1 on android, otherwise, segfault will happen in libEGL*/
#ifndef HAVE_ANDROID_OS
	status = eglMakeCurrent( dpy_valid, (EGLSurface)-1, (EGLSurface)-1, context );
	ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for invalid draw surface" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_SURFACE, "eglMakeCurrent did not set EGL_BAD_SURFACE" );
#endif

	/* Case 4: Call eglMakeCurrent with invalid read surface */
	/* create a valid draw surface */
	status = egl_test_get_window_config( test_suite, dpy_valid, EGL_OPENGL_ES2_BIT|EGL_OPENGL_ES_BIT, &config_gles );

	status = egl_test_get_window_config( test_suite, dpy_valid, EGL_OPENVG_BIT, &config_vg );

	if ( (EGLConfig)NULL != config_gles )
	{
		eglBindAPI( EGL_OPENGL_ES_API );

		surface_draw = eglCreateWindowSurface( dpy_valid, config_gles, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface_draw, "eglCreateWindowSurface failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		if ( EGL_NO_SURFACE != surface_draw )
		{

#ifndef HAVE_ANDROID_OS
			status = eglMakeCurrent( dpy_valid, surface_draw, (EGLSurface)-1, context );
			ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for invalid read surface" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_SURFACE, "eglMakeCurrent did not set EGL_BAD_SURFACE" );
#endif
		}
	}

	/* Case 5: Call eglMakeCurrent with invalid context */
	if ( ((EGLConfig)NULL != config_gles) && (EGL_NO_SURFACE !=  surface_draw) )
	{
#ifndef HAVE_ANDROID_OS
		status = eglMakeCurrent( dpy_valid, surface_draw, surface_draw, (EGLContext)-1 );
		ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for invalid context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_CONTEXT, "eglMakeCurrent did not set EGL_BAD_CONTEXT" );
#endif
	}

	/* Case 6: Call eglMakeCurrent with draw or read not compatible with context */
	/* create an OpenVG context */
	if ( ((EGLConfig)NULL != config_vg) && (EGL_NO_SURFACE != surface_draw) )
	{
		eglBindAPI( EGL_OPENVG_API );

		context_vg = eglCreateContext( dpy_valid, config_vg, EGL_NO_CONTEXT, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context_vg, "eglCreateContext failed creating a OpenVG context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		if ( EGL_NO_CONTEXT != context_vg )
		{
			/* try to hook opengl es surfaces into a openvg context */
			status = eglMakeCurrent( dpy_valid, surface_draw, surface_draw, context_vg );
			ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded OpenVG surfaces and OpenGL ES context" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglMakeCurrent did not set EGL_BAD_MATCH" );
		}

		/* Case 7: Call eglMakeCurrent with ctx=EGL_NO_CONTEXT and draw and/or read not EGL_NO_SURFACE */
		status = eglMakeCurrent( dpy_valid, surface_draw, surface_draw, EGL_NO_CONTEXT);
		ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded with invalid parameters" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglMakeCurrent did not set EGL_BAD_MATCH" );

		if ( EGL_NO_CONTEXT != context_vg )
		{
			/* Case 8: Call eglMakeCurrent with ctx!=EGL_NO_CONTEXT and draw or read = EGL_NO_SURFACE */
			status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, context_vg );
			ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded with invalid parameters" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglMakeCurrent did not set EGL_BAD_MATCH (2)" );
		}
	}

	/* Case 9: Call eglMakeCurrent with ctx=EGL_NO_CONTEXT and draw=read=EGL_NO_SURFACE */
	/* make a valid context and surface current */
	/* set it not be current any longer */
	status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglMakeCurrent failed setting context noncurrent" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	if ( EGL_NO_CONTEXT != context_vg )
	{
		status = eglDestroyContext( dpy_valid, context_vg );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed deleting the OpenVG context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
		context_vg = EGL_NO_CONTEXT;
	}

	/* Case 10: Create an OpenGL ES context, make it current, set api to OpenVG, call eglMakeCurrent */

#if defined(EGL_MALI_VG) && defined(EGL_MALI_GLES)
	if ( ((EGLConfig)NULL != config_gles) && (EGL_NO_SURFACE != surface_draw) )
	{
		eglBindAPI( EGL_OPENGL_ES_API );

		context_gles = eglCreateContext( dpy_valid, config_gles, EGL_NO_CONTEXT, context_attribute_gles );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context_gles, "eglCreateContext failed creating a OpenGL ES context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		if ( EGL_NO_CONTEXT != context_gles )
		{
			status = eglMakeCurrent( dpy_valid, surface_draw, surface_draw, context_gles );
			ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglMakeCurrent failed to make context and surface current" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

			if ( EGL_TRUE == status )
			{
				eglBindAPI( EGL_OPENVG_API );

				status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglMakeCurrent failed setting context noncurrent" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

				eglBindAPI( EGL_OPENGL_ES_API );

				/* the opengles context should still be current */
				ASSERT_EGL_EQUAL( context_gles, eglGetCurrentContext(), "eglGetCurrentContext did not return any context" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetCurrentContext sat an error" );

				/* the read and draw surface also */
				ASSERT_EGL_EQUAL( surface_draw, eglGetCurrentSurface( EGL_DRAW ), "eglGetCurrentSurface did not return the draw surface" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetCurrentSurface sat an error" );

				ASSERT_EGL_EQUAL( surface_draw, eglGetCurrentSurface( EGL_READ ), "eglGetCurrentSurface did not return the read surface" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetCurrentSurface sat an error" );


				status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglMakeCurrent failed setting context noncurrent" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );
			}
			status = eglDestroyContext( dpy_valid, context_gles );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed deleting the OpenGL ES context" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
			context_gles = EGL_NO_CONTEXT;
		}
	}
#endif /* EGL_MALI_VG && EGL_MALI_GLES */

	if ( EGL_NO_SURFACE != surface_draw )
	{
		status = eglDestroySurface( dpy_valid, surface_draw );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface failed deleting draw surface" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
		surface_draw = EGL_NO_SURFACE;
	}

	/* Case 11: Call eglMakeCurrent with OpenVG context/surfaces, but the two surfaces not the same surface */
#ifdef EGL_MALI_VG
	eglBindAPI( EGL_OPENVG_API );

	context_vg = eglCreateContext( dpy_valid, config_vg, EGL_NO_CONTEXT, NULL );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context_vg, "eglCreateContext failed creating a OpenVG context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

	/* create two window surfaces */
	surface_draw = eglCreateWindowSurface( dpy_valid, config_vg, win, NULL );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface_draw, "eglCreateWindowSurface failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );
    
	surface_read = eglCreateWindowSurface( dpy_valid, config_vg, win2, NULL );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface_read, "eglCreateWindowSurface failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

	if ( (EGL_NO_CONTEXT != context_vg) && (EGL_NO_SURFACE != surface_draw) && (EGL_NO_SURFACE != surface_read) )
	{
		status = eglMakeCurrent( dpy_valid, surface_draw, surface_read, context_vg );
		ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded setting an OpenVG context with draw and read surface not equal" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglMakeCurrent did not set EGL_BAD_MATCH" );
	}

	if ( EGL_NO_SURFACE != surface_draw )
	{
		status = eglDestroySurface( dpy_valid, surface_draw );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface failed deleting draw surface" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
		surface_draw = EGL_NO_SURFACE;
	}

	if ( EGL_NO_SURFACE != surface_read )
	{
		status = eglDestroySurface( dpy_valid, surface_read );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface failed deleting read surface" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
		surface_read = EGL_NO_SURFACE;
	}

	if ( EGL_NO_CONTEXT != context_vg )
	{
		status = eglDestroyContext( dpy_valid, context_vg );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed deleting the OpenVG context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
		context_vg = EGL_NO_CONTEXT;
	}
#endif /* EGL_MALI_VG */

	/* Case 12: Surface config not compatible with context config (depth, stencil, api) */

	/* look up all GLES configs, look for two configs not having the same depth size */
	eglChooseConfig( dpy_valid, config_attribute_gles, configs_gles, 10, &num_config_gles );
	if ( num_config_gles > 0 ) eglGetConfigAttrib( dpy_valid, configs_gles[0], EGL_DEPTH_SIZE, &depth_size );

	for ( i=0; i<num_config_gles; i++ )
	{
		EGLint value;
		eglGetConfigAttrib( dpy_valid, configs_gles[i], EGL_DEPTH_SIZE, &value );
		if ( value != depth_size )
		{
			conf1 = configs_gles[0];
			conf2 = configs_gles[i];
			break;
		}
	}

	if ( ((EGLConfig)NULL != conf1) && ((EGLConfig)NULL != conf2) )
	{
		eglBindAPI( EGL_OPENGL_ES_API );

		/* Create the context with conf1 */
		context = eglCreateContext( dpy_valid, conf1, EGL_NO_CONTEXT, context_attribute_gles );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed creating a context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		/* Create the surface with conf2 */
		surface = eglCreateWindowSurface( dpy_valid, conf2, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		if ( (EGL_NO_CONTEXT != context) && (EGL_NO_SURFACE != surface) )
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for mismatching configs" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglMakeCurrent did not set EGL_BAD_MATCH" );
		}

		if ( EGL_NO_SURFACE != surface )
		{
			status = eglDestroySurface( dpy_valid, surface );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
			surface = EGL_NO_SURFACE;
		}

		if ( EGL_NO_CONTEXT != context )
		{
			status = eglDestroyContext( dpy_valid, context );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
			context = EGL_NO_CONTEXT;
		}
	}
	conf1 = (EGLConfig)NULL;
	conf2 = (EGLConfig)NULL;

	/* look for two configs not having the same stencil size */
	if ( num_config_gles > 0 ) eglGetConfigAttrib( dpy_valid, configs_gles[0], EGL_STENCIL_SIZE, &stencil_size );

	for ( i=0; i<num_config_gles; i++ )
	{
		EGLint value;
		eglGetConfigAttrib( dpy_valid, configs_gles[i], EGL_STENCIL_SIZE, &value );
		if ( value != stencil_size )
		{
			EGLint depth1, depth2;
			eglGetConfigAttrib( dpy_valid, configs_gles[i], EGL_DEPTH_SIZE, &depth1 );
			eglGetConfigAttrib( dpy_valid, configs_gles[0], EGL_DEPTH_SIZE, &depth2);
			if ( depth1 == depth2 )
			{
				conf1 = configs_gles[0];
				conf2 = configs_gles[i];
				break;
			}
		}
	}

	if ( ((EGLConfig)NULL != conf1) && ((EGLConfig)NULL != conf2) )
	{
		eglBindAPI( EGL_OPENGL_ES_API );

		/* Create the context with conf1 */
		context = eglCreateContext( dpy_valid, conf1, EGL_NO_CONTEXT, context_attribute_gles );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed creating a context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		/* Create the surface with conf2 */
		surface = eglCreateWindowSurface( dpy_valid, conf2, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		if ( (EGL_NO_CONTEXT != context) && (EGL_NO_SURFACE != surface) )
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for mismatching configs" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglMakeCurrent did not set EGL_BAD_MATCH" );
		}

		if ( EGL_NO_SURFACE != surface )
		{
			status = eglDestroySurface( dpy_valid, surface );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
			surface = EGL_NO_SURFACE;
		}

		if ( EGL_NO_CONTEXT != context )
		{
			status = eglDestroyContext( dpy_valid, context );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
			context = EGL_NO_CONTEXT;
		}
	}

	/* look for two configs with same alpha, but different client API */

	/* grab all gles configs */
	status = eglChooseConfig( dpy_valid, config_attribute_gles, configs_gles, 10, &num_config_gles );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglChooseConfig failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglChooseConfig sat an error" );

	/* grab all vg configs */
	status = eglChooseConfig( dpy_valid, config_attribute_vg, configs_vg, 10, &num_config_vg );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglChooseConfig failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglChooseConfig sat an error" );

	if ( (num_config_gles > 0) && (num_config_vg > 0) )
	{
		EGLBoolean found_config = EGL_FALSE;
		i = j = 0;
		while ( (i < num_config_gles ) && (found_config == EGL_FALSE) )
		{
			while ( (j < num_config_vg ) && (found_config == EGL_FALSE) )
			{
				EGLint alpha_size1, alpha_size2;
				EGLint client_api1, client_api2;
				eglGetConfigAttrib( dpy_valid, configs_gles[i], EGL_ALPHA_SIZE, &alpha_size1 );
				eglGetConfigAttrib( dpy_valid, configs_vg[j], EGL_ALPHA_SIZE, &alpha_size2 );
				eglGetConfigAttrib( dpy_valid, configs_gles[i], EGL_RENDERABLE_TYPE, &client_api1 );
				eglGetConfigAttrib( dpy_valid, configs_vg[j], EGL_RENDERABLE_TYPE, &client_api2 );
				if ( alpha_size1 == alpha_size2 )
				{
					/* want gles only config in conf1 and vg only config in conf2 */
					if ( (client_api2 == EGL_OPENVG_BIT ) && (client_api1 == (EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT)) )
					{
						conf1 = configs_gles[i];
						conf2 = configs_vg[j];
						found_config = EGL_TRUE;
					}
				}
				j++;
			}
			j = 0;
			i++;
		}
	}

	if ( ((EGLConfig)NULL != conf1) && ((EGLConfig)NULL != conf2) )
	{
		eglBindAPI( EGL_OPENGL_ES_API );

		/* Create the context with conf1 */
		context = eglCreateContext( dpy_valid, conf1, EGL_NO_CONTEXT, context_attribute_gles );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed creating a context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		/* Create the surface with conf2 */
		surface = eglCreateWindowSurface( dpy_valid, conf2, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		if ( (EGL_NO_CONTEXT != context) && (EGL_NO_SURFACE != surface) )
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for mismatching configs" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglMakeCurrent did not set EGL_BAD_MATCH" );
		}

		if ( EGL_NO_SURFACE != surface )
		{
			status = eglDestroySurface( dpy_valid, surface );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
			surface = EGL_NO_SURFACE;
		}

		if ( EGL_NO_CONTEXT != context )
		{
			status = eglDestroyContext( dpy_valid, context );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
			context = EGL_NO_CONTEXT;
		}

		/* do the same the other way around */
		eglBindAPI( EGL_OPENVG_API );

		/* Create the context with conf2 */
		context = eglCreateContext( dpy_valid, conf2, EGL_NO_CONTEXT, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed creating a context" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

		/* Create the surface with conf1 */
		surface = eglCreateWindowSurface( dpy_valid, conf1, win, NULL );
		ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

		if ( (EGL_NO_SURFACE != surface) && (EGL_NO_CONTEXT != context) )
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent succeeded for mismatching configs" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_MATCH, "eglMakeCurrent did not set EGL_BAD_MATCH" );
		}

		if ( EGL_NO_SURFACE != surface )
		{
			status = eglDestroySurface( dpy_valid, surface );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
		}

		if ( EGL_NO_CONTEXT != context )
		{
			status = eglDestroyContext( dpy_valid, context );
			ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
		}
	}

	/* Case 13: making a surface current when window has been invalidated */
	curr_config = (EGLConfig)NULL;
	if ( (EGLConfig)NULL != config_gles )
	{
		curr_config = config_gles;
		eglBindAPI( EGL_OPENGL_ES_API );
		context = eglCreateContext( dpy_valid, curr_config, EGL_NO_CONTEXT, context_attribute_gles );
	}
	else if ( (EGLConfig)NULL != config_vg )
	{
		curr_config = config_vg;
		eglBindAPI ( EGL_OPENVG_API );
		context = eglCreateContext( dpy_valid, curr_config, EGL_NO_CONTEXT, NULL );
	}

	ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, context, "eglCreateContext failed creating a context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

	surface = eglCreateWindowSurface( dpy_valid, curr_config, win, NULL );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, surface, "eglCreateWindowSurface failed with valid attributes" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

	if ( (EGL_NO_CONTEXT != context) && (EGL_NO_SURFACE != surface) )
	{
		/* invalidate the window. The window can no longer be used after this. */
		status = egl_helper_invalidate_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );

		if(status == EGL_TRUE)
		{
			status = eglMakeCurrent( dpy_valid, surface, surface, context );
			ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglMakeCurrent sat surface with invalid native window current" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_NATIVE_WINDOW, "eglMakeCurrent did not set EGL_BAD_NATIVE_WINDOW" );
		}
		if ( EGL_TRUE == status )
		{
			status = eglMakeCurrent( dpy_valid, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
			ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglMakeCurrent failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );
		}
	}

	if ( EGL_NO_SURFACE != surface )
	{
		status = eglDestroySurface( dpy_valid, surface );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroySurface failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroySurface sat an error" );
	}

	if ( EGL_NO_CONTEXT != context )
	{
		status = eglDestroyContext( dpy_valid, context );
		ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglDestroyContext failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglDestroyContext sat an error" );
	}

	egl_helper_terminate_display( test_suite, dpy_valid, &native_dpy );

	FREE( configs_gles );
	FREE( configs_vg );

	status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window failed" );
	status = egl_helper_destroy_window( EGL_FRAMEWORK_GET_WINDOWING_STATE(), win2 );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window2 failed" );
}

/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_eglMakeCurrent(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "eglMakeCurrent", egl_create_fixture,
	                                egl_remove_fixture, results );
	add_test_with_flags( new_suite, "AGE25_eglMakeCurrent", test_eglMakeCurrent,
	                     FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);
	return new_suite;
}


