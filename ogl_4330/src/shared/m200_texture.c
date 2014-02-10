/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_system.h"

#include "base/mali_debug.h"
#include "shared/m200_td.h"
#include "shared/m200_texture.h"
#include "shared/m200_texel_format.h"
#include "shared/mali_convert.h"

#include <base/mali_byteorder.h>

MALI_STATIC void _m200_texture_deinterleave_2d(
	void*             dst,
	const void*       src,
	s32               width,
	s32               height,
	int               dst_pitch,
	m200_texel_format texel_format )
{
	s32   u, v;
	u32   x_uorder, y_uorder;

	const u8 *src8 = (const u8*)src;
	u8       *dst8 = (u8*)dst;

	s32 bytes_per_texel = __m200_texel_format_get_size( texel_format );
	MALI_DEBUG_ASSERT(
		__m200_texel_format_get_bpp(texel_format) % 8 == 0,
		("unsupported bits per texel: %d\n", __m200_texel_format_get_bpp(texel_format))
	);

	y_uorder = 0;
	for (v=0; v<height; v++)
	{
		/* some constants to help conversion */
		const u32 uorder_plus = 0xAAAAAAAB;
		const u32 uorder_mask = 0x55555555;

		x_uorder = 0;
		for (u = 0; u < width; u++)
		{
			int index = (y_uorder << 1) + (y_uorder ^ x_uorder);
			int b;

			/* Data is copied from mali to mali endianess, since glReadPixels
			 * will do the mali to cpu conversion when pixels are copied to user buffers. */
			if(bytes_per_texel == 2){
				u16 t16;
				t16 = _MALI_GET_U16_ENDIAN_SAFE((u16*)(src8+index * bytes_per_texel));
				_MALI_SET_U16_ENDIAN_SAFE((u16 *)(dst8 + v * dst_pitch + u * bytes_per_texel), t16);
			}
			else{
				for (b = 0; b < bytes_per_texel; ++b)
				{
					dst8[v * dst_pitch + u * bytes_per_texel + b] = src8[index * bytes_per_texel + b];
				}
			}
			x_uorder += uorder_plus;
			x_uorder &= uorder_mask;
		}
		y_uorder += uorder_plus;
		y_uorder &= uorder_mask;
	}
}

void _m200_texture_deinterleave_16x16_blocked(
	void*             dest,
	const void*       src,
	s32               width,
	s32               height,
	int               dst_pitch,
	m200_texel_format texel_format )
{
	int x, y;
	int block_index = 0;
	int texel_size = __m200_texel_format_get_size( texel_format );

	const u8 *src_u8 = (const u8 *)src;
	u8       *dst_u8 = (u8 *)dest;

	MALI_DEBUG_ASSERT(
		__m200_texel_format_get_bpp(texel_format) % 8 == 0,
		("unsupported bits per texel: %d\n", __m200_texel_format_get_bpp(texel_format))
	);

	for (y = 0; y < height; y += 16)
	{
		for (x = 0; x < width; x += 16)
		{
			_m200_texture_deinterleave_2d(
				&dst_u8[(x * texel_size) + (y * dst_pitch)],
				&src_u8[block_index * 16 * 16 * texel_size],
				(width - x) >= 16 ? 16 : width - x,   /* width of source block */
				(height - y) >= 16 ? 16 : height - y, /* height of source block */
				dst_pitch,
				texel_format
			);
			block_index++;
		}
	}
}

