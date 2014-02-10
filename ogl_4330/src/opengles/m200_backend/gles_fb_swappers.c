/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "gles_fb_swappers.h"

/* rgb565 ********************************************************************/

void _color_swap_rgb565( void *src, int width, int height, int padding )
{
	u16 *src16;
	u16 pix16;
	u16 c1,c2,c3;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 2);

	src16 = (u16*)src;

	for ( y = 0; y < height; ++y )
	{
		for ( x = 0; x < width; ++x )
		{
			pix16 = *src16;
			c3 =  pix16 & 0x1f;
			c2 = (pix16 >> 5 ) & 0x3f;
			c1 = (pix16 >> 11) & 0x1f;
			*src16 = (c3<<11)|(c2<<5)|c1;
			++src16;
		}
		src16 += padding;
	}
}

/* al88 ********************************************************************/

void _color_swap_al88( void *src, int width, int height, int padding )
{
	u8 *src8;
	u8 temp;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);

	src8 = (u8*)src;
	for ( y = 0; y < height; ++y )
	{
		for ( x = 0; x < width; ++x )
		{
			temp = *src8; /* alpha to temp */
			*src8 = *(src8+1); /* luminance to alpha */
			*(src8+1) = temp; /* temp to luminance */
			src8 +=2; /* advance 2 components */
		}
		src8 += padding;
	}
}


/* rgb888 ********************************************************************/

void _color_swap_rgb888( void *src, int width, int height, int padding )
{
	u8 *src8;
	u8 temp;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);

	src8 = (u8*)src;
	for ( y = 0; y < height; ++y )
	{
		for ( x = 0; x < width; ++x )
		{
			temp = *src8; /* red to temp */
			*src8 = *(src8+2); /* blue to red */
			*(src8+2) = temp; /* temp to blue */
			src8 +=3; /* advance 3 components */
		}
		src8 += padding;
	}
}

/* argb8888 ******************************************************************/

void _color_swap_argb8888( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u32 *src32;
	u32 pix32;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 4);

	src32 = (u32*)src;

	if (is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix32 = *src32;
				/* XGXA - swap c1 and c3 */
				c4 =  pix32 & 0xff;
				c1 = (pix32 >> 8 ) & 0xff;
				c2 = (pix32 >> 16) & 0xff;
				c3 =  pix32 >> 24;
				*src32 = (c1<<24)|(c2<<16)|(c3<<8)|c4;
				++src32;
			}
			src32 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix32 = *src32;
				/* AXGX - swap c2 and c4 */
				c2 =  pix32 & 0xff;
				c3 = (pix32 >> 8 ) & 0xff;
				c4 = (pix32 >> 16) & 0xff;
				c1 =  pix32 >> 24;
				*src32 = (c1<<24)|(c2<<16)|(c3<<8)|c4;
				++src32;
			}
			src32 += padding;
		}
	}
}

void _color_invert_argb8888( void *src, int width, int height, int padding )
{
	u32 *src32;
	u32 pix32;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 4);

	src32 = (u32*)src;

	for ( y = 0; y < height; ++y )
	{
		for ( x = 0; x < width; ++x )
		{
			pix32 = *src32;
			c4 =  pix32 & 0xff;
			c3 = (pix32 >> 8 ) & 0xff;
			c2 = (pix32 >> 16) & 0xff;
			c1 =  pix32 >> 24;
			*src32 = (c4<<24)|(c3<<16)|(c2<<8)|c1;
			++src32;
		}
		src32 += padding;
	}

}

void _color_swap_and_invert_argb8888( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u32 *src32;
	u32 pix32;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER( src );
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 4);

	src32 = (u32*)src;

	if (is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix32 = *src32;
				/* XGXA - swap c1 and c3 */
				c4 =  pix32 & 0xff;
				c1 = (pix32 >> 8 ) & 0xff;
				c2 = (pix32 >> 16) & 0xff;
				c3 =  pix32 >> 24;
				*src32 = (c4<<24)|(c3<<16)|(c2<<8)|c1;
				++src32;
			}
			src32 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix32 = *src32;
				/* AXGX - swap c2 and c4 */
				c2 =  pix32 & 0xff;
				c3 = (pix32 >> 8 ) & 0xff;
				c4 = (pix32 >> 16) & 0xff;
				c1 =  pix32 >> 24;
				*src32 = (c4<<24)|(c3<<16)|(c2<<8)|c1;
				++src32;
			}
			src32 += padding;
		}
	}
}

/* argb4444 ******************************************************************/

