/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * glClear
 *
 * CASE #1: verify that the correct error is generated on invalid input.
 * CASE #2: Test all combinations of input values, check that no errors are generated.
 * CASE #3: Clear the framebuffer with scissoring and viewport set, only the union between these should be cleared.
 */

#include "../gl2_framework.h"
#include "suite.h"

static void test_glClear(suite* test_suite)
{
	GLenum err;
	BackbufferImage backbuffer;
	int qw = WIDTH/4;
	int hw = 2*qw;
	int qh = HEIGHT/4;
	int hh = 2*qh;
	GLuint prog;

	prog = load_program("pass_two_attribs.vert", "white.frag");
	glUseProgram(prog);

	/* CASE 1: check if glClear generates an error on invalid input */
	glClear(GL_TEXTURE_2D);
	err = glGetError();
	assert_ints_equal(err, GL_INVALID_VALUE, "glClear did not generate GL_INVALID_VALUE on invalid input");



	/* CASE 2: Check if glClear generates an error on valid input */
	glClear(GL_COLOR_BUFFER_BIT);
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "glClear generated an error");

	glClear(GL_DEPTH_BUFFER_BIT);
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "glClear generated an error");

	glClear(GL_STENCIL_BUFFER_BIT);
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "glClear generated an error");

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "glClear generated an error");

	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "glClear generated an error");

	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "glClear generated an error");

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "glClear generated an error");



	/* CASE 3: check if glClear is affected by glScissor and NOT glViewport */

	/* clear the screen with white */
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 1.0);

	/* enable scissoring, clear the screen */
	glScissor(qw, qh, qw, qh);  /* this scissorbox should affect the area cleared */
	glEnable(GL_SCISSOR_TEST);
	glViewport(2, 2, 1, 1); /* this viewport should not affect anything */
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);

	/* check results, the range [WIDTH/4,HEIGHT/4] to [WIDTH/2-1, HEIGHT/2-1] should be cleared */
	get_backbuffer(&backbuffer);
	swap_buffers(test_suite);

	/* check the corners of the expected cleared quad */

	/* upper-left corner */
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw,   hh-1, gles_color_make_4f(0, 0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw,   hh,   gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw-1, hh,   gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw-1, hh-1, gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);

	/* upper-right corner */
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw-1, hh-1, gles_color_make_4f(0, 0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw,   hh,   gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw-1, hh,   gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw,   hh-1, gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);

	/* bottom-left corner */
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw, qh,     gles_color_make_4f(0, 0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw-1, qh-1, gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw,   qh-1, gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw-1, qh,   gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);

	/* bottom-right corner */
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw-1, qh,   gles_color_make_4f(0, 0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw,   qh,   gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw-1, qh-1, gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw,   qh-1, gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon);

	/* Case 4: no mask for color buffer */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glClearColor(1.0, 1.0, 0.0, 1.0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	get_backbuffer(&backbuffer);
	swap_buffers(test_suite);

	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw, qh, gles_color_make_4f(0.5, 0.5, 0.5, 1.0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw, hh, gles_color_make_4f(0.5, 0.5, 0.5, 1.0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw, hh, gles_color_make_4f(0.5, 0.5, 0.5, 1.0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw, qh, gles_color_make_4f(0.5, 0.5, 0.5, 1.0), gles_float_epsilon);

	/* Case 5: only write to red channel */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glColorMask(1, 0, 1, 0);
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	get_backbuffer(&backbuffer);
	swap_buffers(test_suite);

	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw, qh, gles_color_make_4f(1.0, 0.5, 1.0, 1.0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, qw, hh, gles_color_make_4f(1.0, 0.5, 1.0, 1.0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw, hh, gles_color_make_4f(1.0, 0.5, 1.0, 1.0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, hw, qh, gles_color_make_4f(1.0, 0.5, 1.0, 1.0), gles_float_epsilon);
}


suite* create_suite_glClear(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glClear", gles2_create_fixture,
	                                gles2_remove_fixture, results );
	add_test_with_flags(new_suite, "AGE19-glClear", test_glClear, FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);


	return new_suite;
}
