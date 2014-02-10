/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gl2_framework.h"
#include <string.h>
#include "../unit_framework/suite.h"
#include <stdio.h>
#include <stdarg.h>

#ifdef __SYMBIAN32__
extern void SOS_DebugPrintf(const char * format,...);
#define __PRINTF(format)   SOS_DebugPrintf format;  printf format
#define __FPRINTF(format)  SOS_DebugPrintf format;  printf format
#else
static int __fprintf_stderr(const char * format, ...)
{
	va_list arglist;

	va_start(arglist,format);

	vfprintf(stderr,format,arglist);

	va_end(arglist);

	return 0;
}
#define __PRINTF(format)   printf format
#define __FPRINTF(format)  __fprintf_stderr format
#endif

#if defined(TESTBENCH_DESKTOP_GL) || defined(UNDER_CE)
#include <time.h>
#else
#include "sys/time.h"
#endif

#ifdef UNDER_CE
FILE *wince_fopen(const char *filename, const char *mode);
#endif

#define BUFFERSIZE 4096

#ifdef UNDER_CE
FILE *wince_fopen(const char *filename, const char *mode)
{
	TCHAR szPath[MAX_PATH];
	unsigned int i;

	if (filename == NULL || mode == NULL) return NULL;

	/* force binary mode */
	if (strcmp(mode, "r") == 0)
		mode = "rb";
	else if (strcmp(mode, "w") == 0)
		mode = "wb";
	else if (strcmp(mode, "a") == 0)
		mode = "ab";

	/* Leave an absolute path alone */
	if (*filename == '\\') return fopen(filename, mode);

	/* Prefix a relative path with the module load directory */
	i = GetModuleFileName(NULL, szPath, MAX_PATH);
	if ( 0 == i ) return fopen(filename, mode);

	/* Find last occurance of '\' */
	while ( i && TEXT('\\') != szPath[i] )
	{
		i--;
	}

	if ((i + 1 + strlen(filename) + 1)  <= MAX_PATH)
	{
		unsigned int j, k;
		unsigned int filenameLen = strlen(filename);
		char szAbsPath[MAX_PATH];

		for (j = 0; j <= i; j++)
		{
			szAbsPath[j] = (char)szPath[j];
		}

		/* Convert forward slash in file name to back slash */
		for (k = 0; k < filenameLen; k++, j++)
		{
			if (filename[k] == '/')
				szAbsPath[j] = '\\';
			else
				szAbsPath[j] = filename[k];
		}
		szAbsPath[j] = '\0';
		return fopen(szAbsPath, mode);
	}

	return NULL;
}
#endif

const char* error_string(GLenum errcode)
{
	switch(errcode)
	{
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	default: return "Unknown error";
	}
}

int is_extension_enabled(char * extension)
{
	if( strstr((char*) glGetString(GL_EXTENSIONS), extension) != NULL )
	{
		return TRUE;
	}
	return FALSE;
}

void* load_file(const char* filename, int* len, const char* fopen_credentials)
{
	void* retval = NULL;
	FILE* fs;
	size_t elements_read = 0;
	if(!filename) return NULL;
#ifdef UNDER_CE
	fs = wince_fopen(filename, fopen_credentials);
#else
	fs = fopen(filename, fopen_credentials);
#endif
	if(!fs) {
		__PRINTF(("load_file: failed to open %s\r\n", filename));
		return NULL;
	}
	fseek(fs, 0, SEEK_END);
	*len = ftell(fs);
	if( *len >= 0 )
	{
		fseek(fs, 0, SEEK_SET);
		retval = MALLOC(*len + 1);
		if(retval)
		{
			MEMSET(retval, 0, *len + 1);
			elements_read = fread(retval, 1, *len, fs);
		}
	}
	fclose(fs);
	if((int)elements_read != *len)
	{
		__PRINTF(("load_file: expected to read %d bytes but got %d\r\n", *len, elements_read));
		if ( NULL != retval ) FREE(retval);
		return NULL;
	}
	return retval;
}


