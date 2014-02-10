/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include <shared/m200_texture.h>
#include <shared/mali_convert.h>
#include <shared/mali_surface.h>
#include <base/mali_byteorder.h>

/* Remap table for linear to block interleaved */
const u8 mali_convert_block_interleave_lut[] = {
	0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15, 0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55,
	0x03, 0x02, 0x07, 0x06, 0x13, 0x12, 0x17, 0x16, 0x43, 0x42, 0x47, 0x46, 0x53, 0x52, 0x57, 0x56,
	0x0c, 0x0d, 0x08, 0x09, 0x1c, 0x1d, 0x18, 0x19, 0x4c, 0x4d, 0x48, 0x49, 0x5c, 0x5d, 0x58, 0x59,
	0x0f, 0x0e, 0x0b, 0x0a, 0x1f, 0x1e, 0x1b, 0x1a, 0x4f, 0x4e, 0x4b, 0x4a, 0x5f, 0x5e, 0x5b, 0x5a,
	0x30, 0x31, 0x34, 0x35, 0x20, 0x21, 0x24, 0x25, 0x70, 0x71, 0x74, 0x75, 0x60, 0x61, 0x64, 0x65,
	0x33, 0x32, 0x37, 0x36, 0x23, 0x22, 0x27, 0x26, 0x73, 0x72, 0x77, 0x76, 0x63, 0x62, 0x67, 0x66,
	0x3c, 0x3d, 0x38, 0x39, 0x2c, 0x2d, 0x28, 0x29, 0x7c, 0x7d, 0x78, 0x79, 0x6c, 0x6d, 0x68, 0x69,
	0x3f, 0x3e, 0x3b, 0x3a, 0x2f, 0x2e, 0x2b, 0x2a, 0x7f, 0x7e, 0x7b, 0x7a, 0x6f, 0x6e, 0x6b, 0x6a,
	0xc0, 0xc1, 0xc4, 0xc5, 0xd0, 0xd1, 0xd4, 0xd5, 0x80, 0x81, 0x84, 0x85, 0x90, 0x91, 0x94, 0x95,
	0xc3, 0xc2, 0xc7, 0xc6, 0xd3, 0xd2, 0xd7, 0xd6, 0x83, 0x82, 0x87, 0x86, 0x93, 0x92, 0x97, 0x96,
	0xcc, 0xcd, 0xc8, 0xc9, 0xdc, 0xdd, 0xd8, 0xd9, 0x8c, 0x8d, 0x88, 0x89, 0x9c, 0x9d, 0x98, 0x99,
	0xcf, 0xce, 0xcb, 0xca, 0xdf, 0xde, 0xdb, 0xda, 0x8f, 0x8e, 0x8b, 0x8a, 0x9f, 0x9e, 0x9b, 0x9a,
	0xf0, 0xf1, 0xf4, 0xf5, 0xe0, 0xe1, 0xe4, 0xe5, 0xb0, 0xb1, 0xb4, 0xb5, 0xa0, 0xa1, 0xa4, 0xa5,
	0xf3, 0xf2, 0xf7, 0xf6, 0xe3, 0xe2, 0xe7, 0xe6, 0xb3, 0xb2, 0xb7, 0xb6, 0xa3, 0xa2, 0xa7, 0xa6,
	0xfc, 0xfd, 0xf8, 0xf9, 0xec, 0xed, 0xe8, 0xe9, 0xbc, 0xbd, 0xb8, 0xb9, 0xac, 0xad, 0xa8, 0xa9,
	0xff, 0xfe, 0xfb, 0xfa, 0xef, 0xee, 0xeb, 0xea, 0xbf, 0xbe, 0xbb, 0xba, 0xaf, 0xae, 0xab, 0xaa,
};

/* lRGB to sRGB LUT */
const u8 mali_convert_linear_to_nonlinear_lut[256] =
{
	0, 13, 22, 28, 33, 38, 42, 46, 49, 53, 56, 58, 61, 64, 66, 68, 71,
	73, 75, 77, 79, 81, 83, 85, 86, 88, 90, 91, 93, 95, 96, 98, 99,
	101, 102, 103, 105, 106, 108, 109, 110, 112, 113, 114, 115, 116, 118, 119, 120,
	121, 122, 123, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
	138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 147, 148, 149, 150, 151, 152,
	153, 154, 154, 155, 156, 157, 158, 159, 159, 160, 161, 162, 163, 163, 164, 165,
	166, 167, 167, 168, 169, 170, 170, 171, 172, 173, 173, 174, 175, 175, 176, 177,
	178, 178, 179, 180, 180, 181, 182, 182, 183, 184, 184, 185, 186, 186, 187, 188,
	188, 189, 190, 190, 191, 192, 192, 193, 194, 194, 195, 195, 196, 197, 197, 198,
	199, 199, 200, 200, 201, 202, 202, 203, 203, 204, 205, 205, 206, 206, 207, 207,
	208, 209, 209, 210, 210, 211, 211, 212, 213, 213, 214, 214, 215, 215, 216, 216,
	217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 224, 224, 225,
	226, 226, 227, 227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 232, 233, 233,
	234, 234, 235, 235, 236, 236, 237, 237, 237, 238, 238, 239, 239, 240, 240, 241,
	241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 247, 247, 248, 248,
	249, 249, 250, 250, 251, 251, 251, 252, 252, 253, 253, 254, 254, 255, 255
};

/* sRGB to lRGB LUT */
const u8 mali_convert_nonlinear_to_linear_lut[256] =
{
	0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4,
	4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8,
	8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13,
	14, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 18, 19, 19, 20, 21,
	21, 22, 22, 23, 23, 24, 24, 25, 26, 26, 27, 27, 28, 29, 29, 30,
	31, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 41,
	42, 43, 44, 45, 45, 46, 47, 48, 49, 50, 51, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
	72, 73, 74, 76, 77, 78, 79, 80, 81, 82, 84, 85, 86, 87, 88, 90,
	91, 92, 93, 95, 96, 97, 99, 100, 101, 103, 104, 105, 107, 108, 109, 111,
	112, 114, 115, 116, 118, 119, 121, 122, 124, 125, 127, 128, 130, 131, 133, 134,
	136, 138, 139, 141, 142, 144, 146, 147, 149, 151, 152, 154, 156, 157, 159, 161,
	163, 164, 166, 168, 170, 172, 173, 175, 177, 179, 181, 183, 184, 186, 188, 190,
	192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222,
	224, 226, 229, 231, 233, 235, 237, 239, 242, 244, 246, 248, 250, 253, 255
};

#if !defined(__SYMBIAN32__) || defined(SYMBIAN_LIB_MALI_BASE)

/**
 * Helper function for 16 bit linear to block interleaved conversion. 
 * Handles block interleaving for any sub-area.
 * @param dst
 * @param src
 * @param conv_rect - Src and dst conversion rectangle 
 * @param src_pitch - Source pitch
 * @param width_aligned - Block aligned width of destination
 */
static void _tex16_l_to_tex16_b_partial(
	u8*                    dst,
	const u8               *src,
	mali_convert_rectangle *conv_rect,
	s32                    src_pitch,
	u32                    width_aligned )
{
	u32       u, v, dx, dy;
	const u8  *src_row = src + conv_rect->sx*2 + conv_rect->sy*src_pitch;

	dy = conv_rect->dy;
	for( v =  0; v < conv_rect->height; v++ )
	{
		dx = conv_rect->dx;
		for( u = 0; u < conv_rect->width; u++ )
		{
			*((u16*)dst + MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned ) ) = ((u16*)src_row)[u];
			dx++;
		}
		src_row += src_pitch;
		dy++;
	}
}

/**
 * Helper function for 16 bit linear to block interleaved conversion. 
 * Full width specialization, i.e. 16 texels wide.
 * @param dst
 * @param src
 * @param width - Unaligned texture width
 * @param height - Unaligned texture height
 * @param src_pitch - Source pitch
 */
static void _tex16_l_to_tex16_b_full_width(
	u8*         dst,
	const u8    *src,
	u32          height,
	s32          src_pitch )
{
	u32      u, v;
	const u8 *remap        = mali_convert_block_interleave_lut;

	for( v=height; v>0; v-- )
	{
		const u16 *src16 = (u16*)src;
		u16 *dst16 = (u16*)dst;

		for( u=0; u<4; u++ )
		{
			u8 index0 = *remap++;
			u8 index1 = *remap++;
			u8 index2 = *remap++;
			u8 index3 = *remap++;

			dst16[index0] = *src16++;
			dst16[index1] = *src16++;
			dst16[index2] = *src16++;
			dst16[index3] = *src16++;
		}
		src += src_pitch;
	}
}

/**
 * Helper function for 32 bit linear to 16 bit block interleaved conversion. 
 * Handles block interleaving for any sub-area.
 * @param dst
 * @param src
 * @param conv_rect - Src and dst conversion rectangle 
 * @param src_pitch - Source pitch
 * @param width_aligned - Block aligned width of destination
 */
static void _tex32_l_to_tex16_b_partial(
	u8*                    dst,
	const u8               *src, 
	mali_convert_rectangle *conv_rect,
	s32                    src_pitch,
	u32                    width_aligned )
{
	u32       u, v, dx, dy;
	const u8  *src_row = (u8*)src + conv_rect->sx*4 + conv_rect->sy*src_pitch;

	dy = conv_rect->dy;
	for( v =  0; v < conv_rect->height; v++ )
	{
		const u32* src_pos = (const u32*) src_row;
		dx = conv_rect->dx;
		for( u = 0; u < conv_rect->width; u++ )
		{
			u32 tmp = (*src_pos)>>16;
			*((u16*)dst + MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned ) ) = tmp;
			src_pos++;
			dx++;
		}
		src_row += src_pitch;
		dy++;
	}
}


#define TEX_8_24_TO_TEX_24_8( a )  ((a & 0xFFFFFF00) >> 8) | ((a & 0xFF) << 24)
/**
 * Helper function for 8_24 bit linear to 24_8 bit block interleaved conversion. 
 * Handles block interleaving for any sub-area.
 * @param dst
 * @param src
 * @param conv_rect - Src and dst conversion rectangle 
 * @param src_pitch - Source pitch
 * @param width_aligned - Block aligned width of destination
 */
static void _tex8_24_l_to_tex24_8_b_partial(
	u8*                    dst,
	const u8               *src, 
	mali_convert_rectangle *conv_rect,
	s32                    src_pitch,
	u32                    width_aligned )
{
	u32       u, v, dx, dy;
	const u8  *src_row = (u8*)src + conv_rect->sx*4 + conv_rect->sy*src_pitch;

	dy = conv_rect->dy;
	for( v =  0; v < conv_rect->height; v++ )
	{
		const u32* src_pos = (const u32*) src_row;
		dx = conv_rect->dx;
		for( u = 0; u < conv_rect->width; u++ )
		{
			u32 tmp = TEX_8_24_TO_TEX_24_8(*src_pos);
			*((u32*)dst + MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned ) ) = tmp;
			src_pos++;
			dx++;
		}
		src_row += src_pitch;
		dy++;
	}
}

#undef TEX_8_24_TO_TEX_24_8

void _mali_convert_tex32_l_to_tex16_b(
	u8*          dst,
	const u8*    src,
	u32          width,
	u32          height,
	s32          src_pitch )
{
#if 0
	s32 x, y;
	u32 xfull, block, xover, width_aligned;

	/* Calculate the number of full width / height blocks. */
	width_aligned = MALI_ALIGN( width, 16 );
	xfull = (width >> 4) << 4;
	xover = width - xfull;
	block = 0;

	/* Do full block interleaving if available. Partial horizontal interleaving 
	 * is done on outer remaining rectangles */
	for( y=0; y<height; y+=16 )
	{
		u32 yblock = height - y;
		if( yblock > 16 )
		{
			yblock = 16;
		}

		for( x=0; x<xfull; x+=16 )
		{
			_tex32_l_to_tex16_b_full_width(
				&dst[block * 16 * 16 * 2],
				&src[(x*4) + (y * src_pitch)],
				yblock,
				src_pitch );
			block++;
		}

		if( xover ) block++;
	}

	if( xover )
	{
		mali_convert_rectangle conv_rect;
		conv_rect.sx = xfull;
		conv_rect.sy = 0;
		conv_rect.dx = xfull;
		conv_rect.dy = 0;
		conv_rect.width = xover;
		conv_rect.height = height;

		_tex32_l_to_tex16_b_partial(
			dst,
			src,
			&conv_rect,
			src_pitch,
			width_aligned );
	}
#endif

	mali_convert_rectangle conv_rect;
	u32 width_aligned = MALI_ALIGN( width, 16 );
	conv_rect.sx = 0;
	conv_rect.sy = 0;
	conv_rect.dx = 0;
	conv_rect.dy = 0;
	conv_rect.width = width;
	conv_rect.height = height;

	_tex32_l_to_tex16_b_partial(
			dst,
			src,
			&conv_rect,
			src_pitch,
			width_aligned );
}

void _mali_convert_tex8_24_l_to_tex24_8_b(
	u8*          dst,
	const u8*    src,
	u32          width,
	u32          height,
	s32          src_pitch )
{
	mali_convert_rectangle conv_rect;
	u32 width_aligned = MALI_ALIGN( width, 16 );
	conv_rect.sx = 0;
	conv_rect.sy = 0;
	conv_rect.dx = 0;
	conv_rect.dy = 0;
	conv_rect.width = width;
	conv_rect.height = height;

	_tex8_24_l_to_tex24_8_b_partial(
		dst,
		src,
		&conv_rect,
		src_pitch,
		width_aligned );
}

