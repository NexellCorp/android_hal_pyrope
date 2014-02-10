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
 * Public definition of test harness executable.
 */

#include "egl_main_suite.h"
#include "egl_helpers.h"
#include "egl_test_parameters.h"

#include "mem.h"

/*
 *	External declarations of suite factory functions. This is more compact than
 *	a separate .h file for each suite.
 */
extern suite* create_suite_eglGetError( mempool *pool, result_set *results );
extern suite* create_suite_eglGetDisplay( mempool *pool, result_set *results );
extern suite* create_suite_eglInitialize( mempool *pool, result_set *results );
extern suite* create_suite_eglTerminate( mempool *pool, result_set *results );
extern suite* create_suite_eglQueryString( mempool *pool, result_set *results );
extern suite* create_suite_eglGetConfigs( mempool *pool, result_set *results );
extern suite* create_suite_eglChooseConfig( mempool *pool, result_set *results );
extern suite* create_suite_eglGetConfigAttrib( mempool *pool, result_set *results );
extern suite* create_suite_eglCreateWindowSurface( mempool *pool, result_set *results );
extern suite* create_suite_eglCreatePbufferSurface( mempool *pool, result_set *results );
extern suite* create_suite_eglCreatePixmapSurface( mempool *pool, result_set *results );
extern suite* create_suite_eglDestroySurface( mempool *pool, result_set *results );
extern suite* create_suite_eglQuerySurface( mempool *pool, result_set *results );
extern suite* create_suite_eglBindAPI( mempool *pool, result_set *results );
extern suite* create_suite_eglQueryAPI( mempool *pool, result_set *results );
extern suite* create_suite_eglWaitClient( mempool *pool, result_set *results );
extern suite* create_suite_eglReleaseThread( mempool *pool, result_set *results );
extern suite* create_suite_eglCreatePbufferFromClientBuffer( mempool *pool, result_set *results );
extern suite* create_suite_eglSurfaceAttrib( mempool *pool, result_set *results );
extern suite* create_suite_eglBindTexImage( mempool *pool, result_set *results );
extern suite* create_suite_eglReleaseTexImage( mempool *pool, result_set *results );
extern suite* create_suite_eglSwapInterval( mempool *pool, result_set *results );
extern suite* create_suite_eglCreateContext( mempool *pool, result_set *results );
extern suite* create_suite_eglDestroyContext( mempool *pool, result_set *results );
extern suite* create_suite_eglMakeCurrent( mempool *pool, result_set *results );
extern suite* create_suite_eglGetCurrentContext( mempool *pool, result_set *results );
extern suite* create_suite_eglGetCurrentSurface( mempool *pool, result_set *results );
extern suite* create_suite_eglGetCurrentDisplay( mempool *pool, result_set *results );
extern suite* create_suite_eglQueryContext( mempool *pool, result_set *results );
extern suite* create_suite_eglWaitGL( mempool *pool, result_set *results );
extern suite* create_suite_eglWaitNative( mempool *pool, result_set *results );
extern suite* create_suite_eglSwapBuffers( mempool *pool, result_set *results );
extern suite* create_suite_eglCopyBuffers( mempool *pool, result_set *results );
extern suite* create_suite_eglGetProcAddress( mempool *pool, result_set *results );

#ifdef EGL_MALI_GLES
extern suite* create_suite_multiple_gles_contexts( mempool *pool, result_set *results );
#endif

#ifdef EGL_MALI_VG
extern suite* create_suite_multiple_vg_contexts( mempool *pool, result_set *results );
extern suite* create_suite_multiple_vg_surfaces( mempool *pool, result_set *results );
#endif

#ifdef EGL_MALI_GLES
extern suite* create_suite_multiple_gles_surfaces( mempool *pool, result_set *results );
#endif

