/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>

#include <shared/m200_texel_format.h>
#include <shared/m200_td.h>


#ifndef NDEBUG   /* #if debug */
#define M200_SANITY_CHECK( expr, X )				\
	do {											\
		if ( (expr) ) {								\
			MALI_DEBUG_ERROR_START_MSG;				\
			MALI_PRINTF ("SANITY CHECK FAILED: ");	\
			MALI_DEBUG_TRACE();						\
			MALI_PRINTF X;							\
			MALI_DEBUG_ERROR_STOP_MSG;				\
			MALI_ASSERT_QUIT_CMD;					\
		}											\
	} while ( 0 )

#endif /* if debug */

MALI_EXPORT u32 _m200_td_get(m200_td dest, u32 left_index, u32 right_index )
{
	u32 right_word = right_index >> 5;
	u32 left_word = left_index >> 5;
	u32 right_index_small = right_index - ( right_word << 5 );


	if ( right_word == left_word )
	{
		u32 field = ( ( 1 << ( left_index - right_index + 1 ) ) - 1 ) << ( right_index_small);

		return ( dest[ right_word ] & field ) >> right_index_small ;
	}
	else
	{
		/* Need to get the value from both words and put together */
		u32 left_index_small = left_index - ( left_word << 5 );
		u32 left_field = ( 1 << ( left_index_small + 1 ) ) - 1;
		u32 right_field = ~( ( 1 << right_index_small ) - 1 );
		u32 left_value = dest[ left_word ] & left_field;
		u32 right_value = ( dest[ right_word ] & right_field ) >> right_index_small;

		return left_value << ( 32 - right_index_small ) | right_value;
	}
}

MALI_EXPORT void m200_texture_descriptor_set_defaults( m200_td dest )
{
	_mali_sys_memset( dest, 0x0, sizeof( m200_td ) );

	/* setup sane defaults for the texture descirptor */
	MALI_TD_SET_TEXEL_FORMAT( dest, M200_TEXEL_FORMAT_xRGB_8888 );
	MALI_TD_SET_TEXEL_BIAS_SELECT( dest, M200_TEXEL_BIAS_NONE );
	MALI_TD_SET_TEXEL_RED_BLUE_SWAP( dest, MALI_FALSE );
	MALI_TD_SET_TEXEL_ORDER_INVERT( dest, MALI_FALSE );
	MALI_TD_SET_PALETTE_FORMAT( dest, M200_PALETTE_FORMAT_RGB_565 );
	MALI_TD_SET_TEXTURE_FORMAT( dest, M200_TEXTURE_FORMAT_NORMALIZED );
	MALI_TD_SET_TEXTURE_DIMENSIONALITY( dest, M200_TEXTURE_DIMENSIONALITY_2D );
	MALI_TD_SET_MIPMAPPING_MODE( dest, M200_MIPMAP_MODE_NEAREST );
	MALI_TD_SET_WRAP_MODE_S( dest, M200_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE );
	MALI_TD_SET_WRAP_MODE_T( dest, M200_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE );
	MALI_TD_SET_WRAP_MODE_R( dest, M200_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE );
	MALI_TD_SET_TEXTURE_DIMENSION_S( dest, 1 );
	MALI_TD_SET_TEXTURE_DIMENSION_T( dest, 1 );
	MALI_TD_SET_TEXTURE_DIMENSION_R( dest, 1 );
	MALI_TD_SET_TEXTURE_ADDRESSING_MODE( dest, M200_TEXTURE_ADDRESSING_MODE_LINEAR );
	MALI_TD_SET_SHADOW_MAPPING_CMP_FUNC( dest, M200_SHADOW_MAPPING_COMPARE_FUNC_ALWAYS_FAIL );

}
