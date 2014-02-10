/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/m200_texel_format.h>

MALI_EXPORT s32 __m200_texel_format_get_bytes_per_copy_element( m200_texel_format format )
{
	static const s32 elem_size[] = {
			1,    /* M200_TEXEL_FORMAT_L_1 */
			1,    /* M200_TEXEL_FORMAT_A_1 */
			1,    /* M200_TEXEL_FORMAT_I_1 */
			1,    /* M200_TEXEL_FORMAT_AL_11 */
			1,    /* M200_TEXEL_FORMAT_L_4 */
			1,    /* M200_TEXEL_FORMAT_A_4 */
			1,    /* M200_TEXEL_FORMAT_I_4 */
			1,    /* M200_TEXEL_FORMAT_ARGB_1111 */
			1,    /* M200_TEXEL_FORMAT_AL_44 */
			1,    /* M200_TEXEL_FORMAT_L_8 */
			1,    /* M200_TEXEL_FORMAT_A_8 */
			1,    /* M200_TEXEL_FORMAT_I_8 */
			1,    /* M200_TEXEL_FORMAT_RGB_332 */
			1,    /* M200_TEXEL_FORMAT_ARGB_2222 */
			2,	  /* M200_TEXEL_FORMAT_RGB_565 */
			2,	  /* M200_TEXEL_FORMAT_ARGB_1555 */
			2,	  /* M200_TEXEL_FORMAT_ARGB_4444 */
			1,	  /* M200_TEXEL_FORMAT_AL_88 */
			2,	  /* M200_TEXEL_FORMAT_L_16 */
			2,	  /* M200_TEXEL_FORMAT_A_16 */
			2,	  /* M200_TEXEL_FORMAT_I_16 */
	#ifndef HARDWARE_ISSUE_7486
			1,	  /* M200_TEXEL_FORMAT_RGB_888 */
	#else
			-1, /* need an entry for "format 21", or we offset all subsequent formats wrongly */
	#endif
			1,   /* M200_TEXEL_FORMAT_ARGB_8888 */
			1,   /* M200_TEXEL_FORMAT_xRGB_8888 */
			4,   /* M200_TEXEL_FORMAT_ARGB_2_10_10_10 */
			4,   /* M200_TEXEL_FORMAT_RGB_11_11_10 */
			4,   /* M200_TEXEL_FORMAT_RGB_10_12_10 */
			2,   /* M200_TEXEL_FORMAT_AL_16_16 */
			2,   /* M200_TEXEL_FORMAT_ARGB_16_16_16_16 */
			1,    /* M200_TEXEL_FORMAT_PAL_4 */
			1,    /* M200_TEXEL_FORMAT_PAL_8 */
			-1,	  /* M200_TEXEL_FORMAT_FLXTC */
			1,    /* M200_TEXEL_FORMAT_ETC */
			-1,	  /* Reserved */
			2,	  /* M200_TEXEL_FORMAT_L_FP16 */
			2,	  /* M200_TEXEL_FORMAT_A_FP16 */
			2,	  /* M200_TEXEL_FORMAT_I_FP16 */
			2,	  /* M200_TEXEL_FORMAT_AL_FP16 */
			2, /* M200_TEXEL_FORMAT_ARGB_FP16 */
			-1,	  /* Reserved */
			-1,	  /* Reserved */
			-1,   /* Reserved */
			-1,	  /* Reserved */
			-1,	  /* Reserved */
			4,	  /* M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8 */
			4,   /* M200_TEXEL_FORMAT_CONVOLUTION_TEXTURE_64 */
			2,   /* M200_TEXEL_FORMAT_RGB_16_16_16 */
			2,   /* M200_TEXEL_FORMAT_RGB_FP16 */
			-1,   /* Reserved */
			-1,   /* Reserved */
			4,   /* M200_TEXEL_FORMAT_VERBATIM_COPY32 */
			/* 51-62 are Reserved */
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			0     /* M200_TEXEL_FORMAT_NO_TEXTURE */
		};

		MALI_DEBUG_ASSERT( (sizeof(elem_size) / sizeof(elem_size[0])) > format,
			("invalid texel format 0x%x", format)
		);
		MALI_DEBUG_ASSERT( elem_size[ format ] >= 0,
			("unsupported texel format :(( 0x%x", format)
		);
		return elem_size[ format ];
}