#if defined(EGL_MALI_GLES) && defined(EGL_MALI_VG)
#if MALI_EGL_GLES_MAJOR_VERSION == 1
extern suite* create_suite_same_surface( mempool *pool, result_set *results );
extern suite* create_suite_multiple_switching( mempool *pool, result_set *results );
extern suite* create_suite_context_resource_sharing( mempool *pool, result_set *results );
extern suite* create_suite_cross_API( mempool *pool, result_set *results );
extern suite* create_suite_multiple_process( mempool *pool, result_set *results );
extern suite* create_suite_multiple_thread( mempool *pool, result_set *results );
#endif
#if MALI_EGL_GLES_MAJOR_VERSION == 2
extern suite* create_suite_multiple_both( mempool *pool, result_set *results );
extern suite* create_suite_same_surface( mempool *pool, result_set *results );
extern suite* create_suite_multiple_switching( mempool *pool, result_set *results );
extern suite* create_suite_context_resource_sharing( mempool *pool, result_set *results );
extern suite* create_suite_cross_API( mempool *pool, result_set *results );
extern suite* create_suite_multiple_process( mempool *pool, result_set *results );
extern suite* create_suite_multiple_thread( mempool *pool, result_set *results );
#endif
#endif

extern suite* create_suite_surface_allocation( mempool *pool, result_set *results );

#ifdef EGL_MALI_VG
extern suite* create_suite_context_allocation_vg( mempool *pool, result_set *results );
#endif

#ifdef EGL_MALI_GLES
extern suite* create_suite_context_allocation_gles( mempool *pool, result_set *results );
#endif

extern suite* create_suite_window_surface_resize( mempool* pool, result_set* results ); /* STA04 */
extern suite* create_suite_cross_API_resize( mempool *pool, result_set *results ); /* STA05 */
extern suite* create_suite_cached_memory( mempool *pool, result_set *results ); /* STA06 */

extern suite* create_suite_pixmap_standalone( mempool* pool, result_set* results );
extern suite* create_suite_pixmap_combined( mempool* pool, result_set* results );
extern suite* create_suite_pixmap_combined_api( mempool* pool, result_set* results );
extern suite* create_suite_pixmap_buffer_preservation( mempool *pool, result_set *results );
extern suite* create_suite_pixmap_alignment( mempool *pool, result_set *results );
extern suite* create_suite_buffer_preservation(mempool* pool, result_set* results);
extern suite* create_suite_preserved_content(mempool* pool, result_set* results);

/**
 *	Function which will create, run, and remove a single test suite.
 *
 *	\param results	A result set in which to store the results of the test.
 *	\param factory	A pointer to a factory function that creates the suite to run.
 *
 *	\return			Zero if the suite executed successfully (even if it had test failures),
 *					or non-zero if the suite terminated doe to a fatal error.
 */
static result_status run_suite(result_set* results, test_specifier* test_spec, suite* (*factory)(mempool* pool, result_set* results) )
{
	mempool pool;
	memerr err;
	suite* new_suite;
	result_status status = result_status_pass;

	err = _test_mempool_init(&pool, 1024);
	if ( MEM_OK != err) return result_status_fail;

	results->suites_considered++;

	new_suite = factory(&pool,results);

	switch(test_spec->mode)
	{
		case MODE_EXECUTE:
			status = execute(new_suite, test_spec);
			/*if(status == result_status_pass) results->suites_passed++;*/
			break;

		case MODE_LIST_SUITES:
		case MODE_LIST_TESTS:
		case MODE_LIST_FIXTURES:
			list(new_suite,test_spec);
			break;
	}

	if(status == result_status_pass) results->suites_passed++;

	_test_mempool_destroy(&pool);

	return status;
}

/*
 *	Main test runner function.
 *	Full documentation in header.
 */

