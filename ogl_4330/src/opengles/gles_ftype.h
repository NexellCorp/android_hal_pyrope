/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_ftype.h
 * @brief Defines the float/fixed-wrapper type, GLftype.
 */

#ifndef _GLES_FTYPE_H_
#define _GLES_FTYPE_H_

#include <mali_system.h>
#include "gles_base.h"

#if GLES_FTYPE==GLES_FLOAT
#include "gles_float_type.h"
#elif GLES_FTYPE==GLES_FIXED
#include "gles_fixed_type.h"
#else
#error "Unimplemented FTYPE-value"
#endif

#include <util/mali_fixed.h>

/**
 * @brief convert from fixed to float
 * @param x The fixed value to be converted
 * @return The float value calculated
 */

/** convert from normalized unsigned byte to fixed */
#define UBYTE_TO_FIXED( a ) ( (GLfixed) a * ( ( 1 << 16 ) / MALI_U8_MAX ) )

/** convert from byte to float */
#define BYTE_TO_FIXED( a ) ( GLfixed )( a  * ( ( 1 << 16 ) / MALI_S8_MAX ) )

/** convert from normalized unsigned byte to fixed */
#define UBYTE_TO_FIXED( a ) ( (GLfixed) a * ( ( 1 << 16 ) / MALI_U8_MAX ) )

/** convert from byte to fixed */
#define BYTE_TO_FIXED( a ) ( GLfixed )( a  * ( ( 1 << 16 ) / MALI_S8_MAX ) )

/**
 * @brief convert from float to fixed
 * @param f The float to be converted
 * @return The fixed value calculated
 */
MALI_STATIC_INLINE s32 float_to_fixed(float f)
{
	return (s32)(f * (1<<16));
}

/**
 * @brief convert from fixed to float
 * @param x The fixed point value to be converted
 * @return The float value calculated
 */
float fixed_to_float(s32 x);

/**
 * @brief convert from normalized int to float
 * @param x The int value to be converted
 * @return The normalized float calculated
 */
MALI_STATIC_INLINE float normalized_int_to_float(s32 x)
{
	return (float)x * (1.f / MALI_S32_MAX);
}

/**
 * @brief convert from float to normalized int
 * @param f The float to be converted
 * @return The int calculated
 */
MALI_STATIC_INLINE s32 float_to_normalized_int( float f )
{
 	double val = f;
	if(val > 1.0) val = 1.0;
	if(val < -1.0) val = -1.0;

	return ((s32)(val * (MALI_S32_MAX + 0.5) - 0.5));
}

/**
 * @brief convert from normalized int to fixed
 * @param i The int to be converted
 * @return The fixed value calculated
 */
MALI_STATIC_INLINE s32 normalized_int_to_fixed(s32 i)
{
	/*
	 * Small optimisation: the calculation is reduced to a simple divide.
	 * This is because (( 1 << 16 ) / MALI_S32_MAX) is very close to (1 << 16) / (1 << 32),
	 * that is, to within 1 part in 4*10^9.
	 * The common (1 << 16) factor can then be divided out.
	 * The compiler further optimises the divide to a shift.
	 */
#if MALI_S32_MAX == 2147483647
	return (i / ( 1 << 16 ) );
#else
	/* Otherwise, expand to s64, and do an accurate calculation */
	return (s32)( ((s64)i * (s64)( 1 << 16 )) / (s64)MALI_S32_MAX );
#endif
}

/**
 * @brief convert from fixed to normalized int
 * @param x The fixed value to be converted
 * @return The normalized int calculated
*/
MALI_STATIC_INLINE s32 fixed_to_normalized_int( s32 x )
{
	return (s32) (((s64) x * MALI_S32_MAX ) >> 16);
}

#endif /* _GLES_FTYPE_H_ */