void _mali_convert_tex16_l_to_tex16_b(
	u8*          dst,
	const u8*    src,
	u32          width,
	u32          height,
	s32          src_pitch )
{
	u32 x, y;
	u32 xfull, block, xover, width_aligned;

	/* Calculate the number of full width / height blocks. */
	width_aligned = MALI_ALIGN( width, 16 );
	xfull = (width >> 4) << 4;
	block = 0;
	xover = width - xfull;

	/* Do full block interleaving if available. Partial horizontal interleaving 
	 * is done on outer remaining rectangles */
#if MALI_TEX16_L_TO_TEX16_B_FULL_BLOCK_DEFINED
	{
		u32 yfull, yover;

		yfull = (height >> 4) << 4;
		yover = height - yfull;

		for( y=0; y<yfull; y+=16 )
		{
			for( x=0; x<xfull; x+=16 )
			{
				_mali_osu_tex16_l_to_tex16_b_full_block(
					&dst[block * 16 * 16 * 2],
					&src[(x*2) + (y * src_pitch)],
					src_pitch );
				block++;
			}
			if( xover ) block++;
		}
		if( yover )
		{
			for( x=0; x<xfull; x+=16 )
			{
				_tex16_l_to_tex16_b_full_width(
					&dst[block * 16 * 16 * 2],
					&src[(x*2) + (y * src_pitch)],
					yover,
					src_pitch );
				block++;
			}
		}
	}
#else
	for( y=0; y<height; y+=16 )
	{
		u32 yblock = height - y;
		if( yblock > 16 )
		{
			yblock = 16;
		}

		for( x=0; x<xfull; x+=16 )
		{
			_tex16_l_to_tex16_b_full_width(
				&dst[block * 16 * 16 * 2],
				&src[(x*2) + (y * src_pitch)],
				yblock,
				src_pitch );
			block++;
		}
		if( xover ) block++;
	}
#endif
	if( xover )
	{
		mali_convert_rectangle conv_rect;
		conv_rect.sx = xfull;
		conv_rect.sy = 0;
		conv_rect.dx = xfull;
		conv_rect.dy = 0;
		conv_rect.width = xover;
		conv_rect.height = height;

		_tex16_l_to_tex16_b_partial(
			dst,
			src,
			&conv_rect,
			src_pitch,
			width_aligned );
	}
}

/**
 * TODO: Remove these special cases.
 * The special cases added when OLD_SPECIAL_CASES is set to 1 are not necessary for the correctness
 * of the code, they only speed it up somewhat. The results can be seen in gles2_performance_suite and
 * shared_main_suite.
 * The cases consist of about 1500 lines of code, and should be removed if the performance without them is
 * increased or deemed acceptable.
 */
#define OLD_SPECIAL_CASES 1
#if OLD_SPECIAL_CASES

/**
 * Helper function for 24 bit linear to block interleaved conversion. 
 * Handles any sub-region
 * @param dst
 * @param src
 * @param width - Unaligned texture width
 * @param height - Unaligned texture height
 * @param src_pitch - Source pitch
 */
static void _tex24_l_to_tex24_b_partial(
	u8                           *dst,
	const u8                     *src,
	const mali_convert_rectangle *conv_rect,
	s32                          src_pitch,
	u32                          width_aligned )
{
	u32      u, v, dx, dy;
	const u8  *src_row = src + conv_rect->sx*3 + conv_rect->sy*src_pitch;

	dy = conv_rect->dy;
	for( v = 0; v < conv_rect->height; v++ )
	{
		dx = conv_rect->dx;
		for( u = 0; u < conv_rect->width; u++ )
		{
			u8 *dst_blocked = (u8*)((u32)dst + (3*MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned )));

			dst_blocked[0] = src_row[u*3+0];
			dst_blocked[1] = src_row[u*3+1];
			dst_blocked[2] = src_row[u*3+2];
			dx++;
		}
		src_row += src_pitch;
		dy++;
	}
}

/**
 * Helper function for 24 bit linear to block interleaved conversion. 
 * Full width specialization, i.e. 16 texels wide.
 * @param dst
 * @param src
 * @param height - Unaligned texture height
 * @param src_pitch - Source pitch
 */
static void _tex24_l_to_tex24_b_full_width(
	u8*          dst,
	const u8     *src,
	u32          height,
	s32          src_pitch )
{
	u32      u, v;
	const u8 *remap        = mali_convert_block_interleave_lut;

	for( v=height; v>0; v-- )
	{
		u8 *src8 = (u8*)src;
		for( u=0; u<4; u++ )
		{
			u32 index0 = ((u32)(*remap++)) * 3;
			u32 index1 = ((u32)(*remap++)) * 3;
			u32 index2 = ((u32)(*remap++)) * 3;
			u32 index3 = ((u32)(*remap++)) * 3;

			dst[index0+0] = *src8++;
			dst[index0+1] = *src8++;
			dst[index0+2] = *src8++;

			dst[index1+0] = *src8++;
			dst[index1+1] = *src8++;
			dst[index1+2] = *src8++;

			dst[index2+0] = *src8++;
			dst[index2+1] = *src8++;
			dst[index2+2] = *src8++;

			dst[index3+0] = *src8++;
			dst[index3+1] = *src8++;
			dst[index3+2] = *src8++;
		}
		src += src_pitch;
	}
}

/**
 * Helper function for 8 bit linear to block interleaved conversion. 
 * Full width specialization, i.e. 16 texels wide.
 * @param dst
 * @param src
 * @param width - Unaligned texture width
 * @param height - Unaligned texture height
 * @param src_pitch - Source pitch
 */
static void _tex8_l_to_tex8_b_full_width(
	u8*         dst,
	const u8    *src,
	u32          height,
	s32          src_pitch )
{
	u32      u, v;
	const u8 *remap        = mali_convert_block_interleave_lut;

	for( v=height; v>0; v-- )
	{
		const u8 *src8 = src;
		for( u=16; u>0; u-=4 )
		{
			u8 index0 = *remap++;
			u8 index1 = *remap++;
			u8 index2 = *remap++;
			u8 index3 = *remap++;

			dst[index0] = *src8++;
			dst[index1] = *src8++;
			dst[index2] = *src8++;
			dst[index3] = *src8++;
		}
		src += src_pitch;
	}
}

/**
 * Helper function for 8 bit linear to block interleaved conversion. 
 * Handles block interleaving for any sub-area.
 * @param dst
 * @param src
 * @param conv_rect - Src and dst conversion rectangle 
 * @param src_pitch - Source pitch
 * @param width_aligned - Block aligned width of destination
 */
static void _tex8_l_to_tex8_b_partial(
	u8*                    dst,
	const u8               *src,
	mali_convert_rectangle *conv_rect,
	s32                    src_pitch,
	u32                    width_aligned )
{
	u32       u, v, dx, dy;
	const u8  *src_row = src + conv_rect->sx + conv_rect->sy*src_pitch;

	dy = conv_rect->dy;
	for( v =  0; v < conv_rect->height; v++ )
	{
		dx = conv_rect->dx;
		for( u = 0; u < conv_rect->width; u++ )
		{
			*(dst + MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned ) ) = src_row[u];
			dx++;
		}
		src_row += src_pitch;
		dy++;
	}
}

#define SWAP_32_NO_ORDER( tmp )	tmp = ( ( ( tmp >> 16) | (tmp << 16) ) & mask ) | ( tmp & ~mask );

/**
 * Helper function for 32 bit linear to block interleaved conversion. 
 * Handles block interleaving for any sub-area.
 * @param dst
 * @param src
 * @param conv_rect - Src and dst conversion rectangle 
 * @param src_pitch - Source pitch
 * @param width_aligned - Block aligned width of destination
 */
static void _tex32_l_to_tex32_b_partial_swap(
	u8*                    dst,
	const u8               *src, 
	mali_convert_rectangle *conv_rect,
	s32                    src_pitch,
	u32                    width_aligned,
	u32						mask_in )
{
	u32       u, v, dx, dy;
	const u8  *src_row = (u8*)src + conv_rect->sx*4 + conv_rect->sy*src_pitch;

	dy = conv_rect->dy;
	for( v =  0; v < conv_rect->height; v++ )
	{
		const u32 mask = mask_in;
		dx = conv_rect->dx;
		for( u = 0; u < conv_rect->width; u++ )
		{
			u32 tmp = ((u32*)src_row)[u];
			SWAP_32_NO_ORDER(tmp)
			*((u32*)dst + MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned ) ) = tmp;
			dx++;
		}
		src_row += src_pitch;
		dy++;
	}
}

/**
 * Helper function for 32 bit linear to block interleaved conversion. 
 * Handles block interleaving for any sub-area.
 * @param dst
 * @param src
 * @param conv_rect - Src and dst conversion rectangle 
 * @param src_pitch - Source pitch
 * @param width_aligned - Block aligned width of destination
 */
static void _tex32_l_to_tex32_b_partial(
	u8*                    dst,
	const u8               *src, 
	mali_convert_rectangle *conv_rect,
	s32                    src_pitch,
	u32                    width_aligned )
{
	u32       u, v, dx, dy;
	const u8  *src_row = (u8*)src + conv_rect->sx*4 + conv_rect->sy*src_pitch;

	dy = conv_rect->dy;
	for( v =  0; v < conv_rect->height; v++ )
	{
		dx = conv_rect->dx;
		for( u = 0; u < conv_rect->width; u++ )
		{
			*((u32*)dst + MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned ) ) = ((u32*)src_row)[u];
			dx++;
		}
		src_row += src_pitch;
		dy++;
	}
}

/**
 * Helper function for 32 bit linear to block interleaved conversion which
 * forces alpha to one.
 * @note Handles block interleaving for any sub-area.
 * @param dst
 * @param dst_nonpre - Stores normalized data. Will be ignored if NULL.
 * @param src
 * @param conv_rect - Src and dst conversion rectangle 
 * @param src_pitch - Source pitch
 * @param width_aligned - Block aligned width of destination
 * @param rev_order - rgbx if MALI_TRUE, xbgr if not
 */
static void _tex32_l_to_tex32_b_alpha_to_one_partial(
	u8*                    dst,
	u8*                    dst_nonpre,
	const u8               *src, 
	mali_convert_rectangle *conv_rect,
	s32                    src_pitch,
	u32                    width_aligned,
	mali_bool              rev_order )
{
	u32       u, v, dx, dy;
	u32       alpha_mask = rev_order ? 0xFF : 0xFF000000;
	const u8  *src_row = (u8*)src + conv_rect->sx*4 + conv_rect->sy*src_pitch;

	if ( NULL != dst_nonpre )
	{
		dy = conv_rect->dy;
		for( v =  0; v < conv_rect->height; v++ )
		{
			dx = conv_rect->dx;
			for( u = 0; u < conv_rect->width; u++ )
			{
				u32 offset = MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned );
				u32* dst32 = ((u32*)dst + offset);
				u32* dst32_nonpre = ((u32*)dst_nonpre + offset);
				*dst32 = (((u32*)src_row)[u]|alpha_mask);
				*dst32_nonpre = *dst32;
				dx++;
			}
			src_row += src_pitch;
			dy++;
		}
	}
	else
	{
		dy = conv_rect->dy;
		for( v =  0; v < conv_rect->height; v++ )
		{
			dx = conv_rect->dx;
			for( u = 0; u < conv_rect->width; u++ )
			{
				*((u32*)dst + MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned ) ) = (((u32*)src_row)[u]|alpha_mask);
				dx++;
			}
			src_row += src_pitch;
			dy++;
		}
	}
}

/**
 * Helper function for 32 bit linear to block interleaved conversion. 
 * Full width specialized, i.e. 16 texels wide.
 * @param dst
 * @param src
 * @param height - Unaligned texture height
 * @param src_pitch - Source pitch
 */
static void _tex32_l_to_tex32_b_full_width(
	u8*          dst,
	const u8     *src,
	u32          height,
	s32          src_pitch )
{
	u32      u, v;
	const u8 *remap        = mali_convert_block_interleave_lut;

	for( v=height; v>=1; v-- )
	{
		const u32 *src32 = (u32*)src;
		u32 *dst32 = (u32*)dst;
		for( u=0; u<4; u++ )
		{
			u32 index0 = *remap++;
			u32 index1 = *remap++;
			u32 index2 = *remap++;
			u32 index3 = *remap++;

			dst32[index0] = *src32++;
			dst32[index1] = *src32++;
			dst32[index2] = *src32++;
			dst32[index3] = *src32++;
		}
		src += src_pitch;
	}
}

/**
 * Helper function for 32 bit linear to block interleaved conversion with swap. 
 * Full width specialized, i.e. 16 texels wide.
 * @param dst
 * @param src
 * @param height - Unaligned texture height
 * @param src_pitch - Source pitch
 */
static void _tex32_l_to_tex32_b_full_width_swap(
	u8*          dst,
	const u8     *src,
	u32          height,
	s32          src_pitch,
	u32          mask_in )
{
	u32      u, v;
	const u8 *remap        = mali_convert_block_interleave_lut;

	for( v=height; v>=1; v-- )
	{
		const u32 mask = mask_in;
		const u32 *src32 = (u32*)src;
		u32 *dst32 = (u32*)dst;
		for( u=0; u<4; u++ )
		{
			u32 tmp0 = *src32++;
			u32 tmp1 = *src32++;
			u32 tmp2 = *src32++;
			u32 tmp3 = *src32++;

			u32 index0 = *remap++;
			u32 index1 = *remap++;
			u32 index2 = *remap++;
			u32 index3 = *remap++;

			SWAP_32_NO_ORDER(tmp0)
			SWAP_32_NO_ORDER(tmp1)
			SWAP_32_NO_ORDER(tmp2)
			SWAP_32_NO_ORDER(tmp3)

			dst32[index0] = tmp0;
			dst32[index1] = tmp1;
			dst32[index2] = tmp2;
			dst32[index3] = tmp3;
		}
		src += src_pitch;
	}
}

