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
 * glBlendFunc -
 *
 * CASE #1: Sequentially input illegal enums to all parameters of both glBlendFunc and glBlendFuncSeparate.
 * CASE #2: Set destination factor to GL_ZERO and source factors to each of the inputs (see notes). When inputting an illegal input, assert an error state is set. If not, draw a blended quad on a suitable background .
 * CASE #3: As point 2, only this time, set source to GL_ZERO and vary the destination factor across all inputs.
 * CASE #4: Clear screen black. Then call glBlendFuncSeparate with parameters GL_SRC_ALPHA, GL_ONE, GL_ZERO,  and GL_ZERO and draw a blended quad with color(0.5, 0.5, 0.5, 0.1);.
 * CASE #5: Repeat step 4, but with the last parameter in glBlendFuncSeparate being GL_ONE.
 * CASE #6: Call functions with two sets of legal inputs (see notes). glGet inputted values.
 *
 */

#include "../gl2_framework.h"
#include "../gles_helpers.h"
#include "test_helpers.h"
#include "suite.h"

static GLuint program;
static GLint colorLocation;


static void draw_colored_square(float r, float g, float b, float a)
{
	glUniform4f(colorLocation, r, g, b, a);
	draw_square();
}

static void test_blendmode(suite* test_suite, GLenum src_mode, GLenum dst_mode, struct gles_color clear_color, struct gles_color draw_color, struct gles_color expected_color, const char *file, int line)
{
	BackbufferImage img;
	struct gles_color drawn_color;

	/* setup */
	glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);

	/* setup */
	glBlendFunc(src_mode, dst_mode);
	/* draw */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	draw_colored_square(draw_color.r, draw_color.g, draw_color.b, draw_color.a);
	/* check */
	allocate_and_get_backbuffer(&img);
	drawn_color = gles_fb_get_pixel_color(&img, 0, 0);
	assert_fail(
		1 == gles_fb_color_equal(drawn_color, expected_color, 0.05f),
		dsprintf(
			test_suite,
			"blend result is <%.3f, %.3f, %.3f, %.3f>, expected <%.3f, %.3f, %.3f, %.3f> (%s:%d)",
			drawn_color.r, drawn_color.g, drawn_color.b, drawn_color.a,
			expected_color.r, expected_color.g, expected_color.b, expected_color.a,
			file, line
		)
	);
	free_backbuffer(&img);

	/* setup separate */
	glBlendFuncSeparate(src_mode, dst_mode, src_mode, dst_mode);
	/* draw */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	draw_colored_square(draw_color.r, draw_color.g, draw_color.b, draw_color.a);
	/* check */
	allocate_and_get_backbuffer(&img);
	drawn_color = gles_fb_get_pixel_color(&img, 0, 0);
	assert_fail(
		1 == gles_fb_color_equal(drawn_color, expected_color, 0.05f),
		dsprintf(
			test_suite,
			"separate blend result is <%.3f, %.3f, %.3f, %.3f>, expected <%.3f, %.3f, %.3f, %.3f> (%s:%d)",
			drawn_color.r, drawn_color.g, drawn_color.b, drawn_color.a,
			expected_color.r, expected_color.g, expected_color.b, expected_color.a,
			file, line
		)
	);
	free_backbuffer(&img);
}

