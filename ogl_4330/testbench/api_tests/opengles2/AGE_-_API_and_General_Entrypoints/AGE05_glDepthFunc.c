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
 * glDepthFunc - check that state, return values and error states are set correctly.
 * 				Verify that all the valid functions discards the incoming fragments as specified by the input function.
 * 				Clears screen and depth to black and 0.5, and draws a quad in the middle of the screen that sets its
 *              region to a known color and depth (black, ~0.5 - the reason this is done with geometry is to make sure
 *              z is invariant for a following quad). Next, three quads are drawn on top of this, with a set of depth-funcs.
 *              One of the quads should be greater in z, one should be equal, and one should be less, and they should be
 *              drawn as white.
 *
 * Shaders used: "pass_two_attribs.vert" and "color.frag"
 *
 * CASE  #1 - input invalid func and check initial func - sets error state to GL_INVALID_ENUM
 * CASE  #2 - input valid  func - verify correct state with glGet
 * CASE  #3 - geometry verify. func=GL_LESS. 	Result: less section=white, equal section=black, greater section=black
 * CASE  #4 - geometry verify. func=GL_LEQUAL. Result: less section=white, equal section=white, greater section=black
 * CASE  #5 - geometry verify. func=GL_EQUAL. Result: less section=black, equal section=white, greater section=black
 * CASE  #6 - geometry verify. func=GL_GREATER. Result: less section=black, equal section=black, greater section=white
 * CASE  #7 - geometry verify. func=GL_GEQUAL. Result: less section=black, equal section=white, greater section=white
 * CASE  #8 - geometry verify. func=GL_NOTEQUAL. Result: less section=white, equal section=black, greater section=white
 * CASE  #9 - geometry verify. func=GL_NEVER. Result: less section=black, equal section=black, greater section=black
 * CASE #10 - geometry verify. func=GL_ALWAYS. Result: less section=white, equal section=white, greater section=white
 */


#include "../gl2_framework.h"
#include "suite.h"
#include "test_helpers.h"
#include "../gles_helpers.h"
#include <assert.h>

static void test_setting_valid_depth_func(suite* test_suite, GLenum depth_func)
{
	GLenum err, state;
	glDepthFunc(depth_func);
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);
	glGetIntegerv(GL_DEPTH_FUNC, (GLint*)&state);
	ASSERT_GLENUMS_EQUAL(state, depth_func);
}

static float vertices1[4 * 3];
static float vertices2[4 * 3];
static float vertices3[4 * 3];

static void draw_custom_colored_square(float vertices[4 * 3], float color[4])
{
	GLint color_location;
	GLint program;

	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	color_location = glGetUniformLocation(program, "color");

	if (color_location >= 0)
	{
		glUniform4fv(color_location, 1, color);
	}

	draw_custom_square(vertices);
}


static void draw_test_geometry(suite* test_suite, GLenum depth_func)
{
	float white[4] = {1.0, 1.0, 1.0, 1.0 };
	float black[4] = {0.0, 0.0, 0.0, 1.0 };

	/* clear entire screen to black and 0.5 */
	glClearDepthf(0.5);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* make sure quad #2 gets invariant z */
	glDepthFunc(GL_ALWAYS);
	draw_custom_colored_square(vertices2, black);

	/* draw three quads with the given depth function */
	glDepthFunc(depth_func);
	draw_custom_colored_square(vertices1, white);
	draw_custom_colored_square(vertices2, white);
	draw_custom_colored_square(vertices3, white);
}

static void test_depth_func(suite* test_suite, GLenum depth_func)
{
	test_setting_valid_depth_func(test_suite, depth_func);
	draw_test_geometry(test_suite, depth_func);
}