GLuint load_shader(const char* shadername, GLenum shadertype)
{
	char *buffer = MALLOC(BUFFERSIZE);
	GLuint retval;

	if ( NULL == buffer ) return 0;

	retval = glCreateShader(shadertype);

#ifdef USING_BINARY_SHADERS
	{
		GLsizei len;
		void* shaderdata;
		sprintf(buffer, "shaders/%s.binshader", shadername);
		shaderdata = load_file(buffer, &len, "rb");
		if(!shaderdata)
		{
			__FPRINTF(("Unable to load '%s'\n", buffer));
			return 0;
		}
		glShaderBinary(1, &retval, GL_MALI_SHADER_BINARY_ARM, shaderdata, len);
		FREE(shaderdata);
	}
#else
	{
		GLsizei len;
		char* shaderdata;
		GLint compiled;

#ifdef __SYMBIAN32__
		sprintf(buffer, "z:\\shaders\\%s.glsl", shadername);
#else
		sprintf(buffer, "shaders/%s.glsl", shadername);
#endif
		shaderdata = (char*) load_file(buffer, &len, "r");
		if(!shaderdata)
		{
			__FPRINTF(("Unable to load '%s'\n", buffer));
			compiled = GL_FALSE;
		} else
		{
			glShaderSource(retval, 1, (const char**) &shaderdata, NULL);
			glCompileShader(retval);
			glGetShaderiv(retval, GL_COMPILE_STATUS, &compiled);
		}

		if(compiled == GL_FALSE)
		{
			__FPRINTF(("Critical Error: Unable to compile shader '%s' \n", buffer));
			glGetShaderInfoLog(retval, BUFFERSIZE -1, NULL, buffer);
			__FPRINTF((" - reason:\n%s\n\n", buffer));
		}
		FREE(shaderdata);
	}
#endif
	FREE(buffer);
	return retval;
}

GLuint load_program(const char* in_vshader, const char* in_fshader)
{
	char *buffer = MALLOC(BUFFERSIZE);
	GLint linked = GL_FALSE;
	GLuint vs, fs, proggy;

	if ( NULL == buffer ) return 0;

	MEMSET(buffer, 0, BUFFERSIZE);
	vs = load_shader(in_vshader, GL_VERTEX_SHADER);
	fs = load_shader(in_fshader, GL_FRAGMENT_SHADER);

	proggy = glCreateProgram();
	if (proggy == 0)
	{
		FREE(buffer);
		return proggy;
	}

	glAttachShader(proggy, fs);
	glAttachShader(proggy, vs);

	glBindAttribLocation(proggy, 0, "position");

	glLinkProgram(proggy);
	glGetProgramiv(proggy, GL_LINK_STATUS, &linked);

	if(linked == GL_FALSE)
	{
		glGetProgramInfoLog(proggy, BUFFERSIZE -1, NULL, buffer);
		__FPRINTF(("Critical Error: Unable to link program - reason:\n%s\n\n", buffer));
		FREE(buffer);
		return 0;
	}


	glUseProgram(proggy);

	{
		GLint pos = glGetUniformLocation(proggy, "mvp");
		if(pos != -1)
		{
			float transform[] = { 1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1};
			glUniformMatrix4fv(pos, 1, GL_FALSE, transform);
		}
	}

	{
		GLint colorpos = glGetUniformLocation(proggy, "color");
		if(colorpos != -1)
		{
			float colors[] = {1.0f, 1.0f, 1.0f, 1.0f};
			glUniform4fv(colorpos, 1, colors);
		}
	}
	FREE(buffer);
	return proggy;
}

