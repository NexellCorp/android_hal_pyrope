/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifdef __SYMBIAN32__
extern void SOS_DebugPrintf(const char * format,...);
#define __PRINTF(format)   SOS_DebugPrintf format;  printf format
#else
#define __PRINTF(format)   printf format
#endif

#include "../helpers/api_tests_utils.h"
#include "egl_framework_actual.h"
#include "egl_helpers.h"
#include <mali_system.h>
#include <string.h>
#include <stdio.h>
#ifdef EXCLUDE_UNIT_FRAMEWORK
#include <stdlib.h>
#else /* EXCLUDE_UNIT_FRAMEWORK */
#include "../unit_framework/suite.h"
#include "../unit_framework/test_helpers.h"
#include "egl_helpers_pixmap.h"


void* egl_create_fixture( suite* test_suite )
{
	EGLBoolean status;
	mempool* pool = &test_suite->fixture_pool;
	egl_framework_fixture* fix = (egl_framework_fixture *) _test_mempool_alloc( pool, sizeof(egl_framework_fixture) );

	/* Initialize the windowing system */
	status = egl_helper_windowing_init( &fix->windowing_state, test_suite );
	if ( EGL_TRUE != status )
	{
		warn( test_suite, "windowing abstraction failed to initialize" );
		/* fixture freed as part of the fixture pool */
		return NULL;
	}

	return fix;
}

void egl_remove_fixture( suite *test_suite )
{
	EGLBoolean status;
	egl_framework_fixture *fix = (egl_framework_fixture *)test_suite->fixture;

	/* Fixture removal doesn't happen when the fixture didn't init */

	/* Deinit the windowing system */
	if ( NULL != fix->windowing_state )
	{
		status = egl_helper_windowing_deinit( &fix->windowing_state );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_windowing_deinit failed" );
	}

	/* fix is on the fixture memory pool, so will be freed for us */
}
#endif /* EXCLUDE_UNIT_FRAMEWORK */


const char* egl_error_string(EGLint errcode)
{
	switch(errcode)
	{
		case EGL_NOT_INITIALIZED:     return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:          return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:           return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:       return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT:         return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG:          return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:         return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE:         return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH:           return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER:       return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP:   return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:   return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST:        return "EGL_CONTEXT_LOST";
		case EGL_SUCCESS:             return "EGL_SUCCESS";
		default: return "Unknown error";
	}
}

/* ppm rasters from left->right, top->bottom */
int ppm_save( const char *name, u8 *buffer, int w, int h )
{
#ifdef __SYMBIAN32__
	char path[256];
    mali_file* fp;
#else
	FILE *fp;
#endif
	int x, y;

	if ( NULL == name || NULL == buffer ) return 1;

#ifdef __SYMBIAN32__
	_mali_sys_snprintf(path, 255, "c:\\%s", name);
	fp = _mali_sys_fopen( path, "w" );
#else
	fp = egl_helper_fopen( name, "wt" );
#endif

	if ( NULL == fp ) return 1;

    API_SUITE_SYS_FPRINTF( fp, "P3\n%d %d\n255\n", w, h );

	for ( y=0; y<h; y++ )
	{
		for ( x=0; x<w; x++ )
		{
			int ofs = (x + (h-y-1)*w)*3;
			u8 r = buffer[ofs+0];
			u8 g = buffer[ofs+1];
			u8 b = buffer[ofs+2];
			if ( (7 == (x & 7)) || (x == (w - 1)) )
			{
                API_SUITE_SYS_FPRINTF( fp, "%d %d %d\n", r, g, b );
			}
            else
            {
                API_SUITE_SYS_FPRINTF( fp, "%d %d %d ", r, g, b );
            }
		}
	}

#ifdef __SYMBIAN32__
    _mali_sys_fclose( fp );
#else
	fclose( fp );
#endif

	return 0;
}

