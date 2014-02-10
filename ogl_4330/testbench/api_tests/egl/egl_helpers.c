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
 * This file contains common helpers. See egl_helpers_<platform>.c for
 * platform specific implementations
 */

#include "egl_helpers.h"
#include <shared/mali_egl_image.h>
#include <stdio.h>
#include <unistd.h>

/* Internal signals used for the coprocess routines */
#define PIPE_SIGNAL_OK 0x00  /* Used for coprocces initalization only */
#define PIPE_SIGNAL_ERR 0x01 /* Used for coprocces initalization only */
#define PIPE_SIGNAL_CHILD_TERMINATED 0xff

const char *get_eglenum_name(EGLenum e)
{
	switch (e)
	{
#define ENTRY(e) case e: return #e;
		ENTRY(EGL_SUCCESS)
		ENTRY(EGL_NOT_INITIALIZED)
		ENTRY(EGL_BAD_ACCESS)
		ENTRY(EGL_BAD_ALLOC)
		ENTRY(EGL_BAD_ATTRIBUTE)
		ENTRY(EGL_BAD_CONFIG)
		ENTRY(EGL_BAD_CONTEXT)
		ENTRY(EGL_BAD_CURRENT_SURFACE)
		ENTRY(EGL_BAD_DISPLAY)
		ENTRY(EGL_BAD_MATCH)
		ENTRY(EGL_BAD_NATIVE_PIXMAP)
		ENTRY(EGL_BAD_NATIVE_WINDOW)
		ENTRY(EGL_BAD_PARAMETER)
		ENTRY(EGL_BAD_SURFACE)
		ENTRY(EGL_CONTEXT_LOST)

		ENTRY(MALI_EGL_IMAGE_SUCCESS)
		ENTRY(MALI_EGL_IMAGE_BAD_IMAGE)
		ENTRY(MALI_EGL_IMAGE_BAD_LOCK)
		ENTRY(MALI_EGL_IMAGE_BAD_MAP)
		ENTRY(MALI_EGL_IMAGE_BAD_ACCESS)
		ENTRY(MALI_EGL_IMAGE_BAD_ATTRIBUTE)
		ENTRY(MALI_EGL_IMAGE_IN_USE)
		ENTRY(MALI_EGL_IMAGE_BAD_PARAMETER)
		ENTRY(MALI_EGL_IMAGE_BAD_POINTER)
		ENTRY(MALI_EGL_IMAGE_SYNC_TIMEOUT)
		ENTRY(MALI_EGL_IMAGE_BAD_VERSION)
#undef	ENTRY
	}
	return "unknown enum";
}

#ifdef EGL_MALI_VG
const char *get_vgenum_name(EGLenum e)
{
	switch (e)
	{
#define ENTRY(e) case e: return #e;
		ENTRY(VG_NO_ERROR)
		ENTRY(VG_BAD_HANDLE_ERROR)
		ENTRY(VG_ILLEGAL_ARGUMENT_ERROR)
		ENTRY(VG_OUT_OF_MEMORY_ERROR)
		ENTRY(VG_PATH_CAPABILITY_ERROR)
		ENTRY(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR)
		ENTRY(VG_UNSUPPORTED_PATH_FORMAT_ERROR)
		ENTRY(VG_IMAGE_IN_USE_ERROR)
		ENTRY(VG_NO_CONTEXT_ERROR)
#undef	ENTRY
	}
	return "unknown enum";
}
#endif

#ifdef EGL_MALI_GLES
const char *get_glesenum_name(GLenum e)
{
   switch (e)
   {
#define ENTRY(e) case e: return #e;
	  ENTRY(GL_INVALID_ENUM)
	  ENTRY(GL_INVALID_VALUE)
	  ENTRY(GL_INVALID_OPERATION)
	  ENTRY(GL_OUT_OF_MEMORY)
	  ENTRY(GL_NO_ERROR)
#undef	ENTRY
   }
	return "unknown enum";
}
#endif

egl_helper_windowing_state* egl_helper_get_windowing_state(suite *test_suite)
{
	return ((egl_framework_fixture*)(test_suite->fixture))->windowing_state;
}