void draw_custom_triangle(const float *vertices, const float *color)
{
	GLushort indices[] = {0, 1, 2};
	GLint col;

	GLfloat color_orig[4];

	GLint proggy;
	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	col = glGetUniformLocation( proggy, "color" );
	if (col != -1)
	{
		glGetUniformfv(proggy, col, color_orig);
	}
	else
	{
		__FPRINTF((" Warning: Current Program has no color uniform \n"));
	}

	glUniform4fv( col, 1, color );

	{
		GLint pos;
		pos = glGetAttribLocation(proggy, "position");
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	}
	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);

	/* reset state to original */
	glUniform4fv( col, 1, color_orig );
}

void draw_custom_square(GLfloat vertices[3 * 4])
{
	GLushort indices[] = {0,1,2,0,2,3};

	/* setup position varying */
	GLint pos;
	GLint proggy;
	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	pos = glGetAttribLocation(proggy, "position");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

	/* draw it! */
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void draw_square()
{
	GLfloat vertices[] =
		{
			-1, -1, 0.5,
			1, -1, 0.5,
			1,  1, 0.5,
			-1,  1, 0.5
		};

	draw_custom_square( vertices );
}

static void get_normalized_square_vertices(float vertices[4 * 4], float x, float y, float w, float h)
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	/* bottom-left vertex */
	vertices[0 * 3 + 0] = (x + 0) / (viewport[2] / 2.0f) - 1.0f;
	vertices[0 * 3 + 1] = (y + 0) / (viewport[3] / 2.0f) - 1.0f;
	vertices[0 * 3 + 2] = 0.5;

	/* top-left vertex */
	vertices[1 * 3 + 0] = (x + 0) / (viewport[2] / 2.0f) - 1.0f;
	vertices[1 * 3 + 1] = (y + h) / (viewport[3] / 2.0f) - 1.0f;
	vertices[1 * 3 + 2] = 0.5;

	/* top-right vertex */
	vertices[2 * 3 + 0] = (x + w) / (viewport[2] / 2.0f) - 1.0f;
	vertices[2 * 3 + 1] = (y + h) / (viewport[3] / 2.0f) - 1.0f;
	vertices[2 * 3 + 2] = 0.5;

	/* bottom-right vertex */
	vertices[3 * 3 + 0] = (x + w) / (viewport[2] / 2.0f) - 1.0f;
	vertices[3 * 3 + 1] = (y + 0) / (viewport[3] / 2.0f) - 1.0f;
	vertices[3 * 3 + 2] = 0.5;
}

void draw_normalized_square_pos(float x, float y, float w, float h)
{
	float vertices[4 * 4];
	get_normalized_square_vertices(vertices, x, y, w, h);
	draw_custom_square( vertices );
}