#ifndef EXCLUDE_UNIT_FRAMEWORK
void pixmap_save( suite *test_suite, const char *filename, EGLNativePixmapType pixmap )
{
	u32 i, j;
	u8 *data;
	u32 pixel;
	u32 bpp = egl_helper_get_nativepixmap_bits_per_pixel( pixmap );
	u32 width = egl_helper_get_nativepixmap_width( pixmap );
	u32 height = egl_helper_get_nativepixmap_height( pixmap );
	pixmap_spec *pixmap_handle;

	pixmap_handle = egl_helper_nativepixmap_prepare_data( test_suite, pixmap );

	data = ( u8 *)MALLOC( width*height*4 );
	if ( NULL == data ) return;

	for ( i=0; i<height; i++ )
	{
		for ( j=0; j<width; j++ )
		{
			u8 r = 0, g = 0, b = 0;

			switch ( bpp )
			{
				case 16:
					/** @note There's an implicit assumption here about the mapping between bpp and display format! */
					pixel = egl_helper_nativepixmap_get_pixel( pixmap_handle, j, i, DISPLAY_FORMAT_RGB565 );
					#define REPLICATE_BITS( a, bits, shift ) \
 						((a) << (shift)) | (((a) >> ((bits)-(shift))) & ((1 << (shift))-1))

						/* convert from 565 to 888 */
						r = ((u32)REPLICATE_BITS( (pixel >> 11), 5, 3 ));
						g = ((u32)REPLICATE_BITS( ((pixel >> 5) & 0x3F), 6, 2 ));
			  			b = ((u32)REPLICATE_BITS( (pixel & 0x1F), 5, 3 ));
					#undef REPLICATE_BITS

					break;

				case 32:
					/** @note There's an implicit assumption here about the mapping between bpp and display format! */
					pixel = egl_helper_nativepixmap_get_pixel( pixmap_handle, j, i, DISPLAY_FORMAT_ARGB8888 );
					r = (pixel & 0x00FF0000)>>16;
					g = (pixel & 0x0000FF00)>>8;
					b = (pixel & 0x000000FF);
					break;

				default:
					__PRINTF(("Unknown pixmap format!\n"));
					break;
			}

			data[(width*i+j)*3+0] = r; /*r*/
			data[(width*i+j)*3+1] = g; /*g*/
			data[(width*i+j)*3+2] = b; /*b*/
		}
	}

	egl_helper_nativepixmap_release_data( pixmap_handle );

    ppm_save( filename, data, width, height );

    FREE( data );
}

void egl_pixmap_fill_grid( suite *test_suite, EGLNativePixmapType pixmap, const u32 colors[], EGLint ncolumns, EGLint nrows, display_format format )
{
	pixmap_spec *handle;

	EGL_HELPER_ASSERT_POINTER( colors );
	EGL_HELPER_ASSERT_MSG( ncolumns > 0, ( "Bad number of columns (%d)\n", ncolumns ) );
	EGL_HELPER_ASSERT_MSG( nrows > 0, ( "Bad number of rows (%d)\n", nrows ) );

	handle = egl_helper_nativepixmap_prepare_data( test_suite, pixmap );
	ASSERT_POINTER_NOT_NULL( handle );
	if ( NULL != handle )
	{
		EGLint row, height, width, column_width;
		pixmap_rect area;

		/* Get the dimensions of the pixmap to be filled */
		height = egl_helper_get_nativepixmap_height( pixmap );
		width = egl_helper_get_nativepixmap_width( pixmap );
		column_width = width / ncolumns;

		/* Set up the topmost rectangle (others will be derived from this). */
		area.y = 0;
		area.height = height / nrows;

		/* Fill each row of rectangles in turn */
		for ( row = 0 ; row < nrows; ++row )
		{
			EGLint column;

			/* Ensure that the final row reaches the bottom of the pixmap */
			if ( row == nrows - 1 ) area.height = height - area.y;

			/* Set up the leftmost rectangle (others will be derived from this). */
			area.x = 0;
			area.width = column_width;

			/* Fill each column within the current row */
			for ( column = 0; column < ncolumns; ++column, ++colors )
			{
				/* Ensure that the final column reaches the righthand side of the pixmap */
				if ( column == ncolumns - 1 ) area.width = width - area.x;

				/* Fill a rectangle of the pixmap using a color from the caller's array */
				egl_helper_nativepixmap_plain_fill( handle, &area, *colors, format );

				area.x += area.width;
			}
			area.y += area.height;
		}

		egl_helper_nativepixmap_release_data( handle );
	}
}
#endif /* EXCLUDE_UNIT_FRAMEWORK */

