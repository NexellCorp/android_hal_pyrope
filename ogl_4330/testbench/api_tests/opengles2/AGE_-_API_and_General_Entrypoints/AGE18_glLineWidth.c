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
 * glLineWidth - Check that line width is set correctly.
 *
 * CASE #1: Verify that the correct error is generated on invalid input (<=0).
 * CASE #2: Verify that input is NOT clamped to maximum range.
 * CASE #3: Draw a horizontal line, verify that the line has the correct width.
 * CASE #4: dra a horizontal line that is wider than the systems maximum width, chech that drawing is clamped.
 */


#include "../gl2_framework.h"
#include "suite.h"
#include "test_helpers.h"
#include "../gles_helpers.h"

static void draw_horizontal_line(suite* test_suite, float pixeloffset);

static void test_glLineWidth(suite* test_suite)
{
	GLenum err;
	GLfloat lineWidthRange[2];
	GLfloat currentLineWidth;
	int acc=0;
	int i;
	GLfloat drawLineWidth = 4.0f;

	BackbufferImage backbuffer;

	GLuint prog;

	if(WIDTH < 16 || HEIGHT < 16)
	{
		assert_warn(0, "test need at least a resolution of 16x16 to work");
		return;
	}

	prog = load_program("pass_two_attribs.vert", "white.frag");
	glUseProgram(prog);

	/*get lineWidthRange*/
	glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, lineWidthRange);

	/* make sure we got no initial GL errors */
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "Initial GL error present");


	/********************************************/
	/* CASE 1: Verify that the correct error is generated on invalid input (<=0). */
	glLineWidth(-1.0f);
	err = glGetError();
	assert_ints_equal(err, GL_INVALID_VALUE, "Case 1: Negative line width did not generate GL_INVALID_VALUE");


	/* Try to set line width to zero */
	glLineWidth(0.0f);
	err = glGetError();
	assert_ints_equal(err, GL_INVALID_VALUE, "Case 1: Zero line width did not generate GL_INVALID_VALUE");


	/************************************************************************/
	/* CASE 2: Verify that input is NOT clamped to maximum range. */
	glLineWidth(lineWidthRange[1]+1.0f);
	err = glGetError();
	assert_ints_equal(err, GL_NO_ERROR, "Case 2: Line width above maximum generated an error");
	glGetFloatv(GL_LINE_WIDTH, &currentLineWidth);
	assert_floats_equal(currentLineWidth, lineWidthRange[1]+1.0f, 0.01f, "Case 2: Line width was clamped to maximum");


	/****************************************************/
	/* CASE 3: draw a horizontal line, check line width */
	glClear(GL_COLOR_BUFFER_BIT);
	glLineWidth(drawLineWidth);

		/* drawlinewidth is smaller than screen height. Draw entire line at center of screen */
		draw_horizontal_line(test_suite, (float)(HEIGHT/2 + (HEIGHT % 2)));

		/*verify the result*/
		get_backbuffer(&backbuffer);
		acc = 0;
		for (i=0; i<backbuffer.height; i++)
		{
			int x = 8;
			int y = i;
			struct gles_color c = gles_fb_get_pixel_color(&backbuffer, x, y);
			if (!gles_colors_rgb_equal(c, gles_color_make_3f(0, 0, 0), gles_float_epsilon))
			{
				acc++;
			}
		}

		assert_ints_equal(acc, drawLineWidth, "Case 3: wrong line width");

	/********************************************************************************************/
	/* case 4, drawing a horizontal line that is wider than the systems maximum supported with. */
	/* Expected output: a horizontal line with width that equals the systems maximum line width */

	/*set the line width bigger than the maximum supported*/
	if(lineWidthRange[1] < HEIGHT-2)
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glLineWidth(lineWidthRange[1] + 1);

		/* draw a horizontal line */
		draw_horizontal_line(test_suite, (float)(HEIGHT/2 + (HEIGHT % 2)));

		/*verify the result */
		get_backbuffer(&backbuffer);
		acc = 0;
		for (i=0; i<backbuffer.height; i++)
		{
			int x = WIDTH / 2;
			int y = i;
			struct gles_color c = gles_fb_get_pixel_color(&backbuffer, x, y);
			if (!gles_colors_rgb_equal(c, gles_color_make_3f(0, 0, 0), gles_float_epsilon))
			{
				acc++;
			}
		}

		{
			int max_w = (int)lineWidthRange[1];
			if (HEIGHT < max_w) max_w = HEIGHT;

			assert_ints_equal(acc, max_w, "Case 4: Line width was not clamped correctly");
		}
	}
}


suite* create_suite_glLineWidth(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glLineWidth", gles2_create_fixture,
	                                gles2_remove_fixture, results );
	add_test_with_flags(new_suite, "AGE18-glLineWidth", test_glLineWidth,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}

static void draw_horizontal_line(suite* test_suite, float pixeloffset)
{
	/* draw a horisontal line 4 pixels down */
	GLfloat vertices[6];

	vertices[0] = -1.0f;
    vertices[1] = -1.0f + (2.0f*pixeloffset) / HEIGHT;
    vertices[2] = 0.5f;
    vertices[3] = 1.0f;
    vertices[4] = -1.0f + (2.0f*pixeloffset) / HEIGHT;
    vertices[5] = 0.5f;

	{
		GLint pos;
		GLint prg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prg);
		pos = glGetAttribLocation(prg, "position");
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	}
	glDrawArrays(GL_LINES, 0, 2);
	{
		GLint pos;
		GLint prg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prg);
		pos = glGetAttribLocation(prg, "position");
		glDisableVertexAttribArray(pos);
	}
}