EGLConfig egl_helper_get_config_exact( suite *test_suite, EGLDisplay dpy, EGLint renderable_type, EGLint surface_type, EGLint red_size, EGLint green_size, EGLint blue_size, EGLint alpha_size, EGLNativePixmapType* pixmap_match )
{
	EGLint num_config = 0;
	EGLint max_config = 30;
	EGLConfig *configs = NULL;
	EGLConfig match_config = (EGLConfig)NULL;
	EGLint parameter[7*2+1];
	EGLint i = 0, j = 0;
	EGLBoolean status;

	configs = (EGLConfig *)MALLOC( sizeof( EGLConfig ) * max_config );
	ASSERT_POINTER_NOT_NULL( configs );
	if ( NULL == configs ) return match_config;

	if ( EGL_DONT_CARE != renderable_type )
	{
		parameter[i++] = EGL_RENDERABLE_TYPE;
		parameter[i++] = renderable_type;
	}

	if ( EGL_DONT_CARE != surface_type )
	{
		parameter[i++] = EGL_SURFACE_TYPE;
		parameter[i++] = surface_type;
	}

	if ( EGL_DONT_CARE != red_size )
	{
		parameter[i++] = EGL_RED_SIZE;
		parameter[i++] = red_size;
	}

	if ( EGL_DONT_CARE != green_size )
	{
		parameter[i++] = EGL_GREEN_SIZE;
		parameter[i++] = green_size;
	}

	if ( EGL_DONT_CARE != blue_size )
	{
		parameter[i++] = EGL_BLUE_SIZE;
		parameter[i++] = blue_size;
	}

	if ( EGL_DONT_CARE != alpha_size )
	{
		parameter[i++] = EGL_ALPHA_SIZE;
		parameter[i++] = alpha_size;
	}

	if ( NULL != pixmap_match )
	{
		parameter[i++] = EGL_MATCH_NATIVE_PIXMAP;
		parameter[i++] = (EGLint)*pixmap_match;
	}

	parameter[i++] = EGL_NONE;

	status = eglChooseConfig( dpy, parameter, configs, max_config, &num_config );
	if ( NULL != test_suite )
	{
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglChooseConfig failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglChooseConfig set an error" );
	}

	/* Find a config matching exactly */
	for ( j=0; j<num_config; j++ )
	{
		EGLint value;
		EGLConfig config;
		config = configs[j];

		if ( EGL_DONT_CARE != red_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_RED_SIZE, &value);
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != red_size ) continue;
		}

		if ( EGL_DONT_CARE != green_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_GREEN_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != green_size ) continue;
		}

		if ( EGL_DONT_CARE != blue_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_BLUE_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != blue_size ) continue;
		}

		if ( EGL_DONT_CARE != alpha_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_ALPHA_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != alpha_size ) continue;
		}
		match_config = config;
		break;
	}

	FREE( configs );
	return match_config;
}

EGLConfig egl_helper_get_dualconfig( suite *test_suite, EGLDisplay dpy, EGLint red_size, EGLint green_size, EGLint blue_size, EGLint alpha_size )
{
	EGLint num_config = 0;
	EGLint max_config = 200;
	EGLConfig *configs = NULL;
	EGLConfig match_config = (EGLConfig)NULL;
	EGLint parameter[7*2+1];
	EGLint i = 0, j = 0;
	EGLBoolean status;

	configs = (EGLConfig *)MALLOC( sizeof( EGLConfig ) * max_config );
	ASSERT_POINTER_NOT_NULL( configs );

	parameter[i++] = EGL_RENDERABLE_TYPE;
	parameter[i++] = EGL_OPENVG_BIT|EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT;

	parameter[i++] = EGL_SURFACE_TYPE;
	parameter[i++] = EGL_WINDOW_BIT;

	parameter[i++] = EGL_CONFORMANT;
	parameter[i++] = 0;

	if ( EGL_DONT_CARE != red_size )
	{
		parameter[i++] = EGL_RED_SIZE;
		parameter[i++] = red_size;
	}

	if ( EGL_DONT_CARE != green_size )
	{
		parameter[i++] = EGL_GREEN_SIZE;
		parameter[i++] = green_size;
	}

	if ( EGL_DONT_CARE != blue_size )
	{
		parameter[i++] = EGL_BLUE_SIZE;
		parameter[i++] = blue_size;
	}

	if ( EGL_DONT_CARE != alpha_size )
	{
		parameter[i++] = EGL_ALPHA_SIZE;
		parameter[i++] = alpha_size;
	}

	parameter[i++] = EGL_NONE;

	status = eglChooseConfig( dpy, parameter, configs, max_config, &num_config );
	if ( NULL != test_suite )
	{
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglChooseConfig failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglChooseConfig set an error" );
	}

	/* Find a config matching exactly */
	for ( j=0; j<num_config; j++ )
	{
		EGLint value;
		EGLConfig config;
		config = configs[j];

		status = eglGetConfigAttrib( dpy, config, EGL_RENDERABLE_TYPE, &value );
		if ( NULL != test_suite )
		{
			ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
		}
		if ( !(value&EGL_OPENVG_BIT) || ( !(value&EGL_OPENGL_ES_BIT) && !(value&EGL_OPENGL_ES2_BIT) ) ) continue;

		if ( EGL_DONT_CARE != red_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_RED_SIZE, &value);
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != red_size ) continue;
		}

		if ( EGL_DONT_CARE != green_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_GREEN_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != green_size ) continue;
		}

		if ( EGL_DONT_CARE != blue_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_BLUE_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != blue_size ) continue;
		}

		if ( EGL_DONT_CARE != alpha_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_ALPHA_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != alpha_size ) continue;
		}
		match_config = config;
		break;
	}

	FREE( configs );
	return match_config;
}

