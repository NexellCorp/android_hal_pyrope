/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "shared/mali_convert.h"

MALI_EXPORT void _mali_convert_get_16bit_shifts(int shift[4], enum mali_convert_pixel_format format)
{
	switch (format)
	{
	case MALI_CONVERT_PIXEL_FORMAT_R5G6B5:
		shift[0] = 11;
		shift[1] = 5;
		shift[2] = 0;
		shift[3] = 0;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_R4G4B4A4:
		shift[0] = 12;
		shift[1] = 8;
		shift[2] = 4;
		shift[3] = 0;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_R5G5B5A1:
		shift[0] = 11;
		shift[1] = 6;
		shift[2] = 1;
		shift[3] = 0;
		break;

		/* handle errors */
	default: MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
	}
}

MALI_EXPORT void _mali_convert_get_16bit_component_size(int size[4], enum mali_convert_pixel_format format)
{
	switch (format)
	{
	case MALI_CONVERT_PIXEL_FORMAT_R5G6B5:
		size[0] = 5;
		size[1] = 6;
		size[2] = 5;
		size[3] = 0;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_R4G4B4A4:
		size[0] = 4;
		size[1] = 4;
		size[2] = 4;
		size[3] = 4;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_R5G5B5A1:
		size[0] = 5;
		size[1] = 5;
		size[2] = 5;
		size[3] = 1;
		break;

		/* handle errors */
	default: MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
	}
}

MALI_STATIC MALI_INLINE int _mali_extract_value(int v, int shift, int size)
{
	return (v >> shift) & ((1 << size) - 1);
}

MALI_STATIC MALI_INLINE int _mali_expand_to_8bit(int v, const int size)
{
	MALI_DEBUG_ASSERT((8 > size), ("source format is bigger than destination (%d bits)", size));
	return v << (8 - size);
}

MALI_EXPORT void _mali_convert_16bit_to_rgba8888(u8 *dst, const u16 *src, int count, enum mali_convert_pixel_format src_format)
{
	int i;
	int shift[4];
	int size[4];
	int implicit_alpha = 0;

	_mali_convert_get_16bit_shifts(shift, src_format);
	_mali_convert_get_16bit_component_size(size, src_format);

	/* if size of alpha channel is 0, it should be implicit 1 */
	if (size[3] == 0) implicit_alpha = 0xFF;

	for (i = 0; i < count; ++i)
	{
		int j;
		int channels[4];

		/* fetch pixel */
		unsigned int pixel = *src++;

		/* extract and convert channels */
		for (j = 0; j < 4; ++j)
		{
			int temp = _mali_extract_value(pixel, shift[j], size[j]);
			channels[j] = _mali_expand_to_8bit(temp, size[j]);
		}

		/* Write to RGBA8888 buffer */
		*dst++ = channels[0];
		*dst++ = channels[1];
		*dst++ = channels[2];
		*dst++ = channels[3] | implicit_alpha;
	}
}

MALI_EXPORT void _mali_convert_rgba8888_to_16bit(u16 *dst, const u8 *src, int count, enum mali_convert_pixel_format src_format)
{
	int i;
	int shift[4];
	int size[4];

	_mali_convert_get_16bit_shifts(shift, src_format);
	_mali_convert_get_16bit_component_size(size, src_format);

	for (i = 0; i < count; ++i)
	{
		int j;
		int channels[4];
		unsigned int pixel = 0;

		/* fetch pixel */
		channels[0] = *src++;
		channels[1] = *src++;
		channels[2] = *src++;
		channels[3] = *src++;

		/* extract and convert channels */
		for (j = 0; j < 4; ++j)
		{
			int temp = channels[j] >> (8 - size[j]);
			pixel |= temp << shift[j];
		}

		*dst++ = pixel;
	}
}


void _mali_convert_get_from_8bit_to_rgba8888_byte_indices(int index[4], enum mali_convert_pixel_format format)
{
	switch (format)
	{
	case MALI_CONVERT_PIXEL_FORMAT_R8G8B8:
		index[0] = 0;
		index[1] = 1;
		index[2] = 2;
		index[3] = -1;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_R8G8B8A8:
		index[0] = 0;
		index[1] = 1;
		index[2] = 2;
		index[3] = 3;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_L8:
		index[0] = 0;
		index[1] = 0;
		index[2] = 0;
		index[3] = -1;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_L8A8:
		index[0] = 0;
		index[1] = 0;
		index[2] = 0;
		index[3] = 1;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_A8:
		index[0] = -1;
		index[1] = -1;
		index[2] = -1;
		index[3] = 0;
		break;

	default: MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
	}
}

