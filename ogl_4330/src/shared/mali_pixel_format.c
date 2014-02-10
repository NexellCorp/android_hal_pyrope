/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifdef __SYMBIAN32__
#ifdef USING_MALI200
#include <shared/m200_td.h>	/* Needed for mali_pixel enum */
#endif
#endif

#include <shared/mali_pixel_format.h>
#include <base/mali_debug.h>

MALI_EXPORT s32 __mali_pixel_format_get_bpp(enum mali_pixel_format pixel_format)
{
	switch (pixel_format)
	{
		case MALI_PIXEL_FORMAT_R5G6B5:   return 16;
		case MALI_PIXEL_FORMAT_A1R5G5B5: return 16;
		case MALI_PIXEL_FORMAT_A4R4G4B4: return 16;
		case MALI_PIXEL_FORMAT_A8R8G8B8: return 32;
		case MALI_PIXEL_FORMAT_B8:       return 8;
		case MALI_PIXEL_FORMAT_G8B8:     return 16;
		case MALI_PIXEL_FORMAT_ARGB_FP16:return 64;
		case MALI_PIXEL_FORMAT_B_FP16:   return 16;
		case MALI_PIXEL_FORMAT_GB_FP16:  return 32;
		case MALI_PIXEL_FORMAT_S8:       return 8;
		case MALI_PIXEL_FORMAT_Z16:      return 16;
		case MALI_PIXEL_FORMAT_S8Z24:    return 32;
		case MALI_PIXEL_FORMAT_NONE:     return 0;
		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ("missing pixel format: %d", pixel_format));
			return 0;
	}
}

MALI_EXPORT void _mali_pixel_format_get_bpc( enum mali_pixel_format pixel_format, u32 *r, u32 *g, u32 *b, u32 *a, u32 *d, u32 *s )
{
	u32 r_var, g_var, b_var, a_var, d_var, s_var;

	r_var = g_var = b_var = a_var = d_var = s_var = 0;

	switch( pixel_format )
	{
		case MALI_PIXEL_FORMAT_R5G6B5:    r_var = 5; g_var = 6; b_var = 5; break;
		case MALI_PIXEL_FORMAT_A1R5G5B5:  r_var = 5;  g_var = 5;  b_var = 5;  a_var = 1;  break;
		case MALI_PIXEL_FORMAT_A4R4G4B4:  r_var = 4;  g_var = 4;  b_var = 4;  a_var = 4;  break;
		case MALI_PIXEL_FORMAT_A8R8G8B8:  r_var = 8;  g_var = 8;  b_var = 8;  a_var = 8;  break;
		case MALI_PIXEL_FORMAT_B8:        b_var = 8;  break;
		case MALI_PIXEL_FORMAT_G8B8:      g_var = 8;  b_var = 8;  break;
		case MALI_PIXEL_FORMAT_ARGB_FP16: r_var = 16; g_var = 16; b_var = 16; a_var = 16; break;
		case MALI_PIXEL_FORMAT_B_FP16:    b_var = 16; break;
		case MALI_PIXEL_FORMAT_GB_FP16:   g_var = 16; b_var = 16; break;
		case MALI_PIXEL_FORMAT_S8:        s_var = 8;  break;
		case MALI_PIXEL_FORMAT_Z16:       d_var = 16; break;
		case MALI_PIXEL_FORMAT_S8Z24:     s_var = 8;  d_var = 24; break;
		case MALI_PIXEL_FORMAT_NONE:      break;
		default: 
			MALI_DEBUG_ASSERT( MALI_FALSE, ("missing pixel format: %d", pixel_format));
	}

	if( r != NULL ) *r = r_var;
	if( g != NULL ) *g = g_var;
	if( b != NULL ) *b = b_var;
	if( a != NULL ) *a = a_var;
	if( d != NULL ) *d = d_var;
	if( s != NULL ) *s = s_var;
}

MALI_EXPORT enum m200_texel_format _mali_pixel_to_texel_format (enum mali_pixel_format pixel_format)
{
	switch (pixel_format)
	{
		case MALI_PIXEL_FORMAT_R5G6B5:    return M200_TEXEL_FORMAT_RGB_565;
		case MALI_PIXEL_FORMAT_A1R5G5B5:  return M200_TEXEL_FORMAT_ARGB_1555;
		case MALI_PIXEL_FORMAT_A4R4G4B4:  return M200_TEXEL_FORMAT_ARGB_4444;
		case MALI_PIXEL_FORMAT_A8R8G8B8:  return M200_TEXEL_FORMAT_ARGB_8888;
		case MALI_PIXEL_FORMAT_S8Z24:     return M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8;
		case MALI_PIXEL_FORMAT_ARGB_FP16: return M200_TEXEL_FORMAT_ARGB_FP16;
		default:
			return M200_TEXEL_FORMAT_NO_TEXTURE;
	}
}