int run_main_suite(int mask, test_specifier* test_spec)
{
	memerr err;
	int worst_error = 0;
	mempool pool;
	result_set* results = NULL;

	err = _test_mempool_init(&pool, 1024);
	if ( MEM_OK != err) return 1; /* critical error */

	results = create_result_set(&pool);
	if( results == NULL )
	{
	_test_mempool_destroy(&pool);
	return 1; /* critical error */
	}

	/* Instead of adding another #if 0 here... I know you're just about to do that... */
	/* Use the filter parameter instead: ./egl_main_suite --suite=suitename  */
#ifndef ARM_INTERNAL_BUILD /* subset of API tests for customer release */
	run_suite(results, test_spec, create_suite_eglGetError);
	run_suite(results, test_spec, create_suite_eglCreateWindowSurface);
	run_suite(results, test_spec, create_suite_eglMakeCurrent);
	run_suite(results, test_spec, create_suite_eglSwapBuffers);
#if defined(EGL_MALI_GLES) && defined(EGL_MALI_VG)
	run_suite(results, test_spec, create_suite_cross_API);
#endif

#else
	run_suite(results, test_spec, create_suite_eglGetError);
	run_suite(results, test_spec, create_suite_eglGetDisplay);
	run_suite(results, test_spec, create_suite_eglInitialize);
	run_suite(results, test_spec, create_suite_eglTerminate);
	run_suite(results, test_spec, create_suite_eglQueryString);
	run_suite(results, test_spec, create_suite_eglGetConfigs);
	run_suite(results, test_spec, create_suite_eglChooseConfig);
	run_suite(results, test_spec, create_suite_eglGetConfigAttrib);
	run_suite(results, test_spec, create_suite_eglCreateWindowSurface);
	run_suite(results, test_spec, create_suite_eglCreatePbufferSurface);
	run_suite(results, test_spec, create_suite_eglCreatePixmapSurface);
	run_suite(results, test_spec, create_suite_eglDestroySurface);
	run_suite(results, test_spec, create_suite_eglQuerySurface);
	run_suite(results, test_spec, create_suite_eglBindAPI);
	run_suite(results, test_spec, create_suite_eglQueryAPI);
	run_suite(results, test_spec, create_suite_eglWaitClient);
	run_suite(results, test_spec, create_suite_eglReleaseThread);
	run_suite(results, test_spec, create_suite_eglCreatePbufferFromClientBuffer);
	run_suite(results, test_spec, create_suite_eglSurfaceAttrib);
	run_suite(results, test_spec, create_suite_eglBindTexImage);
	run_suite(results, test_spec, create_suite_eglReleaseTexImage);
	run_suite(results, test_spec, create_suite_eglSwapInterval);
	run_suite(results, test_spec, create_suite_eglCreateContext);
	run_suite(results, test_spec, create_suite_eglDestroyContext);
	run_suite(results, test_spec, create_suite_eglMakeCurrent);
	run_suite(results, test_spec, create_suite_eglGetCurrentContext);
	run_suite(results, test_spec, create_suite_eglGetCurrentSurface);
	run_suite(results, test_spec, create_suite_eglGetCurrentDisplay);
	run_suite(results, test_spec, create_suite_eglQueryContext);
	run_suite(results, test_spec, create_suite_eglWaitGL);
	run_suite(results, test_spec, create_suite_eglWaitNative);
	run_suite(results, test_spec, create_suite_eglSwapBuffers);
	run_suite(results, test_spec, create_suite_eglCopyBuffers);
	run_suite(results, test_spec, create_suite_eglGetProcAddress);

#if defined(EGL_MALI_GLES) && defined(EGL_MALI_VG)
	#if MALI_EGL_GLES_MAJOR_VERSION == 1
	run_suite(results, test_spec, create_suite_multiple_gles_contexts);
	run_suite(results, test_spec, create_suite_multiple_gles_surfaces);
	run_suite(results, test_spec, create_suite_multiple_vg_contexts);
	run_suite(results, test_spec, create_suite_multiple_vg_surfaces);
	run_suite(results, test_spec, create_suite_same_surface);
	run_suite(results, test_spec, create_suite_multiple_switching);
	run_suite(results, test_spec, create_suite_cross_API);
	run_suite(results, test_spec, create_suite_multiple_process);
	run_suite(results, test_spec, create_suite_multiple_thread);
	#endif

	#if MALI_EGL_GLES_MAJOR_VERSION == 2

	run_suite(results, test_spec, create_suite_multiple_gles_contexts);
	run_suite(results, test_spec, create_suite_multiple_vg_contexts);
	run_suite(results, test_spec, create_suite_multiple_vg_surfaces);
	run_suite(results, test_spec, create_suite_multiple_gles_surfaces);

	run_suite(results, test_spec, create_suite_multiple_both);
	run_suite(results, test_spec, create_suite_same_surface);
	run_suite(results, test_spec, create_suite_multiple_switching);
	run_suite(results, test_spec, create_suite_context_resource_sharing);
	run_suite(results, test_spec, create_suite_cross_API);
	run_suite(results, test_spec, create_suite_multiple_process);
	run_suite(results, test_spec, create_suite_multiple_thread);
	#endif
#endif

	run_suite(results, test_spec, create_suite_surface_allocation);

#ifdef EGL_MALI_VG
	run_suite(results, test_spec, create_suite_context_allocation_vg);
#endif

#ifdef EGL_MALI_GLES
	run_suite(results, test_spec, create_suite_context_allocation_gles);
#endif

	run_suite( results, test_spec, create_suite_window_surface_resize ); /* STA04 */
	run_suite( results, test_spec, create_suite_cross_API_resize ); /* STA05 */
	run_suite( results, test_spec, create_suite_cached_memory ); /* STA06 */
#ifndef HAVE_ANDROID_OS
	run_suite( results, test_spec, create_suite_pixmap_standalone );
	run_suite( results, test_spec, create_suite_pixmap_combined );
	run_suite( results, test_spec, create_suite_pixmap_combined_api );
	run_suite( results, test_spec, create_suite_pixmap_buffer_preservation );
	run_suite( results, test_spec, create_suite_pixmap_alignment );
#endif
	run_suite( results, test_spec, create_suite_buffer_preservation );
	run_suite( results, test_spec, create_suite_preserved_content );

#endif /* ARM_INTERNAL_BUILD */

	/* Print results, clean up, and exit */
	worst_error = 0;
	if (test_spec->mode == MODE_EXECUTE)
	{
		/* Print results, clean up, and exit */
		worst_error = print_results(results, mask);
	}

	remove_result_set( results );
	_test_mempool_destroy(&pool);

	return worst_error;
}