EGLConfig egl_helper_get_specific_dualconfig( suite *test_suite, EGLDisplay dpy, EGLint renderable_type, EGLint red_size, EGLint green_size, EGLint blue_size, EGLint alpha_size )
{
	EGLint num_config = 0;
	EGLint max_config = 200;
	EGLConfig *configs = NULL;
	EGLConfig match_config = (EGLConfig)NULL;
	EGLint parameter[7*2+1];
	EGLint i = 0, j = 0;
	EGLBoolean status;

	configs = (EGLConfig *)MALLOC( sizeof( EGLConfig ) * max_config );
	ASSERT_POINTER_NOT_NULL( configs );

	parameter[i++] = EGL_RENDERABLE_TYPE;
	parameter[i++] = renderable_type;

	parameter[i++] = EGL_SURFACE_TYPE;
	parameter[i++] = EGL_WINDOW_BIT;

	parameter[i++] = EGL_CONFORMANT;
	parameter[i++] = 0;

	if ( EGL_DONT_CARE != red_size )
	{
		parameter[i++] = EGL_RED_SIZE;
		parameter[i++] = red_size;
	}

	if ( EGL_DONT_CARE != green_size )
	{
		parameter[i++] = EGL_GREEN_SIZE;
		parameter[i++] = green_size;
	}

	if ( EGL_DONT_CARE != blue_size )
	{
		parameter[i++] = EGL_BLUE_SIZE;
		parameter[i++] = blue_size;
	}

	if ( EGL_DONT_CARE != alpha_size )
	{
		parameter[i++] = EGL_ALPHA_SIZE;
		parameter[i++] = alpha_size;
	}

	parameter[i++] = EGL_NONE;

	status = eglChooseConfig( dpy, parameter, configs, max_config, &num_config );
	if ( NULL != test_suite )
	{
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglChooseConfig failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglChooseConfig set an error" );
	}

	/* find a config matching exactly */
	for ( j=0; j<num_config; j++ )
	{
		EGLint value;
		EGLConfig config;
		config = configs[j];

		status = eglGetConfigAttrib( dpy, config, EGL_RENDERABLE_TYPE, &value );
		if ( NULL != test_suite )
		{
			ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
		}
		if ( !(value&EGL_OPENVG_BIT) || ( !(value&EGL_OPENGL_ES_BIT) && !(value&EGL_OPENGL_ES2_BIT) ) ) continue;

		if ( EGL_DONT_CARE != red_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_RED_SIZE, &value);
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != red_size ) continue;
		}

		if ( EGL_DONT_CARE != green_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_GREEN_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != green_size ) continue;
		}

		if ( EGL_DONT_CARE != blue_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_BLUE_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != blue_size ) continue;
		}

		if ( EGL_DONT_CARE != alpha_size )
		{
			status = eglGetConfigAttrib( dpy, config, EGL_ALPHA_SIZE, &value );
			if ( NULL != test_suite )
			{
				ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglGetConfigAttrib failed" );
				ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib set an error" );
			}
			if ( value != alpha_size ) continue;
		}
		match_config = config;
		break;
	}

	FREE( configs );
	return match_config;
}
void egl_helper_query_check_surface_value( suite *test_suite, EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value, EGLint wanted_value )
{
	EGLBoolean status;
	EGLint error;

	status = eglQuerySurface( dpy, surface, attribute, value );
	error = eglGetError();
	if ( NULL != test_suite )
	{
		assert_warn( EGL_TRUE == status, "eglQuerySurface failed" );
		assert_warn( error == EGL_SUCCESS, "eglQuerySurface set an error" );
		assert_warn( wanted_value == *value, "wanted value not equal to queried value");
	}
}