static void test_BlendFunc(suite* test_suite)
{
	GLenum e;
	GLint i;
	BackbufferImage img;
	struct gles_color expected;

	float fb_epsilon = 0.05f;

	GLint alpha_bits = 0;
	glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);

	/*
	 * Commented out endianess restriction
	 * Found no reason for it
	 */
	/*assert_little_endian(test_suite);*/

	e = glGetError();
	assert_ints_equal(e, GL_NO_ERROR, "Initial GL error");

	/* CASE #1: Sequentially input illegal enums to all parameters of both glBlendFunc and glBlendFuncSeparate. */
	glBlendFunc(GL_TRIANGLES, GL_SRC_ALPHA);
	e = glGetError();
	assert_ints_equal(e, GL_INVALID_ENUM, "Wrong error code");

	glBlendFunc(GL_SRC_ALPHA, GL_TEXTURE_2D);
	e = glGetError();
	assert_ints_equal(e, GL_INVALID_ENUM, "Wrong error code");

	glBlendFuncSeparate(GL_COLOR_BUFFER_BIT, GL_ONE, GL_ONE, GL_ONE);
	e = glGetError();
	assert_ints_equal(e, GL_INVALID_ENUM, "Wrong error code");

	glBlendFuncSeparate(GL_ONE, GL_COLOR_BUFFER_BIT, GL_ONE, GL_ONE);
	e = glGetError();
	assert_ints_equal(e, GL_INVALID_ENUM, "Wrong error code");

	glBlendFuncSeparate(GL_ONE, GL_ONE, GL_COLOR_BUFFER_BIT, GL_ONE);
	e = glGetError();
	assert_ints_equal(e, GL_INVALID_ENUM, "Wrong error code");

	glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_COLOR_BUFFER_BIT);
	e = glGetError();
	assert_ints_equal(e, GL_INVALID_ENUM, "Wrong error code");

	program = load_program("pass_two_attribs.vert", "color.frag");
	colorLocation = glGetUniformLocation(program, "color");

	/* CASE #2: Set destination factor to GL_ZERO and source factors to each of the inputs (see notes). When inputting an illegal input, assert an error state is set. If not, draw a blended quad on a suitable background. */
	glEnable(GL_BLEND);

	/* ZERO, ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f(0, 0, 0, 1);
	else                 expected = gles_color_make_4f(0, 0, 0, 0);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_ZERO,               /* mode */
		gles_color_make_4f(1, 1, 1, 1), /* clear color */
		gles_color_make_4f(1, 0, 0, 1), /* draw color */
		expected,                       /* expected color */
		__FILE__, __LINE__
	);

	/* ONE, ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f(1, 0, 0, 1);
	else                 expected = gles_color_make_4f(1, 0, 0, 0);
	test_blendmode(
		test_suite,
		GL_ONE, GL_ZERO,                /* mode */
		gles_color_make_4f(1, 1, 1, 0), /* clear color */
		gles_color_make_4f(1, 0, 0, 0), /* draw color */
		expected,                       /* expected color */
		__FILE__, __LINE__
	);

	/* SRC_COLOR, ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f, 0, 0, 1.0f);
	else                 expected = gles_color_make_4f(0.25f, 0, 0, 0.25f);
	test_blendmode(
		test_suite,
		GL_SRC_COLOR, GL_ZERO,                /* mode */
		gles_color_make_4f(1, 1, 1, 1),       /* clear color */
		gles_color_make_4f(0.5f, 0, 0, 0.5f), /* draw color */
		expected,                             /* expected color */
		__FILE__, __LINE__
	);

	/* ONE_MINUS_SRC_COLOR, ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.1875f, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.1875f, 0, 0, 0);
	test_blendmode(
		test_suite,
		GL_ONE_MINUS_SRC_COLOR, GL_ZERO,    /* mode */
		gles_color_make_4f(1, 1, 1, 1),     /* clear color */
		gles_color_make_4f(0.25f, 0, 0, 1), /* draw color */
		expected,                           /* expected color */
		__FILE__, __LINE__
	);

	/* DST_COLOR, ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f, 0, 0, 1.0f);
	else                 expected = gles_color_make_4f(0.25f, 0, 0, 0.0f);
	test_blendmode(
		test_suite,
		GL_DST_COLOR, GL_ZERO,             /* mode */
		gles_color_make_4f(0.5f, 0, 0, 0), /* clear color */
		gles_color_make_4f(0.5f, 0, 0, 1), /* draw color */
		expected,                          /* expected color */
		__FILE__, __LINE__
	);

	/* ONE_MINUS_DST_COLOR, ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f((1 - 0.25) * 0.75, 0, 0, 1);
	else                 expected = gles_color_make_4f((1 - 0.25) * 0.75, 0, 0, 0);
	test_blendmode(
		test_suite,
		GL_ONE_MINUS_DST_COLOR, GL_ZERO,    /* mode */
		gles_color_make_4f(0.25f, 0, 0, 1), /* clear color */
		gles_color_make_4f(0.75f, 0, 0, 1), /* draw color */
		expected,                           /* expected color */
		__FILE__, __LINE__
	);

	/* SRC_ALPHA, ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.5, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.5, 0, 0, 0.25f);
	test_blendmode(
		test_suite,
		GL_SRC_ALPHA, GL_ZERO,            /* mode */
		gles_color_make_4f(0, 0, 0, 0),   /* clear color */
		gles_color_make_4f(1, 0, 0, 0.5), /* draw color */
		expected,                         /* expected color */
		__FILE__, __LINE__
	);

	/* ONE_MINUS_SRC_ALPHA, GL_ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.75, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.75, 0, 0, 0.25f * 0.75f);
	test_blendmode(
		test_suite,
		GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,    /* mode */
		gles_color_make_4f(0, 0, 0, 0),     /* clear color */
		gles_color_make_4f(1, 0, 0, 0.25f), /* draw color */
		expected,                           /* expected color */
		__FILE__, __LINE__
	);

	/* DST_ALPHA, GL_ZERO */
	if (alpha_bits == 0) expected = gles_color_make_4f(1.0f, 0, 0, 1.0f);
	else                 expected = gles_color_make_4f(0.5f, 0, 0, 0.5f);
	test_blendmode(
		test_suite,
		GL_DST_ALPHA, GL_ZERO,             /* mode */
		gles_color_make_4f(0, 0, 0, 0.5f), /* clear color */
		gles_color_make_4f(1, 0, 0, 1.0f), /* draw color */
		expected,                          /* expected color */
		__FILE__, __LINE__
	);

	/* ONE_MINUS_DST_ALPHA, GL_ZERO */
	if (alpha_bits == 0) expected = gles_color_make_3f(0, 0, 0);
	else                 expected = gles_color_make_4f((1.0f - 0.25f), 0, 0, (1.0f - 0.25f));
	test_blendmode(
		test_suite,
		GL_ONE_MINUS_DST_ALPHA, GL_ZERO,    /* mode */
		gles_color_make_4f(0, 0, 0, 0.25f), /* clear color */
		gles_color_make_4f(1, 0, 0, 1.0f),  /* draw color */
		expected,                           /* expected color */
		__FILE__, __LINE__
	);

	/* SRC_ALPHA_SATURATE, GL_ZERO */
	if (alpha_bits == 0) expected = gles_color_make_3f(0, 0, 0);
	else                 expected = gles_color_make_4f(0.5f, 0, 0, 1.0f);
	test_blendmode(
		test_suite,
		GL_SRC_ALPHA_SATURATE, GL_ZERO,    /* mode */
		gles_color_make_4f(0, 0, 0, 0.5f), /* clear color */
		gles_color_make_4f(1, 0, 0, 1.0f), /* draw color */
		expected,                          /* expected color */
		__FILE__, __LINE__
	);


	/* CASE #3: As point 2, only this time, set source to GL_ZERO and vary the destination factor across all inputs. */

	/* ZERO, ONE */
	if (alpha_bits == 0) expected = gles_color_make_4f(1, 0, 0, 1.0f);
	else                 expected = gles_color_make_4f(1, 0, 0, 0.5f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_ONE,                   /* mode */
		gles_color_make_4f(1, 0, 0, 0.5f), /* clear color */
		gles_color_make_4f(1, 0, 0, 1),    /* draw color */
		expected,                          /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, SRC_COLOR */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f * 0.5f, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.25f * 0.5f, 0, 0, 0.25f * 0.5f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_SRC_COLOR,                  /* mode */
		gles_color_make_4f(0.5f,  0, 0, 0.5f),  /* clear color */
		gles_color_make_4f(0.25f, 0, 0, 0.25f), /* draw color */
		expected,                               /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, ONE_MINUS_SRC_COLOR */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f * 0.5f, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.25f * 0.5f, 0, 0, 0.25f * 0.5f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_ONE_MINUS_SRC_COLOR,        /* mode */
		gles_color_make_4f(0.5f,  0, 0, 0.5f),  /* clear color */
		gles_color_make_4f(0.75f, 0, 0, 0.75f), /* draw color */
		expected,                               /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, DST_COLOR */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.25f, 0, 0, 0.25f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_DST_COLOR,                  /* mode */
		gles_color_make_4f(0.5f,  0, 0, 0.5f),  /* clear color */
		gles_color_make_4f(0,     1, 0, 0.75f), /* draw color */
		expected,                               /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, ONE_MINUS_DST_COLOR */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f * 0.75f, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.25f * 0.75f, 0, 0, 0.25f * 0.75f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_ONE_MINUS_DST_COLOR,        /* mode */
		gles_color_make_4f(0.25f, 0, 0, 0.25f), /* clear color */
		gles_color_make_4f(1,     1, 0, 0.75f), /* draw color */
		expected,                               /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, SRC_ALPHA */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f * 0.75f, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.25f * 0.75f, 0, 0, 0.75f * 0.75f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_SRC_ALPHA,                  /* mode */
		gles_color_make_4f(0.25f, 0, 0, 0.75f), /* clear color */
		gles_color_make_4f(1,     1, 0, 0.75f), /* draw color */
		expected,                               /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, ONE_MINUS_SRC_ALPHA */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f * 0.75f, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.25f * 0.75f, 0, 0, 0.75f * 0.75f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_ONE_MINUS_SRC_ALPHA,        /* mode */
		gles_color_make_4f(0.25f, 0, 0, 0.75f), /* clear color */
		gles_color_make_4f(1,     1, 0, 0.25f), /* draw color */
		expected,                               /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, DST_ALPHA */
	if (alpha_bits == 0) expected = gles_color_make_4f(0.25f, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.75f * 0.25f, 0, 0, 0.75f * 0.75f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_DST_ALPHA,                  /* mode */
		gles_color_make_4f(0.25f, 0, 0, 0.75f), /* clear color */
		gles_color_make_4f(1,     1, 0, 0.75f), /* draw color */
		expected,                               /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, ONE_MINUS_DST_ALPHA */
	if (alpha_bits == 0) expected = gles_color_make_4f(0, 0, 0, 1);
	else                 expected = gles_color_make_4f(0.75f * 0.25f, 0, 0, 0.75f * 0.25f);
	test_blendmode(
		test_suite,
		GL_ZERO, GL_ONE_MINUS_DST_ALPHA,        /* mode */
		gles_color_make_4f(0.25f, 0, 0, 0.25f), /* clear color */
		gles_color_make_4f(1,     1, 0, 0.75f), /* draw color */
		expected,                               /* expected color */
		__FILE__, __LINE__
	);

	/* ZERO, SRC_ALPHA_SATURATE */
	glClearColor(0, 0, 0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glBlendFuncSeparate(GL_ZERO, GL_ZERO, GL_ZERO, GL_SRC_ALPHA_SATURATE);
	e = glGetError();
	assert_ints_equal(e, GL_INVALID_ENUM, "wrong error code for glBlendFunc(GL_ZERO, GL_SRC_ALPHA_SATURATE);");

	/* CASE #4: Clear screen black. Then call glBlendFuncSeparate with parameters GL_SRC_ALPHA, GL_ONE, GL_ZERO,  and GL_ZERO and draw a blended quad with color(0.5, 0.5, 0.5, 0.1);. */
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ZERO);
	draw_colored_square(0.5f, 0.5f, 0.5f, 0.1f);

	if (alpha_bits == 0) expected = gles_color_make_4f(0.05f, 0.05f, 0.05f, 1.0f);
	else                 expected = gles_color_make_4f(0.05f, 0.05f, 0.05f, 0.0f);
	get_backbuffer(&img);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0, 0, expected, gles_float_epsilon + fb_epsilon);

	/* CASE #5: Repeat step 4, but with the last parameter in glBlendFuncSeparate being GL_ONE. */
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
	draw_colored_square(0.5f, 0.5f, 0.5f, 0.1f);

	if (alpha_bits == 0) expected = gles_color_make_4f(0.05f, 0.05f, 0.05f, 1.0f);
	else                 expected = gles_color_make_4f(0.05f, 0.05f, 0.05f, 0.1f);
	get_backbuffer(&img);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0, 0, expected, gles_float_epsilon + fb_epsilon);

    /* CASE #6: Call functions with two sets of legal inputs (see notes). glGet inputted values. */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glGetIntegerv(GL_BLEND_SRC_RGB, &i);
	assert_ints_equal(i, GL_SRC_ALPHA, "wrong value returned by glGetIntegerv");
	glGetIntegerv(GL_BLEND_DST_RGB, &i);
	assert_ints_equal(i, GL_ONE_MINUS_SRC_ALPHA, "wrong value returned by glGetIntegerv");
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &i);
	assert_ints_equal(i, GL_SRC_ALPHA, "wrong value returned by glGetIntegerv");
	glGetIntegerv(GL_BLEND_DST_ALPHA, &i);
	assert_ints_equal(i, GL_ONE_MINUS_SRC_ALPHA, "wrong value returned by glGetIntegerv");

	glBlendFuncSeparate(GL_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA);
	glGetIntegerv(GL_BLEND_SRC_RGB, &i);
	assert_ints_equal(i, GL_SRC_ALPHA, "wrong value returned by glGetIntegerv");
	glGetIntegerv(GL_BLEND_DST_RGB, &i);
	assert_ints_equal(i, GL_DST_ALPHA, "wrong value returned by glGetIntegerv");
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &i);
	assert_ints_equal(i, GL_ONE_MINUS_SRC_ALPHA, "wrong value returned by glGetIntegerv");
	glGetIntegerv(GL_BLEND_DST_ALPHA, &i);
	assert_ints_equal(i, GL_ONE_MINUS_DST_ALPHA, "wrong value returned by glGetIntegerv");

}



suite* create_suite_glBlendFunc(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glBlendFunc", gles2_create_fixture, gles2_remove_fixture, results );
	add_test_with_flags(new_suite, "ABL01-glBlendFunc", test_BlendFunc,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}

