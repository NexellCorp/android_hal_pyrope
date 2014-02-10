/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "gles_mipmap_filter.h"
#include <gles_config.h>

/*
 * Help function that read count pixels from src using the src_format 
 * as bit format and, depending on method, converts them into 
 * RGBA8888 pixels writing them into dst.
 */
MALI_STATIC void gles_mipmap_filter_convert_to_rgba8888(u8 *dst, const void *src, int count, enum mali_convert_pixel_format src_format, mali_convert_method method)
{
	/* convert span from format to rgba8888 from source */
	switch (method)
	{
	case MALI_CONVERT_8BITS:
		_mali_convert_8bit_to_rgba8888(dst, (const u8*)src, count, src_format);
		break;

	case MALI_CONVERT_PACKED:
		_mali_convert_16bit_to_rgba8888(dst, (const u16*)src, count, src_format);
		break;

		/* handle errors */
	default: MALI_DEBUG_ASSERT(0, ("Invalid type size!"));
	}

}

/*
 * Help function that read count RGBA8888 pixels from src and, depending on 
 * method, converts them into the bit format of dst_format writing them into dst.
 */
MALI_STATIC void gles_mipmap_filter_convert_from_rgba8888(void *dst, const u8 *src, int count, enum mali_convert_pixel_format dst_format, mali_convert_method method)
{
	/* convert span from rgba8888 to format to destination */
	switch (method)
	{
	case MALI_CONVERT_8BITS:
		_mali_convert_rgba8888_to_8bit((u8*)dst, src, count, dst_format);
		break;

	case MALI_CONVERT_PACKED:
		_mali_convert_rgba8888_to_16bit((u16*)dst, src, count, dst_format);
		break;

		/* handle errors */
	default: MALI_DEBUG_ASSERT(0, ("Invalid type size!"));
	}

}

void _gles_downsample_span_rgba8888(const u8 *src, int src_pitch, u8 *dst, int src_width, int scale_x, int scale_y, unsigned int scale_rcp)
{
	int x;

	/* iterate through destination-pixels */
	for (x = 0; x < src_width; x += scale_x)
	{
		int i, ix, iy;
		unsigned int accum[4] = { 0, 0, 0, 0 };

		/* iterate through source-pixels */
		for (iy = 0; iy < scale_y; ++iy)
		{
			for (ix = 0; ix < scale_x; ++ix)
			{
				int pixel_offset = src_pitch * iy + (x + ix) * 4;

				/* accumulate all channels for this pixel */
				for (i = 0; i < 4; ++i)
				{
					accum[i] += src[pixel_offset + i];
				}
			}
		}

		/* write out destination pixel */
		for (i = 0; i < 4; ++i)
		{
			dst[i] = (u8)((accum[i] * scale_rcp) >> 24);
		}
		dst += 4;
	}
}