void _color_swap_argb4444( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u16 *src16;
	u16 pix16;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 2);

	src16 = (u16*)src;

	if (is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* XGXA - swap c1 and c3 */
				c4 =  pix16 & 0xf;
				c1 = (pix16 >> 4 ) & 0xf;
				c2 = (pix16 >> 8 ) & 0xf;
				c3 = (pix16 >> 12) & 0xf;
				*src16 = (u16)((c1<<12)|(c2<<8)|(c3<<4)|c4);
				++src16;
			}
			src16 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* AXGX - swap c2 and c4 */
				c2 =  pix16 & 0xf;
				c3 = (pix16 >> 4 ) & 0xf;
				c4 = (pix16 >> 8 ) & 0xf;
				c1 = (pix16 >> 12) & 0xf;
				*src16 = (u16)((c1<<12)|(c2<<8)|(c3<<4)|c4);
				++src16;
			}
			src16 += padding;
		}
	}
}

void _color_invert_argb4444( void *src, int width, int height, int padding )
{
	u16 *src16;
	u16 pix16;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 2);

	src16 = (u16*)src;

	for ( y = 0; y < height; ++y )
	{
		for ( x = 0; x < width; ++x )
		{
			pix16 = *src16;
			c4 =  pix16 & 0xf;
			c3 = (pix16 >> 4 ) & 0xf;
			c2 = (pix16 >> 8 ) & 0xf;
			c1 = (pix16 >> 12) & 0xf;
			*src16 = (u16)((c4<<12)|(c3<<8)|(c2<<4)|c1);
			++src16;
		}
		src16 += padding;
	}
}

void _color_swap_and_invert_argb4444( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u16 *src16;
	u16 pix16;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 2);

	src16 = (u16*)src;

	if (is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* XGXA - swap c1 and c3 */
				c4 =  pix16 & 0xf;
				c1 = (pix16 >> 4 ) & 0xf;
				c2 = (pix16 >> 8 ) & 0xf;
				c3 = (pix16 >> 12) & 0xf;
				*src16 = (u16)((c4<<12)|(c3<<8)|(c2<<4)|c1);
				++src16;
			}
			src16 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* AXGX - swap c2 and c4 */
				c2 =  pix16 & 0xf;
				c3 = (pix16 >> 4 ) & 0xf;
				c4 = (pix16 >> 8 ) & 0xf;
				c1 = (pix16 >> 12) & 0xf;
				*src16 = (u16)((c4<<12)|(c3<<8)|(c2<<4)|c1);
				++src16;
			}
			src16 += padding;
		}
	}
}

/* argb1555 ******************************************************************/

void _color_swap_argb1555( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u16 *src16;
	u16 pix16;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 2);

	src16 = (u16*)src;

	if(is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* XGXA 5551 - swap c1 and c3 */
				c4 =  pix16 & 0x1; /* 1 bit alpha */
				c1 = (pix16 >> 1 ) & 0x1f;
				c2 = (pix16 >> 6 ) & 0x1f;
				c3 = (pix16 >> 11) & 0x1f;
				*src16 = (u16)((c1<<11)|(c2<<6 )|(c3<<1)|c4);
				++src16;
			}
			src16 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* AXGX 1555 - swap c2 and c4 */
				c2 =  pix16 & 0x1f;
				c3 = (pix16 >> 5 ) & 0x1f;
				c4 = (pix16 >> 10) & 0x1f;
				c1 = (pix16 >> 15) & 0x1; /* 1 bit alpha */
				*src16 = (u16)((c1<<15)|(c2<<10)|(c3<<5)|c4);
				++src16;
			}
			src16 += padding;
		}
	}
}

void _color_invert_argb1555( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u16 *src16;
	u16 pix16;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 2);

	src16 = (u16*)src;

	if(is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* XGXA 5551 */
				c4 =  pix16 & 0x1; /* 1 bit alpha */
				c3 = (pix16 >> 1 ) & 0x1f;
				c2 = (pix16 >> 6 ) & 0x1f;
				c1 = (pix16 >> 11) & 0x1f;
				*src16 = (u16)((c4<<15)|(c3<<10)|(c2<<5)|c1);
				++src16;
			}
			src16 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* AXGX 1555 */
				c4 =  pix16 & 0x1f;
				c3 = (pix16 >> 5 ) & 0x1f;
				c2 = (pix16 >> 10) & 0x1f;
				c1 = (pix16 >> 15) & 0x1; /* 1 bit alpha */
				*src16 = (u16)((c4<<11)|(c3<<6 )|(c2<<1)|c1);
				++src16;
			}
			src16 += padding;
		}
	}
}