#include "../unit_framework/framework_main.h"


static egl_test_parameters* create_egl_test_parameters( mempool* pool )
{
	egl_test_parameters* test_parameters = (egl_test_parameters*)_test_mempool_alloc( pool, sizeof(egl_test_parameters) );
	if ( NULL == test_parameters )
	{
		return NULL;
	}

	test_parameters->window_pixel_format.red_size = 5;
	test_parameters->window_pixel_format.green_size = 6;
	test_parameters->window_pixel_format.blue_size = 5;
	test_parameters->window_pixel_format.alpha_size = 0;

	return test_parameters;
}

static int argument_parser( test_specifier* test_spec, mempool* pool, const char* argument )
{
	if ( NULL == argument )
	{
		/* initialization */
		test_spec->api_parameters = (void*)create_egl_test_parameters( pool );
		return NULL != test_spec->api_parameters;
	}
	else
	{
		/* check argument */
		egl_test_parameters* testparm = (egl_test_parameters*)test_spec->api_parameters;

#define RGBA_ARG_NAME "--window-rgba="
		if ( 0 == strncmp( RGBA_ARG_NAME, argument, strlen(RGBA_ARG_NAME) ) )
		{
			int rgba[4];
			int i = 0;
			/* chars following the argument are expected to
			 * be ASCII digits specifying red-, green-,
			 * blue- and alpha-size respectively.
			 */
			const char* asciidigit = argument + strlen(RGBA_ARG_NAME);
			for (; i<4; ++i, ++asciidigit)
			{
				if ( (*asciidigit < '0') || ( *asciidigit > '9'))
				{
					printf("Error parsing " RGBA_ARG_NAME " arguments, given: %s\n", argument);
					return 0;
				}
				rgba[i] = *asciidigit - '0';
			}
			testparm->window_pixel_format.red_size = rgba[0];
			testparm->window_pixel_format.green_size = rgba[1];
			testparm->window_pixel_format.blue_size = rgba[2];
			testparm->window_pixel_format.alpha_size = rgba[3];
			return 1;
		}
#undef RGBA_ARG_NAME
		return 0; /* unknown argument */
	}
}

int main(int argc, char **argv)
{
    int r = egl_helper_initialize();
    if (r)
    {
        /* Initialization failed */
        return r;
    }

    r = test_main(run_main_suite, &argument_parser, argc, argv);
    if (r)
     {
         /* Initialization failed */
         return r;
     }
    egl_helper_terminate();
    return r;
}