void _gles_downsample_rgba8888(
	const void *src, int src_width, int src_height, int src_pitch,
	void *dst, int dst_width, int dst_height, int dst_pitch,
	enum mali_convert_pixel_format format
	)
{
	int y;
	mali_convert_method method;
	int pixel_size;
	int scale_x;
	int scale_y;
	unsigned int scale_rcp;

	/* if original texture has an odd edge different by 1 and we are actually scaling */
	if( ( ( src_width & 1 ) && ( src_width != 1 ) && ( src_width != dst_width ) ) || 
		( ( src_height & 1 ) && ( src_height != 1 ) && ( src_height != dst_height ) ) ) 
	{
		/* call the NPOT downsample function */
		_gles_odd_npot_to_even_and_downsample_rgba8888(
			src, src_width, src_height, src_pitch, 
			dst, dst_width, dst_height, dst_pitch,
			format
			);

		return;
	}

	method = _mali_convert_pixel_format_get_convert_method(format);
	pixel_size = _mali_convert_pixel_format_get_size(format);

	/* calculate scaling factors */
	scale_x = src_width / dst_width;
	scale_y = src_height / dst_height;

	scale_rcp = (1 << 24) / (scale_x * scale_y);

	/* verify that scaling factors are integer */
 	MALI_DEBUG_ASSERT(scale_x * dst_width  == src_width,  ("non-integer x-scaling (non-power-of-two input? %d, %d) ", src_width, dst_width));
	MALI_DEBUG_ASSERT(scale_y * dst_height == src_height, ("non-integer y-scaling (non-power-of-two input? %d, %d) ", src_height, dst_height));
	MALI_DEBUG_ASSERT(scale_x == 1 || scale_x == 2, ("scale_x should be 1 or 2, it is %d", scale_x));
	MALI_DEBUG_ASSERT(scale_y == 1 || scale_y == 2, ("scale_y should be 1 or 2, it is %d", scale_x));

	/* the function will require (2 * 4 + 2) * MAX_SPAN_WIDTH bytes of stack storage.
	   MAX_SPAN_WIDTH must be a multiple of two. */
#define MAX_SPAN_WIDTH 128 /* 1280 bytes stack usage */

	for (y = 0; y < dst_height; ++y)
	{
		int src_x = 0;
		int dst_x = 0;

		/* subdivide a destintation-scanline into a set of spans of maximum MAX_SPAN_WIDTH */
		while (dst_x < dst_width)
		{
			u8 src_span_rgba8888[2][4 * MAX_SPAN_WIDTH];
			u8 dst_span_rgba8888[4 * MAX_SPAN_WIDTH];

			int src_span_width = MIN(MAX_SPAN_WIDTH, src_width - src_x);
			int dst_span_width = src_span_width / scale_x;

			/* convert span from format to rgba8888 from source */
			/* span from first scanline */
			gles_mipmap_filter_convert_to_rgba8888(src_span_rgba8888[0], (u8*)src + src_x * pixel_size, src_span_width, format, method);
			/* span from second scanline */
			if (1 < scale_y) gles_mipmap_filter_convert_to_rgba8888(src_span_rgba8888[1], (u8*)src + src_pitch + src_x * pixel_size, src_span_width, format, method);

			/* filter span */
			_gles_downsample_span_rgba8888(
				&src_span_rgba8888[0][0], MAX_SPAN_WIDTH * 4, /* src, src_pitch */
				dst_span_rgba8888,                            /* dst */
				src_span_width,                               /* src_width (before scaling) */
				scale_x, scale_y, scale_rcp                   /* scale factors */
				);

			/* convert span from rgba8888 to format to destination */
			gles_mipmap_filter_convert_from_rgba8888( (u8*)dst + dst_x * pixel_size, dst_span_rgba8888, dst_span_width, format, method);

			src_x += src_span_width;
			dst_x += dst_span_width;
		}

		/* update pointers */
		src = (u8*)src + src_pitch * scale_y;
		dst = (u8*)dst + dst_pitch;
	}

#undef MAX_SPAN_WIDTH
}

