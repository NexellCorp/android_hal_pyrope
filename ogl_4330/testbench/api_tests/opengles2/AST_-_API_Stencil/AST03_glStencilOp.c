/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * glStencilOp - Test all stencil op modes, test all stencil op functionality with z buffering.
 *
 * Shaders used: pass_two_attribs.vert and white.frag
 *
 * CASE  #1 - Sequentially input illegal enums to all params in both functions.
 * CASE  #2 - Set stencilop to REPLACE, KEEP, KEEP. Draw a stenciltested quad across the screen. See notes.
 * CASE  #3 - Set stencilop to INVERT, DECR, INCR. Draw a stenciltested and depthtested quad at z=0.5 across the screen. See notes.
 * CASE  #4 - Set stencilop to  INCR_WRAP, ZERO, INCR. Draw a stenciltested and depthtested quad at z=0.5 across the screen. See notes.
 * CASE  #5 - Set frontfaced separate stencil op to ZERO, ZERO, INCR. Set backfaced separate stencil op to ZERO, ZERO, ZERO.
 * 				Draw a frontfaced stenciltested and depthtested quad at top half of the screen. Draw a backfaced stenciltested
 * 				and depthtested quad at bottom half of the screen.
 */


#include "../gl2_framework.h"
#include <suite.h>


static void test_StencilOpSub(suite* test_suite, GLenum a, GLenum b, GLenum c)
{
	GLenum e;
	glStencilOp(a, b, c);
	e = glGetError();
	ASSERT_GLENUMS_EQUAL(e, GL_INVALID_ENUM);
	glStencilOpSeparate(GL_FRONT, a, b, c);
	e = glGetError();
	ASSERT_GLENUMS_EQUAL(e, GL_INVALID_ENUM);
	glStencilOpSeparate(GL_BACK, a, b, c);
	e = glGetError();
	ASSERT_GLENUMS_EQUAL(e, GL_INVALID_ENUM);
	glStencilOpSeparate(GL_TEXTURE_2D, a, b, c);
	e = glGetError();
	ASSERT_GLENUMS_EQUAL(e, GL_INVALID_ENUM);
}