typedef struct __m200_texel_format_bpc 
{
	u32 r;
	u32 g;
	u32 b;
	u32 a;
	u32 d;
	u32 s;
	u32 l;
	u32 i;
} __m200_texel_format_bpc;

MALI_STATIC __m200_texel_format_bpc __m200_texel_format_bpc_table[] =
{
	{0, 0, 0, 0, 0, 0, 1, 0},	/* M200_TEXEL_FORMAT_L_1 = 0,  */
	{0, 0, 0, 1, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_A_1 = 1,  */
	{0, 0, 0, 0, 0, 0, 0, 1},	/* M200_TEXEL_FORMAT_I_1 = 2,  */
	{0, 0, 0, 1, 0, 0, 1, 0},	/* M200_TEXEL_FORMAT_AL_11 = 3,  */
	{0, 0, 0, 0, 0, 0, 4, 0},	/* M200_TEXEL_FORMAT_L_4 = 4,  */
	{0, 0, 0, 4, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_A_4 = 5,  */
	{0, 0, 0, 0, 0, 0, 0, 4},	/* M200_TEXEL_FORMAT_I_4 = 6,  */
	{1, 1, 1, 1, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ARGB_1111 = 7,  */
	{0, 0, 0, 4, 0, 0, 4, 0},	/* M200_TEXEL_FORMAT_AL_44 = 8,  */
	{0, 0, 0, 0, 0, 0, 8, 0},	/* M200_TEXEL_FORMAT_L_8 = 9,  */
	{0, 0, 0, 8, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_A_8 = 10,  */
	{0, 0, 0, 0, 0, 0, 0, 8},	/* M200_TEXEL_FORMAT_I_8 = 11,  */
	{3, 3, 2, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_RGB_332 = 12,  */
	{2, 2, 2, 2, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ARGB_2222 = 13,  */
	{5, 6, 5, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_RGB_565 = 14,  */
	{5, 5, 5, 1, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ARGB_1555 = 15,  */
	{4, 4, 4, 4, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ARGB_4444 = 16,  */
	{0, 0, 0, 8, 0, 0, 8, 0},	/* M200_TEXEL_FORMAT_AL_88 = 17,  */
	{0, 0, 0, 0, 0, 0, 16, 0},	/* M200_TEXEL_FORMAT_L_16 = 18,  */
	{0, 0, 0, 16, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_A_16 = 19,  */
	{0, 0, 0, 0, 0, 0, 0, 16},	/* M200_TEXEL_FORMAT_I_16 = 20,  */
	{8, 8, 8, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_RGB_888 = 21,  */
	{8, 8, 8, 8, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ARGB_8888 = 22,  */
	{8, 8, 8, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_xRGB_8888 = 23,  */
	{10, 10, 10, 2, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ARGB_2_10_10_10 = 24,  */
	{11, 11, 10, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_RGB_11_11_10 = 25,  */
	{10, 12, 10, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_RGB_10_12_10 = 26,  */
	{0, 0, 0, 0, 16, 0, 16, 0},	/* M200_TEXEL_FORMAT_AL_16_16 = 27,  */
	{16, 16, 16, 16, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ARGB_16_16_16_16 = 28,  */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_PAL_4 = 29,  */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_PAL_8 = 30,  */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_FLXTC = 31,  */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ETC = 32,  */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 33 is Reserved */
	{0, 0, 0, 0, 0, 0, 16, 0},	/* M200_TEXEL_FORMAT_L_FP16 = 34,  */
	{0, 0, 0, 16, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_A_FP16 = 35,  */
	{0, 0, 0, 0, 0, 0, 0, 16},	/* M200_TEXEL_FORMAT_I_FP16 = 36,  */
	{0, 0, 0, 16, 0, 0, 16, 0},	/* M200_TEXEL_FORMAT_AL_FP16 = 37,  */
	{16, 16, 16, 16, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_ARGB_FP16 = 38,  */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 39 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 40 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 41 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 42 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 43 is Reserved */
	{0, 0, 0, 0, 24, 8, 0, 0},	/* M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8 = 44,  */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_CONVOLUTION_TEXTURE_64 = 45,  */
	{16, 16, 16, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_RGB_16_16_16 = 46,  */
	{16, 16, 16, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_RGB_FP16 = 47, */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 48 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 49 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_VERBATIM_COPY32 = 50,  */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 51 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 52 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 53 is YCCa */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 54 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 55 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 56 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 57 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 58 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 59 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 60 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 61 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* 62 is Reserved */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_NO_TEXTURE = 63  */
	/* Formats below are virtual and cant't be used by the HW*/
	{0, 0, 0, 0, 32, 0, 0, 0},	/* M200_TEXEL_FORMAT_VIRTUAL_DEPTH32            = 64,  */
	{0, 0, 0, 0, 16, 0, 0, 0},	/* M200_TEXEL_FORMAT_VIRTUAL_DEPTH16            = 65,  */
	{0, 0, 0, 0, 24, 8, 0, 0},	/* M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24 = 66,  */
	{8, 8, 8, 0, 0, 0, 0, 0},	/* M200_TEXEL_FORMAT_VIRTUAL_RGB888             = 67,  */
	{0, 0, 0, 0, 16, 0, 0, 0},	/* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_16_0 = 68,  */
	{0, 0, 0, 0, 24, 0, 0, 0},	/* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_24_0 = 69,  */
	{0, 0, 0, 0, 0, 4, 0, 0},	/* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_4  = 70,  */
	{0, 0, 0, 0, 0, 8, 0, 0},	/* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_8  = 71,  */
};

MALI_EXPORT s32 __m200_texel_format_get_bpp( m200_texel_format format )
{
	static const s32 texel_size[] = {
		1,    /* M200_TEXEL_FORMAT_L_1 */
		1,    /* M200_TEXEL_FORMAT_A_1 */
		1,    /* M200_TEXEL_FORMAT_I_1 */
		2,    /* M200_TEXEL_FORMAT_AL_11 */
		4,    /* M200_TEXEL_FORMAT_L_4 */
		4,    /* M200_TEXEL_FORMAT_A_4 */
		4,    /* M200_TEXEL_FORMAT_I_4 */
		4,    /* M200_TEXEL_FORMAT_ARGB_1111 */
		8,    /* M200_TEXEL_FORMAT_AL_44 */
		8,    /* M200_TEXEL_FORMAT_L_8 */
		8,    /* M200_TEXEL_FORMAT_A_8 */
		8,    /* M200_TEXEL_FORMAT_I_8 */
		8,    /* M200_TEXEL_FORMAT_RGB_332 */
		8,    /* M200_TEXEL_FORMAT_ARGB_2222 */
		16,   /* M200_TEXEL_FORMAT_RGB_565 */
		16,   /* M200_TEXEL_FORMAT_ARGB_1555 */
		16,   /* M200_TEXEL_FORMAT_ARGB_4444 */
		16,   /* M200_TEXEL_FORMAT_AL_88 */
		16,   /* M200_TEXEL_FORMAT_L_16 */
		16,   /* M200_TEXEL_FORMAT_A_16 */
		16,   /* M200_TEXEL_FORMAT_I_16 */
#if !RGB_IS_XRGB
		24,   /* M200_TEXEL_FORMAT_RGB_888 */
#else
		-1,   /* need an entry for "format 21", or we offset all subsequent formats wrongly */
#endif
		32,   /* M200_TEXEL_FORMAT_ARGB_8888 */
		32,   /* M200_TEXEL_FORMAT_xRGB_8888 */
		32,   /* M200_TEXEL_FORMAT_ARGB_2_10_10_10 */
		32,   /* M200_TEXEL_FORMAT_RGB_11_11_10 */
		32,   /* M200_TEXEL_FORMAT_RGB_10_12_10 */
		32,   /* M200_TEXEL_FORMAT_AL_16_16 */
		64,   /* M200_TEXEL_FORMAT_ARGB_16_16_16_16 */
		4,    /* M200_TEXEL_FORMAT_PAL_4 */
		8,    /* M200_TEXEL_FORMAT_PAL_8 */
		-1,   /* M200_TEXEL_FORMAT_FLXTC */
		4,    /* M200_TEXEL_FORMAT_ETC */
		-1,   /* Reserved */
		16,   /* M200_TEXEL_FORMAT_L_FP16 */
		16,   /* M200_TEXEL_FORMAT_A_FP16 */
		16,   /* M200_TEXEL_FORMAT_I_FP16 */
		32,   /* M200_TEXEL_FORMAT_AL_FP16 */
		16*4, /* M200_TEXEL_FORMAT_ARGB_FP16 */
		-1,   /* Reserved */
		-1,   /* Reserved */
		-1,   /* Reserved */
		-1,   /* Reserved */
		-1,   /* Reserved */
		32,   /* M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8 */
		64,   /* M200_TEXEL_FORMAT_CONVOLUTION_TEXTURE_64 */
		48,   /* M200_TEXEL_FORMAT_RGB_16_16_16 */
		48,   /* M200_TEXEL_FORMAT_RGB_FP16 */
		-1,   /* Reserved */
		-1,   /* Reserved */
		32,   /* M200_TEXEL_FORMAT_VERBATIM_COPY32 */
		-1, -1, /* 51-52 are Reserved */
		-1,   /* M200_TEXEL_FORMAT_YCCA */
		-1, -1, -1, -1, -1, -1, -1, -1, -1, /* 54-62 are Reserved */
		0,    /* M200_TEXEL_FORMAT_NO_TEXTURE */
		32,   /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH32 */
		16,   /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH16 */
		32,   /* M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24 */
		24,   /* M200_TEXEL_FORMAT_VIRTUAL_RGB888 */	
		16,   /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_16_0 */
		24,   /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_24_0 */
		4,    /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_4 */
		8,    /* M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_8 */
	};

	MALI_DEBUG_ASSERT( (sizeof(texel_size) / sizeof(texel_size[0])) > format,
		("invalid texel format 0x%x", format)
	);
	MALI_DEBUG_ASSERT( texel_size[ format ] >= 0,
		("unsupported texel format :(( 0x%x", format)
	);
	return texel_size[ format ];
}

MALI_EXPORT void __m200_texel_format_flag_support( m200_texel_format format, mali_bool *rb, mali_bool* ro )
{
	*rb = *ro = MALI_FALSE;
	switch(format)
	{
		case M200_TEXEL_FORMAT_L_1: return;
		case M200_TEXEL_FORMAT_A_1: return;
		case M200_TEXEL_FORMAT_I_1: return;
		case M200_TEXEL_FORMAT_AL_11: *ro = MALI_TRUE; return; /* AL formats supports revorder, turns it into LA */
		case M200_TEXEL_FORMAT_L_4: return;
		case M200_TEXEL_FORMAT_A_4: return;
		case M200_TEXEL_FORMAT_I_4: return;
		case M200_TEXEL_FORMAT_ARGB_1111: *rb = *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_AL_44: *ro = MALI_TRUE; return; /* AL formats supports revorder, turns it into LA */
		case M200_TEXEL_FORMAT_L_8: return;
		case M200_TEXEL_FORMAT_A_8: return;
		case M200_TEXEL_FORMAT_I_8: return;
		case M200_TEXEL_FORMAT_RGB_332: *rb = MALI_TRUE; return; /* ro undef@WBx, def@TD. Use rb instead - does the same */
		case M200_TEXEL_FORMAT_ARGB_2222: *rb = *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_RGB_565: *rb = MALI_TRUE; return; /* ro undef@WBx, def@TD. Use rb instead - does the same */
		case M200_TEXEL_FORMAT_ARGB_1555: *rb = *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_ARGB_4444: *rb = *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_AL_88: *ro = MALI_TRUE; return; /* AL formats supports revorder, turns it into LA */
		case M200_TEXEL_FORMAT_L_16: return;
		case M200_TEXEL_FORMAT_A_16: return;
		case M200_TEXEL_FORMAT_I_16: return;
#if !RGB_IS_XRGB
		case M200_TEXEL_FORMAT_RGB_888: *rb = MALI_TRUE; return; /* ro undef@WBx, def@TD. Use rb instead - does the same */
#endif
		case M200_TEXEL_FORMAT_ARGB_8888: *rb = *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_xRGB_8888: *rb =  MALI_TRUE; return; /* ro not supported due to HW cutting the feature */ 
		case M200_TEXEL_FORMAT_ARGB_2_10_10_10: *rb = *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_RGB_11_11_10: *rb = MALI_TRUE; return; /* ro undef@WBx, def@TD. Use rb instead - does the same */
		case M200_TEXEL_FORMAT_RGB_10_12_10: *rb = MALI_TRUE; return; /* ro undef@WBx, def@TD. Use rb instead - does the same */
		case M200_TEXEL_FORMAT_AL_16_16: *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_ARGB_16_16_16_16: *rb = *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_PAL_4: return; /* flags not defined for compressed formats */
		case M200_TEXEL_FORMAT_PAL_8: return; /* flags not defined for compressed formats */
		case M200_TEXEL_FORMAT_FLXTC: return; /* flags not defined for compressed formats */
		case M200_TEXEL_FORMAT_ETC: return; /* flags nor defined for compressed formats */
		case M200_TEXEL_FORMAT_L_FP16: return;
		case M200_TEXEL_FORMAT_A_FP16: return;
		case M200_TEXEL_FORMAT_I_FP16: return;
		case M200_TEXEL_FORMAT_AL_FP16: *ro = MALI_TRUE; return; /* AL formats supports revorder, turns it into LA */
		case M200_TEXEL_FORMAT_ARGB_FP16: *rb = *ro = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8: return; /* flags not defined for depth/stencil */
		case M200_TEXEL_FORMAT_CONVOLUTION_TEXTURE_64: return; /* flags not supported */
		case M200_TEXEL_FORMAT_RGB_16_16_16: *rb = MALI_TRUE; return; /* ro undef@WBx, def@TD. Use rb instead - does the same */
		case M200_TEXEL_FORMAT_RGB_FP16: *rb = MALI_TRUE; return; /* ro undef@WBx, def@TD. Use rb instead - does the same */
		case M200_TEXEL_FORMAT_VERBATIM_COPY32: return;
		case M200_TEXEL_FORMAT_NO_TEXTURE: return;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH32: return;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH16: return;
		case M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24: return;
		case M200_TEXEL_FORMAT_VIRTUAL_RGB888: *rb = MALI_TRUE; return;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_16_0: return;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_24_0: return;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_4: return;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_8: return;
		default: MALI_DEBUG_ASSERT(0, ("Unrecognized texel format: %i", format)); return;
	}
}

MALI_EXPORT mali_bool __m200_texel_format_has_alpha( m200_texel_format format )
{
	switch ( format )
	{
		case M200_TEXEL_FORMAT_L_1: return MALI_FALSE;
		case M200_TEXEL_FORMAT_A_1: return MALI_TRUE;
		case M200_TEXEL_FORMAT_I_1: return MALI_FALSE;
		case M200_TEXEL_FORMAT_AL_11: return MALI_TRUE;
		case M200_TEXEL_FORMAT_L_4: return MALI_FALSE;
		case M200_TEXEL_FORMAT_A_4: return MALI_TRUE;
		case M200_TEXEL_FORMAT_I_4: return MALI_FALSE;
		case M200_TEXEL_FORMAT_ARGB_1111: return MALI_TRUE;
		case M200_TEXEL_FORMAT_AL_44: return MALI_TRUE;
		case M200_TEXEL_FORMAT_L_8: return MALI_FALSE;
		case M200_TEXEL_FORMAT_A_8: return MALI_TRUE;
		case M200_TEXEL_FORMAT_I_8: return MALI_FALSE;
		case M200_TEXEL_FORMAT_RGB_332: return MALI_FALSE;
		case M200_TEXEL_FORMAT_ARGB_2222: return MALI_TRUE;
		case M200_TEXEL_FORMAT_RGB_565: return MALI_FALSE;
		case M200_TEXEL_FORMAT_ARGB_1555: return MALI_TRUE;
		case M200_TEXEL_FORMAT_ARGB_4444: return MALI_TRUE;
		case M200_TEXEL_FORMAT_AL_88: return MALI_TRUE;
		case M200_TEXEL_FORMAT_L_16: return MALI_FALSE;
		case M200_TEXEL_FORMAT_A_16: return MALI_TRUE;
		case M200_TEXEL_FORMAT_I_16: return MALI_FALSE;
#if !RGB_IS_XRGB
		case M200_TEXEL_FORMAT_RGB_888: return MALI_FALSE;
#endif
		case M200_TEXEL_FORMAT_ARGB_8888: return MALI_TRUE;
		case M200_TEXEL_FORMAT_xRGB_8888: return MALI_FALSE;
		case M200_TEXEL_FORMAT_ARGB_2_10_10_10: return MALI_TRUE;
		case M200_TEXEL_FORMAT_RGB_11_11_10: return MALI_FALSE;
		case M200_TEXEL_FORMAT_RGB_10_12_10: return MALI_FALSE;
		case M200_TEXEL_FORMAT_AL_16_16: return MALI_TRUE;
		case M200_TEXEL_FORMAT_ARGB_16_16_16_16: return MALI_TRUE;
		case M200_TEXEL_FORMAT_PAL_4: return MALI_FALSE;
		case M200_TEXEL_FORMAT_PAL_8: return MALI_FALSE;
		case M200_TEXEL_FORMAT_FLXTC: return MALI_FALSE;
		case M200_TEXEL_FORMAT_ETC: return MALI_FALSE;
		case M200_TEXEL_FORMAT_L_FP16: return MALI_FALSE;
		case M200_TEXEL_FORMAT_A_FP16: return MALI_TRUE;
		case M200_TEXEL_FORMAT_I_FP16: return MALI_FALSE;
		case M200_TEXEL_FORMAT_AL_FP16: return MALI_TRUE;
		case M200_TEXEL_FORMAT_ARGB_FP16: return MALI_TRUE;
		case M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8: return MALI_FALSE;
		case M200_TEXEL_FORMAT_CONVOLUTION_TEXTURE_64: return MALI_FALSE;
		case M200_TEXEL_FORMAT_RGB_16_16_16: return MALI_FALSE;
		case M200_TEXEL_FORMAT_RGB_FP16: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VERBATIM_COPY32: return MALI_FALSE;
		case M200_TEXEL_FORMAT_NO_TEXTURE: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH32: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH16: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VIRTUAL_RGB888: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_16_0: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_24_0: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_4: return MALI_FALSE;
		case M200_TEXEL_FORMAT_VIRTUAL_DEPTH_STENCIL_0_8: return MALI_FALSE;
		default: MALI_DEBUG_ASSERT(0, ("Unrecognized texel format: %i", format)); return MALI_FALSE;
	}

}

MALI_EXPORT mali_bool __m200_texel_format_is_alpha( m200_texel_format format )
{
	return ( M200_TEXEL_FORMAT_A_1 == format 
		|| M200_TEXEL_FORMAT_A_4 == format 
		|| M200_TEXEL_FORMAT_A_8 == format 
		|| M200_TEXEL_FORMAT_A_16 == format 
		|| M200_TEXEL_FORMAT_A_FP16 == format ) ? MALI_TRUE : MALI_FALSE;
}

MALI_EXPORT mali_bool __m200_texel_format_has_luminance( m200_texel_format format )
{
	return ( M200_TEXEL_FORMAT_L_1 == format
		|| M200_TEXEL_FORMAT_L_4 == format
		|| M200_TEXEL_FORMAT_L_8 == format
		|| M200_TEXEL_FORMAT_L_16 == format
		|| M200_TEXEL_FORMAT_L_FP16 == format
		|| M200_TEXEL_FORMAT_AL_11 == format
		|| M200_TEXEL_FORMAT_AL_44 == format
		|| M200_TEXEL_FORMAT_AL_88 == format
		|| M200_TEXEL_FORMAT_AL_16_16 == format
		|| M200_TEXEL_FORMAT_AL_FP16 == format ) ? MALI_TRUE : MALI_FALSE;
}

MALI_EXPORT mali_bool __m200_texel_format_is_intensity( m200_texel_format format )
{
	return ( M200_TEXEL_FORMAT_I_1 == format 
		|| M200_TEXEL_FORMAT_I_4 == format 
		|| M200_TEXEL_FORMAT_I_8 == format 
		|| M200_TEXEL_FORMAT_I_16 == format 
		|| M200_TEXEL_FORMAT_I_FP16 == format ) ? MALI_TRUE : MALI_FALSE;
}

MALI_EXPORT void __m200_texel_format_get_bpc( m200_texel_format format, u32* r, u32* g, u32* b, u32* a, u32* d, u32* s, u32* l, u32* i )
{
	__m200_texel_format_bpc bpc = __m200_texel_format_bpc_table[format];
	*r = bpc.r;
	*g = bpc.g;
	*b = bpc.b;
	*a = bpc.a;
	*d = bpc.d;
	*s = bpc.s;
	*l = bpc.l;
	*i = bpc.i;
}

MALI_EXPORT u8 __m200_texel_format_get_abits( m200_texel_format format )
{
	return __m200_texel_format_bpc_table[format].a;
}