/**
 * Helper function for 32 bit linear to block interleaved conversion 
 * with premult and non-premult storage.
 * @param dst_premult - Destination to be interleaved and premultiplied with alpha
 * @param dst_nonpre - Destination to be interleaved. No premultiplication.
 * @param src
 * @param conv_rect - Src and dst conversion rectangle 
 * @param src_pitch 
 * @param rev_order - Specifies if reverse alpha or not,i.e [RGBA] vs [ARGB]
 * @param width_aligned - Block aligned width
 *
 */
static void _tex32_l_to_tex32_b_partial_premult(
	u8                           *dst_premult,
	u8                           *dst_nonpre,
	const u8                     *src,
	const mali_convert_rectangle *conv_rect,
	u32                          src_pitch,
	u32                          width_aligned,
	mali_bool                    rev_order )
{
#define UNPACK_TEXEL( t, r,g,b,a ) \
	do { \
		(r) = ( (t) >> 24 ) & 0xFF; \
		(g) = ( (t) >> 16 ) & 0xFF; \
		(b) = ( (t) >> 8 ) & 0xFF; \
		(a) = ( (t) ) & 0xFF; \
	} while( 0 )

	u32 i,j;
	u32 dx,dy;
	u8 *src_row;

	MALI_DEBUG_ASSERT_POINTER( src );
	MALI_DEBUG_ASSERT_POINTER( dst_premult );
	MALI_DEBUG_ASSERT_POINTER( dst_nonpre );
	MALI_DEBUG_ASSERT_POINTER( conv_rect );

	src_row = ( u8 * )src + conv_rect->sy * src_pitch + conv_rect->sx * 4;
	dy = conv_rect->dy;

	if ( MALI_TRUE == rev_order )
	{
		for ( j = 0; j < conv_rect->height; j++ )
		{
			dx = conv_rect->dx;
			for ( i = 0; i < conv_rect->width; i++ )
			{
				u32 texel = ( ( u32 * ) src_row )[i];
				u32 c1;
				u32 c2;
				u32 c3;
				u32 a;
				u32 pre_texel;
				u32 dest_address;

				UNPACK_TEXEL( texel, c1,c2,c3,a );
				c1 = _mali_convert_to_premult ( c1, a );
				c2 = _mali_convert_to_premult ( c2, a );
				c3 = _mali_convert_to_premult ( c3, a );
				pre_texel = _mali_convert_pack_8888( c1, c2, c3, a );

				dest_address = MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned );
				*( ( u32 * ) dst_premult + dest_address ) = pre_texel;
				*( ( u32 * ) dst_nonpre + dest_address ) = texel;
				dx++;
			}
			dy++;
			src_row += src_pitch;
		}
	}
	else
	{
		for ( j = 0; j < conv_rect->height; j++ )
		{
			dx = conv_rect->dx;
			for ( i = 0; i < conv_rect->width; i++ )
			{
				u32 texel = ( ( u32 * ) src_row )[i];
				u32 c1;
				u32 c2;
				u32 c3;
				u32 a;
				u32 pre_texel;
				u32 dest_address;

				UNPACK_TEXEL( texel, a, c3,c2,c1 );
				c1 = _mali_convert_to_premult ( c1, a );
				c2 = _mali_convert_to_premult ( c2, a );
				c3 = _mali_convert_to_premult ( c3, a );
				pre_texel = _mali_convert_pack_8888( a, c3, c2, c1 );

				dest_address = MALI_CONVERT_BLOCKED_ADDRESS ( dx, dy, width_aligned );
				*( ( u32 * ) dst_premult + dest_address ) = pre_texel;
				*( ( u32 * ) dst_nonpre + dest_address ) = texel;
				dx++;
			}
			dy++;
			src_row += src_pitch;
		}
	}

#undef UNPACK_TEXEL
}

void _mali_convert_tex32_l_to_tex32_b_premult(
	u8*          dst_pre,
	u8*          dst_nonpre,
	const u8*    src,
	u32          width,
	u32          height,
	s32          src_pitch,
	mali_bool    rev_order )
{
	u32      width_aligned;

	width_aligned = MALI_ALIGN( width, 16 );

#if MALI_TEX32_L_TO_TEX32_B_FULL_BLOCK_PREMULT_REVERSE_DEFINED
	{
		u32 x, y;
		u32 xfull, block, yfull, xover, yover;

		/* Calculate the number of full width / height blocks. */
		xfull = (width >> 4) << 4;
		yfull = (height >> 4) << 4;
		xover = width - xfull;
		yover = height - yfull;
		block = 0;

		/* First use optimized full block swizzling. Then do partial 
		 * swizzling on the two outer rectangles. */
		if ( MALI_TRUE == rev_order )
		{
			for( y=0; y<yfull; y+=16 )
			{
				for( x=0; x<xfull; x+=16 )
				{
					const u8* src_ptr = src + (x*4) + (y * src_pitch);
					_mali_osu_tex32_l_to_tex32_b_full_block_premult_reverse(
						&dst_pre[block * 16 * 16 * 4],
						&dst_nonpre[block * 16 * 16 * 4 ],
						src_ptr,
						src_pitch );
					block++;
				}

				/* Need to index outer horizontal block */
				if( xover ) block++;
			}
		}
		else
		{
			for( y=0; y<yfull; y+=16 )
			{
				for( x=0; x<xfull; x+=16 )
				{
					_mali_osu_tex32_l_to_tex32_b_full_block_premult(
						&dst_pre[block * 16 * 16 * 4],
						&dst_nonpre[block * 16 * 16 * 4 ],
						&src[(x*4) + (y * src_pitch)],
						src_pitch );
					block++;
				}

				/* Need to index outer horizontal block */
				if( xover ) block++;
			}
		}
		if( yover )
		{
			mali_convert_rectangle conv_rect;
			conv_rect.sx = 0;
			conv_rect.sy = yfull;
			conv_rect.dx = 0;
			conv_rect.dy = yfull;
			conv_rect.width = width;
			conv_rect.height = yover;

			_tex32_l_to_tex32_b_partial_premult(
				dst_pre,
				dst_nonpre,
				src,
				&conv_rect,
				src_pitch,
				width_aligned,
				rev_order );
		}
		if( xover )
		{
			mali_convert_rectangle conv_rect;
			conv_rect.sx = xfull;
			conv_rect.sy = 0;
			conv_rect.dx = xfull;
			conv_rect.dy = 0;
			conv_rect.width = xover;
			conv_rect.height = yfull;

			_tex32_l_to_tex32_b_partial_premult(
				dst_pre,
				dst_nonpre,
				src,
				&conv_rect,
				src_pitch,
				width_aligned,
				rev_order );
		}
	}
#else
	{
		/* No full block premult implementation available. 
		 * Use sw-fallback on whole area */
		mali_convert_rectangle conv_rect;
		conv_rect.sx = 0;
		conv_rect.sy = 0;
		conv_rect.dx = 0;
		conv_rect.dy = 0;
		conv_rect.width = width;
		conv_rect.height = height;

		_tex32_l_to_tex32_b_partial_premult(
			dst_pre,
			dst_nonpre,
			src,
			&conv_rect,
			src_pitch,
			width_aligned,
			rev_order );
	}
#endif
}

void _mali_convert_tex32_l_to_tex32_b_alpha_to_one(
	u8*               dst,
	u8*               dst_nonpre,
	const u8*         src,
	u32               width,
	u32               height,
	s32               src_pitch,
	mali_bool         rev_order )
{
	/* Calculate the number of full width / height blocks. */
	u32 width_aligned = MALI_ALIGN( width, 16 );

#if MALI_TEX32_L_TO_TEX32_B_XBGR_FULL_BLOCK_DEFINED
	/* Do full block interleaving if available. Partial interleaving 
	 * done on outer remaining rectangles */
	{
		u32 x, y;
		u32 xfull, block, xover;
		u32 yfull, yover;
		yfull = (height >> 4) << 4;
		yover = height - yfull;
		xfull = (width >> 4) << 4;
		xover = width - xfull;
		block = 0;

		for( y=0; y<yfull; y+=16 )
		{
			for( x=0; x<xfull; x+=16 )
			{
				const u8* src_ptr = src + (x*4) + (y * src_pitch);
				if ( NULL != dst_nonpre )
				{
					if ( MALI_TRUE == rev_order )
					{
						_mali_osu_tex32_l_to_tex32_b_rgbx_premult_full_block(
							&dst[block * 16 * 16 * 4],
							src_ptr,
							src_pitch,
							&dst_nonpre[block * 16 * 16 * 4] );
					}
					else
					{
						_mali_osu_tex32_l_to_tex32_b_xbgr_premult_full_block(
							&dst[block * 16 * 16 * 4],
							src_ptr,
							src_pitch,
							&dst_nonpre[block * 16 * 16 * 4] );
					}
				}
				else
				{
					if ( MALI_TRUE == rev_order )
					{
						_mali_osu_tex32_l_to_tex32_b_rgbx_full_block(
							&dst[block * 16 * 16 * 4],
							src_ptr,
							src_pitch );
					}
					else
					{
						_mali_osu_tex32_l_to_tex32_b_xbgr_full_block(
							&dst[block * 16 * 16 * 4],
							src_ptr,
							src_pitch );
					}
				}
				block++;
			}
			if( xover ) block++;
		}

		if( yover )
		{

			mali_convert_rectangle conv_rect;
			conv_rect.sx = 0;
			conv_rect.sy = yfull;
			conv_rect.dx = 0;
			conv_rect.dy = yfull;
			conv_rect.width = xfull;
			conv_rect.height = yover;

			_tex32_l_to_tex32_b_alpha_to_one_partial(
				dst,
				dst_nonpre,
				src,
				&conv_rect,
				src_pitch,
				width_aligned,
				rev_order );
		}

		if( xover )
		{
			mali_convert_rectangle conv_rect;
			conv_rect.sx = xfull;
			conv_rect.sy = 0;
			conv_rect.dx = xfull;
			conv_rect.dy = 0;
			conv_rect.width = xover;
			conv_rect.height = height;

			_tex32_l_to_tex32_b_alpha_to_one_partial(
				dst,
				dst_nonpre,
				src,
				&conv_rect,
				src_pitch,
				width_aligned,
				rev_order );
		}
	}
#else
	{
		mali_convert_rectangle conv_rect;
		conv_rect.sx = 0;
		conv_rect.sy = 0;
		conv_rect.dx = 0;
		conv_rect.dy = 0;
		conv_rect.width = width;
		conv_rect.height = height;

		_tex32_l_to_tex32_b_alpha_to_one_partial(
			dst,
			dst_nonpre,
			src,
			&conv_rect,
			src_pitch,
			width_aligned,
			rev_order);
	}
#endif

}

void _mali_convert_tex32_l_to_tex32_b(
	u8*          dst,
	const u8*    src,
	u32          width,
	u32          height,
	s32          src_pitch )
{
	u32 x, y;
	u32 xfull, block, xover, width_aligned;

	/* Calculate the number of full width / height blocks. */
	width_aligned = MALI_ALIGN( width, 16 );
	xfull = (width >> 4) << 4;
	xover = width - xfull;
	block = 0;

	/* Do full block interleaving if available. Partial interleaving 
	 * done on outer remaining rectangles */
#if MALI_TEX32_L_TO_TEX32_B_FULL_BLOCK_DEFINED
	{
		u32 yfull, yover;

		yfull = (height >> 4) << 4;
		yover = height - yfull;

		for( y=0; y<yfull; y+=16 )
		{
			for( x=0; x<xfull; x+=16 )
			{
				const u8* src_ptr = src + (x*4) + (y * src_pitch);
				_mali_osu_tex32_l_to_tex32_b_full_block(
					&dst[block * 16 * 16 * 4],
					src_ptr,
					src_pitch );
				block++;
			}
			if( xover ) block++;
		}
		if( yover )
		{
			for( x=0; x<xfull; x+=16 )
			{
				_tex32_l_to_tex32_b_full_width(
					&dst[block * 16 * 16 * 4],
					&src[(x*4) + (y * src_pitch)],
					yover,
					src_pitch );
				block++;
			}
		}
	}
#else
	for( y=0; y<height; y+=16 )
	{
		u32 yblock = height - y;
		if( yblock > 16 )
		{
			yblock = 16;
		}

		for( x=0; x<xfull; x+=16 )
		{
			_tex32_l_to_tex32_b_full_width(
				&dst[block * 16 * 16 * 4],
				&src[(x*4) + (y * src_pitch)],
				yblock,
				src_pitch );
			block++;
		}

		if( xover ) block++;
	}
#endif

	if( xover )
	{
		mali_convert_rectangle conv_rect;
		conv_rect.sx = xfull;
		conv_rect.sy = 0;
		conv_rect.dx = xfull;
		conv_rect.dy = 0;
		conv_rect.width = xover;
		conv_rect.height = height;

		_tex32_l_to_tex32_b_partial(
			dst,
			src,
			&conv_rect,
			src_pitch,
			width_aligned );
	}
}

