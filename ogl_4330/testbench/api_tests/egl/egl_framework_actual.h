/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _EGL_FRAMEWORK_ACTUAL_
#define _EGL_FRAMEWORK_ACTUAL_

#include <mali_system.h>

#ifdef EGL_MALI_GLES
#include <opengles/gles_base.h>
#endif

#ifdef EGL_MALI_VG
#include <VG/mali_openvg.h>
#include <VG/mali_vgu.h>
#endif
#include <EGL/mali_egl.h>

#ifndef EXCLUDE_UNIT_FRAMEWORK
#include "suite.h"
#include "egl_helpers_pixmap.h"
#include "test_helpers.h"
#include <suite.h>
#include "backbufferimage_vg.h"
#else /* EXCLUDE_UNIT_FRAMEWORK */
#ifdef __SYMBIAN32__
#define USE_MALI_SYS_FUNCS_TO_ALLOW_LEAK_TESTING	1
#else
#define USE_MALI_SYS_FUNCS_TO_ALLOW_LEAK_TESTING	0
#endif
#if USE_MALI_SYS_FUNCS_TO_ALLOW_LEAK_TESTING /* Enable this to check for memory leaks in the tests and framework in addition to the driver */
    #define MALLOC_FUNC _mali_sys_malloc
    #define CALLOC_FUNC _mali_sys_calloc
    #define FREE_FUNC   _mali_sys_free
    #define MEMSET_FUNC _mali_sys_memset
#else
    #define MALLOC_FUNC malloc
    #define CALLOC_FUNC calloc
    #define FREE_FUNC   free
    #define MEMSET_FUNC memset
#endif

#define MALLOC(a) MALLOC_FUNC((a))
#define CALLOC(a,b) CALLOC_FUNC((a), (b))
#define FREE(a) FREE_FUNC((a))
#define MEMSET(dst,val,size) MEMSET_FUNC((dst),(val),(size))
#endif

#define TEST_DEFAULT_EGL_WIDTH  16
#define TEST_DEFAULT_EGL_HEIGHT 16
#define TEST_DEFAULT_EGL_WIDTH_LARGE  64
#define TEST_DEFAULT_EGL_HEIGHT_LARGE 64

#ifndef EXCLUDE_UNIT_FRAMEWORK
void* egl_create_fixture(suite* test_suite);
void egl_remove_fixture(suite* test_suite);
void pixmap_save( suite *test_suite, const char *filename, EGLNativePixmapType pixmap );
/**
 * @brief Fill a native pixmap with colored rectangles
 *
 * Subdivides the area of a native pixmap into a grid of @a nrows by
 * @a ncolumns. Each cell will be filled using a color read from a given
 * array of colors. The colors must be specified in row-major order from
 * left to right, and the number of colors must be greater than or equal to
 * @a nrows multiplied by @a ncolumns.
 *
 * @param[in] test_suite   Test suite context to make allocations/log errors
 * @param[in] pixmap       Native pixmap to be filled.
 * @param[in] colors       Array of colors for rectangles (row-major order).
 * @param[in] ncolumns     Number of columns in the @a colors array.
 * @param[in] nrows        Number of rows in the @a colors array.
 * @param[in] format       Format of the colors in the arrays (must match
 *                         the pixmap).
 *
 * @warning You should not use a two-dimensional array with this function
 *          because (&colors[0][0])[x] is undefined for x >= ncolumns,
 *          according to a strict interpretation of the ANSI C standard.
 */
void egl_pixmap_fill_grid( suite *test_suite, EGLNativePixmapType pixmap, const u32 colors[], EGLint ncolumns, EGLint nrows, display_format format );
#endif /* EXCLUDE_UNIT_FRAMEWORK */

#ifdef EGL_MALI_VG
void vg_save_backbuffer( const char *filename, int x, int y, int width, int height );
#endif

#ifdef EGL_MALI_GLES
void gl_save_backbuffer( const char *filename, int x, int y, int width, int height );
void egl_draw_custom_triangle(float *vertices, float *color);
#if MALI_EGL_GLES_MAJOR_VERSION == 1
void egl_draw_custom_square_offset( float *vertices, float *color, GLfloat xoffset, GLfloat yoffset );
void egl_draw_square_textured_offset(GLfloat* vertices, GLfloat xoffset, GLfloat yoffset, GLuint texture_id);
void egl_draw_triangle(float *vertices, float *color);
void egl_draw_triangle_offset(float *vertices, float *color, GLfloat xoffset, GLfloat yoffset);
#else
GLuint egl_unload_shader2( GLuint proggy, GLuint vs, GLuint fs);
GLuint egl_load_shader2(char* in_vshader, char* in_fshader, GLuint *vs, GLuint *fs);
void egl_draw_custom_square_textured_offset(GLfloat* vertices, GLfloat xoffset, GLfloat yoffset, GLuint texture_id);
void egl_draw_custom_triangle_offset(float *vertices, float *color, GLfloat xoffset, GLfloat yoffset);
void egl_draw_custom_square_offset( float *vertices, float *color, GLfloat xoffset, GLfloat yoffset );
#endif
#endif

#ifndef EXCLUDE_EGL_SPECIFICS

/** Opaque type for windowing abstraction state */
typedef struct egl_helper_windowing_state_T egl_helper_windowing_state;

/** Fixture for use across all EGL tests */
typedef struct egl_framework_fixture_T
{
       egl_helper_windowing_state *windowing_state;
} egl_framework_fixture;

#endif /* EXCLUDE_EGL_SPECIFICS */

#endif /* _EGL_FRAMEWORK_ACTUAL_ */

