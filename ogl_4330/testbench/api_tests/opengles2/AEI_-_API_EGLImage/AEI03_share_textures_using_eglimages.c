/*
 * $Copyright:
 * ----------------------------------------------------------------
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 * ----------------------------------------------------------------
 * $
 */
#define GL_GLEXT_PROTOTYPES
#ifndef __SYMBIAN32__
#define EGL_EGLEXT_PROTOTYPES
#endif
#include "../gl2_framework.h"
#include <EGL/mali_eglext.h>
#include "suite.h"

static void test_EGLimage_share_textures(suite* test_suite)
{
	/* test spec calls for a context A and a context B. I'll use the default context as context A, and create a new context with the same capabilities as ctx B */
	EGLContext ctx_a, ctx_b;
	EGLContext dpy;
	EGLConfig config;
	EGLImageKHR egl_image_a, egl_image_b;
	EGLint attribs_context[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLSurface surf_a, surf_b;
	EGLNativeWindowType window_type;
	EGLBoolean status;
	GLuint tex_a, tex_b, tex_b_in_ctx_a, tex_a_in_ctx_b;
	char *redbuffer;
	char *greybuffer;
	int i;
	BackbufferImage backbuffer;

	status = egl_helper_create_window(	gles2_framework_get_windowing_state(test_suite),
										gles2_window_width(test_suite),
										gles2_window_height(test_suite),
										&window_type );
	ASSERT_FAIL( EGL_TRUE == status );
	if ( EGL_TRUE != status )
	{
		status = egl_helper_destroy_window( gles2_framework_get_windowing_state(test_suite), window_type );
		ASSERT_FAIL( EGL_TRUE == status );
		return;
	}
	/* fill colorbuffers */
	redbuffer = _test_mempool_alloc(&test_suite->fixture_pool, WIDTH*HEIGHT*4);
	greybuffer = _test_mempool_alloc(&test_suite->fixture_pool, WIDTH*HEIGHT*4);
	assert_fatal(redbuffer, "failure to allocate the red buffer");
	assert_fatal(greybuffer, "failure to allocate the grey buffer");

	for(i = 0; i < WIDTH*HEIGHT; i++)
	{
			redbuffer[i*4 + 0] = 0xFF;
			redbuffer[i*4 + 1] = 0x0;
			redbuffer[i*4 + 2] = 0x0;
			redbuffer[i*4 + 3] = 0xFF;

			greybuffer[i*4 + 0] = 0x7F;
			greybuffer[i*4 + 1] = 0x7F;
			greybuffer[i*4 + 2] = 0x7F;
			greybuffer[i*4 + 3] = 0xFF;
	}

	dpy = gles2_window_get_display(test_suite);
	config = gles2_window_get_config(test_suite);
	surf_a = gles2_window_get_surface(test_suite);

	ctx_a = gles2_window_get_context(test_suite);

	ctx_b = eglCreateContext( dpy, config, EGL_NO_CONTEXT, attribs_context );
	assert_fatal(ctx_b, "context creation failed");

	surf_b = eglCreateWindowSurface(dpy, config, window_type, NULL);
	assert_fatal(surf_b, "surface creation failed");

	/* context A is bound by default - create texture in ctx a */
	glGenTextures(1, &tex_a);
	glBindTexture(GL_TEXTURE_2D, tex_a);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, redbuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* create EGL image from miplvl 0 in texture a. */
	egl_image_a = eglCreateImageKHR( dpy, ctx_a, EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer) tex_a, NULL);
	assert_fatal(eglGetError() == EGL_SUCCESS, "image creation from texture a failed");

	/* swap to ctx b, create texture B and make an EGL image out of this one as well */
	eglMakeCurrent(dpy, surf_b, surf_b, ctx_b);
	glGenTextures(1, &tex_b);
	glBindTexture(GL_TEXTURE_2D, tex_b);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, greybuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	egl_image_b = eglCreateImageKHR( dpy, ctx_b, EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer) tex_b, NULL);
	assert_fatal(eglGetError() == EGL_SUCCESS, "image creation from texture a failed");


	/* Now we try to use tex_b in ctx_a. To do that, we must create a new texture object. */
	eglMakeCurrent(dpy, surf_a, surf_a, ctx_a);
	glGenTextures(1, &tex_b_in_ctx_a);
	glBindTexture(GL_TEXTURE_2D, tex_b_in_ctx_a);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)egl_image_b);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	load_program("pass_two_attribs.vert", "texture.frag");
	draw_textured_square();
	get_backbuffer(&backbuffer);
	ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_3f(0.5, 0.5, 0.5), gles_float_epsilon);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glBindTexture(GL_TEXTURE_2D, 0);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* And use tex_a in ctx_b. */
	eglMakeCurrent(dpy, surf_b, surf_b, ctx_b);
	glGenTextures(1, &tex_a_in_ctx_b);
	glBindTexture(GL_TEXTURE_2D, tex_a_in_ctx_b);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)egl_image_a);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	load_program("pass_two_attribs.vert", "texture.frag");
	draw_textured_square();
	get_backbuffer(&backbuffer);
	ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_3f(1.0, 0.0, 0.0), gles_float_epsilon);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glBindTexture(GL_TEXTURE_2D, 0);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	eglMakeCurrent(dpy, surf_a, surf_a, ctx_a);

	eglDestroyImageKHR(dpy, egl_image_a);
	eglDestroyImageKHR(dpy, egl_image_b);

	/* cleanup additional context */
 	eglDestroyContext( dpy, ctx_b );
	eglDestroySurface( dpy, surf_b );

 	status = egl_helper_destroy_window( gles2_framework_get_windowing_state(test_suite), window_type );
	ASSERT_FAIL( EGL_TRUE == status );
}

suite* create_suite_EGLimage_share_textures(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "EGLImage-AEI03", gles2_create_fixture,
	                                gles2_remove_fixture, results );

	add_test_with_flags(new_suite, "AEI03_share_textures_using_eglimages",
	                    test_EGLimage_share_textures,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}
