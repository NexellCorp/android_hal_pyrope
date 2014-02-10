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
 * glTexImage2D -
 *
 * CASE #1 - Upload a cube-map with 2x2 solid colored faces and mipmaps (of the same color). Positive x should be red, negative x should be green,
 *           positive y should be blue, negative y should be purple, positive z should be yellow, negative z should be gray.
 *			 Result: No GL-error
 *
 * CASE #2-7 - Turn on mipmapping. Render 1x1 quads in the bottom left corner of the screen with constant texture coordinates of:
 *			   2. <1,0,0>
 *			   3. <-1,0,0>
 *			   4. <0,1,0>
 *			   5. <0,-1,0>
 *			   6. <0,0,1>
 *			   7. <0,0,-1>
 *			   Result #2: red
 *			   Result #3: green
 *			   Result #4: blue
 *             Result #5: purple
 *             Result #6: yellow
 *             Result #7: gray
 */

#include "../gl2_framework.h"
#include <suite.h>

static void drawQuadCubeMapTextured( suite* test_suite, GLfloat tx, GLfloat ty, GLfloat tz, GLint tex )
{

	GLushort indices[] =
	{
		0, 1, 2, /* first triangle */
		0, 2, 3  /* second triangle */
	};

	GLfloat verts[12];
	GLfloat texcoords[12];

	GLint prog;
	GLint loc;

	verts[0]  = -1.0f;
	verts[1]  = -1.0f;
	verts[2]  =  0.0f;
	verts[3]  = -1.0f;
	verts[4]  = -1.0f+(2.0f/HEIGHT);
	verts[5]  =  0.0f;
	verts[6]  = -1.0f+(2.0f/WIDTH);
	verts[7]  = -1.0f+(2.0f/HEIGHT);
	verts[8]  =  0.0f;
	verts[9]  = -1.0f+(2.0f/WIDTH);
	verts[10] = -1.0f;
	verts[11] =  0.0f;

	texcoords[0]  = tx;
	texcoords[1]  = ty;
	texcoords[2]  = tz;
	texcoords[3]  = tx;
	texcoords[4]  = ty;
	texcoords[5]  = tz;
	texcoords[6]  = tx;
	texcoords[7]  = ty;
	texcoords[8]  = tz;
	texcoords[9]  = tx;
	texcoords[10] = ty;
	texcoords[11] = tz;

	glGetIntegerv(GL_CURRENT_PROGRAM, &prog);

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

static void test_resolve_constraint_cube_non_addresable_level(suite* test_suite)
{
	const u32 cube_edge_dim = 1024;
	const u32 tex_bpp = 4;
	const u32 tex_pitch = cube_edge_dim*tex_bpp; 

	u8* red = malloc( cube_edge_dim*cube_edge_dim*tex_bpp );
	u8* green = malloc( cube_edge_dim*cube_edge_dim*tex_bpp );
	u8* blue = malloc( cube_edge_dim*cube_edge_dim*tex_bpp );
	u8* purple = malloc( cube_edge_dim*cube_edge_dim*tex_bpp );
	u8* yellow = malloc( cube_edge_dim*cube_edge_dim*tex_bpp );
	u8* gray = malloc( cube_edge_dim*cube_edge_dim*tex_bpp );

	GLubyte* colors[6];

	u32 j,i;

	GLfloat x[6] = { 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	GLfloat y[6] = { 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f };
	GLfloat z[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f };

	GLint cubemap;
	GLuint prog;
	BackbufferImage backbuffer;

	for(j = 0; j < cube_edge_dim ; ++j )
	{
		for(i = 0; i < cube_edge_dim ; ++i )
		{
			u32* texel = (u32*) ( red+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0xFF0000FF;
			texel = (u32*) (green+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0x00FF00FF;
			texel = (u32*) (blue+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0x0000FFFF;
			texel = (u32*) (purple+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0x8B00FFFF;
			texel = (u32*) (yellow+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0xFFFF00FF;
			texel = (u32*) (gray+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0x808080FF;
		}
	}

	colors[0] = red;
    	colors[1] = green;
    	colors[2] = blue;
    	colors[3] = purple;
    	colors[4] = yellow;
    	colors[5] = gray;

	prog = load_program("pass_two_attribs.vert", "cube_texture.frag");
	glUseProgram(prog);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	glGenTextures( 1, (GLuint*)&cubemap );
	glBindTexture( GL_TEXTURE_CUBE_MAP, cubemap );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Upload a wrong level 0 */
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, cube_edge_dim, cube_edge_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, cube_edge_dim, cube_edge_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, cube_edge_dim, cube_edge_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, cube_edge_dim, cube_edge_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, cube_edge_dim, cube_edge_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, cube_edge_dim, cube_edge_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	{
		u32 levelw = cube_edge_dim>>1;
		j=1;
		while (levelw!=0)
		{
			glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, j-1, GL_RGBA, levelw<<1, levelw<<1, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

			glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, j, GL_RGBA, levelw, levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, j, GL_RGBA, levelw, levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, j, GL_RGBA, levelw, levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, j, GL_RGBA, levelw, levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, j, GL_RGBA, levelw, levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, j, GL_RGBA, levelw, levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			++j;

			levelw >>= 1;
		} 
	}

	/* Enable mipmapping */
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	/* Redraw 1x1 quad at [0,0] and ensure that it's the right color */
	for (i=0; i<6; i++)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		drawQuadCubeMapTextured( test_suite, x[i], y[i], z[i], cubemap);
		get_backbuffer(&backbuffer);
		ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( (colors[i])[0], (colors[i])[1], (colors[i])[2], (colors[i])[3] ), gles_float_epsilon);
	}

	free(red);
	free(green);
	free(blue);
	free(purple);
	free(yellow);
	free(gray);
}

static void test_resolve_constraint_cube_face_wrong_size_reallocate(suite* test_suite)
{
	const u32 tex_width  = 512;
	const u32 tex_height = 512;
	const u32 tex_bpp = 4;
	const u32 tex_pitch = tex_width*tex_bpp; 

	u8 red[tex_width*tex_height*tex_bpp];
	u8 green[tex_width*tex_height*tex_bpp];
	u8 blue[tex_width*tex_height*tex_bpp];
	u8 purple[tex_width*tex_height*tex_bpp];
	u8 yellow[tex_width*tex_height*tex_bpp];
	u8 gray[tex_width*tex_height*tex_bpp];

	GLubyte* colors[6];

	u32 j,i;

	GLfloat x[6] = { 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	GLfloat y[6] = { 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f };
	GLfloat z[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f };

	GLint cubemap;
	GLuint prog;
	BackbufferImage backbuffer;


	for(j = 0; j < tex_height ; ++j )
	{
		for(i = 0; i < tex_width ; ++i )
		{
			u32* texel = (u32*) ( red+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0xFF0000FF;
			texel = (u32*) (green+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0x00FF00FF;
			texel = (u32*) (blue+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0x0000FFFF;
			texel = (u32*) (purple+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0x8B00FFFF;
			texel = (u32*) (yellow+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0xFFFF00FF;
			texel = (u32*) (gray+(j*tex_pitch)+(i*tex_bpp) );
			*texel = 0x808080FF;


		}
	}

	colors[0] = red;
    	colors[1] = green;
    	colors[2] = blue;
    	colors[3] = purple;
    	colors[4] = yellow;
    	colors[5] = gray;

	prog = load_program("pass_two_attribs.vert", "cube_texture.frag");
	glUseProgram(prog);
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	glGenTextures( 1, (GLuint*)&cubemap );
	glBindTexture( GL_TEXTURE_CUBE_MAP, cubemap );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Level 0 */

	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	{
		u32 levelw = tex_width;
		j=1;
		do 
		{
			levelw >>= 1;

			glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, j, GL_RGBA, levelw<<1, levelw <<1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);


 
			glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, j, GL_RGBA, levelw , levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, j, GL_RGBA, levelw , levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, j, GL_RGBA, levelw , levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, j, GL_RGBA, levelw , levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, j, GL_RGBA, levelw , levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, j, GL_RGBA, levelw , levelw, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
			ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
			++j;
		} while (levelw!=0);
	}

	/* Enable mipmapping */
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	/* Redraw 1x1 quad at [0,0] and ensure that it's the right color */
	for (i=0; i<6; i++)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		drawQuadCubeMapTextured( test_suite, x[i], y[i], z[i], cubemap);
		get_backbuffer(&backbuffer);
		ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( (colors[i])[0], (colors[i])[1], (colors[i])[2], (colors[i])[3] ), gles_float_epsilon);
	}


}


static void test_resolve_constraint_cube_face_wrong_size(suite* test_suite)
{
	/* Textures used on the cube map */
	GLubyte red[16] = { 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255 };
	GLubyte green[16] = { 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255 };
	GLubyte blue[16] = { 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255 };
	GLubyte purple[16] = { 139, 0, 255, 255, 139, 0, 255, 255, 139, 0, 255, 255, 139, 0, 255, 255 };
	GLubyte yellow[16] = { 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255 };
	GLubyte gray[16] = { 128, 128, 128, 255, 128, 128, 128, 255 , 128, 128, 128, 255 , 128, 128, 128, 255 };
    	GLubyte *colors[6];


	/* Coordinates used in case #2 */
	GLfloat x[6] = { 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	GLfloat y[6] = { 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f };
	GLfloat z[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f };

	int i=0;
	GLint cubemap;
	GLuint prog;
	BackbufferImage backbuffer;

	colors[0] = red;
    	colors[1] = green;
    	colors[2] = blue;
    	colors[3] = purple;
    	colors[4] = yellow;
    	colors[5] = gray;

	prog = load_program("pass_two_attribs.vert", "cube_texture.frag");
	glUseProgram(prog);

	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	
	glGenTextures( 1, (GLuint*)&cubemap );
	glBindTexture( GL_TEXTURE_CUBE_MAP, cubemap );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Level 1 */
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Level 0 */
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);


	/* Overwrite lev 1 faces with the new ones with the correct size */
	
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	

	/* Enable mipmapping */
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	/* Redraw 1x1 quad at [0,0] and ensure that it's the right color */
	for (i=0; i<6; i++)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		drawQuadCubeMapTextured( test_suite, x[i], y[i], z[i], cubemap);
		get_backbuffer(&backbuffer);
		ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( (colors[i])[0], (colors[i])[1], (colors[i])[2], (colors[i])[3] ), gles_float_epsilon);
	}


}
	

static void test_constraint_resolve_reset_flag(suite* test_suite)
{
	/* Textures used on the cube map */
	GLubyte red[16] = { 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255 };
	GLubyte green[16] = { 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255 };
	GLubyte blue[16] = { 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255 };
	GLubyte purple[16] = { 139, 0, 255, 255, 139, 0, 255, 255, 139, 0, 255, 255, 139, 0, 255, 255 };
	GLubyte yellow[16] = { 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255 };
	GLubyte gray[16] = { 128, 128, 128, 255, 128, 128, 128, 255 , 128, 128, 128, 255 , 128, 128, 128, 255 };
    	GLubyte *colors[6];


	/* Coordinates used in case #2 */
	GLfloat x[6] = { 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	GLfloat y[6] = { 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f };
	GLfloat z[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f };

	int i=0;
	GLint cubemap;
	GLuint prog;
	BackbufferImage backbuffer;

	colors[0] = red;
    	colors[1] = green;
    	colors[2] = blue;
    	colors[3] = purple;
    	colors[4] = yellow;
    	colors[5] = gray;

	prog = load_program("pass_two_attribs.vert", "cube_texture.frag");
	glUseProgram(prog);

	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	
	glGenTextures( 1, (GLuint*)&cubemap );
	glBindTexture( GL_TEXTURE_CUBE_MAP, cubemap );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Level 0 */
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Level 1 */
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Overwrite lev 1 faces with the new ones with the correct size */
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Enable mipmapping */
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	/* Redraw 1x1 quad at [0,0] and ensure that it's the right color */
	for (i=0; i<6; i++)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		drawQuadCubeMapTextured( test_suite, x[i], y[i], z[i], cubemap);
		get_backbuffer(&backbuffer);
		ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( (colors[i])[0], (colors[i])[1], (colors[i])[2], (colors[i])[3] ), gles_float_epsilon);
	}


}


static void test_glTexImage2D3(suite* test_suite)
{
	/*
	* Definitions
	*/

	/* Textures used on the cube map */
	GLubyte red[16] = { 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255 };
	GLubyte green[16] = { 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255 };
	GLubyte blue[16] = { 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255 };
	GLubyte purple[16] = { 139, 0, 255, 255, 139, 0, 255, 255, 139, 0, 255, 255, 139, 0, 255, 255 };
	GLubyte yellow[16] = { 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255 };
	GLubyte gray[16] = { 128, 128, 128, 255, 128, 128, 128, 255 , 128, 128, 128, 255 , 128, 128, 128, 255 };
	GLubyte *colors[6];


	/* Coordinates used in case #2 */
	GLfloat x[6] = { 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	GLfloat y[6] = { 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f };
	GLfloat z[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f };

	int i=0;
	GLint cubemap;
	GLuint prog;
	BackbufferImage backbuffer;
	GLenum err;

	colors[0] = red;
	colors[1] = green;
	colors[2] = blue;
	colors[3] = purple;
	colors[4] = yellow;
	colors[5] = gray;

	prog = load_program("pass_two_attribs.vert", "cube_texture.frag");
	glUseProgram(prog);

	/*
	* Common
	*/
	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);

	/*
	* CASE #1
	*/

	/* Upload a basic cubemap*/
	glGenTextures( 1, (GLuint*)&cubemap );
	glBindTexture( GL_TEXTURE_CUBE_MAP, cubemap );
	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);

	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);


	/* Ensure mipmap-complete cubemap*/
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, red );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, yellow );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gray );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);
	glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, purple );
	ASSERT_GLERROR_EQUAL(GL_NO_ERROR);

	/* Ensure that no error is generated */
	err = glGetError();
	ASSERT_GLENUMS_EQUAL(err, GL_NO_ERROR);


	/*
	* CASE #2
	*/

	/* Enable mipmapping */
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	/* Redraw 1x1 quad at [0,0] and ensure that it's the right color */
	for (i=0; i<6; i++)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		drawQuadCubeMapTextured( test_suite, x[i], y[i], z[i], cubemap);
		get_backbuffer(&backbuffer);
		ASSERT_GLES_FB_COLORS_EQUAL(&backbuffer, 0, 0, gles_color_make_4b( (colors[i])[0], (colors[i])[1], (colors[i])[2], (colors[i])[3] ), gles_float_epsilon);
	}

	/*
	* Cleanup
	*/
}

suite* create_suite_glTexImage2D3(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glTexImage2D3", gles2_create_fixture, gles2_remove_fixture, results );

	add_test_with_flags(new_suite, "ATX12-glTexImage2D", test_glTexImage2D3, FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	add_test(new_suite,"ATX12-test_constraint_resolve_reset_flag", test_constraint_resolve_reset_flag);
	add_test(new_suite,"ATX12-test_resolve_constraint_cube_face_wrong_size", test_resolve_constraint_cube_face_wrong_size);
	add_test(new_suite,"ATX12-test_resolve_constraint_cube_face_wrong_size_reallocate", test_resolve_constraint_cube_face_wrong_size_reallocate);
	add_test(new_suite,"ATX12-test_resolve_constraint_cube_non_addresable_level",test_resolve_constraint_cube_non_addresable_level);

	return new_suite;
}