static void test_glDepthFunc(suite* test_suite)
{
	GLint state;
	GLenum status;
	GLboolean enabled;

	BackbufferImage img;
	GLuint prog;

	/* bottom rectangle */
	/* bottom left */
	vertices1[ 0] = -1;
	vertices1[ 1] = -1;
	vertices1[ 2] = -0.9f;

	/* bottom right */
	vertices1[ 3] =  1;
	vertices1[ 4] = -1;
	vertices1[ 5] = -0.9f;

	/* top right */
	vertices1[ 6] =  1;
	vertices1[ 7] = -(2.0f / 3) / 2.0f;
	vertices1[ 8] = -0.9f;

	/* top left */
	vertices1[ 9] = -1;
	vertices1[10] = -(2.0f / 3) / 2.0f;
	vertices1[11] = -0.9f;


	/* middle rectangle */
	/* bottom left */
	vertices2[ 0] = -1;
	vertices2[ 1] = -(2.0f / 3) / 2.0f;
	vertices2[ 2] =  0;

	/* bottom right */
	vertices2[ 3] =  1;
	vertices2[ 4] = -(2.0f / 3) / 2.0f;
	vertices2[ 5] =  0;

	/* top right */
	vertices2[ 6] =  1;
	vertices2[ 7] =  (2.0f / 3) / 2.0f;
	vertices2[ 8] =  0;

	/* top left */
	vertices2[ 9] = -1;
	vertices2[10] =  (2.0f / 3) / 2.0f;
	vertices2[11] =  0;


	/* top rectangle */
	/* bottom left */
	vertices3[ 0] = -1;
	vertices3[ 1] = (2.0f / 3) / 2.0f;
	vertices3[ 2] =  0.9f;

	/* bottom right */
	vertices3[ 3] =  1;
	vertices3[ 4] =  (2.0f / 3) / 2.0f;
	vertices3[ 5] =  0.9f;

	/* top right */
	vertices3[ 6] =  1;
	vertices3[ 7] =  1;
	vertices3[ 8] =  0.9f;

	/* top left */
	vertices3[ 9] = -1;
	vertices3[10] =  1;
	vertices3[11] =  0.9f;

	prog = load_program("pass_two_attribs.vert", "color.frag");
	glUseProgram(prog);

	/* CASE #1 - input invalid func and check initial func - sets error state to GL_INVALID_ENUM */
	{
		glDepthFunc(GL_GREATER + 10);
		status = glGetError();
		assert_ints_equal(status, GL_INVALID_ENUM, "invalid func did not return expected value");
		glGetIntegerv(GL_DEPTH_FUNC, &state);
		assert_ints_equal(state, GL_LESS, "initial state not as expected");
		enabled = glIsEnabled(GL_DEPTH_TEST);
		assert_ints_equal(enabled, GL_FALSE, "initial state not as expected");
	}


	/* CASE #2 - input valid  func - verify correct state with glGet */
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc( GL_GREATER );
		status = glGetError();
		assert_ints_equal(status, GL_NO_ERROR, "invalid func did not return expected value");
		glGetIntegerv(GL_DEPTH_FUNC, &state);
		assert_ints_equal(state, GL_GREATER, "#2 value not as expected");
		enabled = glIsEnabled(GL_DEPTH_TEST);
		assert_ints_equal(enabled, GL_TRUE, "initial state not as expected");
	}


	/* CASE #3 - geometry verify. func=GL_LESS. 	Result: less section=white, equal section=black, greater section=black */
	{
		test_depth_func(test_suite, GL_LESS);
		get_backbuffer(&img);
#if EGL_HELPER_TEST_FBDEV_DUMP
		glFinish();
#endif

		swap_buffers( test_suite );

#if EGL_HELPER_TEST_FBDEV_DUMP
		egl_helper_fbdev_test( gles2_framework_get_windowing_state( test_suite ) );
#endif

		/* less section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, 0,        gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
		/* equal section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT/2, gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
		/* greater section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT-1, gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
	}

	/* CASE #4 - geometry verify. func=GL_LEQUAL. Result: less section=white, equal section=white, greater section=black */
	{
		test_depth_func(test_suite, GL_LEQUAL);
		get_backbuffer(&img);

		/* less section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, 0,        gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
		/* equal section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT/2, gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
		/* greater section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT-1, gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
	}

	/* CASE #5 - geometry verify. func=GL_EQUAL. Result: less section=black, equal section=white, greater section=black */
	{
		test_depth_func(test_suite, GL_EQUAL);
		get_backbuffer(&img);

		/* less section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, 0,        gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
		/* equal section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT/2, gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
		/* greater section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT-1, gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
	}

	/* CASE #6 - geometry verify. func=GL_GREATER. Result: less section=black, equal section=black, greater section=white */
	{
		test_depth_func(test_suite, GL_GREATER);
		get_backbuffer(&img);

		/* less section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, 0,        gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
		/* equal section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT/2, gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
		/* greater section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT-1, gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
	}

	/* CASE #7 - geometry verify. func=GL_GEQUAL. Result: less section=black, equal section=white, greater section=white */
	{
		test_depth_func(test_suite, GL_GEQUAL);
		get_backbuffer(&img);

		/* less section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, 0,        gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
		/* equal section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT/2, gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
		/* greater section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT-1, gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
	}

	/* CASE #8 - geometry verify. func=GL_NOTEQUAL. Result: less section=white, equal section=black, greater section=white */
	{
		test_depth_func(test_suite, GL_NOTEQUAL);
		get_backbuffer(&img);

		/* less section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, 0,        gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
		/* equal section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT/2, gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
		/* greater section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT-1, gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
	}

	/* CASE #9 - geometry verify. func=GL_NEVER. Result: less section=black, equal section=black, greater section=black */
	{
		test_depth_func(test_suite, GL_NEVER);
		get_backbuffer(&img);

		/* less section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, 0,        gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
		/* equal section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT/2, gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
		/* greater section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT-1, gles_color_make_3f(0.0, 0.0, 0.0), gles_float_epsilon);
	}

	/* CASE #10 - geometry verify. func=GL_ALWAYS. Result: less section=white, equal section=white, greater section=white */
	{
		test_depth_func(test_suite, GL_ALWAYS);
		get_backbuffer(&img);

		/* less section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, 0,        gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
		/* equal section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT/2, gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
		/* greater section */
		ASSERT_GLES_FB_COLORS_EQUAL(&img, WIDTH/2, HEIGHT-1, gles_color_make_3f(1.0, 1.0, 1.0), gles_float_epsilon);
	}


	glDisable(GL_DEPTH_TEST);
	enabled = glIsEnabled(GL_DEPTH_TEST);
	assert_ints_equal(enabled, GL_FALSE, "initial state not as expected");

}


/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_glDepthFunc(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glDepthFunc", gles2_create_fixture, gles2_remove_fixture, results );

	add_test_with_flags(new_suite, "AGE05-glDepthFunc", test_glDepthFunc, FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}