void draw_custom_square_textured_offset(const GLfloat* vertices, GLfloat xoffset, GLfloat yoffset, GLuint texture_id)
{
	GLfloat texcoords[] =
		{
			0, 0,
			1, 0,
			1, 1,
			0, 1
		};

	GLushort indices[] =
		{
			0, 1, 2, /* first triangle */
			0, 2, 3  /* second triangle */
		};

	GLint pos;
	GLint proggy;

    GLfloat offsetVerts[12];
	offsetVerts[0]  = vertices[0] + xoffset;
	offsetVerts[1]  = vertices[1] + yoffset;
	offsetVerts[2]  = vertices[2];
	offsetVerts[3]  = vertices[3] + xoffset;
	offsetVerts[4]  = vertices[4] + yoffset;
	offsetVerts[5]  = vertices[5];
	offsetVerts[6]  = vertices[6] + xoffset;
	offsetVerts[7]  = vertices[7] + yoffset;
	offsetVerts[8]  = vertices[8];
	offsetVerts[9]  = vertices[9] + xoffset;
	offsetVerts[10] = vertices[10] + yoffset;
	offsetVerts[11] = vertices[11];

    glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	pos = glGetAttribLocation(proggy, "position");

	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, offsetVerts);

	pos = glGetAttribLocation(proggy, "in_tex0");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

	pos = glGetUniformLocation(proggy, "diffuse");
	glUniform1i(pos, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void draw_custom_textured_square( const GLfloat* vertices, const GLfloat *texcoords, GLuint texture_id)
{
	GLushort indices[] =
	{
		0, 1, 2, /* first triangle */
		0, 2, 3  /* second triangle */
	};

	GLint pos;
	GLint proggy;

	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	pos = glGetAttribLocation(proggy, "position");

	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

	pos = glGetAttribLocation(proggy, "in_tex0");
	if(pos < 0) mali_tpi_printf("%s: cannot find symbol in_tex0: location is %d\n", __func__, pos);
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

	pos = glGetUniformLocation(proggy, "diffuse");
	glUniform1i(pos, 0);
	if(pos < 0) mali_tpi_printf("%s: cannot find symbol diffuse: location is %d\n", __func__, pos);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void draw_textured_square(void)
{
	GLfloat vertices[] =
		{
			-1, -1, 0.5,
			1, -1, 0.5,
			1,  1, 0.5,
			-1,  1, 0.5
		};

	GLfloat texcoords[] =
		{
			0, 0,
			1, 0,
			1, 1,
			0, 1
		};

	GLushort indices[] =
		{
			0, 1, 2, /* first triangle */
			0, 2, 3  /* second triangle */
		};

	GLint pos;
	GLint proggy;
	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);

	pos = glGetAttribLocation(proggy, "position");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

	pos = glGetAttribLocation(proggy, "in_tex0");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void draw_normalized_textured_square_offset(float x, float y, float w, float h, float s_offset, float t_offset)
{
	GLint viewport[4];
	GLfloat vertices[4 * 4];
	GLint proggy;
	GLint pos;

	GLushort indices[] =
		{
			0, 1, 2,
			0, 2, 3
		};

	GLfloat texcoords[8];
	texcoords[0] = 0 + s_offset;
	texcoords[1] = 0 + t_offset;
	texcoords[2] = 0 + s_offset;
	texcoords[3] = 1 + t_offset;
	texcoords[4] = 1 + s_offset;
	texcoords[5] = 1 + t_offset;
	texcoords[6] = 1 + s_offset;
	texcoords[7] = 0 + t_offset;

	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	glGetIntegerv(GL_VIEWPORT, viewport);

	/* bottom-left vertex */
	vertices[0*3+0] = (x + 0) / (viewport[2] / 2.0f) - 1.0f;
	vertices[0*3+1] = (y + 0) / (viewport[3] / 2.0f) - 1.0f;
	vertices[0*3+2] = 0.5f;

	/* top-left vertex */
	vertices[1*3+0] = (x + 0) / (viewport[2] / 2.0f) - 1.0f;
	vertices[1*3+1] = (y + h) / (viewport[3] / 2.0f) - 1.0f;
	vertices[1*3+2] = 0.5f;

	/* top-right vertex */
	vertices[2*3+0] = (x + w) / (viewport[2] / 2.0f) - 1.0f;
	vertices[2*3+1] = (y + h) / (viewport[3] / 2.0f) - 1.0f;
	vertices[2*3+2] = 0.5f;

	/* bottom-right vertex */
	vertices[3*3+0] = (x + w) / (viewport[2] / 2.0f) - 1.0f;
	vertices[3*3+1] = (y + 0) / (viewport[3] / 2.0f) - 1.0f;
	vertices[3*3+2] = 0.5f;

	pos = glGetAttribLocation(proggy, "in_tex0");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

	pos = glGetUniformLocation(proggy, "diffuse");
	if(pos == -1)
	{
		__FPRINTF(("Warning: No diffuse sampler found!!!\n"));
		return;
	}
	glUniform1i(pos, 0);

	pos = glGetAttribLocation(proggy, "position");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void draw_normalized_cube_textured_square(float x, float y, float w, float h)
{
	GLint viewport[4];
	GLfloat vertices[4 * 4];
	GLint proggy;
	GLint pos;

	GLfloat texcoords[] =
		{
			-1, -1, 1,
			-1,  1, 1,
			1,  1, 1,
			1, -1, 1,
		};

	GLushort indices[] =
		{
			0, 1, 2,
			0, 2, 3
		};

	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	glGetIntegerv(GL_VIEWPORT, viewport);

	/* bottom-left vertex */
	vertices[0*3+0] = (x + 0) / (viewport[2] / 2.0f) - 1.0f;
	vertices[0*3+1] = (y + 0) / (viewport[3] / 2.0f) - 1.0f;
	vertices[0*3+2] = 0.5f;

	/* top-left vertex */
	vertices[1*3+0] = (x + 0) / (viewport[2] / 2.0f) - 1.0f;
	vertices[1*3+1] = (y + h) / (viewport[3] / 2.0f) - 1.0f;
	vertices[1*3+2] = 0.5f;

	/* top-right vertex */
	vertices[2*3+0] = (x + w) / (viewport[2] / 2.0f) - 1.0f;
	vertices[2*3+1] = (y + h) / (viewport[3] / 2.0f) - 1.0f;
	vertices[2*3+2] = 0.5f;

	/* bottom-right vertex */
	vertices[3*3+0] = (x + w) / (viewport[2] / 2.0f) - 1.0f;
	vertices[3*3+1] = (y + 0) / (viewport[3] / 2.0f) - 1.0f;
	vertices[3*3+2] = 0.5f;

	pos = glGetAttribLocation(proggy, "in_tex0");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, texcoords);

	pos = glGetUniformLocation(proggy, "cube");
	if(pos == -1)
	{
		__FPRINTF(("Warning: No sampler found!!!\n"));
		return;
	}
	glUniform1i(pos, 0);

	pos = glGetAttribLocation(proggy, "position");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void draw_normalized_textured_square(float x, float y, float w, float h)
{
	draw_normalized_textured_square_offset(x, y, w, h, 0.0f, 0.0f);
}

void draw_line(const float *vertices, const float *color, const float size)
{
	GLint pos;
	GLint proggy;
	GLint col;

	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);

	col = glGetUniformLocation( proggy, "color" );
	glUniform4fv( col, 1, color );

	pos = glGetAttribLocation(proggy, "position");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, vertices);

	glLineWidth(size);

	glDrawArrays(GL_LINES, 0, 2);
}


void draw_point(const float *vertices, const float *color, const float size)
{
	GLint pos;
	GLint proggy;
	GLint col;
	GLint loc;

	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);

	col = glGetUniformLocation( proggy, "color" );
	glUniform4fv( col, 1, color );

	pos = glGetAttribLocation(proggy, "position");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

	loc = glGetUniformLocation(proggy, "point_size");
	if(loc == -1)
	{
		__FPRINTF(("Warning: No pointsize found!!!\n"));
		return;
	}
	glUniform1f(loc, size);

	glDrawArrays(GL_POINTS, 0, 1);
}