#ifdef EGL_MALI_VG
void vg_save_backbuffer( const char *filename, int x, int y, int width, int height )
{
    int i, j;

    u8* data = (u8 *)MALLOC(width*height*4);
	if ( NULL == data ) return;

	vgReadPixels( data, width*4, VG_sABGR_8888, x, y, width, height);

    /* repack the data into RGB instead of RGBA */
    for ( i=0; i<height; i++ )
    {
        for ( j=0; j<width; j++ )
        {
            data[(width*i+j)*3+0]=data[(width*i+j)*4+0];
            data[(width*i+j)*3+1]=data[(width*i+j)*4+1];
            data[(width*i+j)*3+2]=data[(width*i+j)*4+2];
        }
    }

   ppm_save( filename, data, width, height );

   FREE( data );
}
#endif

#ifdef EGL_MALI_GLES

void gl_save_backbuffer( const char *filename, int x, int y, int width, int height )
{
    int i, j;
    u8* data = (u8 *)MALLOC(width*height*4);
    if ( NULL == data ) return;

    glReadPixels( x, y, width, height, GL_RGBA,GL_UNSIGNED_BYTE, data );

    /* repack the data into RGB instead of RGBA */
    for ( i=0; i<height; i++ )
    {
        for ( j=0; j<width; j++ )
        {
            data[(width*i+j)*3+0]=data[(width*i+j)*4+0];
            data[(width*i+j)*3+1]=data[(width*i+j)*4+1];
            data[(width*i+j)*3+2]=data[(width*i+j)*4+2];
        }
   }

   ppm_save( filename, data, width, height );

   FREE( data );
}

#if MALI_EGL_GLES_MAJOR_VERSION == 1
void egl_draw_custom_triangle(float *vertices, float *color)
{
	egl_draw_triangle( vertices, color );
}
void egl_draw_triangle(float *vertices, float *color)
{
	unsigned short indices[] = {0, 1, 2};

	glColor4f(color[0], color[1], color[2], color[3]);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);

}

void egl_draw_triangle_offset(float *vertices, float *color, GLfloat xoffset, GLfloat yoffset)
{
	unsigned short indices[] = {0, 1, 2};
	GLfloat *verts;
	GLfloat *vertsp;

	vertsp = verts = (GLfloat*)MALLOC(3*3*sizeof(GLfloat));
	if ( NULL == vertsp ) return;

	verts[0] = vertices[0] + xoffset;
	verts[1] = vertices[1] + yoffset;
	verts[2] = vertices[2];

	verts[3] = vertices[3] + xoffset;
	verts[4] = vertices[4] + yoffset;
	verts[5] = vertices[5];

	verts[6] = vertices[6] + xoffset;
	verts[7] = vertices[7] + yoffset;
	verts[8] = vertices[8];

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);

	FREE( vertsp );
}