EGLBoolean egl_helper_prepare_display( suite *test_suite, EGLDisplay *display, EGLBoolean initialize, EGLNativeDisplayType *retNativeDpy )
{
	EGLBoolean status;
	EGLNativeDisplayType nativedpy;
	EGLDisplay egl_display;
	EGLenum i;

	/* Set initial value in failure case */
	*display = EGL_NO_DISPLAY;

	status = egl_helper_get_native_display( test_suite, &nativedpy, (egl_helper_dpy_flags)0 );
	if ( EGL_FALSE == status )
	{
		if ( NULL != test_suite ) ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_get_native_display failed" );
		return status;
	}

	egl_display = eglGetDisplay( nativedpy ); /* 3rdPartys sometimes just use the identity function here */

	i = eglGetError();

	if (i != EGL_SUCCESS
#ifdef EGL_3RDPARTY /* Some 3rd-party implementations return '0' for success here... */
			|| i != 0
#endif
			)
	{
		if ( NULL != test_suite ) ASSERT_EGL_ERROR( i, EGL_SUCCESS, "eglGetDisplay set an error" );
		egl_helper_release_native_display( test_suite, &nativedpy );
		return EGL_FALSE;
	}

	if ( EGL_NO_DISPLAY == egl_display )
	{
		if ( NULL != test_suite ) ASSERT_EGL_NOT_EQUAL( egl_display, EGL_NO_DISPLAY, "eglGetDisplay returned EGL_NO_DISPLAY" );
		egl_helper_release_native_display( test_suite, &nativedpy );
		return EGL_FALSE;
	}

	if ( EGL_TRUE == initialize )
	{
		status = eglInitialize( egl_display, NULL, NULL );
		if ( NULL != test_suite )
		{
			ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglInitialize failed" );
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglInitialize set an error" );
		}
		if ( EGL_FALSE == status )
		{
			eglTerminate( egl_display );
			egl_helper_release_native_display( test_suite, &nativedpy );
			return EGL_FALSE;
		}
	}

	/* Success : write out the values */
	*display = egl_display;
	*retNativeDpy = nativedpy;

	return EGL_TRUE;
}

void egl_helper_terminate_display( suite *test_suite, EGLDisplay dpy, EGLNativeDisplayType *inoutNativeDpy )
{
	EGLBoolean status;

	status = eglTerminate( dpy );
	if ( NULL != test_suite )
	{
	/* Error will return on android if try to terminate an uninitialized display */
#if defined(HAVE_ANDROID_OS)
		while(status == EGL_TRUE)
		{
		/* There is an internal ref count on dpy, ref count increase when
		 * eglInitialize is called and decreased when eglTerminate is called a
		 * real display termination will only be triggered when ref count is
		 * decreased to 0.  eglTermination will return EGL_FALSE if
		 * eglTermination is called on an uninitialized display on ics,jb
		 * but since jb_mr1, eglTerminate wrapper has changed, it allows to
		 * terminate a display that has already been terminated and return EGL_TRUE in this case
		 * due to different behavior on different android versions,
		 * add a eglQueryString call here to test if display is terminated.
		 */
			(void*) eglQueryString( dpy, EGL_CLIENT_APIS );
			if( eglGetError() == EGL_NOT_INITIALIZED )
			{
				break;
			}
			status = eglTerminate(dpy);
		}
#else
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "eglterminate failed" );
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglterminate sat an error" );
#endif
	}

	/* must do this regardless of whether eglterminate failed: */
	egl_helper_release_native_display( test_suite, inoutNativeDpy );
}