static void setup_conditions(suite* test_suite)
{
	/* Setup conditions:
		1. Setup minimal rendering state
		2. Set stencilfunc to func=GL_EQUAL, ref=1, mask=all 1's
		3. Set up the stencilbuffer so that the left half of the screen has a
		   stencil value = 1, and the right-half of the screen has a stencil
		   value of all 1's.
		4. Set up the depth buffer of the bottom half of the screen to z = 0,
		   top half to z="inifinite" (ie, default value, 1).
		5. Depthfunc must be at standard value (GL_LESS) throughout the test.
	*/
	BackbufferImage fb;
	get_backbuffer(&fb);

	glEnable(GL_STENCIL_TEST);
	glStencilMask(~0);

	glStencilFunc(GL_EQUAL, 1, ~0);
	glDepthFunc(GL_LESS);

	glEnable(GL_SCISSOR_TEST);

	glClear(GL_COLOR_BUFFER_BIT);

	glScissor(0, 0, fb.width/2, fb.height);
	glClearStencil(1);
	glClear(GL_STENCIL_BUFFER_BIT);
	glScissor(fb.width/2, 0, fb.width - (fb.width/2), fb.height);
	glClearStencil(~0);
	glClear(GL_STENCIL_BUFFER_BIT);

	glScissor(0, 0, fb.width, fb.height/2);
	glClearDepthf(0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glScissor(0, fb.height/2, fb.width, fb.height - (fb.height/2));
	glClearDepthf(1);
	glClear(GL_DEPTH_BUFFER_BIT);

	glDisable(GL_SCISSOR_TEST);
}

static void test_glStencilOp(suite* test_suite)
{
	GLint state;
	GLuint prog;
	BackbufferImage fb;
	float custom_square3[] =
	{
		-1, -1, 0.5,
		 1, -1, 0.5,
		 1,  1, 0.5,
		-1,  1, 0.5
	};

	prog = load_program("pass_two_attribs.vert", "white.frag");
	glUseProgram(prog);

	/*
		1. Sequentially input illegal enums to all params in both functions.
	*/
	test_StencilOpSub(test_suite, GL_INVALID_ENUM, GL_REPLACE, GL_INCR);
	test_StencilOpSub(test_suite, GL_KEEP, GL_INVALID_ENUM, GL_KEEP);
	test_StencilOpSub(test_suite, GL_KEEP, GL_KEEP, GL_INVALID_ENUM);

	/*
		2.  Set stencilop to REPLACE, KEEP, KEEP. Draw a stenciltested quad
			across the screen. See notes.
			Expected result: Entire stencilbuffer is set to 1.
			Test this through a stenciltested drawcall.
	*/
	setup_conditions(test_suite);
	glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
	draw_square();
	glClear(GL_COLOR_BUFFER_BIT);
	glStencilFunc(GL_EQUAL, 1, ~0);
	draw_square();
	get_backbuffer(&fb);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, 1, 1, gles_color_make_3f(1,1,1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, fb.width-1, 1, gles_color_make_3f(1,1,1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, fb.width-1, fb.height-1, gles_color_make_3f(1,1,1), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, 1, fb.height-1, gles_color_make_3f(1,1,1), gles_float_epsilon);

	/*
		3.	Set stencilop to INVERT, DECR, INCR. Draw a stenciltested and
			depthtested quad at z=0.5 across the screen. See notes.

			Expected result: top-left quarter is set to 2, rest of the
			stencilbuffer is set to 0. Test this through two stencil
			tested drawcalls.
	*/
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	setup_conditions(test_suite);
	glStencilOp(GL_INVERT, GL_DECR, GL_INCR);
	glEnable(GL_DEPTH_TEST);
	draw_custom_square(custom_square3);

	glDisable(GL_DEPTH_TEST);
	glStencilFunc(GL_EQUAL, 2, ~0);
	draw_square();

	get_backbuffer(&fb);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, 1, 1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, fb.width-1, 1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, fb.width-1, fb.height-1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, 1, fb.height-1, gles_color_make_3f(1, 1, 1), gles_float_epsilon);

	/*
		4.	Set stencilop to  INCR_WRAP, ZERO, INCR. Draw a stenciltested and
			depthtested quad at z=0.5 across the screen. See notes.
			Expected result: same as for 3
	*/
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	setup_conditions(test_suite);
	glStencilOp(GL_INCR_WRAP, GL_ZERO, GL_INCR);
	glEnable(GL_DEPTH_TEST);
	draw_custom_square(custom_square3);

	glDisable(GL_DEPTH_TEST);
	glStencilFunc(GL_EQUAL, 2, ~0);
	draw_square();
	get_backbuffer(&fb);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, 1, 1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, fb.width-1, 1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, fb.width-1, fb.height-1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, 1, fb.height-1, gles_color_make_3f(1, 1, 1), gles_float_epsilon);

	/*
		5.	Set frontfaced separate stencil op to ZERO, ZERO, INCR. Set backfaced
			separate stencil op to ZERO, ZERO, ZERO. Draw a frontfaced
			stenciltested and depthtested quad at top half of the screen. Draw a
			backfaced stenciltested and depthtested quad at bottom half of the screen.
			Expected result: same as for 3
	*/
	glStencilOpSeparate(GL_FRONT, GL_ZERO, GL_ZERO, GL_INCR);
	glStencilOpSeparate(GL_BACK, GL_ZERO, GL_ZERO, GL_ZERO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	setup_conditions(test_suite);
	glEnable(GL_DEPTH_TEST);
	glFrontFace(GL_CW);
	draw_bottom_rectangle();
	glFrontFace(GL_CCW);
	draw_top_rectangle();

	glDisable(GL_DEPTH_TEST);
	glStencilFunc(GL_EQUAL, 2, ~0);
	draw_square();
	get_backbuffer(&fb);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, 1, 1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, fb.width-1, 1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, fb.width-1, fb.height-1, gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&fb, 1, fb.height-1, gles_color_make_3f(1, 1, 1), gles_float_epsilon);


	/*
		6.	Set stencil op to ZERO, ZERO, ZERO, and try to update each face-siding.
			Expected result: Only the facing requested to change is changed.
	*/

	glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
	glStencilOpSeparate(GL_FRONT, GL_INCR, GL_ZERO, GL_ZERO);
	glGetIntegerv(GL_STENCIL_FAIL, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_INCR);
	glGetIntegerv(GL_STENCIL_BACK_FAIL, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_ZERO);

	glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
	glStencilOpSeparate(GL_BACK, GL_INCR, GL_ZERO, GL_ZERO);
	glGetIntegerv(GL_STENCIL_FAIL, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_ZERO);
	glGetIntegerv(GL_STENCIL_BACK_FAIL, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_INCR);

	glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
	glStencilOpSeparate(GL_FRONT, GL_ZERO, GL_INCR, GL_ZERO);
	glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_INCR);
	glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_ZERO);

	glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
	glStencilOpSeparate(GL_BACK, GL_ZERO, GL_INCR, GL_ZERO);
	glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_ZERO);
	glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_INCR);

	glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
	glStencilOpSeparate(GL_FRONT, GL_ZERO, GL_ZERO, GL_INCR);
	glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_INCR);
	glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_ZERO);

	glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
	glStencilOpSeparate(GL_BACK, GL_ZERO, GL_ZERO, GL_INCR);
	glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_ZERO);
	glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &state);
	ASSERT_GLENUMS_EQUAL(state, GL_INCR);

}



/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_glStencilOp(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glStencilOp", gles2_create_fixture, gles2_remove_fixture, results );

	add_test_with_flags(new_suite, "AST03-glStencilOp", test_glStencilOp,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);



	return new_suite;
}