void _gles_odd_npot_to_even_and_downsample_rgba8888(
	const void *src, int src_width, int src_height, int src_pitch,
	void* dst, int dst_width, int dst_height, int dst_pitch,
	enum mali_convert_pixel_format format
	)
{
	const u8 reduce_width = src_width & 1 && src_width != 1;
	const u8 reduce_height = src_height & 1 && src_height != 1;
	const int even_src_width = src_width - reduce_width;
	const int even_src_height = src_height - reduce_height;

	/* calculate scaling factors */
	const int scale_x = even_src_width / dst_width;
	const int scale_y = even_src_height / dst_height;

	const unsigned int scale_rcp = (1 << 24) / (scale_x * scale_y);

	/* line_buff is a static vector able to contain at most 3 scanlines */
	/* since the function handles RGBA8888 only, we need to allocate 4 bytes per pixel */
	u8 lines_buff[3][GLES_MAX_TEXTURE_SIZE*4];
	/* dst_span_rgba8888 contains the result of the 2x2 box filter */
	u8 dst_span_rgba8888[GLES_MAX_TEXTURE_SIZE*4];
	
	/* 
	 * To reduce an image to its next smaller even equivalent, we will need to read
	 * at most the content of a 2x2 image window. The largest window we can have is
	 * the one that scale down a 3x3 texture to a 2x2 one: it needs a 3/2 = 1.5 wide window.
	 * The best case is, on the other hand a 4095x4095 scaled down to a 4094x4094 that
	 * need a 1.00024426 wide window. In case there is no scale because one within
	 * the width or the height of the original image are even, one of the filter window's
	 * edges will be equal to 1.
	 * Taking into account these considerations, we will only need to cache 2 lines from
	 * the original image.
	 * dest_line is the line where the algorithm is currently writing
	 */
	u8* curr_line = NULL;
	u8* next_line = NULL;
	u8* dest_line = NULL;

	/* 
	 * The 4 colors needed by our 2x2 downsample filter are stored in t vector
	 * t[0]=(x,y), t[1]=(x+1,y), t[2]=(x,y+1), t[3]=(x+1,y+1) 
	 */
	u8 t[4][4];
	/* 
	 * The percentage of each color we need to take into account for each coordinate
	 * is stored in the 4 w values:
	 * w[0] = x width percentage; w[1] = x+1 width percentage;
	 * w[2] = y heght percentage; w[3] = y+1 height percentage;
	 */
	float w[4];
	
	/* 
	 * src_ptr points to data in the original src image while  
	 * dest_ptr points to data in the destination image
	 */
	u8* src_ptr = (u8*) src;
	u8* dest_ptr = (u8*) dst;

	int x, y, ci;
	
	float x_to_sub = ((float)src_width / (float)even_src_width) - 1.f;
	float y_to_sub = ((float)src_height / (float)even_src_height) - 1.f;
	float inv_mean_factor = 1.0f / ( (x_to_sub + 1.0f) * (y_to_sub + 1.0f) );

	mali_convert_method method =
		_mali_convert_pixel_format_get_convert_method(format);

	w[0] = 1.0f;
	w[1] = x_to_sub;
	w[2] = 1.0f;
	w[3] = y_to_sub;

	curr_line = lines_buff[2];
	gles_mipmap_filter_convert_to_rgba8888(curr_line, src_ptr, src_width, format, method);

	for ( y = 0; y < even_src_height; ++y )
	{
		/* curr_line is the previous next_line */
		curr_line = lines_buff[(y&1)?1:2];
		next_line = lines_buff[(y&1)?2:1];
		dest_line = lines_buff[(y&1)?1:0];
		/* force the dest_line to be always the line 0 if we are not scaling the height */
		if ( !reduce_height ) dest_line = lines_buff[0];

		/* if there is another line, let's copy it into next_line */
		if( y < ( src_height - 1) )
		{
			src_ptr += src_pitch;
			gles_mipmap_filter_convert_to_rgba8888(next_line, src_ptr, src_width, format, method);
		}

		for ( x = 0; x < even_src_width; ++x )
		{
			float color_accumulator[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			
			/*
			 * Multiplying the percentage for each axis between them,
			 * we obtain the percentage we need for each color in the 
			 * 2x2 window
			 */
			float mw[4];
			mw[0] = w[0] * w[2];
			mw[1] = w[1] * w[2];
			mw[2] = w[0] * w[3];
			mw[3] = w[1] * w[3];

			/* sample required pixels */
			for ( ci = 3; ci >= 0; --ci ) {
				/*
				 * Due to the nature of this scaling, we always need the pixel at position (x,y).
				 * We need the pixels at y+1 (next_line) only if we need to reduce the height and
				 * we need the pixels at x+1 only if we are going to reduce the width.
				 */
				t[0][ci] = curr_line[( ( x ) * 4 ) + ci];
				t[1][ci] = ( reduce_width ) ? ( curr_line[( ( x + 1 ) * 4 ) + ci] ) : 0x0;
				t[2][ci] = ( reduce_height ) ? ( next_line[( ( x ) * 4 ) + ci] ) : 0x0;
				t[3][ci] = ( reduce_height && reduce_width ) ? ( next_line[( ( x + 1 ) * 4 ) + ci] ) : 0x0;
				
				/* Accumulates the colors in a float accumulator */
				color_accumulator[ci] += mw[0] * (float)(t[0][ci]);
				color_accumulator[ci] += mw[1] * (float)(t[1][ci]);
				color_accumulator[ci] += mw[2] * (float)(t[2][ci]);
				color_accumulator[ci] += mw[3] * (float)(t[3][ci]);

				/* It is ok to assign the color to the current line 
				 * since the color will not be read again. 
				 * The (u8) casting floors the float value, so we need to add 0.5 
				 */
				dest_line[x*4 + ci] = (u8) ((color_accumulator[ci] * inv_mean_factor) + 0.5f );
			}

			/* update x weigths for next iteration */
			w[0] -= x_to_sub;
			w[1] = ( x_to_sub + 1.0f ) - w[0];
		}

		/* reset x weigths for next iteration */
		w[0] = 1.0f;
		w[1] = x_to_sub;

		/* update y weigths for next iteration */
		w[2] -= y_to_sub;
		w[3] = ( y_to_sub + 1.0f ) - w[2];

		/* if we are not reducing the height we must apply the 2x1 box filter for every y */
		/* otherwise, we must apply the 2x2 or 1x2 box filter only for odd y */
		if ( ( !reduce_height ) || y&1 )
		{
			/* filter span */
			_gles_downsample_span_rgba8888(
				&lines_buff[0][0], GLES_MAX_TEXTURE_SIZE*4,   /* src, src_pitch */
				dst_span_rgba8888,                            /* dst */
				even_src_width,                               /* src_width */
				scale_x, scale_y, scale_rcp                   /* scale factors */
				);

			/* convert span from rgba8888 to format to destination */
			gles_mipmap_filter_convert_from_rgba8888( dest_ptr, dst_span_rgba8888, dst_width, format, method);
			dest_ptr += dst_pitch;
		}
	}
}
