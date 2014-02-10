/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "egl_convert.h"
#include <shared/mali_pixel_format.h>

void _egl_convert_get_shifts( int shift[4], mali_pixel_format format )
{
	switch ( format )
	{
		case MALI_PIXEL_FORMAT_R5G6B5:
			shift[0] = 11;
			shift[1] = 5;
			shift[2] = 0;
			shift[3] = 0;
			break;

		case MALI_PIXEL_FORMAT_A8R8G8B8:
			shift[0] = 24;
			shift[1] = 16;
			shift[2] = 8;
			shift[3] = 0;
			break;

		case MALI_PIXEL_FORMAT_A4R4G4B4:
			shift[0] = 12;
			shift[1] = 8;
			shift[2] = 4;
			shift[3] = 0;
			break;

		case MALI_PIXEL_FORMAT_A1R5G5B5:
			shift[0] = 15;
			shift[1] = 10;
			shift[2] = 5;
			shift[3] = 0;
			break;

		default: MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
	}
}

void _egl_convert_get_component_size( int size[4], mali_pixel_format format )
{
	switch (format)
	{
		case MALI_PIXEL_FORMAT_R5G6B5:
			size[0] = 5;
			size[1] = 6;
			size[2] = 5;
			size[3] = 0;
			break;

		case MALI_PIXEL_FORMAT_A8R8G8B8:
			size[0] = 8;
			size[1] = 8;
			size[2] = 8;
			size[3] = 8;
			break;

		case MALI_PIXEL_FORMAT_A4R4G4B4:
			size[0] = 4;
			size[1] = 4;
			size[2] = 4;
			size[3] = 4;
			break;

		case MALI_PIXEL_FORMAT_A1R5G5B5:
			size[0] = 1;
			size[1] = 5;
			size[2] = 5;
			size[3] = 5;
			break;

		default: MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
	}
}

MALI_STATIC_INLINE int _egl_convert_extract_value( int v, int shift, int size )
{
	return (v>>shift) & ( (1<<size) - 1);
}

MALI_STATIC_INLINE int _egl_convert_expand_to_8bit( int v, const int size )
{
	MALI_DEBUG_ASSERT( (8 > size), ("source format is bigger than destination (%d bits)", size));
	return v << (8 - size);
}

void _egl_convert_32bit_to_32bit( u32 *dst, const u8 *src, int width, int height, int dst_pitch, int src_pitch, mali_pixel_format src_format, int dst_shift[4], int dst_size[4] )
{
	int x, y, j;
	int src_shift[4];
	int src_size[4];

	_egl_convert_get_shifts( src_shift, src_format );
	_egl_convert_get_component_size( src_size, src_format );

	for ( y=0; y<height; y++ )
	{
		for ( x=0; x<width; x++ )
		{
			u32 channels[4];

			channels[0] = *src++;
			channels[1] = *src++;
			channels[2] = *src++;
			channels[3] = *src++;
			*dst = 0;

			for ( j=0; j<4; j++ ) *dst |= ( (channels[j] & ((1<<dst_size[j])-1) ) << dst_shift[j] );
			dst++;
		}
		dst += (dst_pitch>>2) - width;
		src += src_pitch - (width<<2);
	}
}

void _egl_convert_32bit_to_16bit( u16 *dst, const u8 *src, int width, int height, int dst_pitch, int src_pitch, mali_pixel_format src_format, int dst_shift[4], int dst_size[4] )
{
	int x, y;
	int src_shift[4];
	int src_size[4];
	int channels[4];
	unsigned int pixel = 0;

	_egl_convert_get_shifts( src_shift, src_format );
	_egl_convert_get_component_size( src_size, src_format );

	for ( y=0; y<height; y++ )
	{
		for ( x=0; x<width; x++ )
		{
			pixel = 0;

			channels[2] = *src++;
			channels[1] = *src++;
			channels[0] = *src++;
			channels[3] = *src++;

			pixel |= (channels[0] >> ( 8 - dst_size[0] ) ) << dst_shift[0];
			pixel |= (channels[1] >> ( 8 - dst_size[1] ) ) << dst_shift[1];
			pixel |= (channels[2] >> ( 8 - dst_size[2] ) ) << dst_shift[2];
			pixel |= (channels[3] >> ( 8 - dst_size[3] ) ) << dst_shift[3];

			*dst++ = pixel;
		}

		dst+= (dst_pitch>>1) - width;
		src+= src_pitch - (width<<2);
	}

}

void _egl_convert_16bit_to_32bit( u32 *dst, u16 *src, int width, int height, int dst_pitch, int src_pitch, mali_pixel_format src_format, int dst_shift[4], int dst_size[4] )
{
	int x, y, j;
	int src_shift[4];
	int src_size[4];
	MALI_IGNORE( dst_size );

	_egl_convert_get_shifts( src_shift, src_format );
	_egl_convert_get_component_size( src_size, src_format );

	for ( y=0; y<height; y++ )
	{
		for ( x=0; x<width; x++ )
		{
			int channels[4];
			u16 pixel = *src++;

			for ( j=0; j<4; j++ )
			{
				u32 tmp = _egl_convert_extract_value( pixel, src_shift[j], src_size[j] );
				channels[j] = _egl_convert_expand_to_8bit( tmp, src_size[j] );
			}

			*dst++ = (channels[2] << dst_shift[2] ) | (channels[1] << dst_shift[1]) | (channels[0] << dst_shift[0] );
		}

		dst+= (dst_pitch>>2) - width;
		src+= (src_pitch>>1) - width;
	}
}

void _egl_convert_16bit_to_16bit( u16 *dst, const u16 *src, int width, int height, int dst_pitch, int src_pitch, mali_pixel_format src_format, int dst_shift[4], int dst_size[4] )
{
	int x, y, j;
	int src_shift[4];
	int src_size[4];

	_egl_convert_get_shifts( src_shift, src_format );
	_egl_convert_get_component_size( src_size, src_format );

	for ( y=0; y<height; y++ )
	{
		for ( x=0; x<width; x++ )
		{
			unsigned int pixel_src = *src++;
			unsigned int pixel_dst = 0;

			for ( j=0; j<4; j++ )
			{
				int tmp = _egl_convert_extract_value( pixel_src, src_shift[j], src_size[j] );
				pixel_dst |= ( (tmp & ((1<<dst_size[j])-1) ) << dst_shift[j] );
			}

			*dst++ = pixel_dst;
		}
		dst += (dst_pitch/2) - width;
		src += (src_pitch/2) - width;
	}
}

