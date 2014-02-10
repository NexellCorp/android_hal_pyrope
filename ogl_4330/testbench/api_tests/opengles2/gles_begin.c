/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "gl2_framework.h"
#include <stdlib.h>

static GLenum gles_mode;
static GLuint gles_vertex_count = 0;
static GLuint gles_vertex_room = 0;
static GLfloat *gles_vertex_array = NULL;
static GLfloat *gles_color_array = NULL;
static GLfloat gles_curr_color[4] = { 1.0, 1.0, 1.0, 1.0 };

void glesBegin(GLenum mode)
{
	gles_mode = mode;
	gles_vertex_count = 0;
}

void gles_reserve_room(GLuint needed_room)
{
	if (gles_vertex_room < needed_room)
	{
		int new_room = (int)(needed_room * 1.5f); /* grow exponentially */
		gles_vertex_array = (GLfloat*)REALLOC(gles_vertex_array, new_room * sizeof(GLfloat) * 4);
		gles_color_array = (GLfloat*)REALLOC(gles_color_array, new_room * sizeof(GLfloat) * 4);
		gles_vertex_room = new_room;
	}
}

void glesVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	/* make sure we have enough memory */
	gles_reserve_room(gles_vertex_count + 1);

	if (NULL != gles_vertex_array)
	{
		gles_vertex_array[gles_vertex_count * 4 + 0] = x;
		gles_vertex_array[gles_vertex_count * 4 + 1] = y;
		gles_vertex_array[gles_vertex_count * 4 + 2] = z;
		gles_vertex_array[gles_vertex_count * 4 + 3] = w;
	}

	if (NULL != gles_color_array)
	{
		gles_color_array[gles_vertex_count * 4 + 0] = gles_curr_color[0];
		gles_color_array[gles_vertex_count * 4 + 1] = gles_curr_color[1];
		gles_color_array[gles_vertex_count * 4 + 2] = gles_curr_color[2];
		gles_color_array[gles_vertex_count * 4 + 3] = gles_curr_color[3];
	}
	gles_vertex_count++;
}

void glesColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	gles_curr_color[0] = r;
	gles_curr_color[1] = g;
	gles_curr_color[2] = b;
	gles_curr_color[3] = a;
}

void glesEnd(void)
{
	GLint program, position_loc, color_loc;

	glGetIntegerv(GL_CURRENT_PROGRAM, &program);

	position_loc = glGetAttribLocation(program, "position");
	glVertexAttribPointer(position_loc, 4, GL_FLOAT, GL_FALSE, 0, gles_vertex_array);
	glEnableVertexAttribArray(position_loc);

	color_loc = glGetAttribLocation(program, "color");
	if (color_loc >= 0)
	{
		glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, 0, gles_color_array);
		glEnableVertexAttribArray(color_loc);
	}

	glDrawArrays(gles_mode, 0, gles_vertex_count);

	glDisableVertexAttribArray(position_loc);
	if (color_loc >= 0) glDisableVertexAttribArray(color_loc);

	FREE(gles_vertex_array);
	gles_vertex_array = NULL;

	FREE(gles_color_array);
	gles_color_array = NULL;

	gles_vertex_room = 0;
	gles_vertex_count = 0;
}

/* wrappers */
void glesVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	glesVertex4f(x, y, z, 1.0f);
}

void glesVertex2f(GLfloat x, GLfloat y)
{
	glesVertex4f(x, y, 0.0f, 1.0f);
}

void glesColor3f(GLfloat r, GLfloat g, GLfloat b)
{
	glesColor4f(r, g, b, 1.0f);
}
