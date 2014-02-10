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
 * glTexImage2D - verify cube map texturing
 *
 * Shaders used: pass_two_attribs.vert and cube_texture.frag
 *
 * Conditions: A 2x2 cubemap with:{ red, green, blue, purple } uploaded to all faces
 *
 * Draw 2x3 quads with the following texture coordinates (specified in the order bottom left, top left, top right, bottom right):
 *
 * CASE  #1 - <2,-1,-1>, <2,1,-1>, <2,1,1>, <2,-1,1>
 * CASE  #2 - <-2,-1,-1>, <-2,1,-1>, <-2,1,1>, <-2,-1,1>
 * CASE  #3 - <-1,2,-1>, <1,2,-1>, <1,2,1>, <-1,2,1>
 * CASE  #4 - <-1,-2,-1>, <1,-2,-1>, <1,-2,1>, <-1,-2,1>
 * CASE  #5 - <-1,-1,2>-, <1,-1,2>, <1,1,2>, <-1,1,2>
 * CASE  #6 - <-1,-1,-2>-, <1,-1,-2>, <1,1,-2>, <-1,1,-2>
 *
 */

#include "../gl2_framework.h"
#include <suite.h>



static void drawQuadCubeMapTextured(GLfloat* verts,  GLfloat* texcoords, GLuint tex);

static void test_glTexImage2D4(suite* test_suite)
{
	GLuint prog;
	GLuint cubeMap = 0;
	int i=0;
	BackbufferImage backbuffer;

	/* color data for cubemap, red, green, blue, purple */
	#define RED    255, 0, 0, 255
	#define GREEN  0, 255, 0, 255
	#define BLUE   0, 0, 255, 255
	#define PURPLE 139, 0, 255, 255

	static GLubyte cubeMapData[16] =
	{
		RED,  GREEN,
		BLUE, PURPLE
	};

	/* vertex data for a 2x3px square in the lower left corner of the screen */
	GLfloat verts[3*4];
    GLfloat* texcoords[6];

	/* Six different sets of texture coordinates*/
	GLfloat tc0[12] = {  2, -1, -1,  2,  1, -1,  2,  1,  1,  2, -1,  1 };
	GLfloat tc1[12] = {  -2, -1, -1, -2, 1, -1, -2, 1,  1, -2, -1,  1 };
	GLfloat tc2[12] = { -1,  2, -1, 1,  2, -1,  1,  2,  1, -1,  2,  1 };
	GLfloat tc3[12] = { -1, -2, -1,  1, -2, -1,  1, -2,  1, -1, -2,  1 };
	GLfloat tc4[12] = { -1, -1,  2,  1, -1,  2,  1,  1,  2, -1,  1,  2 };
	GLfloat tc5[12] = { -1, -1, -2,  1, -1, -2,  1,  1, -2, -1,  1, -2 };

	/* vertex data for a 2x3px square in the lower left corner of the screen */
    verts[0]  = -1.0f;
    verts[1]  = -1.0f;
    verts[2]  =  0.0f;                  /* bottom left */
	verts[3]  = -1.0f;
    verts[4]  = -1.0f+2.0f*(2.0f/HEIGHT);
    verts[5]  =  0.0f;	                /* top left */
	verts[6]  = -1.0f+2.0f*(2.0f/WIDTH);
    verts[7]  = -1.0f+2.0f*(2.0f/HEIGHT);
    verts[8]  =  0.0f;                  /* top right */
	verts[9]  = -1.0f+2.0f*(2.0f/WIDTH);
    verts[10] = -1.0f;
    verts[11] =  0.0f;		            /* bottom right */

	texcoords[0] = tc0;
    texcoords[1] = tc1;
    texcoords[2] = tc2;
    texcoords[3] = tc3;
    texcoords[4] = tc4;
    texcoords[5] = tc5;

	prog = load_program("pass_two_attribs.vert", "cube_texture.frag");
	glUseProgram(prog);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	/* upload a 2x2 cubemap */
	glGenTextures(1, &cubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, cubeMapData);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, cubeMapData);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, cubeMapData);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, cubeMapData);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, cubeMapData);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, cubeMapData);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	/* Loop through all sets of texture coordinates */
	for (i=0; i<6; i++)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		drawQuadCubeMapTextured(verts, texcoords[i], cubeMap);
		get_backbuffer(&backbuffer);

		/* verify that the correct texture data is used */
		switch (i)
		{
		case 0:
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 1, gles_color_make_4b( GREEN ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 1, gles_color_make_4b( RED ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( PURPLE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 0, gles_color_make_4b( BLUE ), gles_float_epsilon);
			break;
		case 1:
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 1, gles_color_make_4b( RED ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 1, gles_color_make_4b( GREEN ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( BLUE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 0, gles_color_make_4b( PURPLE ), gles_float_epsilon);
			break;
		case 2:
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 1, gles_color_make_4b( GREEN ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 1, gles_color_make_4b( PURPLE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( RED ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 0, gles_color_make_4b( BLUE ), gles_float_epsilon);
			break;
		case 3:
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 1, gles_color_make_4b( PURPLE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 1, gles_color_make_4b( GREEN ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( BLUE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 0, gles_color_make_4b( RED ), gles_float_epsilon);
			break;
		case 4:
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 1, gles_color_make_4b( PURPLE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 1, gles_color_make_4b( GREEN ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( BLUE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 0, gles_color_make_4b( RED ), gles_float_epsilon);
			break;
		case 5:
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 1, gles_color_make_4b( BLUE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 1, gles_color_make_4b( RED ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( PURPLE ), gles_float_epsilon);
			ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 1, 0, gles_color_make_4b( GREEN ), gles_float_epsilon);
			break;
		}
	}

}


static void drawQuadCubeMapTextured(GLfloat* verts,  GLfloat* texcoords, GLuint tex)
{
	GLushort indices[] =
	{
		0, 1, 2, /* first triangle */
		0, 2, 3  /* second triangle */
	};

	GLuint prog;
	GLint loc;
	glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)(&prog));


	loc = glGetAttribLocation(prog, "position");
	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, verts);

	loc = glGetAttribLocation(prog, "in_tex0");
	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, texcoords);

	loc = glGetUniformLocation(prog, "cube");
	glUniform1i(loc, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}


/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_glTexImage2D4(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glTexImage2D4", gles2_create_fixture, gles2_remove_fixture, results );

	add_test_with_flags(new_suite, "ATX17-glTexImage2D", test_glTexImage2D4,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}