MALI_EXPORT MALI_CHECK_RESULT mali_err_code _m200_texture_swizzle(
	void*                        dest,
	m200_texture_addressing_mode dest_mode,
	const void*                  src,
	m200_texture_addressing_mode src_mode,
	s32                          width,
	s32                          height,
	m200_texel_format            format,
	int dst_pitch,
	int src_pitch )
{

	/* use destination mode */
	switch ( dest_mode )
	{
		case M200_TEXTURE_ADDRESSING_MODE_LINEAR:
			/* linear can be converted to any interleaved format */
			switch ( src_mode )
			{
				case M200_TEXTURE_ADDRESSING_MODE_LINEAR:
					{
						int y;
						/* we can do a linear copy; copy line-by-line */
						for (y = 0; y < height; ++y)
						{
							/* get base-address as 8bit pointers */
							const u8 *src8 = (const u8*)src;
							u8 *dst8 = (u8*)dest;

							/* move pointers to the right */
							src8 += y * src_pitch;
							dst8 += y * dst_pitch;

							/* TODO: CHECK THIS BRANCH! api test never come here, _mali_sys_memcpy_endian_safe() here */
							MALI_DEBUG_PRINT(6, ("target==M200_TEXTURE_ADDRESSING_MODE_LINEAR, src==M200_TEXTURE_ADDRESSING_MODE_LINEAR\n"));
							_mali_sys_memcpy(dst8, src8, (width * __m200_texel_format_get_bpp( format ) + 7)/8);
						}
					}
					break;

				case M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED:
					MALI_DEBUG_PRINT(6, ("target==M200_TEXTURE_ADDRESSING_MODE_LINEAR, src==M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED\n"));
					_m200_texture_deinterleave_16x16_blocked(dest, src, width, height, dst_pitch, format);
					break;

				default:
					MALI_DEBUG_ERROR(("invalid src_mode %d\n", src_mode));
					break;
			}
			break;


		case M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED:
			switch ( src_mode )
			{
				case M200_TEXTURE_ADDRESSING_MODE_LINEAR:

					MALI_DEBUG_PRINT(6, ("target==M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED, src==M200_TEXTURE_ADDRESSING_MODE_LINEAR\n"));

					if ( M200_TEXEL_FORMAT_ETC == format )
					{
						_m200_texture_interleave_16x16_blocked_etc( dest, src, width, height, src_pitch, format );
					} else
					{
						_m200_texture_interleave_16x16_blocked( dest, src, width, height, src_pitch, format );
					}
					break;

				case M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED:

					/* Source seems to be already in LE format, no need for byte swapping (mali-to-mali copy) */
					MALI_DEBUG_PRINT(6, ("target==M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED, src==M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED\n"));
					_mali_sys_memcpy( dest, src, (((width + 0xf) & ~0xf) * ((height + 0xf) & ~0xf) * __m200_texel_format_get_bpp( format )+7)/8 );

					break;

				case M200_TEXTURE_ADDRESSING_MODE_INVALID:
					MALI_DEBUG_ERROR( ("**Error** You tried to convert from an invalid texture!\n") );
					break;

				default:
					MALI_DEBUG_ERROR(("invalid src_mode %d\n", src_mode));
					break;
			}
			break;
		case M200_TEXTURE_ADDRESSING_MODE_INVALID:
			MALI_DEBUG_ERROR( ("**Error** You tried to convert to an invalid texture!\n") );
			break;
		default:
			MALI_DEBUG_ERROR(("invalid dest_mode %d\n", dest_mode));
			break;
	}

	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT void _m200_texture_interleave_2d(
	void*             dst,
	const void*       src,
	s32               width,
	s32               height,
	int               src_pitch,
	m200_texel_format texel_format,
	s32               texels_per_block )
{
	s32   u, v;
	u32   x_uorder, y_uorder;

	const u8 *src8 = (const u8*)src;
	u8       *dst8 = (u8*)dst;

	const u32 bpp = __m200_texel_format_get_bpp( texel_format );
	const u32 elem_size = __m200_texel_format_get_bytes_per_copy_element(texel_format);

	s32 bytes_per_block = (bpp*texels_per_block+7)/8;

	/* some constants to help conversion */
	const u32 uorder_plus = 0xAAAAAAAB;
	const u32 uorder_mask = 0x55555555;

	MALI_DEBUG_ASSERT(
			(M200_TEXEL_FORMAT_ETC == texel_format) || (__m200_texel_format_get_bpp(texel_format) % 8 == 0),
			("unsupported bits per texel: %d\n", __m200_texel_format_get_bpp(texel_format))
	);

	y_uorder = 0;

	switch(elem_size)
	{
	case 1:
	{
		for (v=0; v<height; v++)
		{
			int v2 = v * src_pitch;

			x_uorder = 0;

			for (u = 0; u < width; u++)
			{
				int index = (y_uorder << 1) + (y_uorder ^ x_uorder);
				int index2 = index * bytes_per_block;
				int u2 = u*bytes_per_block;
				int b;
				for (b = 0; b < bytes_per_block; b += elem_size)
				{
					u8* dstx = (u8*)&dst8[index2 + b];
					u8* srcx = (u8*)&src8[v2 + u2 + b];
					_MALI_SET_U8_ENDIAN_SAFE(dstx, *(u8*)srcx);
				}
				x_uorder += uorder_plus;
				x_uorder &= uorder_mask;
			}
			y_uorder += uorder_plus;
			y_uorder &= uorder_mask;
		}
		break;
	}
	case 2:
	{
		for (v=0; v<height; v++)
		{
			int v2 = v * src_pitch;

			x_uorder = 0;

			for (u = 0; u < width; u++)
			{
				int index = (y_uorder << 1) + (y_uorder ^ x_uorder);
				int index2 = index * bytes_per_block;
				int u2 = u*bytes_per_block;
				int b;
				for (b = 0; b < bytes_per_block; b += elem_size)
				{
					u16* dstx = (u16*)&dst8[index2 + b];
					u16* srcx = (u16*)&src8[v2 + u2 + b];
					_MALI_SET_U16_ENDIAN_SAFE(dstx, *(u16*)srcx);
				}
				x_uorder += uorder_plus;
				x_uorder &= uorder_mask;
			}
			y_uorder += uorder_plus;
			y_uorder &= uorder_mask;
		}
		break;
	}
	case 4:
	{
		for (v=0; v<height; v++)
		{
			int v2 = v * src_pitch;

			x_uorder = 0;

			for (u = 0; u < width; u++)
			{
				int index = (y_uorder << 1) + (y_uorder ^ x_uorder);
				int index2 = index * bytes_per_block;
				int u2 = u*bytes_per_block;
				int b;
				for (b = 0; b < bytes_per_block; b += elem_size)
				{
					u32* dstx = (u32*)&dst8[index2 + b];
					u32* srcx = (u32*)&src8[v2 + u2 + b];
					_MALI_SET_U32_ENDIAN_SAFE(dstx, *(u32*)srcx);
				}
				x_uorder += uorder_plus;
				x_uorder &= uorder_mask;
			}
			y_uorder += uorder_plus;
			y_uorder &= uorder_mask;
		}
		break;
	}
	default:
	{
		MALI_DEBUG_ASSERT(0, ("Unknown copy size for element"));
		break;
	}
	} /* switch */

}

MALI_EXPORT void _m200_texture_interleave_16x16_blocked(
	void*             dest,
	const void*       src,
	s32               width,
	s32               height,
	int               src_pitch,
	m200_texel_format texel_format )
{
	int x, y;
	int block_index = 0;
	int texel_size = __m200_texel_format_get_size( texel_format );

	const u8 *src_u8 = (const u8 *)src;
	u8       *dst_u8 = (u8 *)dest;

	MALI_DEBUG_ASSERT(
		__m200_texel_format_get_bpp(texel_format) % 8 == 0,
		("unsupported bits per texel: %d\n", __m200_texel_format_get_bpp(texel_format))
	);

	for (y = 0; y < height; y += 16)
	{
		for (x = 0; x < width; x += 16)
		{
			_m200_texture_interleave_2d(
				&dst_u8[block_index * 16 * 16 * texel_size],
				&src_u8[(x * texel_size) + (y * src_pitch)],
				(width - x) >= 16 ? 16 : width - x,   /* width of source block */
				(height - y) >= 16 ? 16 : height - y, /* height of source block */
				src_pitch,
				texel_format,
				1
			);
			block_index++;
		}
	}
}

MALI_EXPORT void _m200_texture_interleave_16x16_blocked_etc(
	void*             dest,
	const void*       src,
	s32               width,
	s32               height,
	int               src_pitch,
	m200_texel_format texel_format )
{
#define ETC_BLOCK_DIM       (4)
#define ETC_TEXELS_PER_BYTE (2)
#define ETC_BLOCK_SIZE      ( (ETC_BLOCK_DIM)*(ETC_BLOCK_DIM)/(ETC_TEXELS_PER_BYTE) )

	int x, y;
	int block_index = 0;
	int texel_bpp = __m200_texel_format_get_bpp( texel_format );
	const u8 *src_u8 = (const u8 *)src;
	u8       *dst_u8 = (u8 *)dest;

	MALI_DEBUG_ASSERT( M200_TEXEL_FORMAT_ETC == texel_format, ("Invalid texel format, only ETC is supported here"));
	MALI_DEBUG_ASSERT( 4 == texel_bpp, ("Bits per texel should be 4 for ETC"));

	for (y = 0; y < height; y += 16)
	{
		for (x = 0; x < width; x += 16)
		{
			s32 block_width = (width - x) >= 16 ? 16 : width - x;
			s32 block_height = (height - y) >= 16 ? 16 : height - y;

			block_width = MAX( 1, block_width/ETC_BLOCK_DIM );
			block_height = MAX( 1, block_height/ETC_BLOCK_DIM );

			_m200_texture_interleave_2d(
				&dst_u8[(block_index * 16 * 16 * texel_bpp)/8 ],
				&src_u8[(x*ETC_BLOCK_SIZE/ETC_BLOCK_DIM) + (y * src_pitch)],
				block_width,                 /* width of source block */
				block_height,                /* height of source block */
				MAX(ETC_BLOCK_SIZE,src_pitch*ETC_BLOCK_DIM), /* the pitch must be specified in 4x4 block units */
				texel_format,
				ETC_BLOCK_DIM*ETC_BLOCK_DIM  /* the number of texels per block (since ETC is interleaved per block) */
			);
			block_index++;
		}
	}
#undef ETC_BLOCK_DIM
#undef ETC_TEXELS_PER_BYTE
#undef ETC_BLOCK_SIZE
}