void draw_left_rectangle(void)
{
	GLfloat vertices[] = {-1, -1, 0.5,
	                      0, -1, 0.5,
	                      0, 1, 0.5,
	                      -1, 1, 0.5 };
	draw_custom_square( vertices );
}
void draw_right_rectangle(void)
{
	GLfloat vertices[] = {0, -1, 0.5,
	                      1, -1, 0.5,
	                      1, 1, 0.5,
	                      0, 1, 0.5 };
	draw_custom_square( vertices );
}

void draw_top_rectangle(void)
{
	GLfloat vertices[] = {-1, 0, 0.5,
	                      1, 0, 0.5,
	                      1, 1, 0.5,
	                      -1, 1, 0.5 };
	draw_custom_square( vertices );
}

void draw_bottom_rectangle(void)
{
	GLfloat vertices[] = {-1, -1, 0.5,
	                      1, -1, 0.5,
	                      1, 0, 0.5,
	                      -1, 0, 0.5 };
	draw_custom_square( vertices );
}

unsigned long long get_time(void)
{
#if defined(_WIN32) || defined(UNDER_CE)
	return GetTickCount() * 1000; /* return time in microsecond resolution */
#else
	/* Currently using gettimeofday, but more precise counters could be added later */
	struct timeval tval;
	gettimeofday(&tval,NULL);
	return tval.tv_sec*1000000 + tval.tv_usec;
#endif
}

