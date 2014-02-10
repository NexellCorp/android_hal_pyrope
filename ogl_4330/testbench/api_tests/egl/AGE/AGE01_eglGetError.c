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
 * Verifies the functionality of eglGetError
 *
 */

#include "../egl_framework.h"
#include <suite.h>

static void test_eglGetError( suite* test_suite )
{
	EGLBoolean status;
	EGLDisplay dpy;

	/* Case 1: error state should be EGL_SUCCESS to start with */
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetError returned wrong error" );

	/* Case 2: error state should be EGL_SUCCESS after releasing a thread */
	ASSERT_EGL_EQUAL( eglReleaseThread(), EGL_TRUE, "Thread could not be released" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetError returned wrong error" );

	/* Case 3: check if the last error is returned in case of multiple errors - do some checks in between */
	dpy = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	ASSERT_EGL_NOT_EQUAL( dpy, EGL_NO_DISPLAY, "eglGetDisplay did not return valid handle");
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetError returned wrong error" );

	if ( EGL_NO_DISPLAY != dpy )
	{
		status = eglInitialize( (EGLDisplay)-1, NULL, NULL ); /* should generate EGL_BAD_DISPLAY */
		ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglInitialize allowed invalid handle");
		ASSERT_EGL_ERROR( eglGetError(), EGL_BAD_DISPLAY, "eglGetError returned wrong error" );

		status = eglCopyBuffers( dpy, (EGLSurface)NULL, (EGLNativePixmapType)NULL ); /* should generate EGL_NOT_INITIALIZED */
		ASSERT_EGL_EQUAL( status, EGL_FALSE, "eglCopyBuffers allowed uninitialized display");

		ASSERT_EGL_ERROR( eglGetError(), EGL_NOT_INITIALIZED, "Error state was not written over");

#if defined(HAVE_ANDROID_OS) && (MALI_ANDROID_API < 17)
		ASSERT_EGL_EQUAL( eglTerminate( dpy ), EGL_FALSE, "eglTerminate failed");
		/*on ICS, JB, eglTerminate will return EGL_NOT_INITIALIZED if try to terminate on a uninitialized display*/
		ASSERT_EGL_ERROR( eglGetError(), EGL_NOT_INITIALIZED, "eglTerminate sat an error");
#else
		ASSERT_EGL_EQUAL( eglTerminate( dpy ), EGL_TRUE, "eglTerminate failed");
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglTerminate sat an error");
#endif
	}
}


/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_eglGetError(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "eglGetError", egl_create_fixture,
	                                egl_remove_fixture, results );

	add_test_with_flags(new_suite, "AGE01_eglGetError", test_eglGetError,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}

