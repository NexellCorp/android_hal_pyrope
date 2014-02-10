/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_convert.c
 * @brief Utility functionality for conversion that need not be inlined.
 */

#include "gles_convert.h"

MALI_CHECK_RESULT GLenum _gles_convert_mali_err_do( mali_err_code err )
{
	switch( err )
	{
	case MALI_ERR_NO_ERROR:
	case MALI_ERR_EARLY_OUT:	return GL_NO_ERROR;
#if MALI_TEST_API
	case MALI_ERR_CHECK_ERROR:
#endif
	case MALI_ERR_OUT_OF_MEMORY:
	case MALI_ERR_FUNCTION_FAILED:	return GL_OUT_OF_MEMORY;

	default:
		MALI_DEBUG_ASSERT( MALI_FALSE, ( "Unhandled enum in convert_mali_err: %08x\n", err ) );
		return GL_NO_ERROR;
	};
}

#define IEEE754_MANTISSA_SIZE 23
#define IEEE754_EXPONENT_OFFSET 127
#define CLZ_FOR_ONE_IN_FIXED16_16 15
#define IEEE754_NEGATIVE_BIT 0x80000000
float fixed_to_float(s32 x)
{
	u32 zeros;
	u32 nbits;
	u32 sign = 0;
	u32 exponent;

	union
	{
		float f;
		u32 bitpattern;
	} result;

	/* Simple check for zero */
	if (0 == x)
	{
		return 0.0f;
	}

	/* Make positive, and store the sign */
	if (x < 0)
	{
		x = -x;
		sign = IEEE754_NEGATIVE_BIT;
	}

	/* Find how many zeros there are, before we take off the implicit '1'*/
	zeros = _mali_clz(x);

	/* We use a logical shift to create a bitpattern that has a single one in the position of our first '1' in x
	 * Bitwise negation of this results in a bitpattern that chops off the leading '1' in x*/
	x = x & ~(((u32) IEEE754_NEGATIVE_BIT) >> zeros);

	/* Convert this to how many bits are required for our number, less the 1 bit for the implicit '1'
	 * Note: using u32 for nbits, hence we rely on x != 0 at this point. */
	nbits = (32 - zeros) - 1;
#if 0
	/* Slower, but you can see what happens */
	if (nbits <= IEEE754_MANTISSA_SIZE )
	{
		/* Shift up to get our mantissa */
		u32 shift = IEEE754_MANTISSA_SIZE - nbits;
		/* x is positive here, so our upper portion of result will be zero */
		result.bitpattern = x << shift;
	}
	else
	{
		/* Shift down, losing precision on our mantissa */
		u32 shift = nbits - IEEE754_MANTISSA_SIZE;

		/* x is positive here, so our upper portion of result will be zero */
		result.bitpattern = x >> shift;
	}
#else
	/* Faster, but slightly more obscure */
	{
		s32	shift = IEEE754_MANTISSA_SIZE - nbits;

		if (shift >= 0 )
		{
			/* Shift up to get our mantissa */

			/* x is positive here, so our upper portion of result will be zero */
			result.bitpattern = x << shift;
		}
		else
		{
			/* Shift down, losing precision on our mantissa */

			/* x is positive here, so our upper portion of result will be zero */
			result.bitpattern = x >> (-shift);
		}
	}
#endif

	/* at this point, result contains correct mantissa */

	/* calculate exponent:
	 *   as 'zeros' increases, our exponent should get smaller, proportionally
	 *   when 'zeros'==(zeros for 0x10000), exponent should be IEEE754_EXPONENT_OFFSET */
	exponent = IEEE754_EXPONENT_OFFSET + CLZ_FOR_ONE_IN_FIXED16_16 - zeros;
	result.bitpattern |= exponent << IEEE754_MANTISSA_SIZE;

	/* now we have exponent and mantissa, just add the sign: */
	result.bitpattern |= sign;

	/* It is not possible for this function to fail */
	return result.f;
}

GLftype _gles_convert_element_to_ftype( const GLvoid *a, const s32 i, const gles_datatype type )
{
	if (NULL == a) return 0;

	switch( type )
	{
	case GLES_FLOAT:          return FLOAT_TO_FTYPE(((GLfloat*)(a))[(i)]);
#if MALI_USE_GLES_1
	case GLES_FIXED:          return FIXED_TO_FTYPE(((GLfixed*)(a))[(i)]);
#endif

	case GLES_INT:            return INT_TO_FTYPE(((GLint*)(a))[(i)]);

	case GLES_NORMALIZED_INT: return NORMALIZED_INT_TO_FTYPE(((GLint*)(a))[(i)]);

	default:
		MALI_DEBUG_ASSERT( 0, ("illegal gles datatype\n") );
		/* dummy return to avoid warnings from armcc */
		return 0;
	}
}