#ifndef MIN
#define MIN(v1, v2) ((v1) > (v2) ? (v2) : (v1))
#endif

#ifndef MAX
#define MAX(v1, v2) ((v1) < (v2) ? (v2) : (v1))
#endif

#ifndef CLAMP
#define CLAMP(val, min, max) MAX(MIN((val), (max)), (min))
#endif

struct gles_color gles_color_make_3f(float r, float g, float b)
{
	struct gles_color col;
	col.r = r;
	col.g = g;
	col.b = b;
	col.a = 1.0f;

	return col;
}

struct gles_color gles_color_make_4f(float r, float g, float b, float a)
{
	struct gles_color col;
	col.r = r;
	col.g = g;
	col.b = b;
	col.a = a;

	return col;
}

struct gles_color gles_color_make_4b(u8 r, u8 g, u8 b, u8 a)
{
	struct gles_color col;
	col.r = (float)r / 255;
	col.g = (float)g / 255;
	col.b = (float)b / 255;
	col.a = (float)a / 255;

	return col;
}

struct gles_color gles_color_make_3b(u8 r, u8 g, u8 b)
{
	struct gles_color col;
	col.r = (float)r / 255;
	col.g = (float)g / 255;
	col.b = (float)b / 255;
	col.a = 1.0f;

	return col;
}

struct gles_color gles_color_clamp(struct gles_color col)
{
	col.r = CLAMP(col.r, 0.0f, 1.0f);
	col.g = CLAMP(col.g, 0.0f, 1.0f);
	col.b = CLAMP(col.b, 0.0f, 1.0f);
	col.a = CLAMP(col.a, 0.0f, 1.0f);

	return col;
}

int gles_colors_rgb_equal(struct gles_color c1, struct gles_color c2, float epsilon)
{
	if (fabs(c1.r - c2.r) > epsilon) return 0;
	if (fabs(c1.g - c2.g) > epsilon) return 0;
	if (fabs(c1.b - c2.b) > epsilon) return 0;
	return 1;
}