void _color_swap_and_invert_argb1555( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u16 *src16;
	u16 pix16;
	u32 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 2);

	src16 = (u16*)src;

	if(is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* XGXA 5551 - swap c1 and c3 */
				c4 =  pix16 & 0x1; /* 1 bit alpha */
				c1 = (pix16 >> 1 ) & 0x1f;
				c2 = (pix16 >> 6 ) & 0x1f;
				c3 = (pix16 >> 11) & 0x1f;
				*src16 = (u16)((c4<<15)|(c3<<10)|(c2<<5)|c1);
				++src16;
			}
			src16 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix16 = *src16;
				/* AXGX 1555 - swap c2 and c4 */
				c2 =  pix16 & 0x1f;
				c3 = (pix16 >> 5 ) & 0x1f;
				c4 = (pix16 >> 10) & 0x1f;
				c1 = (pix16 >> 15) & 0x1; /* 1 bit alpha */
				*src16 = (u16)((c4<<11)|(c3<<6 )|(c2<<1)|c1);
				++src16;
			}
			src16 += padding;
		}
	}
}

/* al_16_16 ********************************************************************/

void _color_swap_al_16_16( void *src, int width, int height, int padding )
{
	u16 *src16;
	u16 temp;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);

	src16 = (u16*)src;
	for ( y = 0; y < height; ++y )
	{
		for ( x = 0; x < width; ++x )
		{
			temp = *src16; /* alpha to temp */
			*src16 = *(src16+1); /* luminance to alpha */
			*(src16+1) = temp; /* temp to luminance */
			src16 +=2; /* advance 2 components */
		}
		src16 += padding;
	}
}

/* argb_16_16_16_16 ******************************************************************/

void _color_swap_argb_16_16_16_16( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u64 *src64;
	u64 pix64;
	u64 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 4);

	src64 = (u64*)src;

	if (is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix64 = *src64;
				/* XGXA - swap c1 and c3 */
				c4 =  pix64 & 0xffff;
				c1 = (pix64 >> 16 ) & 0xffff;
				c2 = (pix64 >> 32) & 0xffff;
				c3 =  pix64 >> 48;
				*src64 = (c1<<48)|(c2<<32)|(c3<<16)|c4;
				++src64;
			}
			src64 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix64 = *src64;
				/* AXGX - swap c2 and c4 */
				c2 =  pix64 & 0xffff;
				c3 = (pix64 >> 16 ) & 0xffff;
				c4 = (pix64 >> 32) & 0xffff;
				c1 =  pix64 >> 48;
				*src64 = (c1<<48)|(c2<<32)|(c3<<16)|c4;
				++src64;
			}
			src64 += padding;
		}
	}
}

void _color_invert_argb_16_16_16_16( void *src, int width, int height, int padding )
{
	u64 *src64;
	u64 pix64;
	u64 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 4);

	src64 = (u64*)src;

	for ( y = 0; y < height; ++y )
	{
		for ( x = 0; x < width; ++x )
		{
			pix64 = *src64;
			c4 =  pix64 & 0xffff;
			c3 = (pix64 >> 16 ) & 0xffff;
			c2 = (pix64 >> 32) & 0xffff;
			c1 =  pix64 >> 48;
			*src64 = (c4<<48)|(c3<<32)|(c2<<16)|c1;
			++src64;
		}
		src64 += padding;
	}

}

void _color_swap_and_invert_argb_16_16_16_16( void *src, int width, int height, int padding, GLboolean is_src_inverted )
{
	u64 *src64;
	u64 pix64;
	u64 c1,c2,c3,c4;
	int x,y;

	MALI_DEBUG_ASSERT_POINTER( src );
	MALI_DEBUG_ASSERT_ALIGNMENT(src, 4);

	src64 = (u64*)src;

	if (is_src_inverted)
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix64 = *src64;
				/* XGXA - swap c1 and c3 */
				c4 =  pix64 & 0xffff;
				c1 = (pix64 >> 16 ) & 0xffff;
				c2 = (pix64 >> 32) & 0xffff;
				c3 =  pix64 >> 48;
				*src64 = (c4<<48)|(c3<<32)|(c2<<16)|c1;
				++src64;
			}
			src64 += padding;
		}
	}
	else
	{
		for ( y = 0; y < height; ++y )
		{
			for ( x = 0; x < width; ++x )
			{
				pix64 = *src64;
				/* AXGX - swap c2 and c4 */
				c2 =  pix64 & 0xffff;
				c3 = (pix64 >> 16 ) & 0xffff;
				c4 = (pix64 >> 32) & 0xffff;
				c1 =  pix64 >> 48;
				*src64 = (c4<<48)|(c3<<32)|(c2<<16)|c1;
				++src64;
			}
			src64 += padding;
		}
	}
}


