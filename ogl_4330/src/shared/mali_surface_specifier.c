/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/mali_surface_specifier.h>
#include <regs/MALI200/mali200_core.h>

#define MALI_ALIGN_ANYTHING(x, a) ((((x) + ((a)-1)) / a) * a)

MALI_EXPORT u32 _mali_surface_specifier_bpp(const mali_surface_specifier* format)
{
	/* if we have a clearly mapped pixel format, get the number of bpp here */
	if(format->pixel_format != MALI_PIXEL_FORMAT_NONE)
	{
		u32 pixel_bpp = __mali_pixel_format_get_bpp(format->pixel_format);
		/* pixel formats are given in bits, but always follow whole byte boundaries on all supported formats */
		MALI_DEBUG_ASSERT((pixel_bpp % 8)==0, ("invalid bpp"));
		return pixel_bpp;
	}

	/* otherwise, fallback to the number of bpp on the texel format */
	if(format->texel_format != M200_TEXEL_FORMAT_NO_TEXTURE)
	{
		u32 pixel_bpp = __m200_texel_format_get_bpp(format->texel_format);
		MALI_DEBUG_ASSERT(pixel_bpp !=0, ("invalid bpp"));
		return pixel_bpp;
	}
	MALI_DEBUG_ASSERT(0, ("Not enough information on bits per pixel"));
	return 0;
}

/* calculates the greates commond divisor of two numbers */
MALI_STATIC unsigned int gcd(unsigned int a, unsigned int b)
{
	if( b == 0 ) return a;
	return gcd(b, a%b);
}

MALI_STATIC unsigned int lcm(unsigned int a, unsigned int b)
{
	return a*b/gcd(b, a%b);
}

MALI_EXPORT u32 _mali_surface_specifier_calculate_minimum_pitch(const mali_surface_specifier* format)
{
	u32 bpp;
	u32 return_pitch;
	int minimum_pitch_alignment = 1; /* this is the minimum alignment for all pitches, as per (1) below */
	
	MALI_DEBUG_ASSERT_POINTER(format);
	MALI_DEBUG_ASSERT( MALI_PIXEL_LAYOUT_LINEAR == format->pixel_layout, ("only makes sense with linear surface layout"));
	
	return_pitch = 0;
	bpp = _mali_surface_specifier_bpp( format );

	/* (1) Anywhere the pitch is used the pitch must be aligned to at least a full byte. 
	 *     The pitch is only used for linear surfaces, not for blockinterleaved surfaces.
	 *     Calculating the pitch is useful only for linear surfaces as a consequence. 
	 *     The pitch is used in the WBx and the M400 TD , but not directly in the M200 TD.
	 *     Due to a desire to express the pitch in full bytes, this requirement is always 
	 *     enforced anwyay. 
	 *     
	 * (2) The WBx requires the pitch to be divisible by 8, so any writeable surfaces must 
	 *     adhere to this requirement. 
	 *
	 * (3) On M200, pitch/bytes_per_pixel is used in place of the width to specify surface 
	 *     dimensions. This means the pitch must be aligned to a full pixel on M200. 
	 *
	 * The first alignment requirement plays nice with everyone, but the second and third may be at odds. 
	 * A hypotetical case of a 24bit writeable texture (hypotetical, not supported in HW) require a pitch divisible by 3 AND 8. 
	 * The least common multiple in this case is 24. 
	 *
	 * We want a minimum sized pitch fitting all usage requirements. 
	 */
	
	/* writeable surfaces require a pitch divisible by 8, as per (2) */
	if(format->pixel_format != MALI_PIXEL_FORMAT_NONE)
	{
		minimum_pitch_alignment = lcm(minimum_pitch_alignment, MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT);
	}
		
#ifdef USING_MALI200
	/* surfaces in M200 must have a pitch aligned to a full pixel, as per (3) */
	minimum_pitch_alignment = lcm(minimum_pitch_alignment , (bpp+7)/8);
#endif

	/* Calculate the pitch padded up to full bytes - and apply the pitch requirement */
	return_pitch = ( ( format->width * bpp ) + 7 ) / 8;
	return_pitch = MALI_ALIGN_ANYTHING( return_pitch, minimum_pitch_alignment );

	return return_pitch;
}


MALI_EXPORT u32 _mali_surface_specifier_datasize(const mali_surface_specifier* format)
{
	u32 datasize;
	u32 local_pitch;
	u32 bpp;

	MALI_DEBUG_ASSERT_POINTER(format);

	switch ( format->pixel_layout )
	{
		case MALI_PIXEL_LAYOUT_LINEAR:
			if(format->pitch == 0)
			{
				local_pitch = _mali_surface_specifier_calculate_minimum_pitch(format);
			} else {
				local_pitch = format->pitch;
			}
			
#if HARDWARE_ISSUE_7878
			if ( MALI_PIXEL_FORMAT_NONE != format->pixel_format )
			{
				/* writable surfaces need height tilealigned in HW rev <= R0P4 */
				datasize = local_pitch * MALI_ALIGN( format->height, MALI200_TILE_SIZE ); 
			}
			else
#endif /* HARDWARE_ISSUE_7878 */
			{
				/* GPU doesn't write to this surface or HW rev > R0P4 */
				datasize = local_pitch * format->height;
			}
			break;

		case MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS:
			/* multiply by dimensions, round up to the nearest byte, return bytes */
			bpp = _mali_surface_specifier_bpp( format );
			datasize = ( ( bpp * MALI_ALIGN( format->width, MALI200_TILE_SIZE ) *
			               MALI_ALIGN( format->height, MALI200_TILE_SIZE ) ) + 7 ) / 8;
			break;

		default:
			MALI_DEBUG_ASSERT( 0, ( "Unsupported pixel layout %d", format->pixel_layout ) );
			return 0;
	}
	return datasize;
}