int gles_fb_color_equal(struct gles_color fb_color, struct gles_color expected, float epsilon)
{
	struct gles_color min;
	struct gles_color max;

	GLint fb_r_bits, fb_g_bits, fb_b_bits, fb_a_bits;
	GLint fb_max_r, fb_max_g, fb_max_b, fb_max_a;

	glGetIntegerv(GL_RED_BITS,   &fb_r_bits);
	glGetIntegerv(GL_GREEN_BITS, &fb_g_bits);
	glGetIntegerv(GL_BLUE_BITS,  &fb_b_bits);
	glGetIntegerv(GL_ALPHA_BITS, &fb_a_bits);

	/* calculate maximum framebuffer components */
	fb_max_r = (1 << fb_r_bits) - 1;
	fb_max_g = (1 << fb_g_bits) - 1;
	fb_max_b = (1 << fb_b_bits) - 1;
	fb_max_a = (1 << fb_a_bits) - 1;

	/* simulate rounding to framebuffer (fragment color -> framebuffer) */
	/* minimum color values */
	min.r = (float)floor(fb_max_r * (expected.r - epsilon));
	min.g = (float)floor(fb_max_g * (expected.g - epsilon));
	min.b = (float)floor(fb_max_b * (expected.b - epsilon));
	/* maximum color values */
	max.r = (float)ceil(fb_max_r * (expected.r + epsilon));
	max.g = (float)ceil(fb_max_g * (expected.g + epsilon));
	max.b = (float)ceil(fb_max_b * (expected.b + epsilon));

	/* simulate rounding to 8888 (framebuffer -> glReadPixels) */
	/* minimum color values */
	min.r = (float)floor((min.r / fb_max_r) * 255);
	min.g = (float)floor((min.g / fb_max_g) * 255);
	min.b = (float)floor((min.b / fb_max_b) * 255);
	/* maximum color values */
	max.r = (float)ceil((max.r / fb_max_r) * 255);
	max.g = (float)ceil((max.g / fb_max_g) * 255);
	max.b = (float)ceil((max.b / fb_max_b) * 255);

	/* check for components outside legal range */
	if ((float)floor(fb_color.r * 255) > max.r) return 0;
	if ((float)ceil(fb_color.r * 255) < min.r) return 0;
	if ((float)floor(fb_color.g * 255) > max.g) return 0;
	if ((float)ceil(fb_color.g * 255) < min.g) return 0;
	if ((float)floor(fb_color.b * 255) > max.b) return 0;
	if ((float)ceil(fb_color.b * 255) < min.b) return 0;

	if (fb_a_bits > 0)
	{
		min.a = (float)floor(fb_max_a * (expected.a - epsilon));
		max.a = (float)ceil(fb_max_a * (expected.a + epsilon));
		min.a = (float)floor((min.a / fb_max_a) * 255);
		max.a = (float)ceil((max.a / fb_max_a) * 255);
		if ((float)floor(fb_color.a * 255) > max.a) return 0;
		if ((float)ceil(fb_color.a * 255) < min.a) return 0;
	}

	/* success, colors are equal */
	return 1;
}

int gles_fb_color_rgb_equal(struct gles_color fb_color, struct gles_color expected, float epsilon)
{
	return gles_fb_color_equal(
		gles_color_make_3f(fb_color.r, fb_color.g, fb_color.b),
		gles_color_make_3f(expected.r, expected.g, expected.b),
		epsilon
		);
}

static void micro_assert(const char *expr, const char *file, unsigned int line)
{
	__FPRINTF(("ASSERT(%s) failed at %s:%d\n", expr, file, line));
#ifdef TESTBENCH_DESKTOP_GL
	abort();
#else
	mali_tpi_abort();
#endif
}

/* Symabin OS: ASSERT is defined in bin\Techview\epoc32\include\e32def.h */
#ifdef __SYMBIAN32__
#define MALI_ASSERT(expr)  if(!(expr)) micro_assert(#expr, __FILE__, __LINE__)
#else
#define MALI_ASSERT(expr) ((expr) ? 0 : micro_assert(#expr, __FILE__, __LINE__))
#endif

struct gles_color gles_fb_get_pixel_color(const BackbufferImage *img, int x, int y)
{
	u8 r, g, b, a;
	MALI_ASSERT(x >= 0);
	MALI_ASSERT(x <  img->width);

	MALI_ASSERT(y >= 0);
	MALI_ASSERT(y <  img->height);

	r = img->test_mem_pool_data_buffer[((x) + (img)->width*(y)) * 4 + 0];
	g = img->test_mem_pool_data_buffer[((x) + (img)->width*(y)) * 4 + 1];
	b = img->test_mem_pool_data_buffer[((x) + (img)->width*(y)) * 4 + 2];
	a = img->test_mem_pool_data_buffer[((x) + (img)->width*(y)) * 4 + 3];

	return gles_color_make_4b(r, g, b, a);
}

struct gles_color gles_buffer_4b_get_pixel_color(const u8 *buf, int width, int height, int x, int y)
{
	int index;
	u8 r, g, b, a;

	MALI_ASSERT(x >= 0);
	MALI_ASSERT(x <  width);

