/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_util.h
 * @brief Utility functionality like float-to-fixed conversions etc
 */

#ifndef _GLES_UTIL_H_
#define _GLES_UTIL_H_

#include <mali_system.h>
#include <shared/mali_convert.h>
#include <base/mali_debug.h>
#include <util/mali_fixed.h>
#include "gles_ftype.h"
#include "gles_base.h"

/** make a version string of a minor and major version constant set */
/* we need two levels of macros in order to properly expand the name */
#define GLES_MAKE_VERSION_STRING_INTERNAL(major, minor) #major "." #minor
#define GLES_MAKE_VERSION_STRING(major, minor) GLES_MAKE_VERSION_STRING_INTERNAL(major, minor)

#ifndef CLAMP
/** clamp input to [MIN, MAX]
 * NOTE: THIS EVALUTES X UP TO 3 TIMES, MIN UP TO 2 TIMES, MAX UP TO 2 TIMES */
#define CLAMP( X, MIN, MAX )  ( (X)<(MIN) ? (MIN) : ((X)>(MAX) ? (MAX) : (X)) )
#endif

#ifndef MAX
/** clamp input to [-inf, MAX]
 * NOTE: THIS EVALUTES BOTH ARGUMENTS EXACTLY TWICE */
#define MAX( X, Y ) ( (X) > (Y) ? (X) : (Y) )
#endif

#ifndef MIN
/** clamp input to [MIN, inf]
 * NOTE: THIS EVALUTES BOTH ARGUMENTS EXACTLY TWICE */
#define MIN( X, Y ) ( (X) > (Y) ? (Y) : (X) )
#endif

/** print out a warning at run-time, telling us that the implementation of the function is incomplete */
#define INCOMPLETE /*_mali_sys_printf("\n%s: %s() is not fully implemented. (line %d)\n", __FILE__, MALI_FUNCTION, __LINE__);*/
/** Print out a warning at run-time, telling us that this has not been thoroughly tested yet */
#define UNTESTED /*_mali_sys_printf("\n%s: %s() is not fully TESTED. (line %d)\n", __FILE__, MALI_FUNCTION, __LINE__);*/

#define FP16_INF (((1 << 5) - 1) << 10) /* all exponent bits set, zero mantissa */
#define FP16_NAN 0xFFFF                 /* all bits set */


/** The different internal datatypes we support/use */
typedef enum gles_datatype
{
	GLES_FLOAT          = 0,
	GLES_FIXED          = 1,
	GLES_NORMALIZED_INT = 2,
	GLES_INT            = 3,
	GLES_BOOLEAN        = 4
} gles_datatype;


/* datatypes for 16bit(1:5:10) and 32bit IEEE floating point */
typedef float gles_fp32;
typedef u16   gles_fp16;

/**
 * @brief Gets the sign of a 32bit IEEE float
 * @param f The float
 * @return MALI_TRUE if negative, otherwise MALI_FALSE
 */
MALI_STATIC_INLINE mali_bool _gles_fp32_is_negative(gles_fp32 f)
{
	return (_mali_convert_fp32_to_binary(f) & 0x80000000 ? MALI_TRUE : MALI_FALSE);
}

/**
 * @brief Gets the mantissa of a 32bit IEEE float. don't convert anything or add implicit 1 bit.
 * @param f The float
 * @return The mantissa
 */
MALI_STATIC_INLINE GLfixed _gles_fp32_get_raw_mantissa(gles_fp32 f)
{
	return (_mali_convert_fp32_to_binary(f) & 0x7fffff );
}

/**
 * @brief Gets the mantissa of a 32bit IEEE float and constructs a 16.16 fixed point of it
 * @param f The float
 * @return The fixed point representing the mantissa
 */