void egl_draw_square_textured_offset(GLfloat* vertices, GLfloat xoffset, GLfloat yoffset, GLuint texture_id)
{
	GLfloat* offsetVerts = NULL;
	GLfloat* offsetVertsp = NULL;
	GLfloat texcoords[] = {
		0, 1,
		1, 1,
		1, 0,
		0, 0
	};

	GLushort indices[] =
	{
		0, 1, 2, /* first triangle */
		0, 2, 3  /* second triangle */
	};


	offsetVertsp = offsetVerts = (GLfloat*)MALLOC(3*4*sizeof(GLfloat));
	if ( NULL == offsetVertsp) return;

	offsetVerts[0] = vertices[0] + xoffset;
	offsetVerts[1] = vertices[1] + yoffset;
	offsetVerts[2] = vertices[2];
	offsetVerts[3] = vertices[3] + xoffset;
	offsetVerts[4] = vertices[4] + yoffset;
	offsetVerts[5] = vertices[5];
	offsetVerts[6] = vertices[6] + xoffset;
	offsetVerts[7] = vertices[7] + yoffset;
	offsetVerts[8] = vertices[8];
	offsetVerts[9] = vertices[9] + xoffset;
	offsetVerts[10] = vertices[10] + yoffset;
	offsetVerts[11] = vertices[11];

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, offsetVerts);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2, GL_FLOAT, 0, texcoords );

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

	FREE( offsetVertsp );
}