void _mali_convert_tex32_l_to_tex32_b_swap(
	u8*          dst,
	const u8*    src,
	u32          width,
	u32          height,
	s32          src_pitch,
	u32			mask )
{
	u32 x, y;
	u32 xfull, block, xover, width_aligned;

	/* Calculate the number of full width / height blocks. */
	width_aligned = MALI_ALIGN( width, 16 );
	xfull = (width >> 4) << 4;
	xover = width - xfull;
	block = 0;

	/* Do full block interleaving if available. Partial interleaving 
	 * done on outer remaining rectangles */
#if MALI_TEX32_L_TO_TEX32_B_FULL_BLOCK_SWAP_DEFINED
	{
		u32 yfull, yover;

		yfull = (height >> 4) << 4;
		yover = height - yfull;

		for( y=0; y<yfull; y+=16 )
		{
			for( x=0; x<xfull; x+=16 )
			{
				const u8* src_ptr = src + (x*4) + (y * src_pitch);
				_mali_osu_tex32_l_to_tex32_b_full_block(
					&dst[block * 16 * 16 * 4],
					src_ptr,
					src_pitch );
				block++;
			}
			if( xover ) block++;
		}
		if( yover )
		{
			for( x=0; x<xfull; x+=16 )
			{
				_tex32_l_to_tex32_b_full_width_swap(
					&dst[block * 16 * 16 * 4],
					&src[(x*4) + (y * src_pitch)],
					yover,
					src_pitch, mask);
				block++;
			}
		}
	}
#else
	for( y=0; y<height; y+=16 )
	{
		u32 yblock = height - y;
		if( yblock > 16 )
		{
			yblock = 16;
		}

		for( x=0; x<xfull; x+=16 )
		{
			_tex32_l_to_tex32_b_full_width_swap(
				&dst[block * 16 * 16 * 4],
				&src[(x*4) + (y * src_pitch)],
				yblock,
				src_pitch, mask );
			block++;
		}

		if( xover ) block++;
	}
#endif

	if( xover )
	{
		mali_convert_rectangle conv_rect;
		conv_rect.sx = xfull;
		conv_rect.sy = 0;
		conv_rect.dx = xfull;
		conv_rect.dy = 0;
		conv_rect.width = xover;
		conv_rect.height = height;

		_tex32_l_to_tex32_b_partial_swap(
			dst,
			src,
			&conv_rect,
			src_pitch,
			width_aligned, mask );
	}
}

void _mali_convert_tex8_l_to_tex8_b(
 	u8*          dst,
 	const u8*    src,
 	u32          width,
 	u32          height,
 	s32          src_pitch )
{
 	u32 x, y;
	u32 xfull, block, xover, width_aligned;
 
 	/* Calculate the number of full width / height blocks. */
 	width_aligned = MALI_ALIGN( width, 16 );
 	xfull = (width >> 4) << 4;
 	block = 0;
 	xover = width - xfull;
 
 	/* Do full block interleaving if available. Partial horizontal interleaving 
 	 * is done on outer remaining rectangles */
#if MALI_TEX8_L_TO_TEX8_B_FULL_BLOCK_DEFINED
 	{
 		u32 yfull, yover;
 
 		yfull = (height >> 4) << 4;
 		yover = height - yfull;
 
 		for( y=0; y<yfull; y+=16 )
 		{
 			for( x=0; x<xfull; x+=16 )
 			{
 				_mali_osu_tex8_l_to_tex8_b_full_block(
 					&dst[block * 16 * 16],
 					&src[x + (y * src_pitch)],
 					src_pitch );
 				block++;
 			}
 			if( xover ) block++;
 		}
 		if( yover )
 		{
 			for( x=0; x<xfull; x+=16 )
 			{
 				_tex8_l_to_tex8_b_full_width(
 					&dst[block * 16 * 16],
 					&src[x + (y * src_pitch)],
 					yover,
 					src_pitch );
 				block++;
 			}
 		}
 	}
#else
 	for( y=0; y<height; y+=16 )
 	{
 		u32 yblock = height - y;
 		if( yblock > 16 )
 		{
 			yblock = 16;
 		}
 
 		for( x=0; x<xfull; x+=16 )
 		{
 			_tex8_l_to_tex8_b_full_width(
 				&dst[block * 16 * 16],
 				&src[x + (y * src_pitch)],
 				yblock,
 				src_pitch );
 			block++;
 		}
 		if( xover ) block++;
 	}
#endif
 	if( xover )
 	{
 		mali_convert_rectangle conv_rect;
 		conv_rect.sx = xfull;
 		conv_rect.sy = 0;
 		conv_rect.dx = xfull;
 		conv_rect.dy = 0;
 		conv_rect.width = xover;
 		conv_rect.height = height;
 
 		_tex8_l_to_tex8_b_partial(
 			dst,
 			src,
 			&conv_rect,
 			src_pitch,
 			width_aligned );
 	}
}

void _mali_convert_tex24_l_to_tex24_b(
	u8*               dst,
	const u8*         src,
	u32               width,
	u32               height,
	s32               src_pitch )
{
	u32 x, y;
	u32 xfull, block, xover, width_aligned;

	/* Calculate the number of full width / height blocks. */
	xfull = (width >> 4) << 4;
	width_aligned = MALI_ALIGN( width, 16 );
	xover = width - xfull;
	block = 0;

	/* Do full block interleaving if available. Partial horizontal interleaving 
	 * is done on outer remaining rectangles */
#if MALI_TEX24_L_TO_TEX24_B_FULL_BLOCK_DEFINED
	{
		u32 yfull, yover;
		
		yfull = (height >> 4) << 4;
		yover = height - yfull;

		for( y=0; y<yfull; y+=16 )
		{
			for( x=0; x<xfull; x+=16 )
			{
				_mali_osu_tex24_l_to_tex24_b_full_block(
					&dst[block * 16 * 16 * 3],
					&src[(x*3) + (y * src_pitch)],
					src_pitch );
				block++;
			}
			if ( xover ) block++;
		}
		if( yover )
		{
			for( x=0; x<xfull; x+=16 )
			{
				_tex24_l_to_tex24_b_full_width(
					&dst[block * 16 * 16 * 3],
					&src[(x*3) + (y * src_pitch)],
					yover,
					src_pitch );
				block++;
			}
		}
	}
#else
	for( y=0; y<height; y+=16 )
	{
		u32 yblock = height - y;
		if( yblock > 16 )
		{
			yblock = 16;
		}

		for( x=0; x<xfull; x+=16 )
		{
			_tex24_l_to_tex24_b_full_width(
				&dst[block * 16 * 16 * 3],
				&src[(x*3) + (y * src_pitch)],
				yblock,
				src_pitch );
			block++;
		}
		if ( xover ) block++;
	}
#endif
	if( xover )
	{
		mali_convert_rectangle conv_rect;
		conv_rect.sx = xfull;
		conv_rect.sy = 0;
		conv_rect.dx = xfull;
		conv_rect.dy = 0;
		conv_rect.width = xover;
		conv_rect.height = height;

		_tex24_l_to_tex24_b_partial(
			dst,
			src,
			&conv_rect,
			src_pitch,
			width_aligned );
	}
}

/**
 * Converts 32-bit linear textures doing red blue swap.
 * @param convert_request
 * @param ptrs_unaligned
 */
void _mali_convert_tex32_l_to_tex32_l_swap(const mali_convert_request* convert_request , mali_bool ptrs_unaligned )
{
	MALI_DEBUG_ASSERT_POINTER( convert_request );
	MALI_DEBUG_ASSERT( M200_TEXTURE_ADDRESSING_MODE_LINEAR == convert_request->src_format.texel_layout , ("Src must be M200_TEXTURE_ADDRESSING_MODE_LINEAR") );
	MALI_DEBUG_ASSERT( M200_TEXTURE_ADDRESSING_MODE_LINEAR == convert_request->dst_format.texel_layout , ("Dst must be M200_TEXTURE_ADDRESSING_MODE_LINEAR") );
	MALI_DEBUG_ASSERT( MALI_PIXEL_FORMAT_A8R8G8B8 == convert_request->src_format.pixel_format, ("Src pixel formtat must be MALI_PIXEL_FORMAT_A8R8G8B8") );
	MALI_DEBUG_ASSERT( MALI_PIXEL_FORMAT_A8R8G8B8 == convert_request->dst_format.pixel_format, ("Dst pixel formtat must be MALI_PIXEL_FORMAT_A8R8G8B8") );
	MALI_DEBUG_ASSERT( convert_request->dst_format.red_blue_swap != convert_request->src_format.red_blue_swap, ("Src and dst red_blue_swap flags must be different"));
	MALI_DEBUG_ASSERT( convert_request->src_format.reverse_order == convert_request->dst_format.reverse_order, ("Src and dst reverse_order flags must be equal"));
	{
		u32 i,j;
		const s32 x = convert_request->rect.dx;
		const s32 y = convert_request->rect.dy;
		const s32 sx = convert_request->rect.sx;
		const s32 sy = convert_request->rect.sy;
		const u32 height = convert_request->rect.height;
		const u32 width = convert_request->rect.width;
		const s32 src_pitch = convert_request->src_pitch;
		const s32 dst_pitch = convert_request->dst_pitch;
		const u8* src_ptr = (u8*)convert_request->src_ptr;
		const u32 mask = (convert_request->dst_format.reverse_order == MALI_TRUE) ? 0xff00ff00 : 0x00ff00ff ;
		u8* dst_ptr = (u8*)convert_request->dst_ptr;
#if MALI_TEX32_L_TO_TEX32_L_SWAP 
		if( MALI_FALSE == ptrs_unaligned )
		{
			const u32 x_pixels_per_block = 1<<5; 
			const u32 x_tiles = width >> 5;
			const u32 xover = ( width - x_tiles * x_pixels_per_block ) ;
			u8 *src_texel_row = ( u8 * ) src_ptr + sy * src_pitch;
			u8 *dst_texel_row = ( u8 * ) dst_ptr + y * dst_pitch;
			for ( j = 0; j < height; ++j )
			{
				u32* src = ( u32 * ) src_texel_row + sx;
				u32* dst = ( u32 * ) dst_texel_row + x;
				for ( i = 0; i < x_tiles ; ++i )
				{
					if( MALI_FALSE == convert_request->dst_format.reverse_order )
					{
						_mali_osu_tex32_l_to_tex32_l_swap_2_4( (u8*) dst, (u8*) src  );
						
					}
					else
					{
						_mali_osu_tex32_l_to_tex32_l_swap_1_3( (u8*) dst, (u8*) src  );
					}
					src+= x_pixels_per_block;
					dst+= x_pixels_per_block;
				}
				for( i=0; i < xover ; ++i)
				{
					u32 src_texel = *src++;
					SWAP_32_NO_ORDER(src_texel);
					*dst++ = src_texel;
				}
				src_texel_row += src_pitch;
				dst_texel_row += dst_pitch;
			}
		}
		else
#else 
		MALI_IGNORE(ptrs_unaligned);
#endif	
		{	
			u8 *src_texel_row = ( u8 * ) src_ptr + sy * src_pitch;
			u8 *dst_texel_row = ( u8 * ) dst_ptr + y * dst_pitch;
			for ( j = 0; j < height; j++ )
			{
				u32* src = ( u32 * ) src_texel_row + sx;
				u32* dst = ( u32 * ) dst_texel_row + x;
				for ( i = 0; i < width; i++ )
				{
					u32 src_texel = *src++;
					SWAP_32_NO_ORDER(src_texel);
					*dst++ = src_texel;
				}
				src_texel_row += src_pitch;
				dst_texel_row += dst_pitch;
			}
		}
	}

}

#endif /* OLD_SPECIAL_CASES */

MALI_EXPORT u32 _mali_convert_setup_conversion_rules( const mali_surface_specifier *src, const mali_surface_specifier *dest )
{
	u32 conv_rules = 0;

	if ( MALI_TRUE == dest->premultiplied_alpha ) conv_rules |= MALI_CONVERT_DST_PREMULT;
	if ( MALI_SURFACE_COLORSPACE_lRGB == dest->colorspace ) conv_rules |= MALI_CONVERT_DST_LINEAR;
	if ( MALI_TRUE == __m200_texel_format_has_luminance(dest->texel_format) ) conv_rules |= MALI_CONVERT_DST_LUMINANCE;

	if ( MALI_TRUE == src->premultiplied_alpha ) conv_rules |= MALI_CONVERT_SRC_PREMULT;
	if ( MALI_SURFACE_COLORSPACE_lRGB == src->colorspace ) conv_rules |= MALI_CONVERT_SRC_LINEAR;
	if ( MALI_TRUE == __m200_texel_format_has_luminance(src->texel_format) ) conv_rules |= MALI_CONVERT_SRC_LUMINANCE;

	return conv_rules;
}

/** 
 * Helper function for asessing if two formats are the same as seen from format conversion point of view.
 * @param formata 
 * @param formatb
 */
MALI_STATIC_INLINE mali_bool _same_conversion_formats( mali_surface_specifier *formata, mali_surface_specifier *formatb )
{
	MALI_DEBUG_ASSERT_POINTER( formata );
	MALI_DEBUG_ASSERT_POINTER( formatb );

	return ( (formatb->texel_format == formata->texel_format) 
		&& (formatb->red_blue_swap == formata->red_blue_swap) 
		&& (formatb->reverse_order == formata->reverse_order)
		&& (formatb->premultiplied_alpha == formata->premultiplied_alpha )
		&& (formatb->colorspace == formata->colorspace ) );
}

MALI_EXPORT void _mali_convert_request_initialize( 
	mali_convert_request *convert_request, 
	void *dst_ptr, 
	s32 dst_pitch, 
	const mali_surface_specifier *dst_format, 
	const void *src_ptr, 
	s32 src_pitch, 
	const mali_surface_specifier *src_format, 
	void *dst_nonpre_ptr, 
	s32 dst_nonpre_pitch,
	const mali_convert_rectangle *rect,
	mali_bool src_is_malimem,
	mali_bool dst_is_malimem,
	mali_bool alpha_clamp,
	mali_convert_request_source source )
{
	MALI_DEBUG_ASSERT_POINTER( convert_request );
	MALI_DEBUG_ASSERT_POINTER( dst_format );
	MALI_DEBUG_ASSERT_POINTER( rect );
	MALI_DEBUG_ASSERT_POINTER( src_format );

	convert_request->dst_ptr = dst_ptr;
	convert_request->dst_pitch = ( M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED == dst_format->texel_layout ) ? MALI_ALIGN( dst_pitch, 16 ) : dst_pitch;
	convert_request->src_ptr = src_ptr;
	convert_request->src_pitch = ( M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED == src_format->texel_layout ) ? MALI_ALIGN( src_pitch, 16 ) : src_pitch;
	convert_request->dst_nonpre_ptr = dst_nonpre_ptr;
	convert_request->dst_nonpre_pitch = ( M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED == dst_format->texel_layout ) ? MALI_ALIGN( dst_nonpre_pitch, 16 ) : dst_nonpre_pitch;
	convert_request->alpha_clamp = alpha_clamp;
	convert_request->src_is_malimem = src_is_malimem;
	convert_request->dst_is_malimem = dst_is_malimem;
	convert_request->source = source;

	_mali_sys_memcpy( &convert_request->dst_format, dst_format, sizeof( mali_surface_specifier ) );
	_mali_sys_memcpy( &convert_request->src_format, src_format, sizeof( mali_surface_specifier ) );
	_mali_sys_memcpy( &convert_request->rect, rect, sizeof( mali_convert_rectangle ) );
}

