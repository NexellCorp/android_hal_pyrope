/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_convert.h
 * @brief Handles conversion of pixel formats.
 */

#ifndef _EGL_CONVERT_H_
#define _EGL_CONVERT_H_
#include <shared/mali_pixel_format.h>

/** Convert from one 32bit format to another 32bit format
 */
void _egl_convert_32bit_to_32bit( u32 *dst, const u8 *src, int width, int height, int dst_pitch, int src_pitch, mali_pixel_format src_format, int dst_shift[4], int dst_size[4] );

/** Convert from 32bit to 16bit
 */
void _egl_convert_32bit_to_16bit( u16 *dst, const u8 *src, int width, int height, int dst_pitch, int src_pitch, enum mali_pixel_format src_format, int dst_shift[4], int dst_size[4] );


/** Convert from 16bit to 32bit
 */
void _egl_convert_16bit_to_32bit( u32 *dst, u16 *src, int width, int height, int dst_pitch, int src_pitch, mali_pixel_format src_format, int dst_shift[4], int dst_size[4] );

/** Convert from 16bit to another 16bit format
 */
void _egl_convert_16bit_to_16bit( u16 *dst, const u16 *src, int width, int height, int dst_pitch, int src_pitch, enum mali_pixel_format src_format, int dst_shift[4], int dst_size[4] );

#endif /* _EGL_CONVERT_H_ */
