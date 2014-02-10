/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef GLES_MIPMAP_FILTER_H
#define GLES_MIPMAP_FILTER_H

#include "shared/mali_convert.h"

/*
 * _gles_downsample_span_rgba8888 receives as input a portion of a RGBA8888
 * image and writes into dst the result of a 2x2 box filter applyed to it.
 * This function is useful to work with a constant amount of memory since
 * we can pass it our src image piece by piece.
 */
void _gles_downsample_span_rgba8888(const u8 *src, int src_pitch, u8 *dst, int width, int scale_x, int scale_y, unsigned int scale_rcp);

/*
 * _gles_downsample_rgba8888 accept a generic size image as input and applies
 * a 1x1 or 2x1 or 1x2 or 2x2 box filter to it writing the result to dst.
 * If the input image has an odd length or widht, this function calls
 * _gles_odd_npot_to_even_and_downsample_rgba8888.
 */
void _gles_downsample_rgba8888(
	const void *src, int src_width, int src_height, int src_pitch,
	void *dst, int dst_width, int dst_height, int dst_pitch,
	enum mali_convert_pixel_format format
);

/*
 * _gles_odd_npot_to_even_and_downsample_rgba8888 scale the src to its next
 * lower even equivalent and calls _gles_downsample_rgba8888 at the end.
 * For example, if we have a 63x12 texture, we will obtain a 62x12 texture for src.
 * This function does NOT preserve the data inside src: it overwrite it with the new even size texture.
 * http://wiki.emea.arm.com/Sandbox/OddNPOTMipMap
 */
void _gles_odd_npot_to_even_and_downsample_rgba8888(
	const void *src, int src_width, int src_height, int src_pitch,
	void *dst, int dst_width, int dst_height, int dst_pitch,
	enum mali_convert_pixel_format format
);

#endif /* GLES_MIPMAP_FILTER_H */