/**
 * Helper function to test if conversion is not a pure color conversion
 * @param convert_request 
 */
MALI_STATIC_INLINE mali_bool _conversion_not_color( const mali_convert_request *convert_request )
{
	const mali_bool format_not_color[] = {
		MALI_FALSE, /* MALI_TRUE, M200_TEXEL_FORMAT_L_1 = 0, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_1 = 1,*/
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_1 = 2, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_11 = 3, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_4 = 4, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_4 = 5, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_4 = 6, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_1111 = 7, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_44 = 8, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_8 = 9, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_8 = 10, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_8 = 11, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_332 = 12, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_2222 = 13, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_565 = 14, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_1555 = 15, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_4444 = 16, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_88 = 17, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_16 = 18, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_16 = 19, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_16 = 20, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_888 = 21, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_8888 = 22, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_xRGB_8888 = 23, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_2_10_10_10 = 24, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_11_11_10 = 25, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_10_12_10 = 26, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_16_16 = 27, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_16_16_16_16 = 28, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_PAL_4 = 29, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_PAL_8 = 30, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_FLXTC = 31, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_ETC = 32, */
		MALI_TRUE, /* 33 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_FP16 = 34, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_FP16 = 35, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_FP16 = 36, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_FP16 = 37, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_FP16 = 38, */
		MALI_TRUE, /* 39 is Reserved */
		MALI_TRUE, /* 40 is Reserved */
		MALI_TRUE, /* 41 is Reserved */
		MALI_TRUE, /* 42 is Reserved */
		MALI_TRUE, /* 43 is Reserved */
		MALI_TRUE, /* M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8 = 44, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_CONVOLUTION_TEXTURE_64 = 45, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_RGB_16_16_16 = 46, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_RGB_FP16 = 47, */
		MALI_TRUE, /* 48 is Reserved */
		MALI_TRUE, /* 49 is Reserved */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VERBATIM_COPY32 = 50, */
		MALI_TRUE, /* 51 is Reserved */
		MALI_TRUE, /* 52 is Reserved */
		MALI_TRUE, /* M200_TEXEL_FORMAT_YCCA = 53, */
		MALI_TRUE, /* 54 is Reserved */
		MALI_TRUE, /* 55 is Reserved */
		MALI_TRUE, /* 56 is Reserved */
		MALI_TRUE, /* 57 is Reserved */
		MALI_TRUE, /* 58 is Reserved */
		MALI_TRUE, /* 59 is Reserved */
		MALI_TRUE, /* 60 is Reserved */
		MALI_TRUE, /* 61 is Reserved */
		MALI_TRUE, /* 62 is Reserved */
		MALI_TRUE, /* M200_TEXEL_FORMAT_NO_TEXTURE = 63, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH32 = 64, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH16 = 65, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24 = 66, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_RGB888 = 67, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_16_0 */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_24_0 */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_4 */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_8 */
	};
	return format_not_color[convert_request->src_format.texel_format] || format_not_color[convert_request->dst_format.texel_format];
}

/**
 * Helper function for testing if conversion request is supported.
 * @param convert_request
 */
MALI_STATIC_INLINE mali_bool _conversion_supported( const mali_convert_request *convert_request )
{
	const mali_bool format_supported[] = {
		MALI_TRUE, /* M200_TEXEL_FORMAT_L_1 = 0, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_A_1 = 1, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_I_1 = 2, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_11 = 3, DISABLED DUE TO LACKING TESTS */
		MALI_TRUE, /* M200_TEXEL_FORMAT_L_4 = 4, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_A_4 = 5, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_I_4 = 6, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_1111 = 7, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_AL_44 = 8, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_L_8 = 9, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_A_8 = 10, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_I_8 = 11, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_332 = 12, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_2222 = 13, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_RGB_565 = 14, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_ARGB_1555 = 15, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_ARGB_4444 = 16, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_AL_88 = 17, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_16 = 18, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_16 = 19, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_16 = 20, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_RGB_888 = 21, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_ARGB_8888 = 22, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_xRGB_8888 = 23, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_2_10_10_10 = 24, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_11_11_10 = 25, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_10_12_10 = 26, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_16_16 = 27, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_16_16_16_16 = 28, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_PAL_4 = 29, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_PAL_8 = 30, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_FLXTC = 31, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ETC = 32, */
		MALI_FALSE, /* 33 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_FP16 = 34, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_FP16 = 35, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_FP16 = 36, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_FP16 = 37, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_FP16 = 38, */
		MALI_FALSE, /* 39 is Reserved */
		MALI_FALSE, /* 40 is Reserved */
		MALI_FALSE, /* 41 is Reserved */
		MALI_FALSE, /* 42 is Reserved */
		MALI_FALSE, /* 43 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8 = 44, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_CONVOLUTION_TEXTURE_64 = 45, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_16_16_16 = 46, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_FP16 = 47, */
		MALI_FALSE, /* 48 is Reserved */
		MALI_FALSE, /* 49 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VERBATIM_COPY32 = 50, */
		MALI_FALSE, /* 51 is Reserved */
		MALI_FALSE, /* 52 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_YCCA = 53, */
		MALI_FALSE, /* 54 is Reserved */
		MALI_FALSE, /* 55 is Reserved */
		MALI_FALSE, /* 56 is Reserved */
		MALI_FALSE, /* 57 is Reserved */
		MALI_FALSE, /* 58 is Reserved */
		MALI_FALSE, /* 59 is Reserved */
		MALI_FALSE, /* 60 is Reserved */
		MALI_FALSE, /* 61 is Reserved */
		MALI_FALSE, /* 62 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_NO_TEXTURE = 63, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH32 = 64, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH16 = 65, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24 = 66, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_RGB888 = 67, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_16_0 */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_24_0 */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_4 */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_8 */
	};
	MALI_DEBUG_ASSERT_POINTER( convert_request );
	
	return format_supported[convert_request->src_format.texel_format] && format_supported[convert_request->dst_format.texel_format];
}

/**
 * Helper function for testing if conversion request is supported in NEON.
 * @param convert_request
 */
MALI_STATIC_INLINE mali_bool _neon_conversion_supported( const mali_convert_request *convert_request )
{
	const mali_bool neon_format_supported[] = {
		MALI_FALSE, /* MALI_FALSE, M200_TEXEL_FORMAT_L_1 = 0, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_1 = 1,*/
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_1 = 2, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_11 = 3, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_4 = 4, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_4 = 5, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_4 = 6, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_1111 = 7, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_44 = 8, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_L_8 = 9, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_A_8 = 10, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_I_8 = 11, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_332 = 12, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_2222 = 13, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_RGB_565 = 14, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_ARGB_1555 = 15, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_ARGB_4444 = 16, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_AL_88 = 17, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_16 = 18, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_16 = 19, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_16 = 20, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_RGB_888 = 21, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_ARGB_8888 = 22, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_xRGB_8888 = 23, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_2_10_10_10 = 24, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_11_11_10 = 25, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_10_12_10 = 26, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_16_16 = 27, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_16_16_16_16 = 28, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_PAL_4 = 29, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_PAL_8 = 30, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_FLXTC = 31, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ETC = 32, */
		MALI_FALSE, /* 33 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_L_FP16 = 34, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_A_FP16 = 35, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_I_FP16 = 36, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_AL_FP16 = 37, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_ARGB_FP16 = 38, */
		MALI_FALSE, /* 39 is Reserved */
		MALI_FALSE, /* 40 is Reserved */
		MALI_FALSE, /* 41 is Reserved */
		MALI_FALSE, /* 42 is Reserved */
		MALI_FALSE, /* 43 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8 = 44, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_CONVOLUTION_TEXTURE_64 = 45, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_16_16_16 = 46, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_RGB_FP16 = 47, */
		MALI_FALSE, /* 48 is Reserved */
		MALI_FALSE, /* 49 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VERBATIM_COPY32 = 50, */
		MALI_FALSE, /* 51 is Reserved */
		MALI_FALSE, /* 52 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_YCCA = 53, */
		MALI_FALSE, /* 54 is Reserved */
		MALI_FALSE, /* 55 is Reserved */
		MALI_FALSE, /* 56 is Reserved */
		MALI_FALSE, /* 57 is Reserved */
		MALI_FALSE, /* 58 is Reserved */
		MALI_FALSE, /* 59 is Reserved */
		MALI_FALSE, /* 60 is Reserved */
		MALI_FALSE, /* 61 is Reserved */
		MALI_FALSE, /* 62 is Reserved */
		MALI_FALSE, /* M200_TEXEL_FORMAT_NO_TEXTURE = 63, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH32 = 64, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH16 = 65, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24 = 66, */
		MALI_TRUE, /* M200_TEXEL_FORMAT_VIRTUAL_RGB888 = 67, */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_16_0 */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_24_0 */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_4 */
		MALI_FALSE, /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_8 */
	};
	MALI_DEBUG_ASSERT_POINTER( convert_request );

	return neon_format_supported[convert_request->src_format.texel_format] && neon_format_supported[convert_request->dst_format.texel_format];
}

/**
 * Helper function do decide whether or not NEON fastpath can be taken.
 */
mali_bool _mali_convert_can_do_fastpath(
	mali_convert_request *convert_request,
	mali_bool blocked_offset,
	mali_bool do_swizzle,
	mali_bool ptrs_unaligned,
	mali_bool same_formats,
	mali_bool store_nonpre_data,
	mali_bool subbyte_format )
{

	if (MALI_TRUE == ptrs_unaligned || MALI_TRUE == subbyte_format || MALI_TRUE == blocked_offset)
	{
		return MALI_FALSE;
	}
	/* If we only need to do swizzle */
	if (MALI_TRUE == do_swizzle && MALI_TRUE == same_formats && MALI_FALSE == store_nonpre_data)
	{
		return MALI_TRUE;
	}

	return _neon_conversion_supported(convert_request);
}