void _mali_convert_get_from_rgba8888_to_8bit_byte_indices(int index[4], enum mali_convert_pixel_format format)
{
	switch (format)
	{
	case MALI_CONVERT_PIXEL_FORMAT_R8G8B8:
		index[0] = 0;
		index[1] = 1;
		index[2] = 2;
		index[3] = -1;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_R8G8B8A8:
		index[0] = 0;
		index[1] = 1;
		index[2] = 2;
		index[3] = 3;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_L8:
		index[0] = 0;
		index[1] = -1;
		index[2] = -1;
		index[3] = -1;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_L8A8:
		index[0] = 0;
		index[1] = 3;
		index[2] = -1;
		index[3] = -1;
		break;

	case MALI_CONVERT_PIXEL_FORMAT_A8:
		index[0] = 3;
		index[1] = -1;
		index[2] = -1;
		index[3] = -1;
		break;

	default: MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
	}
}

MALI_EXPORT int _mali_convert_pixel_format_get_size(enum mali_convert_pixel_format format)
{
	switch (format)
	{
		/* 8 bit per component formats - return component count */
	case MALI_CONVERT_PIXEL_FORMAT_R8G8B8:   return 3;
	case MALI_CONVERT_PIXEL_FORMAT_R8G8B8A8: return 4;
	case MALI_CONVERT_PIXEL_FORMAT_L8:       return 1;
	case MALI_CONVERT_PIXEL_FORMAT_L8A8:     return 2;
	case MALI_CONVERT_PIXEL_FORMAT_A8:       return 1;

		/* 16 bit per pixel formats */
	case MALI_CONVERT_PIXEL_FORMAT_R5G6B5:
	case MALI_CONVERT_PIXEL_FORMAT_R4G4B4A4:
	case MALI_CONVERT_PIXEL_FORMAT_R5G5B5A1:
		return 2;

		/* handle errors */
	default:
		MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
		return 0;
	}
}

MALI_EXPORT mali_convert_method _mali_convert_pixel_format_get_convert_method(
	enum mali_convert_pixel_format format)
{
	switch (format)
	{
	case MALI_CONVERT_PIXEL_FORMAT_R8G8B8:
	case MALI_CONVERT_PIXEL_FORMAT_R8G8B8A8:
	case MALI_CONVERT_PIXEL_FORMAT_L8:
	case MALI_CONVERT_PIXEL_FORMAT_L8A8:
	case MALI_CONVERT_PIXEL_FORMAT_A8:
		return MALI_CONVERT_8BITS;

	case MALI_CONVERT_PIXEL_FORMAT_R5G6B5:
	case MALI_CONVERT_PIXEL_FORMAT_R4G4B4A4:
	case MALI_CONVERT_PIXEL_FORMAT_R5G5B5A1:
		return MALI_CONVERT_PACKED;

		/* handle errors */
	default:
		MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
		return 0;
	}
}

MALI_EXPORT void _mali_convert_8bit_to_rgba8888(u8 *dst, const u8 *src, int count, enum mali_convert_pixel_format src_format)
{
	int i;
	int index[4];
	int pixel_size;

	_mali_convert_get_from_8bit_to_rgba8888_byte_indices(index, src_format);
	pixel_size = _mali_convert_pixel_format_get_size(src_format);

	for (i = 0; i < count; ++i)
	{
		int j;

		/* fetch components */
		for (j = 0; j < 4; ++j)
		{
			dst[j] = index[j] >= 0 ? src[index[j]] : 0xFF;
		}

		/* advance src pointer */
		src += pixel_size;
		dst += 4;
	}
}

MALI_EXPORT void _mali_convert_rgba8888_to_8bit(u8 *dst, const u8 *src, int count, enum mali_convert_pixel_format src_format)
{
	int i;
	int index[4];
	int pixel_size;

	_mali_convert_get_from_rgba8888_to_8bit_byte_indices(index, src_format);
	pixel_size = _mali_convert_pixel_format_get_size(src_format);

	for (i = 0; i < count; ++i)
	{
		int j;

		/* fetch components */
		for (j = 0; j < 4; ++j)
		{
			if (index[j] >= 0) dst[j] = src[index[j]];
		}

		/* advance pointers */
		dst += pixel_size;
		src += 4;
	}
}
