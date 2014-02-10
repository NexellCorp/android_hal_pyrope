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
 * test_glBufferData - Tests error handling for glBufferData and that we can acctually render using it.
 *
 * CASE #01 - Create and bind a buffer object, call function with target set to GL_INVALID_ENUM and other parameters valid.
 *            Result: Should give GL_INVALID_ENUM error, and GL_BUFFER_SIZE should be 0
 *
 * CASE #02 - Create and bind a buffer object, call function with negative size and other parameters valid.
 *            Result: Should give GL_INVALID_VALUE error, and GL_BUFFER_SIZE should be 0
 *
 * CASE #03 - Create and bind a buffer object, call function with usage set to GL_INVALID_ENUM and other parameters valid.
 *            Result: Should give GL_INVALID_ENUM error, and GL_BUFFER_SIZE should be 0
 *
 * CASE #04 - Create and bind a buffer object, call function with data pointer set to NULL, size set to 8, and usage set to GL_STATIC_DRAW
 *            Result: Should give no errors, GL_BUFFER_SIZE should be 8 and GL_BUFFER_USAGE should be GL_STATIC_DRAW
 *
 * CASE #05 - Create and bind a buffer object set to GL_ARRAY_BUFFER, call glBufferData with data pointer 'vertices',
 *            size '8' and usage set to GL_STATIC_DRAW. Draw a quad using this vertex buffer object as position.
 *            Result: A white quad should be drawn from <WIDTH/2, HEIGHT/2> to <3*(WIDTH/4), 3*(HEIGHT/4)>
 *
 * CASE #06 - Create and bind a buffer object set to GL_ELEMENT_ARRAY_BUFFER, call function with data pointer set to 'indices',
 *            size set to '6' and usage set to GL_STATIC_DRAW. Draw two indexed triangles using the 'vertices' array as position
 *            vertex attrib array, and the buffer object as indices.
 *            Result: A white quad should be drawn from <WIDTH/2, HEIGHT/2> to <3*(WIDTH/4), 3*(HEIGHT/4)>
 *
 * CASE #07 - Create and bind a buffer object to GL_ARRAY_BUFFER and call glBufferData with data pointer 'vertices',
 *            size '6' and usage set to GL_STATIC_DRAW. Try to draw a two triangels using glDrawArrays with the vertex buffer
 *            object as position
 *            Result: A white triangle should be drawn with the vertices (0,0),(1,0),(1,1). Note that specifically a quad
 *            should _not_ be drawn as the driver should truncate the number of vertices draw from 6 to 3 as there are
 *            only six available vertices in the buffer object
 *
 * CASE #08 - Create and bind a buffer object set to GL_ARRAY_BUFFER and call glBufferData with data pointer 'vertices',
 *            size '1' and usage set to GL_STATIC_DRAW. Try to draw a triangle using glDrawArrays with the vertex buffer
 *            object as position.
 *            Result: No triangles should be drawn as there are too few vertices and the program should not crash.
 *
 * CASE #09 - Create and bind a buffer object set to GL_ARRAY_BUFFER and call glBufferData with data pointer 'vertices',
 *            size '2' and usage set to GL_STATIC_DRAW. Try to draw two points using glDrawArrays with the vertex buffer
 *            object as position.
 *            Result: Since there are only enough coordinate data for one point, only one should be displayed.
 *
 * CASE #10 - Create and bind a buffer object set to GL_ARRAY_BUFFER and call glBufferData with data pointer 'vertices',
 *            size '6' and usage set to GL_STATIC_DRAW. Try to draw a two triangles (6 indices) using glDrawElements
 *            with the vertex buffer object as position and the indices list as indices.
 *            Result: The draw call should be discarded and nothing should have been drawn
*/

#include "../gl2_framework.h"
#include <suite.h>