MALI_STATIC_INLINE void _mali_convert_texture_slowpath(
	mali_convert_request *convert_request,
	s32 startx,
	s32 endx,
	s32 starty,
	s32 endy,
	u32 src_bpp,
	u32 dst_bpp,
	mali_bool src_blocked,
	mali_bool dst_blocked,
	u32 conv_rules_nonpre,
	u32 conv_rules_premult,
	mali_bool pass_through,
	mali_bool same_formats )
{
	s32 i, j;
	u32 src_address, dst_address;
	const s32 src_bytespp = src_bpp >> 3;
	const s32 dst_bytespp = dst_bpp >> 3;
	const s32 src_width_aligned = MALI_ALIGN( convert_request->src_format.width, 16 );
	const s32 dst_width_aligned = MALI_ALIGN( convert_request->dst_format.width, 16 );

	for (j = starty; j < endy; j++)
	{
		const u32 src_y = convert_request->rect.sy + j;
		const u32 src_cur_row = src_y * convert_request->src_pitch;

		const u32 dst_y = convert_request->rect.dy + j;
		const u32 dst_cur_row = dst_y * convert_request->dst_pitch;

		for (i = startx; i < endx; i++)
		{
			s32 ks, kd;
			u32 src_texel = 0;
			u32 premult_texel;
			const u32 src_x = convert_request->rect.sx + i;
			const u32 dst_x = convert_request->rect.dx + i;

			/* If no form of conversion is needed */
			if (MALI_TRUE == pass_through && src_bpp >= 8)
			{
				if (MALI_TRUE == src_blocked)
				{
					src_address = MALI_CONVERT_BLOCKED_ADDRESS(src_x, src_y, src_width_aligned) * src_bytespp;
				}
				else
				{
					src_address = src_cur_row + src_x * src_bytespp;
				}
				if (MALI_TRUE == dst_blocked)
				{
					dst_address = MALI_CONVERT_BLOCKED_ADDRESS(dst_x, dst_y, dst_width_aligned) * dst_bytespp;
				}
				else
				{
					dst_address = dst_cur_row + dst_x * dst_bytespp;
				}

				_mali_sys_memcpy((u8*)convert_request->dst_ptr + dst_address,(u8*)convert_request->src_ptr + src_address, src_bytespp); 

				if (convert_request->dst_nonpre_ptr != NULL)
				{
					_mali_sys_memcpy((u8*)convert_request->dst_nonpre_ptr + dst_address,(u8*)convert_request->src_ptr + src_address, src_bytespp);
				}
				continue;
			}

			/* Read source texel */
			switch (src_bpp)
			{
				case 1:
				{
					ks = src_x  >> 3;
					ks = src_x - ( ks << 3 );
					src_texel = *( ( u8 * ) ( ( u32 ) convert_request->src_ptr + src_cur_row + ( src_x  >> 3 ) ) );
					src_texel >>= ks;
					src_texel = ( src_texel & 0x1 );
					break;
				}
				case 2:
				{
					ks = ( (src_x & 0x3) << 1 );
					src_texel = *( ( u8 * ) ( ( u32 ) convert_request->src_ptr + src_cur_row + ( src_x  >> 2 ) ) );
					src_texel >>= ks;
					src_texel = ( src_texel & 0x3 );
					break;
				}
				case 4:
				{
					ks =  (src_x  & 1) ? 4 : 0;
					src_texel = *( ( u8 * ) ( ( u32 ) convert_request->src_ptr + src_cur_row + ( src_x  >> 1 ) ) );
					src_texel >>= ks;
					src_texel = ( src_texel & 0x0F );
					break;
				}
				case 8:
				case 16:
				case 24:
				case 32:
				{	
					if (MALI_TRUE == src_blocked)
					{
						src_address = MALI_CONVERT_BLOCKED_ADDRESS(src_x, src_y, src_width_aligned) * src_bytespp;
					}
					else
					{
						src_address = src_cur_row + src_x * src_bytespp;
					}

					_mali_sys_memcpy(&src_texel, (u8*)convert_request->src_ptr + src_address, src_bytespp);
					break;
				}
				default:
					MALI_DEBUG_ASSERT(0, ("Not yet implemented"));
			}

			/* Do the actual conversion per texel */
			if (MALI_TRUE == pass_through)
			{
				premult_texel = src_texel;
			}
			else
			{
				premult_texel = _mali_convert_texel(&convert_request->src_format, &convert_request->dst_format, src_texel, conv_rules_premult, convert_request->source);
			}

			/* Place texel in correct dst position */
			switch (dst_bpp)
			{
				case 1:
				{
					u32 dst_texel;
					kd = dst_x >> 3;
					kd = dst_x - ( kd << 3 );
					dst_texel = ( (u8*) convert_request->dst_ptr + dst_cur_row )[ dst_x  >> 3 ] & ~( 1 << kd );
					( (u8*) convert_request->dst_ptr + dst_cur_row )[ dst_x >> 3 ] = dst_texel | ( premult_texel << kd );
					break;
				}
				case 2:
				{
					u32 dst_texel;
					kd = ( (dst_x & 0x3) << 1 );
					dst_texel = ( (u8*) convert_request->dst_ptr + src_cur_row )[ dst_x >> 2 ] & ~( 1 << kd );
					( (u8*) convert_request->dst_ptr + dst_cur_row )[ dst_x >> 2 ] = dst_texel | ( premult_texel << kd );
					break;
				}
				case 4:
				{	
					u32 dst_texel;
					kd = dst_x & 1 ? 4 : 0;
					dst_texel = ( (u8*) convert_request->dst_ptr + dst_cur_row )[ dst_x >> 1 ] & ( kd ? 0x0F : 0xF0 );
					( (u8*) convert_request->dst_ptr + dst_cur_row )[ dst_x >> 1 ] = dst_texel | ( premult_texel << kd );
					break;
				}
				case 8:
				case 16:
				case 24:
				case 32:
				{	
					if (MALI_TRUE == dst_blocked)
					{
						dst_address = MALI_CONVERT_BLOCKED_ADDRESS(dst_x, dst_y, dst_width_aligned) * dst_bytespp;
					}
					else
					{
						dst_address = dst_x*dst_bytespp + dst_cur_row;
					}

					if (convert_request->dst_nonpre_ptr != NULL && MALI_FALSE == same_formats)
					{
						src_texel = _mali_convert_texel(&convert_request->src_format, &convert_request->dst_format, src_texel, conv_rules_nonpre, convert_request->source);
					}

					_mali_sys_memcpy((u8*)convert_request->dst_ptr + dst_address, &premult_texel, dst_bytespp);

					if (convert_request->dst_nonpre_ptr != NULL)
					{
						_mali_sys_memcpy((u8*)convert_request->dst_nonpre_ptr + dst_address, &src_texel, dst_bytespp);
					}
					break;
				}
				default:
					MALI_DEBUG_ASSERT(0, ("Not yet implemented"));
			}
		}
	}
}