MALI_STATIC_INLINE GLfixed _gles_fp32_get_mantissa(gles_fp32 f)
{
	GLfixed value = ((_mali_convert_fp32_to_binary(f) & 0x7fffff ) >> (23-16)) | 0x10000;

	if( MALI_TRUE == _gles_fp32_is_negative( f ) )  /** If the number is negative, we need to flip the mantissa and add the signed bit to the mantissa (0x0000 = 0 --> 0x7fff = 32k whereas 0x8000 = -32k --> 0xffff = -1) */
	{
		value = 0x80000000 - value;
		value = value | 0x80000000;
	}

	return value;
}

/**
 * @brief Gets the exponent of a 32bit IEEE float
 * @param f The float
 * @return The biased int representing the exponent
 */
MALI_STATIC_INLINE GLint _gles_fp32_get_exponent(gles_fp32 f)
{
	return ((((s32)_mali_convert_fp32_to_binary(f))  >> 23) & 0xff) - 127;
}

/**
 * @brief Convert a float to virtual format f16
 * @note the conversion is done without rounding to match what the hardware (Mali200/MaliGP2) does.
 * @param f The float
 * @return The f16 value
 */
MALI_STATIC_INLINE gles_fp16 _gles_fp32_to_fp16(gles_fp32 f)
{
	u32 sign     = _gles_fp32_is_negative(f);
	s32 exponent = _gles_fp32_get_exponent(f);
	u32 mantissa = _gles_fp32_get_raw_mantissa(f);

	if(exponent == 0x80 && mantissa != 0) return FP16_NAN;

	/* set implicit 1 bit */
	mantissa |= (1 << 23);

	/* apply exponent */
	exponent += 15;

	/* move sign to correct place */
	sign <<= 15;

	/* truncate mantissa down from 1:23 to 1:10 fixed point */
	mantissa = (mantissa) >> (23 - 10);
	if (mantissa >= (1 << 11)) /* handle overflow in the mantissa*/
	{
		mantissa = 1<<10; /* 1.000 */
		exponent++;
	}

	/* check for inf, clamp exponent */
	if (exponent >= (1 << 5)) return (gles_fp16)(sign | FP16_INF);
	if (exponent < 0) return (gles_fp16)sign; /* +/- 0.0 */

	mantissa &= ~(1 << 10); /* remove implicit 1 bit */
	MALI_DEBUG_ASSERT((mantissa >> 10) == 0, ("mantissa problem in fp32 to fp16 convert"));
	return (gles_fp16)(sign | (exponent << 10) | mantissa);
}


/**
 * @brief Convert a float to virtual format f16, assuming values 0.1, 0.2, ...1.0 are the most common used.
 * @param f The float
 * @return The f16 value
 */
MALI_STATIC_INLINE gles_fp16 _gles_fp32_to_fp16_predefined(gles_fp32 f)
{
   switch (_mali_convert_fp32_to_binary(f))
   {
     case 0x0: return 0;
     case 0x3DCCCCCD: return 11878;      /* 0.1f*/
     case 0x3E4CCCCD: return 12902;      /* 0.2f*/
     case 0x3E99999A: return 13516;      /* 0.3f*/
     case 0x3ECCCCCD: return 13926;      /* 0.4f*/
     case 0x3F000000: return 14336;      /* 0.5f*/
     case 0x3F19999A: return 14540;      /* 0.6f*/
     case 0x3F333333: return 14745;      /* 0.7f*/
     case 0x3F4CCCCD: return 14950;      /* 0.8f*/
     case 0x3F666666: return 15155;      /* 0.9f*/
     case 0x3F800000: return 15360;      /* 1.0f*/
     default:
       return _gles_fp32_to_fp16(f);
   }
   
}

MALI_STATIC_INLINE gles_fp32 _gles_convert_binary_to_fp32(u32 in)
{
	union {
		gles_fp32 f;
		u32   i;
	} v;
	v.i = in;
	return v.f;
}