static void test_glBufferData(suite* test_suite)
{
	GLuint buffer, buffer2;
	signed char vertices[] =
	{
		0, 0,
		1, 0,
		1, 1,
		0, 1
	};
	signed char vertices2[] =
	{
		0, 0,
		1, 1,
		0, 1,
		0, 0,
		1, 0,
		1, 1
	};
	unsigned char indices[] =
	{
		0, 1,
		2, 0,
		2, 3
	};
	GLenum error;
	GLint data;
	GLuint program;
	GLuint pointsize_program;
	GLint pos;
	BackbufferImage backbuffer;

	/*
	* CASE #01 - Create and bind a buffer object, call function with target set to GL_INVALID_ENUM and other parameters valid.
	*/

	/* Generate buffer */
	glGenBuffers( 1, &buffer );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Bind buffer */
	glBindBuffer( GL_ARRAY_BUFFER, buffer );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Pass GL_INVALID_ENUM as target to glBufferData */
	glBufferData( GL_INVALID_ENUM, 3, vertices, GL_STATIC_DRAW );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_INVALID_ENUM );

	/* Get the size of the buffer and assert that it is zero */
	glGetBufferParameteriv( GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &data );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );
	ASSERT_INTS_EQUAL( data, 0 );

	/*
	* CASE #02 - Create and bind a buffer object, call function with negative size and other parameters valid.
	*            Note: Using buffer generated and bound in CASE #01
	*/

	/* Pass negative number as size to glBufferData */
	glBufferData( GL_ARRAY_BUFFER, -3, vertices, GL_STATIC_DRAW );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_INVALID_VALUE );

	/* Get the size of the buffer and assert that it is zero */
	data = 1;
	glGetBufferParameteriv( GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &data );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );
	ASSERT_INTS_EQUAL( data, 0 );

	/*
	* CASE #03 - Create and bind a buffer object, call function with usage set to GL_INVALID_ENUM and other parameters valid.
	*            Note: Using buffer generated and bound in CASE #01
	*/

	/* Pass GL_INVALID_ENUM as usage to glBufferData */
	glBufferData( GL_ARRAY_BUFFER, 3, vertices, GL_INVALID_ENUM );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_INVALID_ENUM );

	/* Get the size of the buffer and assert that it is zero */
	data = 1;
	glGetBufferParameteriv( GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &data );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );
	ASSERT_INTS_EQUAL( data, 0 );

	/*
	* CASE #04 - Create and bind a buffer object, call function with data pointer set to NULL, size set to 8, and usage set to GL_STATIC_DRAW
	*            Note: Using buffer generated and bound in CASE #01
	*/

	/* Pass data pointer NULL, size 8 and usage GL_STATIC_DRAW to glBufferData */
	glBufferData( GL_ARRAY_BUFFER, 8, NULL, GL_STATIC_DRAW );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Get the size of the buffer and assert that it is zero */
	data = 1;
	glGetBufferParameteriv( GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &data );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );
	ASSERT_INTS_EQUAL( data, 8 );

	/*
	* CASE #05 - Create and bind a buffer object set to 'GL_ARRAY_BUFFER', call function with data pointer 'vertices',
	*            size '8' and usage set to 'GL_STATIC_DRAW'. Draw a quad using this vertex buffer object as position.
	*            Note: Using buffer generated and bound in CASE #01
	*/

	/* Load appropriate shader */
	program = load_program( "ABO05_ABO06_scaledown.vert", "white.frag");
	pos = glGetAttribLocation( program, "position" );
	glEnableVertexAttribArray( pos );

	/* Create and initialize the data store */
	glBufferData( GL_ARRAY_BUFFER, 8, vertices, GL_STATIC_DRAW );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	glVertexAttribPointer( pos, 2, GL_BYTE, GL_FALSE, 0, NULL );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Draw quad */
	glClear( GL_COLOR_BUFFER_BIT );
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);

	/* Get the backbuffer and check that the corners of the square are white. */
	get_backbuffer( &backbuffer );
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, WIDTH/2+1, HEIGHT/2+1, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, WIDTH/2+1, 3*(HEIGHT/4)-2, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, 3*(WIDTH/4)-2, 3*(HEIGHT/4)-2, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, 3*(WIDTH/4)-2, HEIGHT/2+1, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);
	/*
	* CASE #06 - Create and bind a buffer object set to 'GL_ELEMENT_ARRAY_BUFFER', call function with data pointer set to 'indices',
	*            size set to '6' and usage set to 'GL_STATIC_DRAW'. Draw two indexed triangles using the 'vertices' array as position
	*            vertex attrib array, and the buffer object as indices.
	*/

	/* Unbind buffer */
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Generate buffer */
	glGenBuffers( 1, &buffer2 );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	glVertexAttribPointer( pos, 2, GL_BYTE, GL_FALSE, 0, vertices );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Bind buffer */
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buffer2 );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Pass GL_INVALID_ENUM as target to glBufferData */
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, 6, indices, GL_STATIC_DRAW );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Clear screen and draw two white triangles */
	glClear( GL_COLOR_BUFFER_BIT );
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);

	/* Get the backbuffer and check that the corners of the square are white. */
	get_backbuffer( &backbuffer );
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, WIDTH/2+1, HEIGHT/2+1, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, WIDTH/2+1, 3*(HEIGHT/4)-2, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, 3*(WIDTH/4)-2, 3*(HEIGHT/4)-2, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, 3*(WIDTH/4)-2, HEIGHT/2+1, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);