void egl_draw_custom_square_offset( float *vertices, float *color, GLfloat xoffset, GLfloat yoffset )
{
	GLfloat *verts;
	GLfloat *vertsp;

	vertsp = verts = (GLfloat*)MALLOC(3*4*sizeof(GLfloat));
	if ( NULL == vertsp ) return;

	verts[0] = vertices[0] + xoffset;
	verts[1] = vertices[1] + yoffset;
	verts[2] = vertices[2];

	verts[3] = vertices[3] + xoffset;
	verts[4] = vertices[4] + yoffset;
	verts[5] = vertices[5];

	verts[6] = vertices[6] + xoffset;
	verts[7] = vertices[7] + yoffset;
	verts[8] = vertices[8];

	verts[9] = vertices[9] + xoffset;
	verts[10] = vertices[10] + yoffset;
	verts[11] = vertices[11];

	glVertexPointer(3, GL_FLOAT, 0, verts);
	glEnableClientState(GL_VERTEX_ARRAY);
	glShadeModel(GL_FLAT);
	glColor4f(color[0], color[1], color[2], color[3]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	FREE( vertsp );
}

#else

GLuint egl_unload_shader2( GLuint proggy, GLuint vs, GLuint fs)
{
	glUseProgram(0);
	glDeleteProgram(proggy);
	glDeleteShader(vs);
	glDeleteShader(fs);
	return 0;
}

void* egl_load_file(char* filename, int buffer_size, int* len, char* fopen_credentials)
{
	void* retval;
	FILE* fs;
	if(!filename) return NULL;

	fs = egl_helper_fopen(filename, fopen_credentials);
	if(!fs) return NULL;

	fseek(fs, 0, SEEK_END);

	*len = (int)ftell(fs);
	if (*len >= buffer_size || 0 > *len )
	{
		fclose(fs);
		return NULL;
	}

	fseek(fs, 0, SEEK_SET);
	retval = MALLOC(*len + 1);
	if ( NULL == retval )
	{
		fclose( fs );
		return NULL;
	}
	MEMSET(retval, 0, *len + 1);
	fread(retval, 1, *len, fs);

	fclose(fs);

	return retval;
}


GLuint egl_load_shader2(char* in_vshader, char* in_fshader, GLuint *vs, GLuint *fs)
{
#define BUF_SIZE 4096
	int len;
	char* vshader;
	char* fshader;
	GLint compiled;
	char* buffer;
	GLint linked = GL_TRUE;
	GLuint proggy = 0;

	buffer = (char *)MALLOC(BUF_SIZE);
	if ( NULL == buffer )	{
		__PRINTF(("Out of memory allocating filename buffer\n"));
		return 0;
	}
	*vs = glCreateShader(GL_VERTEX_SHADER);
	*fs = glCreateShader(GL_FRAGMENT_SHADER);
	proggy = glCreateProgram();
	glAttachShader(proggy, *fs);
	glAttachShader(proggy, *vs);

#ifdef __SYMBIAN32__
	_mali_sys_snprintf(buffer, BUF_SIZE, "z:\\shaders\\%s.glsl", in_vshader);
#else
	mali_tpi_snprintf(buffer, BUF_SIZE, "shaders/%s.glsl", in_vshader);
#endif
	vshader = (char*) egl_load_file(buffer, BUF_SIZE, &len, "r");
	if(!vshader)
	{
		__PRINTF(("Unable to load '%s'\n", buffer));
		FREE(buffer);
		return 0;
	}
	glShaderSource(*vs, 1, (const char**) &vshader, NULL);

	glCompileShader(*vs);


	glGetShaderiv(*vs, GL_COMPILE_STATUS, &compiled);
	if(compiled == GL_FALSE)
	{
		__PRINTF(("Critical Error: Unable to compile vertex shader '%s' \n", buffer));
		glGetShaderInfoLog(*vs, BUF_SIZE, NULL, buffer);
		__PRINTF((" - reason:\n%s\n\n", buffer));
		FREE(buffer);
		FREE(vshader);
		return 0;
	}

#ifdef __SYMBIAN32__
	_mali_sys_snprintf(buffer, BUF_SIZE, "z:\\shaders\\%s.glsl", in_fshader);
#else
	mali_tpi_snprintf(buffer, BUF_SIZE, "shaders/%s.glsl", in_fshader);
#endif
	fshader = (char*) egl_load_file(buffer, BUF_SIZE, &len, "r");
	if(!fshader)
	{
        __PRINTF(("Unable to load '%s'\n", buffer));
		FREE(buffer);
		FREE(vshader);
		return 0;
	}

	glShaderSource(*fs, 1, (const char**) &fshader, NULL);


	glCompileShader(*fs);
	glGetShaderiv(*fs, GL_COMPILE_STATUS, &compiled);
	if(compiled == GL_FALSE)
	{
		__PRINTF(("Critical Error: Unable to compile fragment shader '%s' \n", buffer));
		glGetShaderInfoLog(*fs, BUF_SIZE, NULL, buffer);
		__PRINTF((" - reason:\n%s\n\n", buffer));
		FREE(buffer);
		FREE(fshader);
		FREE(vshader);
		return 0;
	}


	FREE(vshader);
	FREE(fshader);

	glBindAttribLocation(proggy, 0, "position");

	glLinkProgram(proggy);
	glGetProgramiv(proggy, GL_LINK_STATUS, &linked);

	if(linked == GL_FALSE)
	{
		glGetProgramInfoLog(proggy, BUF_SIZE, NULL, buffer);
		__PRINTF(("Critical Error: Unable to link program - reason:\n%s\n\n", buffer));
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
		} else {
			/*__PRINTF(("WARNING: No multiplyviewprojection matrix present in shader \n"));*/
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
	{
		GLint pos = glGetAttribLocation(proggy, "position");
		if(pos == -1)
		{
			__PRINTF(("WARNING: No attribute position present in shader \n"));
		}
	}
	FREE(buffer);
	return proggy;
#undef BUF_SIZE
}


void egl_draw_custom_square_textured_offset(GLfloat* vertices, GLfloat xoffset, GLfloat yoffset, GLuint texture_id)
{
	GLfloat* offsetVerts;
	GLfloat* offsetVertsp;
	GLfloat texcoords[] = {
		0, 1,
		1, 1,
		1, 0,
		0, 0
	};

	GLushort indices[] =
	{
		0, 1, 2, /* first triangle */
		0, 2, 3  /* second triangle */
	};

	GLint pos;
	GLint proggy;
	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);

	offsetVertsp = offsetVerts = (GLfloat*)MALLOC(3*4*sizeof(GLfloat));
	if ( NULL == offsetVertsp ) return;

	offsetVerts[0] = vertices[0] + xoffset;
	offsetVerts[1] = vertices[1] + yoffset;
	offsetVerts[2] = vertices[2];
	offsetVerts[3] = vertices[3] + xoffset;
	offsetVerts[4] = vertices[4] + yoffset;
	offsetVerts[5] = vertices[5];
	offsetVerts[6] = vertices[6] + xoffset;
	offsetVerts[7] = vertices[7] + yoffset;
	offsetVerts[8] = vertices[8];
	offsetVerts[9] = vertices[9] + xoffset;
	offsetVerts[10] = vertices[10] + yoffset;
	offsetVerts[11] = vertices[11];

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
	FREE(offsetVertsp);
}

void egl_draw_custom_triangle(float *vertices, float *color)
{
	unsigned short indices[] = {0, 1, 2};
	GLint col;

	float color_orig[4];

	GLint proggy;
	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	col = glGetUniformLocation( proggy, "color" );
	if (col != -1)
	{
		glGetUniformfv(proggy, col, color_orig);
	}
	else
	{
		__PRINTF((" Warning: Current Program has no color uniform \n"));
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

void egl_draw_custom_triangle_offset(float *vertices, float *color, GLfloat xoffset, GLfloat yoffset)
{
	unsigned short indices[] = {0, 1, 2};
	GLfloat *verts;
	GLfloat *vertsp;
	GLint col;
	GLint proggy;
	float color_orig[4];

	vertsp = verts = (GLfloat*)MALLOC(3*3*sizeof(GLfloat));
	if ( NULL == vertsp ) return;

	verts[0] = vertices[0] + xoffset;
	verts[1] = vertices[1] + yoffset;
	verts[2] = vertices[2];

	verts[3] = vertices[3] + xoffset;
	verts[4] = vertices[4] + yoffset;
	verts[5] = vertices[5];

	verts[6] = vertices[6] + xoffset;
	verts[7] = vertices[7] + yoffset;
	verts[8] = vertices[8];

	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	col = glGetUniformLocation( proggy, "color" );
	if (col != -1)
	{
		glGetUniformfv(proggy, col, color_orig);
	}
	else
	{
		__PRINTF(("Warning: Current Program has no color uniform \n"));
	}

	glUniform4fv( col, 1, color );

	{
		GLint pos;
		pos = glGetAttribLocation(proggy, "position");
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, verts);
	}

	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);

	/* reset state to original */
	glUniform4fv( col, 1, color_orig );

	FREE( vertsp );
}

void egl_draw_custom_square_offset( float *vertices, float *color, GLfloat xoffset, GLfloat yoffset )
{
	GLfloat *verts;
	GLfloat *vertsp;
	GLushort indices[] =
	{
		0, 1, 2, /* first triangle */
		2, 1, 3  /* second triangle */
	};
	GLint col;
	GLint proggy;
	float color_orig[4];

	vertsp = verts = (GLfloat*)MALLOC(3*4*sizeof(GLfloat));
	if ( NULL == vertsp ) return;

	verts[0] = vertices[0] + xoffset;
	verts[1] = vertices[1] + yoffset;
	verts[2] = vertices[2];

	verts[3] = vertices[3] + xoffset;
	verts[4] = vertices[4] + yoffset;
	verts[5] = vertices[5];

	verts[6] = vertices[6] + xoffset;
	verts[7] = vertices[7] + yoffset;
	verts[8] = vertices[8];

	verts[9] = vertices[9] + xoffset;
	verts[10] = vertices[10] + yoffset;
	verts[11] = vertices[11];

	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	col = glGetUniformLocation( proggy, "color" );
	if (col != -1)
	{
		glGetUniformfv(proggy, col, color_orig);
	}
	else
	{
		__PRINTF(("Warning: Current Program has no color uniform \n"));
	}

	glUniform4fv( col, 1, color );

	{
		GLint pos;
		pos = glGetAttribLocation(proggy, "position");
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, verts);
	}

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

	/* reset state to original */
	glUniform4fv( col, 1, color_orig );

	FREE( vertsp );
}

#endif /* MALI_EGL_GLES_MAJOR_VERSION == 2 */
#endif /* EGL_MALI_GLES */