	MALI_ASSERT(y >= 0);
	MALI_ASSERT(y <  height);

	index = x + y * width;

	r = buf[index * 4 + 0];
	g = buf[index * 4 + 1];
	b = buf[index * 4 + 2];
	a = buf[index * 4 + 3];

	return gles_color_make_4b(r, g, b, a);
}

struct gles_color gles_buffer_us565_get_pixel_color(const u16 *buf, int width, int height, int x, int y)
{
	int index;
	u8 r, g, b;
	struct gles_color col;

	MALI_ASSERT(x >= 0);
	MALI_ASSERT(x <  width);

	MALI_ASSERT(y >= 0);
	MALI_ASSERT(y <  height);

	index = x + y * width;

	r = buf[index]>>11 & 0x1F;
	g = buf[index]>>5 & 0x3F;
	b = buf[index] & 0x1F;

	col.r = (float)r / 31;
	col.g = (float)g / 63;
	col.b = (float)b / 31;
	col.a = 1.0f;

	return col;
}

struct gles_color gles_buffer_us5551_get_pixel_color(const u16 *buf, int width, int height, int x, int y)
{
	int index;
	u8 r, g, b, a;
	struct gles_color col;

	MALI_ASSERT(x >= 0);
	MALI_ASSERT(x <  width);

	MALI_ASSERT(y >= 0);
	MALI_ASSERT(y <  height);

	index = x + y * width;

	r = buf[index]>>11 & 0x1F;
	g = buf[index]>>6 & 0x1F;
	b = buf[index]>>1 & 0x1F;
	a = buf[index] & 0x01;

	col.r = (float)r / 31;
	col.g = (float)g / 31;
	col.b = (float)b / 31;
	col.a = (float)a;

	return col;
}

struct gles_color gles_buffer_us4444_get_pixel_color(const u16 *buf, int width, int height, int x, int y)
{
	int index;
	u8 r, g, b, a;
	struct gles_color col;

	MALI_ASSERT(x >= 0);
	MALI_ASSERT(x <  width);

	MALI_ASSERT(y >= 0);
	MALI_ASSERT(y <  height);

	index = x + y * width;

	r = buf[index]>>12 & 0xF;
	g = buf[index]>>8 & 0xF;
	b = buf[index]>>4 & 0xF;
	a = buf[index] & 0xF;

	col.r = (float)r / 15;
	col.g = (float)g / 15;
	col.b = (float)b / 15;
	col.a = (float)a / 15;

	return col;
}

struct gles_color gles_buffer_3b_get_pixel_color(const u8 *buf, int width, int height, int x, int y)
{
	int index;
	u8 r, g, b;

	MALI_ASSERT(x >= 0);
	MALI_ASSERT(x <  width);

	MALI_ASSERT(y >= 0);
	MALI_ASSERT(y <  height);

	index = x + y * width;

	r = buf[index * 3 + 0];
	g = buf[index * 3 + 1];
	b = buf[index * 3 + 2];

	return gles_color_make_3b(r, g, b);
}

struct gles_color gles_buffer_2b_get_pixel_color(const u8 *buf, int width, int height, int x, int y)
{
	int index;
	u8 r, g;

	MALI_ASSERT(x >= 0);
	MALI_ASSERT(x <  width);

	MALI_ASSERT(y >= 0);
	MALI_ASSERT(y <  height);

	index = x + y * width;

	r = buf[index * 2 + 0];
	g = buf[index * 2 + 1];

	return gles_color_make_3b(r, g, 0);
}

struct gles_color gles_buffer_1b_get_pixel_color(const u8 *buf, int width, int height, int x, int y)
{
	int index;
	u8 r;

	MALI_ASSERT(x >= 0);
	MALI_ASSERT(x <  width);

	MALI_ASSERT(y >= 0);
	MALI_ASSERT(y <  height);

	index = x + y * width;

	r = buf[index];

	return gles_color_make_3b(r, r, r);
}
