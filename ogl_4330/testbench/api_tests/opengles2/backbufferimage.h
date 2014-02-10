/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef BACKBUFFERIMAGE_H
#define BACKBUFFERIMAGE_H

#include <stdlib.h>
#include <memory.h>
#include "../unit_framework/suite.h"
#include "gl2_framework.h"

typedef struct BackbufferImage
{
	int width;
	int height;
	u8* test_mem_pool_data_buffer;
} BackbufferImage;

/* this is the fire-and-forget getter for backbuffer memory. Cleans up automatically on testbench exit
 * 
 * if your test does many iterations and needs to grab the backbuffer all the time, don't use this
 * (but the variants below) OR use this only for first time and then call grab_backbuffer during the 
 * following iterations. */
#define get_backbuffer(buffer) \
do { \
		(buffer)->width = WIDTH; \
		(buffer)->height = HEIGHT; \
		(buffer)->test_mem_pool_data_buffer = (u8*) _test_mempool_alloc((&test_suite->fixture_pool), WIDTH*HEIGHT*4); \
		if( (buffer)->test_mem_pool_data_buffer == NULL) \
		{  \
			printf("Out of memory while allocating buffer for readpixels\n"); \
			exit(1); \
		} \
		memset((buffer)->test_mem_pool_data_buffer, 0, WIDTH*HEIGHT*4); \
		glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, (buffer)->test_mem_pool_data_buffer); \
} while (0)

/* in some cases, the above scheme will run out of memory; in case we can use the two below.
 * Calling free_backbuffer on a buffer acquired through get_backbuffer will crash - don't do it. 
 *
 * this only allocates new backbuffer, doesn't grab the contents */
#define allocate_backbuffer(buffer) \
do { \
		(buffer)->width = WIDTH; \
		(buffer)->height = HEIGHT; \
		(buffer)->test_mem_pool_data_buffer = (u8*) malloc(WIDTH*HEIGHT*4); \
		if( (buffer)->test_mem_pool_data_buffer == NULL) \
		{  \
			printf("allocate_backbuffer: Out of memory\n"); \
			exit(1); \
		} \
} while (0)

/* use this to just grab the fb contents into existing buffer */
#define grab_backbuffer(buffer) \
do { \
		if( (buffer)->test_mem_pool_data_buffer == NULL) \
		{  \
			printf("grab_backbuffer: buffer = NULL\n"); \
			exit(1); \
		} \
		memset((buffer)->test_mem_pool_data_buffer, 0, WIDTH*HEIGHT*4); \
		glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, (buffer)->test_mem_pool_data_buffer); \
} while (0)

/* allocate new backbuffer from system memory instead of test memory pool (not automagically freed, 
 * you need to do it yourself after you don't need the buffer anymore) */
#define allocate_and_get_backbuffer(buffer) \
do { \
		allocate_backbuffer(buffer); \
		grab_backbuffer(buffer); \
} while(0)

/* free the backbuffer */
#define free_backbuffer(buffer) \
do { \
		free( (buffer)->test_mem_pool_data_buffer ); \
		(buffer)->test_mem_pool_data_buffer = NULL; \
} while (0)

#define assert_pixel_black(img, x, y, errorlog) ASSERT_GLES_FB_COLORS_RGB_EQUAL(img, x, y, gles_color_make_4f(0, 0, 0, 1), gles_float_epsilon)
#define assert_pixel_red(img, x, y, errorlog)   ASSERT_GLES_FB_COLORS_RGB_EQUAL(img, x, y, gles_color_make_4f(1, 0, 0, 1), gles_float_epsilon)
#define assert_pixel_green(img, x, y, errorlog) ASSERT_GLES_FB_COLORS_RGB_EQUAL(img, x, y, gles_color_make_4f(0, 1, 0, 1), gles_float_epsilon)
#define assert_pixel_blue(img, x, y, errorlog)  ASSERT_GLES_FB_COLORS_RGB_EQUAL(img, x, y, gles_color_make_4f(0, 0, 1, 1), gles_float_epsilon)
#define assert_pixel_white(img, x, y, errorlog) ASSERT_GLES_FB_COLORS_RGB_EQUAL(img, x, y, gles_color_make_4f(1, 1, 1, 1), gles_float_epsilon)

#endif