MALI_STATIC_INLINE u32 _gles_convert_fp32_to_binary(gles_fp32 in)
{
	union {
		gles_fp32 f;
		u32   i;
	} v;
	v.f = in;
	return v.i;
}
/* convert from FP16 to FP32. */
MALI_STATIC_INLINE gles_fp32 _gles_fp16_to_fp32( gles_fp16 inp )
{
    u32 inpx = inp;

    /*
    This table contains, for every FP16 sign/exponent value combination, the difference
    between the input FP16 value and the value obtained by shifting the correct FP32 result
    right by 13 bits. This table allows us to handle every case except denormals and NaN
    with just 1 table lookup, 2 shifts and 1 add.
    */

    static const s32 tbl[64] = {
        0x80000000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000,
        0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000,
        0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000,
        0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x1C000, 0x80038000,
        0x80038000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000,
        0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000,
        0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000,
        0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x54000, 0x80070000 };
    s32 res = tbl[inpx >> 10];
    res += inpx;

    /* the normal cases: the MSB of 'res' is not set. */
    if( res >= 0 ) /* signed compare */
        return _gles_convert_binary_to_fp32(res << 13);

    /* Infinity and Zero: the bottom 10 bits of 'res' are clear. */
    if( (res & 0x3FF ) == 0)
        return _gles_convert_binary_to_fp32(res << 13);

    /* NaN: the exponent field of 'inp' is not zero;
       NaNs must be quitened. */
    if( (inpx & 0x7C00) != 0)
        return _gles_convert_binary_to_fp32((res << 13) | 0x400000);

    /* the remaining cases are Denormals. */
	{
		u32 sign = (inpx & 0x8000) << 16;
		u32 mskval = inpx & 0x7FFF;
		u32 leadingzeroes = _mali_clz( mskval );
		mskval <<= leadingzeroes;
		return _gles_convert_binary_to_fp32((mskval >> 8) + ((0x85 - leadingzeroes) << 23) + sign);
	}
}

/**
 * Converts a float value to signed 6:2 fixed-point with clamping
 * @param factor the value to convert
 * @returns a converted value
 */
MALI_STATIC_INLINE s32 _gles_fp32_to_fixed_6_2( gles_fp32 f )
{
	s32 fixed_factor;

    /* Sometimes f could be NaN */
    if(_mali_sys_isnan(f))
    {
        return 0;
    }

	/* handle factors larger than the representable range */
	if ( f < -31.75f ) return 0x80;
	if ( f > 31.75f ) return 0x7F;

	/* handle values smaller than the representable range */
	if ( (f < 0.25f) && (f > -0.25f) ) return 0x0;

	/* convert to 30:2 signed fixed-point */
	fixed_factor = (s32)(f*(1<<2));

	/* convert to 6:2 signed fixed-point.
	 * This is safe (preserves sign-bit) since the factor is within the representable range at this point
	 */
	fixed_factor &= 0xFF;

	MALI_DEBUG_ASSERT( (fixed_factor&0xFFFFFF00)==0, ("Error in format conversion") );
	MALI_DEBUG_ASSERT( (f >= 0) || ((fixed_factor&0x80)==0x80), ("Format conversion missed sign") );
	return fixed_factor;
}

/**
 * Converts from FP16 to 8-bit unsigned int.
 * All values are clamped to [0,255]
 * This function performs the same rounding operations as Mali200.
 * @note the fixed-function alpha test requires this.
 * @param value the FP16 value to convert
 * @return an integer in the range [0,255]
 */