MALI_EXPORT enum mali_pixel_format _mali_texel_to_pixel_format ( enum m200_texel_format texel_format)
{
	switch (texel_format)
	{
		case M200_TEXEL_FORMAT_RGB_565:               return MALI_PIXEL_FORMAT_R5G6B5;
		case M200_TEXEL_FORMAT_ARGB_1555:             return MALI_PIXEL_FORMAT_A1R5G5B5;
		case M200_TEXEL_FORMAT_ARGB_4444:             return MALI_PIXEL_FORMAT_A4R4G4B4;
		case M200_TEXEL_FORMAT_ARGB_8888:             return MALI_PIXEL_FORMAT_A8R8G8B8;
		case M200_TEXEL_FORMAT_xRGB_8888:             return MALI_PIXEL_FORMAT_A8R8G8B8;
		case M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8:    return MALI_PIXEL_FORMAT_S8Z24;
		case M200_TEXEL_FORMAT_L_8:                   return MALI_PIXEL_FORMAT_B8;
		case M200_TEXEL_FORMAT_I_8:                   return MALI_PIXEL_FORMAT_B8;
		case M200_TEXEL_FORMAT_A_8:                   return MALI_PIXEL_FORMAT_B8;
#if 1 /* added by nexell */		
		case M200_TEXEL_FORMAT_AL_88:                 return MALI_PIXEL_FORMAT_G8B8;
#endif
		case M200_TEXEL_FORMAT_ARGB_FP16:             return MALI_PIXEL_FORMAT_ARGB_FP16;
		default:
			return MALI_PIXEL_FORMAT_NONE;
	}
}

MALI_EXPORT enum m200_texture_addressing_mode _mali_pixel_layout_to_texel_layout( enum mali_pixel_layout pixel_layout )
{
	switch( pixel_layout )
	{
		case MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS:                                 return M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED;
		case MALI_PIXEL_LAYOUT_LINEAR:                                             return M200_TEXTURE_ADDRESSING_MODE_LINEAR;
		case MALI_PIXEL_LAYOUT_INVALID:                                            return M200_TEXTURE_ADDRESSING_MODE_INVALID;
		default: 
			MALI_DEBUG_ASSERT( 0, ("invalid pixel layout: %d\n", pixel_layout )); 
			return M200_TEXTURE_ADDRESSING_MODE_INVALID;
	}
}

MALI_EXPORT enum mali_pixel_layout _mali_texel_layout_to_pixel_layout( enum m200_texture_addressing_mode texel_layout )
{
	switch( texel_layout  )
	{
		case M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED:   return MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS;
		case M200_TEXTURE_ADDRESSING_MODE_LINEAR:          return MALI_PIXEL_LAYOUT_LINEAR;
		case M200_TEXTURE_ADDRESSING_MODE_INVALID:         return MALI_PIXEL_LAYOUT_INVALID;
		default: 
			MALI_DEBUG_ASSERT( 0, ("invalid texel layout: %d\n", texel_layout  )); 
			return MALI_PIXEL_LAYOUT_INVALID;
	}
}

#if MALI_INSTRUMENTED || defined(DEBUG) || defined (__SYMBIAN32__)
MALI_EXPORT const char* _mali_pixel_format_string(enum mali_pixel_format pixel_format)
{
	switch (pixel_format)
	{
	case MALI_PIXEL_FORMAT_NONE:      return "NONE";
	case MALI_PIXEL_FORMAT_R5G6B5:    return "R5G6B5";
	case MALI_PIXEL_FORMAT_A1R5G5B5:  return "A1R5G5B5";
	case MALI_PIXEL_FORMAT_A4R4G4B4:  return "A4R4G4B4";
	case MALI_PIXEL_FORMAT_A8R8G8B8:  return "A8R8G8B8";
	case MALI_PIXEL_FORMAT_B8:        return "B8";
	case MALI_PIXEL_FORMAT_G8B8:      return "G8B8";
	case MALI_PIXEL_FORMAT_ARGB_FP16: return "ARGB_FP16";
	case MALI_PIXEL_FORMAT_B_FP16:    return "B_FP16";
	case MALI_PIXEL_FORMAT_GB_FP16:   return "GB_FP16";
	case MALI_PIXEL_FORMAT_S8:        return "S8";
	case MALI_PIXEL_FORMAT_S8Z24:     return "S8Z24";
	case MALI_PIXEL_FORMAT_Z16:       return "Z16";
	default:
		return " -- unknown -- ";
	}
}
#endif /* MALI_INSTRUMENTED */