display_format egl_helper_get_display_format(u32 red_size, u32 green_size, u32 blue_size, u32 alpha_size, u32 luminance_size)
{
	display_format display = DISPLAY_FORMAT_INVALID;

	if( alpha_size == 0 && red_size == 5 && green_size == 6 && blue_size == 5 && luminance_size == 0)
	{
		display = DISPLAY_FORMAT_RGB565;
	}
	else if( alpha_size == 8 && red_size == 8 && green_size == 8 && blue_size == 8 && luminance_size == 0)
	{
		display = DISPLAY_FORMAT_ARGB8888;
	}
	else if( alpha_size == 0 && red_size == 8 && green_size == 8 && blue_size == 8 && luminance_size == 0)
	{
		display = DISPLAY_FORMAT_ARGB0888;
	}
	else if( alpha_size == 4 && red_size == 4 && green_size == 4 && blue_size == 4 && luminance_size == 0)
	{
		display = DISPLAY_FORMAT_ARGB4444;
	}
	else if( alpha_size == 1 && red_size == 5 && green_size == 5 && blue_size == 5 && luminance_size == 0)
	{
		display = DISPLAY_FORMAT_ARGB1555;
	}
	else if( alpha_size == 0 && red_size == 0 && green_size == 0 && blue_size == 0 && luminance_size == 8)
	{
		display = DISPLAY_FORMAT_L8;
	}
	else if( alpha_size == 8 && red_size == 0 && green_size == 0 && blue_size == 0 && luminance_size == 8)
	{
		display = DISPLAY_FORMAT_AL88;
	}
	else if( alpha_size == 8 && red_size == 0 && green_size == 0 && blue_size == 0 && luminance_size == 0)
	{
		display = DISPLAY_FORMAT_A8;
	}
	else
	{
		EGL_HELPER_DEBUG_PRINT(1, ("Error: Unrecognised pixmap format (R:%u G:%u B:%u A:%u L:%u)\n", red_size, green_size, blue_size, alpha_size, luminance_size));
	}
	return display;
}

void egl_helper_get_invalid_config(EGLConfig *invalid_config)
{
	*invalid_config = (EGLConfig)-1;
#if (defined(HAVE_ANDROID_OS) && (MALI_ANDROID_API < 17))
	*invalid_config = (EGLConfig)0xff;
#endif

}

int egl_helper_create_coprocesses(pipe_t *pipefds, int world_size)
{
	int i, j;
	pid_t cpid = 0;
	int status = 0;
	int pipefd[2];

	char recvbuf, sendbuf;


	/* Create communication pipes */
	for (i=0; i < world_size; ++i)
	{
		status = pipe(pipefds[i]);
		if (0 > status) return -1;
	}

	/* Create forks */
	for (i=1; i < world_size; ++i)
	{
		/* Mother: fork new process */
		cpid = fork();

		if (cpid == 0)
		{
			/* Child: wait for mother to deside it's desteny */
			egl_helper_pipe_recv(pipefds[i], &recvbuf, 1);
			if (recvbuf != PIPE_SIGNAL_OK)
			{
				egl_helper_exit_coprocess(pipefds, world_size, 0);
			}
			return i;
		}
		else if (cpid < 0)
		{
			/* Mother: could not fork, killing offspring */
			sendbuf = PIPE_SIGNAL_ERR;
			for (j=0; j < i; ++j)
			{
				egl_helper_pipe_send(pipefds[j], &sendbuf, 1);
			}
			egl_helper_wait_coprocesses(pipefds, world_size, 0);
			return -1;
		}
	}

	/* Signal ok */
	sendbuf = PIPE_SIGNAL_OK;
	for (i=1; i < world_size; ++i)
	{
		egl_helper_pipe_send(pipefds[i], &sendbuf, 1);
	}

	/* Return mother process rank */
	return 0;
}

void egl_helper_exit_coprocess(pipe_t *pipefds, int world_size, int signal_rank)
{
	int i;
	char s = PIPE_SIGNAL_CHILD_TERMINATED;

	/* signal mother */
	egl_helper_pipe_send(pipefds[signal_rank], &s, sizeof(u32));
	exit(0);
}

void egl_helper_wait_coprocesses(pipe_t *pipefds, int world_size, int rank)
{
	int i;
	char s;

	/* wait for signals */
	for (i=0; i < world_size-1;)
	{
		egl_helper_pipe_recv(pipefds[rank], &s, sizeof(u32));
		if (PIPE_SIGNAL_CHILD_TERMINATED == s) ++i;
	}

	/* close pipes */
	for (i=0; i < world_size; ++i)
	{
		close(pipefds[i][0]);
		close(pipefds[i][1]);
	}
}

size_t egl_helper_pipe_send(pipe_t pipefd, void *buf, size_t length)
{
	return write(pipefd[1], buf, length);
}

size_t egl_helper_pipe_recv(pipe_t pipefd, void *buf, size_t length)
{
	return read(pipefd[0], buf, length);
}

