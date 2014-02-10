/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 *
 * glDrawElements - check that state, return values, error states, initial state are set correctly.
 * 				Verify functionality with simple geometry.
 *
 * Shaders used: pass_two_attribs.vert and white.frag
 *
 * CASE  #1 - point drawing
 * CASE  #2 - Line drawing
 * CASE  #3 - Line strip drawing
 * CASE  #4 - Line loop drawing
 * CASE  #5 - Triangles
 * CASE  #6 - Triangle Strip
 * CASE  #7 - Triangle Fan
 * CASE  #8 - Invalid input mode
 * CASE  #9 - Invalid input type
 * CASE  #10 - negative input count
 * CASE  #11 - indices is NULL
 */


#include "../gl2_framework.h"
#include "../gles_helpers.h"
#include <suite.h>

static void test_glDrawElements(suite* test_suite)
{
	BackbufferImage img;
	GLenum err;

	float xpixel = 2.0f / WIDTH;
	float ypixel = 2.0f / HEIGHT;

	float triangle_corners[] = {
		-1, -1, 0,
		 0, -1, 0,
		 0,  0, 0,
		-1,  0, 0
	};

	unsigned char corner_indices[] = { 0, 1, 2, 3 };
	unsigned short triangle_indices[] = { 0, 1, 2, 0, 2, 3 };
	unsigned short ts_indices[] = { 0, 1, 3, 2 };
	GLuint prog;
	GLint pointSizeLoc;

	float point_line_corners[12];

	point_line_corners[0]  = -1.f + 0.5f*xpixel;
    point_line_corners[1]  = -1.f + 0.5f*ypixel;
    point_line_corners[2]  = 0.f;
    point_line_corners[3]  = -1.f + 12.5f*xpixel;
    point_line_corners[4]  = -1.f + 0.5f*ypixel;
    point_line_corners[5]  = 0.f;
    point_line_corners[6]  = -1.f + 12.5f*xpixel;
    point_line_corners[7]  = -1.f + 12.5f*ypixel;
    point_line_corners[8]  = 0.f;
    point_line_corners[9]  = -1.f + 0.5f*xpixel;
    point_line_corners[10] = -1.f + 12.5f*ypixel;
    point_line_corners[11] = 0.f;

	if(WIDTH < 16 || HEIGHT < 16)
	{
		assert_warn(0, "Test need a resolution of at least 16x16 to funciton correctly");
		return;
	}

	prog = load_program("pass_two_attribs_point_size_custom.vert", "white.frag");
	pointSizeLoc = glGetUniformLocation(prog, "point_size");
	glUniform1f(pointSizeLoc, 1.0f);  /* fill 4 pixels */


/*	glClearColor(1,0,0,0); */
	glClear(GL_COLOR_BUFFER_BIT);


	/* setup for points and lines */
	{
		GLint pos;
		GLint proggy;
		glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
		pos = glGetAttribLocation(proggy, "position");
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, point_line_corners);
	}

	/* CASE 1: point drawing */
	{
		glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, corner_indices);

		get_backbuffer(&img);
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,  0,      gles_color_make_3f(1, 1, 1), gles_float_epsilon);
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12, 0,      gles_color_make_3f(1, 1, 1), gles_float_epsilon);
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12, 12,     gles_color_make_3f(1, 1, 1), gles_float_epsilon);
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,  12,     gles_color_make_3f(1, 1, 1), gles_float_epsilon);
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,  11,     gles_color_make_3f(0, 0, 0), gles_float_epsilon);
	}


	/* CASE 2: Line drawing */
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glLineWidth(2.0);
		glDrawElements(GL_LINES, 4, GL_UNSIGNED_BYTE, corner_indices);

		get_backbuffer(&img);
		/* bottom line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 1,   0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* after start-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 6,   0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* mid-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 11,  0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* before end-point */
		/* upper line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 11,  12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* after start-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 6,   12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* mid-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 1,   12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* before end-point */

		if(FSAA == 1)
		{
			/* last point in line should not be drawn. This only applies to non-FSAA-modes */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12,  0,            gles_color_make_3f(0, 0, 0), gles_float_epsilon); /* end-point shouldn't be drawn */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,   12, gles_color_make_3f(0, 0, 0), gles_float_epsilon); /* end-point shouldn't be drawn */

			/* first point in line should be white, but FSAA-modes will make this antialiased and thus greyish. */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,   0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12,  12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
		}


	}

	/* CASE 3: Line strip drawing */
	{
		glClear(GL_COLOR_BUFFER_BIT);

		glDrawElements(GL_LINE_STRIP, 4, GL_UNSIGNED_BYTE, corner_indices);

		get_backbuffer(&img);
		/* bottom line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 1,             0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* after start-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 6,   0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* mid-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 11, 0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* before end-point */
		/* right line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12,   6, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
		/* upper line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 11,   12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* after start-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 6,   12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* mid-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 1,             12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* before end-point */

		if(FSAA == 1)
		{
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,             0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12,   12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12,   0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* end-point shouldn't be drawn */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,             12, gles_color_make_3f(0, 0, 0), gles_float_epsilon); /* end-point shouldn't be drawn */
		}
	}


	/* CASE 4: Line loop drawing */
	{
		glClear(GL_COLOR_BUFFER_BIT);

		glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_BYTE, corner_indices);

		get_backbuffer(&img);
		/* bottom line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 1,             0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* after start-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 6,   0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* mid-point */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 11, 0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* before end-point */
		/* right line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12,   6,  gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
		/* top line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 6,   12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
		/* left line */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,             6, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
		/* center */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 6,   6, gles_color_make_3f(0, 0, 0), gles_float_epsilon); /* start-point */
		if(FSAA == 1)
		{
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,             0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* start-point */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12,   0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* end-point shouldn't be drawn */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 12,   12, gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* end-point shouldn't be drawn */
			ASSERT_GLES_FB_COLORS_RGB_EQUAL(&img, 0,             0,            gles_color_make_3f(1, 1, 1), gles_float_epsilon); /* end-point shouldn't be drawn */

		}
	}

	/* setup for triangles */
	{
		GLint pos;
		GLint proggy;
		glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
		pos = glGetAttribLocation(proggy, "position");
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, triangle_corners);
	}

	/* CASE 5: Triangles */
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, triangle_indices);

		get_backbuffer(&img);
		assert_pixel_white(&img, 0, 0, "C5 Corner 0 white");
		assert_pixel_white(&img, (WIDTH/2) -1, 0, "C5 Corner 1 white");
		assert_pixel_white(&img, (WIDTH/2) -1, ((HEIGHT/2) -1), "C5 Corner 2 white");
		assert_pixel_white(&img, 0, ((HEIGHT/2) -1), "C5 Corner 3 white");
		assert_pixel_white(&img, (WIDTH/3), (HEIGHT/2) -1, "C5 Lower line white");
		assert_pixel_white(&img, (WIDTH/3), 0, "C5 Upper line white");
		assert_pixel_white(&img, ((WIDTH/2) -1), (HEIGHT/3), "C5 Right line white");
		assert_pixel_white(&img, 0, (HEIGHT/3), "C5 Left line white");
		assert_pixel_white(&img, (WIDTH/3), 1, "C5 Random other point white");
		assert_pixel_white(&img, (WIDTH/3), (HEIGHT/3), "C5 Random other point white");
	}

	/* CASE 6: Triangle Strip */
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, ts_indices);

		get_backbuffer(&img);
		assert_pixel_white(&img, 0, 0, "C6 Corner 0 white");
		assert_pixel_white(&img, (WIDTH/2) -1, 0, "C6 Corner 1 white");
		assert_pixel_white(&img, (WIDTH/2) -1, ((HEIGHT/2) -1), "C6 Corner 2 white");
		assert_pixel_white(&img, 0, ((HEIGHT/2) -1), "C6 Corner 3 white");
		assert_pixel_white(&img, (WIDTH/3), (HEIGHT/2) -1, "C5 Lower line white");
		assert_pixel_white(&img, (WIDTH/3), 0, "C5 Upper line white");
		assert_pixel_white(&img, ((WIDTH/2) -1), (HEIGHT/3), "C5 Right line white");
		assert_pixel_white(&img, 0, (HEIGHT/3), "C5 Left line white");
		assert_pixel_white(&img, (WIDTH/3), 1, "C5 Random other point white");
		assert_pixel_white(&img, (WIDTH/3), (HEIGHT/3), "C5 Random other point white");
	}

	/* CASE 7: Triangle Fan */
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_BYTE, corner_indices);

		get_backbuffer(&img);
		assert_pixel_white(&img, 0, 0, "C7 Corner 0 white");
		assert_pixel_white(&img, (WIDTH/2) -1, 0, "C7 Corner 1 white");
		assert_pixel_white(&img, (WIDTH/2) -1, ((HEIGHT/2) -1), "C7 Corner 2 white");
		assert_pixel_white(&img, 0, ((HEIGHT/2) -1), "C7 Corner 3 white");
		assert_pixel_white(&img, (WIDTH/3), (HEIGHT/2) -1, "C5 Lower line white");
		assert_pixel_white(&img, (WIDTH/3), 0, "C5 Upper line white");
		assert_pixel_white(&img, ((WIDTH/2) -1), (HEIGHT/3), "C5 Right line white");
		assert_pixel_white(&img, 0, (HEIGHT/3), "C5 Left line white");
		assert_pixel_white(&img, (WIDTH/3), 1, "C5 Random other point white");
		assert_pixel_white(&img, (WIDTH/3), (HEIGHT/3), "C5 Random other point white");
	}


	/* CASE 8: Invalid input mode */
	{
		err = glGetError();
		ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);
		glDrawElements(GL_INVALID_ENUM, 4, GL_UNSIGNED_BYTE, corner_indices);
		err = glGetError();
		ASSERT_GLENUMS_EQUAL(err, GL_INVALID_ENUM);
	}

	/* CASE 9: Invalid input type */
	{
		glDrawElements(GL_TRIANGLE_FAN, 4, GL_INVALID_ENUM, corner_indices);
		err = glGetError();
		ASSERT_GLENUMS_EQUAL(err, GL_INVALID_ENUM);
	}

	/* CASE 10: Negative count */
	{
		glDrawElements(GL_TRIANGLE_FAN, -4, GL_UNSIGNED_BYTE, corner_indices);
		err = glGetError();
		ASSERT_GLENUMS_EQUAL(err, GL_INVALID_VALUE);
	}

#ifndef TESTBENCH_DESKTOP_GL
	/* CASE 11: NULL input, segfaults on all tested desktop implementations */
	{
		glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_BYTE, 0);
		err = glGetError();
		ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);
	}
#endif

}



/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 */

suite* create_suite_glDrawElements(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glDrawElements", gles2_create_fixture,
	                                gles2_remove_fixture, results );
	add_test_with_flags(new_suite, "AGE10-glDrawElements", test_glDrawElements,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}