#if 0 
	/* CASE #07 - Create and bind a buffer object set to 'GL_ARRAY_BUFFER', call function with data pointer 'vertices',
	 *            size '6' and usage set to 'GL_STATIC_DRAW'. Try to draw a quad using this vertex buffer object as position
	 *            and all six indices of indices.
	 *            Result: A white triangle should be drawn with the vertices (0,0),(1,0),(1,1). Note that specifically a quad
	 *            should _not_ be drawn as the driver should truncate the number of vertices drawn from 6 to 3 as there are
	 *            only six available vertices in the buffer object
	 */
	/* Bind buffer */
	glBindBuffer( GL_ARRAY_BUFFER, buffer );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Create and initialize the data store. size = 6 bytes or 3 vertexes of 2 bytes each */
	glBufferData( GL_ARRAY_BUFFER, 6, vertices2, GL_STATIC_DRAW );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	glVertexAttribPointer( pos, 2, GL_BYTE, GL_FALSE, 0, NULL );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Draw quad */
	glClear( GL_COLOR_BUFFER_BIT );
	glDrawArrays(GL_TRIANGLES, 0, 6);

	/* Get the backbuffer and check that just one triangle was drawn */
	get_backbuffer( &backbuffer );
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, WIDTH/2+1, 3*(HEIGHT/4)-2, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);

	/* This is inside the triangle that should have been discarded */
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, 3*(WIDTH/4)-2, HEIGHT/2+1, gles_color_make_4b(0, 0, 0, 0), gles_float_epsilon);

    /* case disabled, not defined behavior */
	/*
	 * CASE #08 - Create and bind a buffer object set to GL_ARRAY_BUFFER, call glBufferData with data pointer 'vertices',
	 *            size '1' and usage set to GL_STATIC_DRAW. Try to draw a triangle using glDrawArrays with the vertex buffer
	 *            object as position.
	 *            Result: No triangles should be drawn as there are too few vertices and the program should not crash.
	 */

	/* Bind buffer */
	glBindBuffer( GL_ARRAY_BUFFER, buffer );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Create and initialize the data store. size = 6 bytes or 3 vertexes of 2 bytes each */
	glBufferData( GL_ARRAY_BUFFER, 1, vertices2, GL_STATIC_DRAW );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	glVertexAttribPointer( pos, 2, GL_BYTE, GL_FALSE, 0, NULL );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Draw a triangle */
	glClear( GL_COLOR_BUFFER_BIT );
	glDrawArrays(GL_TRIANGLES, 0, 3);

	/* Should not crash or fail */
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Get the backbuffer and check that no triangles were drawn */
	get_backbuffer( &backbuffer );

	/* This would have the location of the triangle if enough vertex data had been uploaded to the VBO */
	ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, WIDTH/2+1, 3*(HEIGHT/4)-2, gles_color_make_4b(0, 0, 0, 0), gles_float_epsilon);
#endif

	/*
	 * CASE #09 - Create and bind a buffer object set to GL_ARRAY_BUFFER and call glBufferData with data pointer 'vertices',
	 *            size '2' and usage set to GL_STATIC_DRAW. Try to draw two points using glDrawArrays with the vertex buffer
	 *            object as position.
	 *            Result: Since there are only enough coordinate data for one point, only one should be displayed.
	 */

	if(WIDTH >= 32 && HEIGHT >= 32) 
	{

		pointsize_program = load_program( "pass_two_attribs_point_size_10.vert", "white.frag");
		pos = glGetAttribLocation( pointsize_program, "position" );
		error = glGetError();
		ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

		/* Bind buffer */
		glBindBuffer( GL_ARRAY_BUFFER, buffer );
		error = glGetError();
		ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

		/* Create and initialize the data store. size = 6 bytes or 3 vertexes of 2 bytes each */
		glBufferData( GL_ARRAY_BUFFER, 2, vertices2, GL_STATIC_DRAW );
		error = glGetError();
		ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

		glVertexAttribPointer( pos, 2, GL_BYTE, GL_FALSE, 0, NULL );
		error = glGetError();
		ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );
	
		/* Draw a triangle */
		glClear( GL_COLOR_BUFFER_BIT );
		glDrawArrays(GL_POINTS, 0, 2);

		/* Should not crash or fail */
		error = glGetError();
		ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

		/* Get the backbuffer and check that no triangles were drawn */
		get_backbuffer( &backbuffer );

		/* The first pixel should appear */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, WIDTH/2, HEIGHT/2-1, gles_color_make_4b(255, 255, 255, 255), gles_float_epsilon);

		/* The second pixel is outside the range that was uploaded to glBufferData and should not appear */
		ASSERT_GLES_FB_COLORS_RGB_EQUAL(&backbuffer, WIDTH*3/4, HEIGHT*3/4-1, gles_color_make_4b(0, 0, 0, 0), gles_float_epsilon);
	
		glDeleteProgram(pointsize_program);
	}

}

/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_glBufferData(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glBufferData", gles2_create_fixture, gles2_remove_fixture, results );

	add_test_with_flags(new_suite, "ABO05-glBufferData", test_glBufferData,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}