MALI_EXPORT mali_bool _mali_convert_texture(mali_convert_request *convert_request)
{

#define IS_UNALIGNED( src_ptr, texel_size ) (((u32)(src_ptr)) & ((texel_size)-1))

	mali_bool alpha_to_one;
	mali_bool blocked_offset;
	mali_bool both_linear;
	mali_bool do_deswizzle;
	mali_bool do_neon;
	mali_bool do_swizzle;
	mali_bool dst_blocked;
	mali_bool pass_through;
	mali_bool ptrs_unaligned;
	mali_bool retval;
	mali_bool same_formats;
	mali_bool src_blocked;
	mali_bool store_nonpre_data;
	mali_bool subarea;
	mali_bool subbyte_format;
	const u8 *src_ptr;
	u8 *dst_ptr;
	void *dst_nonpre_ptr;
	s32 src_pitch, dst_pitch;
	s32 src_bpp, dst_bpp, src_bytespp, dst_bytespp;
	s32 sx, sy, dx, dy;
	s32 width, height;
	u32 conv_rules_nonpre, conv_rules_premult;
	u16 src_width, src_height, dst_width, dst_height;
	s32 src_width_aligned, dst_width_aligned;
	const u16 src_surfspec = PACK_SURFSPEC( &convert_request->src_format );
	const u16 dst_surfspec = PACK_SURFSPEC( &convert_request->dst_format );
	/* Loop variables */
	s32 i, j, k, l;
	u32 src_address, dst_address;
	s32 xfull, yfull, xover, yover, tile;

#if MALI_BIG_ENDIAN
	void *cpu_ptr = NULL;
#endif

#if OLD_SPECIAL_CASES
	mali_bool both_blocked;
	mali_bool both_32b_argb;
	mali_bool rbswap_mismatch;
#endif

	height = convert_request->rect.height;
	width = convert_request->rect.width;
	src_width = convert_request->src_format.width;
	src_height = convert_request->src_format.height;
	src_width_aligned = MALI_ALIGN( src_width, 16 );
	dst_width = convert_request->dst_format.width;
	dst_height = convert_request->dst_format.height;
	dst_width_aligned = MALI_ALIGN( dst_width, 16 );

	MALI_DEBUG_ASSERT_POINTER( convert_request );
	MALI_DEBUG_ASSERT( NULL != convert_request->src_ptr && NULL != convert_request->dst_ptr, ("Invalid conversion request. Need valid src and dst pointers!") );

	src_ptr = (u8*)convert_request->src_ptr;
	dst_ptr = (u8*)convert_request->dst_ptr;
	dst_nonpre_ptr = convert_request->dst_nonpre_ptr;
	src_bpp = __m200_texel_format_get_bpp(convert_request->src_format.texel_format);
	dst_bpp = __m200_texel_format_get_bpp(convert_request->dst_format.texel_format);
	src_bytespp = src_bpp >> 3;
	dst_bytespp = dst_bpp >> 3;
	src_pitch = convert_request->src_pitch;
	dst_pitch = convert_request->dst_pitch;

	/* JIRA ticket 2694 */
	MALI_IGNORE(src_height);
	MALI_IGNORE(src_width_aligned);
	MALI_IGNORE(dst_surfspec);
	MALI_IGNORE(src_surfspec);
	MALI_IGNORE(src_address);
	MALI_IGNORE(dst_address);

	ptrs_unaligned = IS_UNALIGNED(src_ptr, src_bytespp) || IS_UNALIGNED(dst_ptr, dst_bytespp);
	store_nonpre_data = (dst_nonpre_ptr != NULL) ? MALI_TRUE : MALI_FALSE;
	subbyte_format = ((src_bpp % 8) != 0) ? MALI_TRUE : MALI_FALSE;

	/* Some helper booleans for cleaner code */
	alpha_to_one = convert_request->src_format.alpha_to_one || convert_request->dst_format.alpha_to_one;
	both_linear = ( convert_request->src_format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_LINEAR &&
		        convert_request->dst_format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_LINEAR );
	do_swizzle = ( convert_request->src_format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_LINEAR &&
		       convert_request->dst_format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED );
	do_deswizzle = ( convert_request->src_format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED &&
		         convert_request->dst_format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_LINEAR );
	same_formats = _same_conversion_formats( &convert_request->src_format, &convert_request->dst_format );
	src_blocked = convert_request->src_format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED;
	dst_blocked = convert_request->dst_format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED;
	blocked_offset = ( (src_blocked && convert_request->rect.sy + convert_request->rect.sx > 0) ||
		           (dst_blocked && convert_request->rect.dy + convert_request->rect.dx > 0) );

	do_neon = _mali_convert_can_do_fastpath(
			convert_request,
			blocked_offset,
			do_swizzle,
			ptrs_unaligned,
			same_formats,
			store_nonpre_data,
			subbyte_format );

	MALI_DEBUG_ASSERT( (0 != width + height)
		|| (convert_request->rect.dx + width <= dst_width)
		|| (convert_request->rect.dy + height <= dst_height), ("Invalid conversion request. Need valid area!") );
	MALI_DEBUG_CODE( if ( M200_TEXEL_FORMAT_xRGB_8888 == convert_request->src_format.texel_format ) \
		MALI_DEBUG_ASSERT( MALI_FALSE == convert_request->src_format.reverse_order, ("HW issue 10983 violated. xRGB format with reverse order flag is unsupported.") ); );
	MALI_DEBUG_CODE( if ( MALI_TRUE == __m200_texel_format_is_alpha( convert_request->src_format.texel_layout ) ) MALI_DEBUG_ASSERT( MALI_TRUE == convert_request->src_format.alpha_to_one, ("src_format is alpha only and has alpha_to_one set which makes no sense." ) ); );
	MALI_DEBUG_CODE( if ( MALI_TRUE == __m200_texel_format_is_alpha( convert_request->dst_format.texel_layout ) ) MALI_DEBUG_ASSERT( MALI_TRUE == convert_request->dst_format.alpha_to_one, ("dst_format is alpha only and has alpha_to_one set which makes no sense." ) ); );

	/* Do not support conversions in dimensions (e.g. M200_TEXTURE_ADDRESSING_MODE_LINEAR -> M200_TEXTURE_ADDRESSING_MODE_2D_INTERLEAVED) */
	if (!((convert_request->src_format.texel_layout == convert_request->dst_format.texel_layout) || do_swizzle || do_deswizzle))
	{
		return MALI_FALSE;
	}

	/* 
	* The function assumes that source is in cpu byte order and changing it to
	* handle both endianess is major effort. Instead the source buffer is  
	* converted to cpu byte in a local buffer, this local buffer is then 
	* used as source. Destination is swapped to mali order if necessary.
	*/
#if MALI_BIG_ENDIAN 
	if (MALI_TRUE == convert_request->src_is_malimem)
	{
		s32 j;
		cpu_ptr = _mali_sys_malloc( src_pitch * height );
		if (NULL == cpu_ptr)
		{
			return MALI_FALSE;
		}

		for (j = 0; j < convert_request->rect.sy; j++)
		{
			s32 offset = src_pitch * j + convert_request->rect.sx * src_bytespp;
			_mali_sys_memcpy( cpu_ptr + offset, src_ptr + offset, src_pitch );
			_mali_byteorder_swap_endian( cpu_ptr, width * src_bytespp, src_bytespp );
		}
		src_ptr = cpu_ptr;
		convert_request->rect.sy = 0;
		sy = 0;
	}
#else
	sy = convert_request->rect.sy;
#endif
	sx = convert_request->rect.sx;
	dx = convert_request->rect.dx;
	dy = convert_request->rect.dy;

	subarea = ( (sx + sy + dx + dy > 0) ||
		(width + height != dst_width + dst_height) ); 

	/* SPECIAL CASES */

#if OLD_SPECIAL_CASES
	both_blocked = src_blocked && dst_blocked;

	/* Alpha to one fast path */
	if (MALI_FALSE == both_blocked && MALI_FALSE == do_deswizzle && MALI_TRUE == alpha_to_one && MALI_TRUE == same_formats && src_bpp == 32)
	{
		if (MALI_TRUE == do_swizzle)
		{
			if (MALI_FALSE == subarea)
			{
				_mali_convert_tex32_l_to_tex32_b_alpha_to_one(
					dst_ptr,
					dst_nonpre_ptr,
					src_ptr,
					dst_width,
					dst_height,
					src_pitch,
					convert_request->src_format.reverse_order );
			}
			else
			{
				_tex32_l_to_tex32_b_alpha_to_one_partial(
					dst_ptr,
					convert_request->dst_nonpre_ptr,
					src_ptr,
					&convert_request->rect,
					src_pitch,
					dst_width_aligned,
					convert_request->src_format.reverse_order );
			}
		}
		else
		{
			u8 *dst;
			u8 *src;
			u32 alpha_mask = (convert_request->src_format.reverse_order ? 0xFF : 0xFF000000);

			src = (u8*)src_ptr + sx * src_bytespp + sy * src_pitch;
			dst = dst_ptr + dx * dst_bytespp + dy * dst_pitch;
			if (MALI_TRUE == store_nonpre_data)
			{
				u8 *dst_nonpre = (u8*)dst_nonpre_ptr + dx * dst_bytespp + dy * dst_pitch;
				for (j = 0; j < height; j++)
				{
					const u32* src32 = (u32*)src;
					u32 *dst32 = (u32*)dst;
					u32 *dst32_nonpre = (u32*)dst_nonpre;
					for (i = 0; i < width; i++)
					{
						*(dst32) = *(src32++)|alpha_mask;
						*(dst32_nonpre++) = *(dst32++);
					}			
					src += src_pitch;
					dst += dst_pitch;
					dst_nonpre += dst_pitch;
				}

			}
			else
			{
				for (j = 0; j < height; j++)
				{
					const u32* src32 = (u32*)src;
					u32 *dst32 = (u32*)dst;
					for (i = 0; i < width; i++)
					{
						*(dst32++) = *(src32++)|alpha_mask;
					}			
					src += src_pitch;
					dst += dst_pitch;
				}
			}
		}

		retval = MALI_TRUE;
		goto cleanup;
	}

	both_32b_argb = MALI_PIXEL_FORMAT_A8R8G8B8 == convert_request->src_format.pixel_format && MALI_PIXEL_FORMAT_A8R8G8B8 == convert_request->dst_format.pixel_format;
	rbswap_mismatch = convert_request->dst_format.red_blue_swap != convert_request->src_format.red_blue_swap;

	/* fast path linear 32 bit to 32 bit */
	if ( MALI_TRUE == both_linear &&
	     MALI_TRUE == both_32b_argb &&
	     MALI_TRUE == rbswap_mismatch &&
	     convert_request->src_format.colorspace == convert_request->dst_format.colorspace &&
	     MALI_FALSE == store_nonpre_data )
	{
		u32 conv_rules_nonpre;
		u32 conv_rules_premult;
		u32 alpha_mask;
		
		conv_rules_nonpre = _mali_convert_setup_conversion_rules(&convert_request->src_format, &convert_request->dst_format);
		conv_rules_premult = ( convert_request->dst_nonpre_ptr ? conv_rules_nonpre | MALI_CONVERT_DST_PREMULT : conv_rules_nonpre );

		if (MALI_TRUE == convert_request->src_format.alpha_to_one)
		{
			u8 alpha_size = ( M200_TEXEL_FORMAT_xRGB_8888 == convert_request->src_format.texel_format ? 8 : __m200_texel_format_get_abits( convert_request->src_format.texel_format ) );
			u32 alpha_fill = ((1 << alpha_size)-1);
			alpha_mask = ( convert_request->src_format.reverse_order ? alpha_fill : alpha_fill << (src_bpp - alpha_size) );
		}
		else
		{
			alpha_mask = 0;
			if (MALI_FALSE == store_nonpre_data && convert_request->src_format.reverse_order == convert_request->dst_format.reverse_order)
			{
				_mali_convert_tex32_l_to_tex32_l_swap(convert_request, ptrs_unaligned);
				retval = MALI_TRUE;
				goto cleanup;
			}
		}
	
	  	/* 32 bit to 32 bit blocked 0 normalized 0 */
		{
			  u8 *src_texel_row = (u8*)src_ptr + sy * src_pitch;
			  u8 *dst_texel_row = (u8*)dst_ptr + dy * dst_pitch;
			  for (j = 0; j < height; j++)
			  {
				for (i = 0; i < width; i++)
				{
					u32 src_texel;
					u32 premult_texel;
					const s32 dst_x = dx + i;
					src_texel = ( *( ( u32 * ) ( MALI_REINTERPRET_CAST ( u32 * ) src_texel_row + sx + i ) ) ) | alpha_mask;
					premult_texel = _mali_convert_texel(&convert_request->src_format, &convert_request->dst_format, src_texel, conv_rules_premult, convert_request->source);
					*( MALI_REINTERPRET_CAST ( u32 * ) dst_texel_row + dst_x ) = premult_texel;
				}
				src_texel_row += src_pitch;
				dst_texel_row += dst_pitch;
			  }
		}
		retval = MALI_TRUE;
		goto cleanup;

	}

	/* If no conversion but full-area swizzling we may do fast swizzling for byte-sized formats*/
	if ( MALI_TRUE == do_swizzle &&
	     MALI_FALSE == subbyte_format &&
	     MALI_FALSE == subarea )
	{

		if (MALI_TRUE == same_formats)
		{
			if (MALI_TRUE == store_nonpre_data)
			{
				/* Fast path for swizzling of premultiplied formats with normalized storage.
				 * TODO: Support for other formats than RGBA_8888 */
				if ( MALI_FALSE == ptrs_unaligned &&
				     M200_TEXEL_FORMAT_ARGB_8888 == convert_request->src_format.texel_format &&
				     MALI_TRUE == convert_request->src_format.red_blue_swap &&
				     MALI_FALSE == convert_request->src_format.alpha_to_one )
				{
					if (MALI_TRUE == convert_request->src_format.reverse_order) 
					{
						_mali_convert_tex32_l_to_tex32_b_premult(dst_ptr, convert_request->dst_nonpre_ptr, src_ptr, dst_width, dst_height, src_pitch, MALI_TRUE);
					}
					else 
					{
						_mali_convert_tex32_l_to_tex32_b_premult(dst_ptr, convert_request->dst_nonpre_ptr, src_ptr, dst_width, dst_height, src_pitch, MALI_FALSE);
					}
					retval = MALI_TRUE;
					goto cleanup;
				}
			}
			else
			{
				switch (src_bpp)
				{
					
					case 8: 
						_mali_convert_tex8_l_to_tex8_b(dst_ptr, src_ptr, dst_width, dst_height, src_pitch);
						retval = MALI_TRUE;
						goto cleanup;
					case 16: 
						_mali_convert_tex16_l_to_tex16_b(dst_ptr, src_ptr, dst_width, dst_height, src_pitch);
						retval = MALI_TRUE;
						goto cleanup;
					case 24: 
						_mali_convert_tex24_l_to_tex24_b(dst_ptr, src_ptr, dst_width, dst_height, src_pitch);
						retval = MALI_TRUE;
						goto cleanup;
					case 32: 
						if (MALI_FALSE == ptrs_unaligned)
						{
							_mali_convert_tex32_l_to_tex32_b(dst_ptr, src_ptr, dst_width, dst_height, src_pitch);
							retval = MALI_TRUE;
							goto cleanup;
						}
						/* Fall through: If unaligned, do fallback swizzling */
					default: 
						/* For any other texel sizes use the non-optimal fallback swizzling */
						_m200_texture_interleave_16x16_blocked(dst_ptr, src_ptr, dst_width, dst_height, src_pitch, convert_request->dst_format.texel_format);
						retval = MALI_TRUE;
						goto cleanup;
				}
			}
		}
		else
		{
			if ( rbswap_mismatch &&
			     src_bpp == 32 &&
			     dst_bpp == 32 &&
			     MALI_FALSE == store_nonpre_data &&
			     convert_request->src_format.colorspace == convert_request->dst_format.colorspace &&
			     MALI_FALSE == ptrs_unaligned )
			{
				if (convert_request->dst_format.reverse_order == MALI_FALSE && convert_request->src_format.reverse_order == MALI_FALSE)
				{
					_mali_convert_tex32_l_to_tex32_b_swap(dst_ptr, src_ptr, dst_width, dst_height, src_pitch, 0x00ff00ff);
					retval = MALI_TRUE;
					goto cleanup;
				}
				else if (convert_request->dst_format.reverse_order == MALI_TRUE && convert_request->src_format.reverse_order == MALI_TRUE)
				{
					_mali_convert_tex32_l_to_tex32_b_swap(dst_ptr, src_ptr, dst_width, dst_height, src_pitch, 0xff00ff00);
					retval = MALI_TRUE;
					goto cleanup;
				}
			}
		}
	}

#endif /* OLD_SPECIAL_CASES */

	/* If neither format conversion nor swizzling is needed we do a memcpy for byte-sized formats */
	if ( MALI_FALSE == alpha_to_one &&
	     MALI_FALSE == subbyte_format &&
	     MALI_TRUE == both_linear &&
	     MALI_TRUE == same_formats &&
	     MALI_FALSE == store_nonpre_data )
	{
		if (src_pitch > 0 && src_pitch == dst_pitch && sx + dx == 0 && height + width == dst_height + dst_width)
		{
			_mali_sys_memcpy(dst_ptr + dy*dst_pitch, src_ptr + sy*src_pitch, src_pitch * height);
		}
		else
		{
			u8 *dst;
			u8 *src;

			src = (u8*)src_ptr + sx * src_bytespp + sy * src_pitch;
			dst = dst_ptr + dx * dst_bytespp + dy * dst_pitch;

			for (j = 0; j < height; j++)
			{
				_mali_sys_memcpy(dst, src, width * src_bytespp);
				src += src_pitch;
				dst += dst_pitch;
			}
		}	

		retval = MALI_TRUE;
		goto cleanup;
	}

	/* Support for swizzling of ETC textures */
	if ( convert_request->src_format.texel_format == M200_TEXEL_FORMAT_ETC &&
	     convert_request->dst_format.texel_format == M200_TEXEL_FORMAT_ETC &&
	     MALI_TRUE == do_swizzle )
	{

#define ETC_BLOCK_DIM 4
#define ETC_TEXELS_PER_BYTE 2
#define ETC_BLOCK_SIZE ( (ETC_BLOCK_DIM)*(ETC_BLOCK_DIM)/(ETC_TEXELS_PER_BYTE) )

		/* ETC does not support offsets */
		if (sx + sy + dx + dy > 0)
		{
			return MALI_FALSE;
		}

		/* ETC compresses 4x4 blocks into 64bit elements */
		width = MALI_ALIGN(width, ETC_BLOCK_DIM) / ETC_BLOCK_DIM;
		height = MALI_ALIGN(height, ETC_BLOCK_DIM) / ETC_BLOCK_DIM;
		/* Need pitch to be in bytes, not pixels */
		src_pitch = src_pitch * dst_bpp;

		/* Calculate the number of full width / height blocks. */
		xfull = (width >> 2) << 2;
		yfull = (height >> 2) << 2;
		if (xfull > 0 && yfull > 0)
		{
			xover = width - xfull;
			yover = height - yfull;
		}
		/* If one direction is not of tile length, we must do everything per pixel */
		else
		{
			xfull = yfull = 0;
			xover = width;
			yover = height;
		}
		tile = 0;

		/* Iterate the full tiles */
		for (j = 0; j < yfull; j+=4)
		{
			for (i = 0; i < xfull; i+=4)
			{
				/* NEON is dropped due to instructions not working on 64-bit elements (vrev, vzip) */
				for (l = 0; l < 4; l++)
				{
					const u32 y = j + l;
					const u32 src_cur_row = y * src_pitch;

					for (k = 0; k < 4; k++)
					{
						const u32 x = i + k;
						/* As ETC uses 4x4 tiles instead of 16x16 tiles, we cannot use MALI_CONVERT_BLOCKED_ADDRESS */
						const u32 dst_offset = (tile * 4 * 4 + mali_convert_block_interleave_lut[l*16 + k]) * ETC_BLOCK_SIZE;
						/* Each ETC-element is 64bit; We do a memcpy */
						_mali_sys_memcpy(dst_ptr + dst_offset, src_ptr + src_cur_row + x * ETC_BLOCK_SIZE, ETC_BLOCK_SIZE);
					}
				}
				tile++;
			}
			if (xover)
			{
				tile++;
			}
		}
		if (xover)
		{
			for (j = 0; j < height; j++)
			{
				const u32 src_cur_row = j * src_pitch;

				for (i = xfull; i < width; i++)
				{
					const u32 dst_offset = (((j/4) * (MALI_ALIGN(width, 4)/4) + (i/4)) * 4 * 4 + mali_convert_block_interleave_lut[(j%4)*16 + (i%4)]) * ETC_BLOCK_SIZE;
					_mali_sys_memcpy(dst_ptr + dst_offset, src_ptr + src_cur_row + i * ETC_BLOCK_SIZE, ETC_BLOCK_SIZE);

				}
			}
		}
		if (yover)
		{
			for (j = yfull; j < height; j++)
			{
				const u32 src_cur_row = j * src_pitch;

				/* xfull to width, if existing, is covered by xover */
				for (i = 0; i < xfull; i++)
				{
					const u32 dst_offset = (((j/4) * (MALI_ALIGN(width, 4)/4) + (i/4)) * 4 * 4 + mali_convert_block_interleave_lut[(j%4)*16 + (i%4)]) * ETC_BLOCK_SIZE;
					_mali_sys_memcpy(dst_ptr + dst_offset, src_ptr + src_cur_row + i * ETC_BLOCK_SIZE, ETC_BLOCK_SIZE);
				}
			}
		}
#undef ETC_BLOCK_DIM
#undef ETC_TEXELS_PER_BYTE
#undef ETC_BLOCK_SIZE
		retval = MALI_TRUE;
		goto cleanup;
	}

	/* Handle the virtual formats */
	if ( (convert_request->src_format.texel_format == M200_TEXEL_FORMAT_VIRTUAL_DEPTH32 || 
	      convert_request->src_format.texel_format == M200_TEXEL_FORMAT_VIRTUAL_DEPTH16 ||
	      convert_request->src_format.texel_format == M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24) &&
	     MALI_TRUE == do_swizzle )
	{
		/* TODO: the _partial functions are slow compared to the _full_width or _full_block functions! the following code is not optimal. 
		 *       See optimization bugzilla 10970 for details */
		switch (convert_request->src_format.texel_format)
		{
			case M200_TEXEL_FORMAT_VIRTUAL_DEPTH32:
				if (convert_request->dst_format.texel_format == M200_TEXEL_FORMAT_L_16)
				{
					if (MALI_FALSE == subarea)
					{
						_mali_convert_tex32_l_to_tex16_b(dst_ptr, src_ptr, dst_width, dst_height, src_pitch);
					}
					else
					{
						_tex32_l_to_tex16_b_partial(dst_ptr, src_ptr, &(convert_request->rect), src_pitch, dst_width_aligned);
					}

					retval = MALI_TRUE;
					goto cleanup;
				}
				break;
			case M200_TEXEL_FORMAT_VIRTUAL_DEPTH16:
				if (convert_request->dst_format.texel_format == M200_TEXEL_FORMAT_L_16)
				{
					if (MALI_FALSE == subarea)
					{
						_mali_convert_tex16_l_to_tex16_b(dst_ptr, src_ptr, dst_width, dst_height, src_pitch);
					}
					else
					{
						_tex16_l_to_tex16_b_partial(dst_ptr, src_ptr, &(convert_request->rect), src_pitch, dst_width_aligned);
					}

					retval = MALI_TRUE;
					goto cleanup;
				}
				break;
			case M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24:
				if (convert_request->dst_format.texel_format == M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8)
				{
					if (MALI_FALSE == subarea)
					{
						_mali_convert_tex8_24_l_to_tex24_8_b(dst_ptr, src_ptr, dst_width, dst_height, src_pitch);
					}
					else
					{
						_tex8_24_l_to_tex24_8_b_partial(dst_ptr, src_ptr, &(convert_request->rect), src_pitch, dst_width_aligned);
					}

					retval = MALI_TRUE;
					goto cleanup;
				}
				break;
			default:
				break;
		}
	}

	/* CONVERSION */

	/* Check that conversion is supported */
	if (MALI_FALSE == _conversion_supported(convert_request) || MALI_TRUE == _conversion_not_color(convert_request))
	{
		/* TODO: support for remaining conversions */
		retval = MALI_FALSE;
		goto cleanup;
	}

	/* Set up conversion variables */
	conv_rules_nonpre = _mali_convert_setup_conversion_rules(&convert_request->src_format, &convert_request->dst_format);
	conv_rules_premult = ( dst_nonpre_ptr ? conv_rules_nonpre | MALI_CONVERT_DST_PREMULT : conv_rules_nonpre );

	/* If we do not have to do any conversion on the texels */
	pass_through = same_formats && !alpha_to_one;
	if (convert_request->source == MALI_CONVERT_SOURCE_OPENVG || convert_request->source == MALI_CONVERT_SOURCE_SHARED)
	{
		pass_through &= (conv_rules_nonpre == conv_rules_premult);
	}

	/* Calculate the number of full width / height blocks. */
	xfull = (width >> 4) << 4;
	yfull = (height >> 4) << 4;
	if (xfull > 0 && yfull > 0)
	{
		xover = width - xfull;
		yover = height - yfull;
	}
	/* If one direction is not of tile length, we must do everything per pixel */
	else
	{
		_mali_convert_texture_slowpath(
			convert_request,
			0, width, 0, height,
			src_bpp, dst_bpp,
			src_blocked,
			dst_blocked,
			conv_rules_nonpre,
			conv_rules_premult,
			pass_through,
			same_formats );
		retval = MALI_TRUE;
		goto cleanup;
	}
	tile = 0;

#if (MALI_PLATFORM_ARM_NEON)
	/* Do full tiles instead of pixels near the edges; Optimization only works for blocked->blocked because of their allocated areas. */
	if (MALI_TRUE == do_neon && MALI_TRUE == src_blocked && MALI_TRUE == dst_blocked)
	{
		xfull = MALI_ALIGN(width, 16);
		yfull = MALI_ALIGN(height, 16);
		xover = yover = 0;
	}
#else 
	MALI_IGNORE(do_neon);
#endif

	/* Iterate the full tiles */
	for (j = 0; j < yfull; j+=16)
	{
		for (i = 0; i < xfull; i+=16)
		{
			/* If NEON, can do entire tile */
#if (MALI_PLATFORM_ARM_NEON)
			if (MALI_TRUE == do_neon)
			{
				/* Fastpath: assembly swizzle for same formats */
				if (MALI_TRUE == same_formats && MALI_TRUE == do_swizzle && MALI_FALSE == store_nonpre_data)
				{
					src_address = (sy+j) * src_pitch + (sx+i) * src_bytespp;
					dst_address = tile * 16 * 16 * dst_bytespp;
					switch (src_bpp)
					{
						case 8:
							_mali_osu_tex8_l_to_tex8_b_full_block(dst_ptr + dst_address, src_ptr + src_address, src_pitch);
							break;
						case 16:
							_mali_osu_tex16_l_to_tex16_b_full_block(dst_ptr + dst_address, src_ptr + src_address, src_pitch);
							break;
						case 24:
							_mali_osu_tex24_l_to_tex24_b_full_block(dst_ptr + dst_address, src_ptr + src_address, src_pitch);
							break;
						case 32:
							_mali_osu_tex32_l_to_tex32_b_full_block(dst_ptr + dst_address, src_ptr + src_address, src_pitch);
							break;
						default:
							MALI_DEBUG_ASSERT(0, ("Not yet implemented"));
					}
					tile++;
					continue;
				}
				/* Fastpath: NEON intrinsics operations on 8x8 tiles.
				 * @note: The tiles are looped in a NW, NE, SE, SW pattern to support (de-)swizzling.
				 */
				else
				{
					u32 src_row_offset, dst_row_offset, src_tile_offset, dst_tile_offset;
					if (MALI_TRUE == src_blocked)
					{
						src_address = tile * 16 * 16 * src_bytespp;
						src_row_offset = 8 * src_bytespp;
						src_tile_offset = 8 * src_row_offset;
					}
					else
					{
						src_address = (sy+j) * src_pitch + (sx+i) * src_bytespp;
						src_row_offset = src_pitch;
						src_tile_offset = 8 * src_bytespp;
					}
					if (MALI_TRUE == dst_blocked)
					{
						dst_address = tile * 16 * 16 * dst_bytespp;
						dst_row_offset = 8 * dst_bytespp;
						dst_tile_offset = 8 * dst_row_offset;
					}
					else
					{
						dst_address = (dy+j) * dst_pitch + (dx+i) * dst_bytespp;
						dst_row_offset = dst_pitch;
						dst_tile_offset = 8 * dst_bytespp;
					}
					/* NW */
					_mali_convert_intrinsics_8x8_tile(
						(u8*)src_ptr+src_address,
						src_row_offset,
						src_surfspec,
						(u8*)dst_ptr+dst_address,
						(store_nonpre_data ? dst_nonpre_ptr + dst_address : NULL),
						dst_row_offset,
						dst_surfspec,
						convert_request->source,
						conv_rules_premult,
						do_swizzle,
						do_deswizzle );
					/* NE */
					src_address += src_tile_offset;
					dst_address += dst_tile_offset;
					_mali_convert_intrinsics_8x8_tile(
						(u8*)src_ptr+src_address,
						src_row_offset,
						src_surfspec,
						(u8*)dst_ptr+dst_address,
						(store_nonpre_data ? dst_nonpre_ptr + dst_address : NULL),
						dst_row_offset,
						dst_surfspec,
						convert_request->source,
						conv_rules_premult,
						do_swizzle,
						do_deswizzle );
					/* SE */
					src_address += 8 * src_row_offset;
					dst_address += 8 * dst_row_offset;
					_mali_convert_intrinsics_8x8_tile(
						(u8*)src_ptr+src_address,
						src_row_offset,
						src_surfspec,
						(u8*)dst_ptr+dst_address,
						(store_nonpre_data ? dst_nonpre_ptr + dst_address : NULL),
						dst_row_offset,
						dst_surfspec,
						convert_request->source,
						conv_rules_premult,
						do_swizzle,
						do_deswizzle );
					/* SW */
					if (MALI_TRUE == src_blocked)
					{
						src_address += src_tile_offset;
					}
					else
					{
						src_address -= src_tile_offset;
					}
					if (MALI_TRUE == dst_blocked)
					{
						dst_address += dst_tile_offset;
					}
					else
					{
						dst_address -= dst_tile_offset;
					}
					_mali_convert_intrinsics_8x8_tile(
						(u8*)src_ptr+src_address,
						src_row_offset,
						src_surfspec,
						(u8*)dst_ptr+dst_address,
						(store_nonpre_data ? dst_nonpre_ptr + dst_address : NULL),
						dst_row_offset,
						dst_surfspec,
						convert_request->source,
						conv_rules_premult,
						do_swizzle,
						do_deswizzle );
					tile++;
					continue;
				}
			}
#endif /* (MALI_PLATFORM_ARM_NEON) */

			/* Slowpath: if no NEON, have to loop the texels */
			_mali_convert_texture_slowpath(
				convert_request,
				i, i + 16, j, j + 16,
				src_bpp, dst_bpp,
				src_blocked,
				dst_blocked,
				conv_rules_nonpre,
				conv_rules_premult,
				pass_through,
				same_formats );
		}
#if (MALI_PLATFORM_ARM_NEON)
		/* Special offset rules for interleaved textures near the width-edge.
		 * @note All interleaved textures have allocated space equal to a multiple of tiles.
		 */
		/* If we are swizzling a subarea, we have to offset the dst_address accordingly. */
		if (MALI_TRUE == do_swizzle && dst_width > width)
		{
			tile += (dst_width - width)/16;
		}
		/* If we are deswizzling a subarea, we have to offset the src_address accordingly. */
		if (MALI_TRUE == do_deswizzle && src_width > width)
		{
			tile += (src_width - width)/16;
		}
		/* If the conversion rectangle is not a multiple of tiles, we have to offset all interleaved addresses by one tile */
		if (xover)
		{
			tile++;
		}
#endif /* (MALI_PLATFORM_ARM_NEON) */
	}

	/* Do non-full tiles per texel instead of per tile */
	if (xover)
	{
#if (MALI_PLATFORM_ARM_NEON)
		/* Do NEON conversion on 8x8 blocks in non-tiled edges if possible */
		if (MALI_TRUE == do_neon && xover >= 8 && _neon_conversion_supported(convert_request))
		{
			for (j = 0; j < (height & ~0x07); j += 8)
			{
				u32 src_row_offset, dst_row_offset;
				if (MALI_TRUE == src_blocked)
				{
					src_row_offset = 8 * src_bytespp;
					src_address = ((j & ~0x0F) * src_width_aligned + xfull * 16 + 3 * (j & 0x08) * 8) * src_bytespp;
				}
				else
				{
					src_row_offset = src_pitch;
					src_address = (sy+j) * src_pitch + (sx+xfull) * src_bytespp;
				}
				if (MALI_TRUE == dst_blocked)
				{
					dst_row_offset = 8 * dst_bytespp;
					dst_address = ((j & ~0x0F) * dst_width_aligned + xfull * 16 + 3 * (j & 0x08) * 8) * dst_bytespp;
				}
				else
				{
					dst_row_offset = dst_pitch;
					dst_address = (dy+j) * dst_pitch + (dx+xfull) * dst_bytespp;
				}

				_mali_convert_intrinsics_8x8_tile(
					(u8*)src_ptr+src_address,
					src_row_offset,
					src_surfspec,
					(u8*)dst_ptr+dst_address,
					(store_nonpre_data ? dst_nonpre_ptr + dst_address : NULL),
					dst_row_offset,
					dst_surfspec,
					convert_request->source,
					conv_rules_premult,
					do_swizzle,
					do_deswizzle );
			}
			xfull += 8;
		}
#endif /* (MALI_PLATFORM_ARM_NEON) */
		_mali_convert_texture_slowpath(
			convert_request,
			xfull, width, 0, height,
			src_bpp, dst_bpp,
			src_blocked,
			dst_blocked,
			conv_rules_nonpre,
			conv_rules_premult,
			pass_through,
			same_formats );
	}
	if (yover)
	{
#if (MALI_PLATFORM_ARM_NEON)
		/* Do NEON conversion on 8x8 blocks in non-tiled edges if possible */
		if (MALI_TRUE == do_neon && yover >= 8 && _neon_conversion_supported(convert_request))
		{
			for (i = 0; i < xfull; i += 8)
			{
				u32 src_row_offset, dst_row_offset;
				if (MALI_TRUE == src_blocked)
				{
					src_row_offset = 8 * src_bytespp;
					src_address = (yfull * src_width_aligned + (i & ~0x0F) * 16 + (i & 0x8) * 8) * src_bytespp;
				}
				else
				{
					src_row_offset = src_pitch;
					src_address = (sy+yfull) * src_pitch + (sx+i) * src_bytespp;
				}
				if (MALI_TRUE == dst_blocked)
				{
					dst_row_offset = 8 * dst_bytespp;
					dst_address = (yfull * dst_width_aligned + (i & ~0x0F) * 16 + (i & 0x8) * 8) * dst_bytespp;
				}
				else
				{
					dst_row_offset = dst_pitch;
					dst_address = (dy+yfull) * dst_pitch + (dx+i) * dst_bytespp;
				}

				_mali_convert_intrinsics_8x8_tile(
					(u8*)src_ptr+src_address,
					src_row_offset,
					src_surfspec,
					(u8*)dst_ptr+dst_address,
					(store_nonpre_data ? dst_nonpre_ptr + dst_address : NULL),
					dst_row_offset,
					dst_surfspec,
					convert_request->source,
					conv_rules_premult,
					do_swizzle,
					do_deswizzle );
			}
			yfull += 8;
		}
#endif /* (MALI_PLATFORM_ARM_NEON) */
		/* xfull to width, if existing, is covered by xover */
		_mali_convert_texture_slowpath(
			convert_request,
			0, xfull, yfull, height,
			src_bpp, dst_bpp,
			src_blocked,
			dst_blocked,
			conv_rules_nonpre,
			conv_rules_premult,
			pass_through,
			same_formats );
	}

	retval = MALI_TRUE;

cleanup:

#if MALI_BIG_ENDIAN
	/* On big endian platforms the source data always cpu mem. If destination is mali_mem we need to swap back */
	if (MALI_TRUE == convert_request->dst_is_malimem)
	{
		for (j = 0; j < dy; j++)
		{
			s32 offset = dst_pitch * j + dx * dst_bytespp;
			_mali_byteorder_swap_endian(dst_ptr + offset, width * dst_bytespp, dst_bytespp);
		}
	}
	if (NULL != cpu_ptr)
	{
		_mali_sys_free(cpu_ptr);
	}
#endif

	return retval;
#undef IS_UNALIGNED
}

#endif  /* !__SYMBIAN32__ || SYMBIAN_LIB_MALI_BASE */