MALI_STATIC_INLINE u32 _gles_fp16_to_fix8(gles_fp16 value)
{
	u32 sign =     (value >> 15)  & 0x1;
	u32 exponent = ((value >> 10) & 0x1f);
	u32 mantissa = (1<<10)|(value & 0x3ff);
	u32 shift_amount = 0xe - (exponent&0xf);
	u32 fix88;
	u32 fix8;

	/* return 0 for negative numbers.. */
	if ( sign != 0 ) return 0;
	/* ..and for zero */
	if ( (0==exponent) || ((0x1F==exponent)&&((1<<10)==value)) ) return 0;
	/* return 255 for values >= 1.0 */
	if ( exponent>=0xF ) return 0xFF;

	/* this parts expands the mantissa to an 8:8 fixed-point range */
	fix88 = mantissa << 5;
	if (shift_amount&1) fix88>>=1;
	if (shift_amount&2) fix88>>=2;
	if (shift_amount&4) fix88>>=4;
	if (shift_amount&8) fix88>>=8;

	/* grab the upper 8bits (with rounding) */
	fix8 = ((fix88 - (fix88>>8) + 0x80))>>8;

	/* clamp to max */
	if ( fix8 > 255 ) fix8 = 255;
	return fix8;
}

/**
 * check an enum against a list of valid enums
 * @param enums the enum list
 * @param count the element count of the enum list
 * @param param the enum to check
 * @return MALI_TRUE if the enum was found, MALI_FALSE if not
 */
mali_bool _gles_verify_enum( const GLenum *enums, u32 count, u32 param );

/**
 * check an enum against a list of valid Bools
 * @param enums the bool list
 * @param count the element count of the bool list
 * @param param the value to check for
 * @return MALI_TRUE if the value was found, MALI_FALSE if not
 */
mali_bool _gles_verify_bool( const GLboolean *enums, u32 count, u32 param );


int _gles_get_type_size(GLenum type);

/* @brief Copies a vec3
 * @param dst The destination pointer
 * @param src The source pointer
 */
MALI_STATIC_INLINE void _gles_copy_vec3(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

/* @brief Copies a vec4
 * @param dst The destination pointer
 * @param src The source pointer
 */
MALI_STATIC_INLINE void _gles_copy_vec4(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
}

/* @brief Multiplies two vec3s
 * @param dst The destination pointer
 * @param a The first operand
 * @param b The second operand
 */
MALI_STATIC_INLINE void _gles_mul_vec3(float *dst, const float *a, const float *b)
{
	dst[0] = a[0] * b[0];
	dst[1] = a[1] * b[1];
	dst[2] = a[2] * b[2];
}

/* @brief Multiplies two vec4s
 * @param dst The destination pointer
 * @param a The first operand
 * @param b The second operand
 */
MALI_STATIC_INLINE void _gles_add_vec3(float *dst, const float *a, const float *b)
{
	dst[0] = a[0] + b[0];
	dst[1] = a[1] + b[1];
	dst[2] = a[2] + b[2];
}

/* @brief Checks the cross-product sign of 2 vectors
 * @return  wheather a point on the left (>0), right(<0) or on a line(=0)
 * @param LineA first point on the line
 * @param LineB second point on the line
 * @param point a point
 */
MALI_STATIC_INLINE float _gles_check_2D_line_intersect_point(const float *lineA, const float *lineB, const float *point)
{
  return (  lineB[0]- lineA[0] )*(point[1]-lineA[1]) - (lineB[1] - lineA[1])*(point[0]-lineA[0]);
}

#include <math.h>
/* @brief Normalizes a vec3
 * @param dst The destination pointer
 * @param a The source pointer
 */
MALI_STATIC_INLINE void _gles_normalize_vec3(float *dst, const float *src)
{
	const float length = _mali_sys_sqrt( src[ 0 ] * src[ 0 ] + src[ 1 ] * src[ 1 ] + src[ 2 ] * src[ 2 ] );
	float reclength = 1.0f;

	/* If length is +INF or -INF, then reclength will become 0.0f  */
	/* If length is NaN, then reclength will become NaN  */
	reclength = 1.0f / length;

	dst[0] = src[0] * reclength;
	dst[1] = src[1] * reclength;
	dst[2] = src[2] * reclength;
}

#endif /* _GLES_UTIL_H_ */
