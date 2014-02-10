/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef BACKBUFFERIMAGE_VG_H
#define BACKBUFFERIMAGE_VG_H

#include <stdlib.h>
#include <memory.h>
#include <suite.h>

typedef struct egl_backbuffer
{
	int width;
	int height;
	unsigned char * essl_mem_pool_data_buffer;
} egl_backbuffer;

#ifdef EGL_MALI_VG
#define get_backbuffer_vg(buffer, xres, yres) \
do { \
		(buffer)->width = (xres); \
		(buffer)->height = (yres); \
		(buffer)->essl_mem_pool_data_buffer = (unsigned char*) _test_mempool_alloc((&test_suite->fixture_pool), (xres)*(yres)*4); \
		memset((buffer)->essl_mem_pool_data_buffer, 0, (xres)*(yres)*4); \
		vgReadPixels( (buffer)->essl_mem_pool_data_buffer, (xres)*4, VG_sABGR_8888, 0, 0, (xres), (yres) ); \
} while (0)

#define get_backbuffer_vg_doublesize(buffer) \
do { \
		(buffer)->width = TEST_DEFAULT_EGL_WIDTH*2; \
		(buffer)->height = TEST_DEFAULT_EGL_HEIGHT*2; \
		(buffer)->essl_mem_pool_data_buffer = (unsigned char*) _test_mempool_alloc((&test_suite->fixture_pool), (TEST_DEFAULT_EGL_WIDTH*2)*(TEST_DEFAULT_EGL_HEIGHT*2)*4); \
		memset((buffer)->essl_mem_pool_data_buffer, 0, (TEST_DEFAULT_EGL_WIDTH*2)*(TEST_DEFAULT_EGL_HEIGHT*2)*4); \
		vgReadPixels( (buffer)->essl_mem_pool_data_buffer, (TEST_DEFAULT_EGL_WIDTH*2)*4, VG_sABGR_8888, 0, 0, (TEST_DEFAULT_EGL_WIDTH*2), (TEST_DEFAULT_EGL_HEIGHT*2) ); \
} while (0)


#endif

#ifdef EGL_MALI_GLES
#define get_backbuffer_gles(buffer, xres, yres) \
do { \
		(buffer)->width = (xres); \
		(buffer)->height = (yres); \
		(buffer)->essl_mem_pool_data_buffer = (u8*) _test_mempool_alloc((&test_suite->fixture_pool), (xres)*(yres)*4); \
		memset((buffer)->essl_mem_pool_data_buffer, 0, (xres)*(yres)*4); \
		glReadPixels(0, 0, (xres), (yres), GL_RGBA, GL_UNSIGNED_BYTE, (buffer)->essl_mem_pool_data_buffer); \
} while (0)
#endif

#define assert_pixel_color_and_alpha(img, x, y, r, g, b, a, allowedErrorBits, errorLog) \
	{\
	unsigned char ir = (img)->essl_mem_pool_data_buffer[(x)*4+0 + (img)->width*(y)*4]; \
	unsigned char ig = (img)->essl_mem_pool_data_buffer[(x)*4+1 + (img)->width*(y)*4]; \
	unsigned char ib = (img)->essl_mem_pool_data_buffer[(x)*4+2 + (img)->width*(y)*4]; \
	unsigned char ia = (img)->essl_mem_pool_data_buffer[(x)*4+3 + (img)->width*(y)*4]; \
	float fa = (float)(ia) / 255.0f; \
	float fr = (float)(ir) / 255.0f; \
	float fg = (float)(ig) / 255.0f; \
	float fb = (float)(ib) / 255.0f; \
	float eps = (float)(((int)(1<<allowedErrorBits)) / 255.0f); \
	int wrong_color = 0; \
	char str[256]; \
	if ( fabs(fa - (float)(a)) > fabs(eps) )wrong_color = 1; \
	if ( fabs(fr - (float)(r)) > fabs(eps) )wrong_color = 1; \
	if ( fabs(fg - (float)(g)) > fabs(eps) )wrong_color = 1; \
	if ( fabs(fb - (float)(b)) > fabs(eps) )wrong_color = 1; \
	if ( 1 == wrong_color ) \
	{ \
		sprintf(str, "color is %1.5f %1.5f %1.5f %1.5f, want %1.5f %1.5f %1.5f %1.5f (eps %f)", fr, fg, fb, fa, (float)(r), (float)(g), (float)(b), (float)(a), eps);\
		assert_fail(EGL_FALSE, dsprintf(test_suite, "%s : %s, %s:%i", str, (errorLog), __FILE__, __LINE__)); \
	} \
	}

#define assert_pixel_color(img, x, y, r, g, b, allowedErrorBits, errorLog) \
	{\
	unsigned char ir = (img)->essl_mem_pool_data_buffer[(x)*4+0 + (img)->width*(y)*4]; \
	unsigned char ig = (img)->essl_mem_pool_data_buffer[(x)*4+1 + (img)->width*(y)*4]; \
	unsigned char ib = (img)->essl_mem_pool_data_buffer[(x)*4+2 + (img)->width*(y)*4]; \
	float fr = (float)(ir) / 255.0f; \
	float fg = (float)(ig) / 255.0f; \
	float fb = (float)(ib) / 255.0f; \
	float eps = (float)(((int)(1<<allowedErrorBits)) / 255.0f); \
	assert_floats_equal(fr, r, eps, errorLog); \
	assert_floats_equal(fg, g, eps, errorLog); \
	assert_floats_equal(fb, b, eps, errorLog); \
	}

/* The following required for OSs that have problems with pixmaps alpha channels
 * In this case, we limit the testing to that of just the non-alpha channels, using
 * this abstraction: */
#ifdef EGL_PIXMAP_NOALPHA
/* This ignores the alpha channel */
#define PIXMAP_RELATED_ASSERT_PIXEL_COLOR_AND_ALPHA(img, x, y, r, g, b, a, allowedErrorBits, errorLog) \
	assert_pixel_color(img, x, y, r, g, b, allowedErrorBits, errorLog)
#else /* EGL_PIXMAP_NOALPHA */
/* This would be the same as using assert_pixel_color_and_alpha() */
#define PIXMAP_RELATED_ASSERT_PIXEL_COLOR_AND_ALPHA(img, x, y, r, g, b, a, allowedErrorBits, errorLog) \
	assert_pixel_color_and_alpha(img, x, y, r, g, b, a, allowedErrorBits, errorLog)
#endif /* EGL_PIXMAP_NOALPHA */

#endif

