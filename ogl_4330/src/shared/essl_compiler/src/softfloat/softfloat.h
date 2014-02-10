/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */



/*
Soft-float library.

This library contains an IEEE-754r compliant implementation
of a set of commonly used floating-point operations. The implementation
includes NaN, denormals, and fully selectable rounding mode wherever applicable.

The library supplies dedicated functions for converting between soft-float
and C's native floating-point types; other than that, it strictly abstains from
using the native floating-point types in any way, shape or form.

Except for 'neg' and 'abs', every function that produces a floating-point result
from floating-point inputs will convert an SNaN to a QNaN.

The floating-point to integer conversions will convert NaN to 0.

The arithmetic operations that can produce a NaN also take a 'nan_payload'
argument; this argument is a value that is inserted as the payload into the NaN
if the operation happens to produce a NaN. Note that this only applies when the
NaN is produced by the operation; if the operation receives a NaN on one of its inputs,
it will simply propagate that NaN instead, retaining that NaN's payload.

In the case where an operation receives NaN in more than one of its arguments, it will
quieten both NaNs and then use TotalOrder to determine which NaN to return, according to
 result = TotalOrder( a, b ) ? a : b

There are a number of operations that are not supplied directly by this library,
because they can trivially be obtained by composing library functions or as special
cases of the library functions. This applies to:

 - Subtract: call the appropriate neg_* function for the second argument to add_*
 - Floor(), Ceiling(): Use the rounding mode argument to round_*
 - Greater-than, Greater_than-or-equal: reverse the argument order to less-than and less-than-or-equal
 - Not-equal: Negate the result of equal_*

*/

#ifndef SOFTFLOAT_H_INCLUDED
#define SOFTFLOAT_H_INCLUDED


#if defined __cplusplus
extern "C" {
#endif


/* If C++ and not MSVC, or if C99 and better, we have stdint.h and can include it directly */
#if (defined __cplusplus && !defined(_MSC_VER)) || (__STDC_VERSION__ >= 199901L)
/* if compiling as C++, we need to define these macros in order to
obtain all the macros in stdint.h . */
#ifndef __STDC_LIMIT_MACROS
 #define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
 #define __STDC_CONSTANT_MACROS
#endif
#include <stdint.h>

#else

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
typedef unsigned long long uint64_t;
typedef signed long long int64_t;

#endif


/* targets that don't have UINT32_C probably don't have the rest of C99s stdint.h */
#ifndef UINT32_C

#define MALI_SOFTFLOAT_PASTE(a) a
#define UINT64_C(a) MALI_SOFTFLOAT_PASTE(a##ULL)
#define UINT32_C(a) MALI_SOFTFLOAT_PASTE(a##U)
#define INT64_C(a) MALI_SOFTFLOAT_PASTE(a##LL)
#define INT32_C(a) a

#define PRIX32 "X"
#define PRId32 "d"
#define PRIu32 "u"
#define PRIX64 "LX"
#define PRId64 "Ld"
#define PRIu64 "Lu"

#endif

/* sized soft-float types. These are mapped to the sized integer types of C99,
   instead of C's floating-point types; this is because the library needs to maintain
   exact, bit-level control on all operations on these data types. */
typedef uint16_t sf16;
typedef uint32_t sf32;
typedef uint64_t sf64;

/* the five rounding modes that IEEE-754r defines */
typedef enum
    {
    SF_UP = 0, /* round towards positive infinity */
    SF_DOWN = 1,  /* round towards negative infinity */
    SF_TOZERO = 2,  /* round towards zero */
    SF_NEARESTEVEN = 3,  /* round toward nearest value; if mid-between, round to even value */
    SF_NEARESTAWAY = 4   /* round toward nearest value; if mid-between, round away from zero */
    } roundmode;

/* addition */
sf16  _mali_add_sf16( sf16, sf16, roundmode, uint16_t nan_payload );
sf32  _mali_add_sf32( sf32, sf32, roundmode, uint32_t nan_payload );
sf64  _mali_add_sf64( sf64, sf64, roundmode, uint64_t nan_payload );

/* multiplication */
sf16  _mali_mul_sf16( sf16, sf16, roundmode, uint16_t nan_payload );
sf32  _mali_mul_sf32( sf32, sf32, roundmode, uint32_t nan_payload );
sf64  _mali_mul_sf64( sf64, sf64, roundmode, uint64_t nan_payload );

/* widening multiplication. The result type is twice as wide as the
   input type, allowing the multiply to be performed without rounding. */
sf32  _mali_widen_mul_sf16( sf16, sf16, uint32_t nan_payload );
sf64  _mali_widen_mul_sf32( sf32, sf32, uint64_t nan_payload );

/* narrowing addition */
sf16 _mali_narrow_add_sf32( sf32, sf32, roundmode, uint16_t nan_payload );
sf32 _mali_narrow_add_sf64( sf64, sf64, roundmode, uint32_t nan_payload );

/* fused multiply-add.
   The operation performed is  (a*b) + c
   with no intervening rounding between the multiplication
   and the addition.
   */
sf16  _mali_fma_sf16( sf16 a, sf16 b, sf16 c, roundmode, uint16_t nan_payload );
sf32  _mali_fma_sf32( sf32 a, sf32 b, sf32 c, roundmode, uint32_t nan_payload );
sf64  _mali_fma_sf64( sf64 a, sf64 b, sf64 c, roundmode, uint64_t nan_payload );

/* negation */
sf16  _mali_neg_sf16( sf16 );
sf32  _mali_neg_sf32( sf32 );
sf64  _mali_neg_sf64( sf64 );

/* absolute-value */
sf16  _mali_abs_sf16( sf16 );
sf32  _mali_abs_sf32( sf32 );
sf64  _mali_abs_sf64( sf64 );

/* round to integer */
sf16  _mali_round_sf16( sf16, roundmode );
sf32  _mali_round_sf32( sf32, roundmode );
sf64  _mali_round_sf64( sf64, roundmode );

/* narrowing float->float conversions */
sf16  _mali_sf32_to_sf16( sf32, roundmode );
sf32  _mali_sf64_to_sf32( sf64, roundmode );
sf16  _mali_sf64_to_sf16( sf64, roundmode );

/* widening float->float conversions */
sf32  _mali_sf16_to_sf32( sf16 );
sf64  _mali_sf32_to_sf64( sf32 );

/* conversion from soft-float to unsigned integer */
uint16_t  _mali_sf16_to_u16( sf16, roundmode );
uint32_t  _mali_sf32_to_u32( sf32, roundmode );
uint64_t  _mali_sf64_to_u64( sf64, roundmode );

/* conversion from unsigned integer to soft-float */
sf16  _mali_u16_to_sf16( uint16_t, roundmode );
sf32  _mali_u32_to_sf32( uint32_t, roundmode );
sf64  _mali_u64_to_sf64( uint64_t, roundmode );

/* conversion from soft-float to signed integer */
int16_t  _mali_sf16_to_s16( sf16, roundmode );
int32_t  _mali_sf32_to_s32( sf32, roundmode );
int64_t  _mali_sf64_to_s64( sf64, roundmode );

/* conversion from signed integer to soft-float */
sf16  _mali_s16_to_sf16( int16_t, roundmode );
sf32  _mali_s32_to_sf32( int32_t, roundmode );
sf64  _mali_s64_to_sf64( int64_t, roundmode );


/* IEEE-754r compliant equality test.
   Returns 1 if its two arguments are equal, 0 if they are not.
   Note than if the two arguments are NaN, they are NOT considered
   equal under IEEE-754r, even if they have the same encoding.
   Also, -0 is considered equal to 0.
   */
int  _mali_equal_sf16( sf16, sf16 );
int  _mali_equal_sf32( sf32, sf32 );
int  _mali_equal_sf64( sf64, sf64 );

/* less-than test.
   Returns 1 if 'a' is less than 'b', 0 otherwise.
   If either 'a' or 'b' is NaN, return 0.
*/
int  _mali_lt_sf16( sf16 a, sf16 b );
int  _mali_lt_sf32( sf32 a, sf32 b );
int  _mali_lt_sf64( sf64 a, sf64 b );

/* less-than-or-equal test
   Returns 1 if 'a' is less than or equal to 'b', 0 otherwise.
   If either 'a' or 'b' is NaN, return 0.
*/
int  _mali_le_sf16( sf16 a, sf16 b );
int  _mali_le_sf32( sf32 a, sf32 b );
int  _mali_le_sf64( sf64 a, sf64 b );

/* total-order test: This test imposes a total ordering
   on all representable floating-point values. For the
   most part, it behaves as ordinary IEEE-754r less-than-or-equal,
   with the following exceptions:
    * -0 is considered less than +0
    * Nan with sign bit set is considered less than -INF
    * Nan with sign bit cleared is considered greater than +INF
   This ordering is defined by IEEE-754r.
   */
int  _mali_ord_sf16( sf16 a, sf16 b );
int  _mali_ord_sf32( sf32 a, sf32 b );
int  _mali_ord_sf64( sf64 a, sf64 b );

/* is-NAN test */
int  _mali_isnan_sf16( sf16 );
int  _mali_isnan_sf32( sf32 );
int  _mali_isnan_sf64( sf64 );

/* is-INF test */
int  _mali_isinf_sf16( sf16 );
int  _mali_isinf_sf32( sf32 );
int  _mali_isinf_sf64( sf64 );

/* is-NEG test */
int  _mali_isneg_sf16( sf16 );
int  _mali_isneg_sf32( sf32 );
int  _mali_isneg_sf64( sf64 );

/* is-Denormal test */
int  _mali_isdenormal_sf16( sf16 );
int  _mali_isdenormal_sf32( sf32 );
int  _mali_isdenormal_sf64( sf64 );


/* clear mantissa of denormal */
sf16  _mali_denormal_flushtozero_sf16( sf16 inp );
sf32  _mali_denormal_flushtozero_sf32( sf32 inp );
sf64  _mali_denormal_flushtozero_sf64( sf64 inp );

/* Get exponent */
int  _mali_exponent_sf16( sf16 inp );
int  _mali_exponent_sf32( sf32 inp );
int  _mali_exponent_sf64( sf64 inp );



/* minimum value.
   For the purposes of minimum/maximum value, -0 is
   considered to be strictly less than +0.
 */
sf16  _mali_min_nan_propagate_sf16( sf16, sf16 );
sf32  _mali_min_nan_propagate_sf32( sf32, sf32 );
sf64  _mali_min_nan_propagate_sf64( sf64, sf64 );

/* minimum value with NaN-exclusion.
   If one of the arguments is NaN, then it returns
   the other argument - unlike ordinary minimum value,
   which will return the NaN.
   */
sf16  _mali_min_sf16( sf16, sf16 );
sf32  _mali_min_sf32( sf32, sf32 );
sf64  _mali_min_sf64( sf64, sf64 );

/* maximum value */
sf16  _mali_max_nan_propagate_sf16( sf16, sf16 );
sf32  _mali_max_nan_propagate_sf32( sf32, sf32 );
sf64  _mali_max_nan_propagate_sf64( sf64, sf64 );

/* maximum value with NaN-exclusion.
   If one of the arguments is NaN, then it returns
   the other argument - unlike ordinary minimum value,
   which will return the NaN.
   */
sf16  _mali_max_sf16( sf16, sf16 );
sf32  _mali_max_sf32( sf32, sf32 );
sf64  _mali_max_sf64( sf64, sf64 );

/* testbench equality: this equality function
   differs from IEEE-754 compliant equality test in the following cases:
     * -0 is not considered equal to +0
     * NaN is considered equal to NaN.
   This equality test is expected to be useful for testbenches.
   */
int  _mali_equal_tb_sf16( sf16, sf16 );
int  _mali_equal_tb_sf32( sf32, sf32 );
int  _mali_equal_tb_sf64( sf64, sf64 );

/* conversion from native-float types to soft-float types.
   Conversions that perform narrowing need a round-mode argument */
sf16  _mali_float_to_sf16( float, roundmode );
sf32  _mali_float_to_sf32( float );
sf64  _mali_float_to_sf64( float );
sf16  _mali_double_to_sf16( double, roundmode );
sf32  _mali_double_to_sf32( double, roundmode );
sf64  _mali_double_to_sf64( double );

/* conversion from soft-float types to native-float types.
   Conversions that perform narrowing need a round-mode argument */
float  _mali_sf16_to_float( sf16 );
float  _mali_sf32_to_float( sf32 );
float  _mali_sf64_to_float( sf64, roundmode );
double  _mali_sf16_to_double( sf16 );
double  _mali_sf32_to_double( sf32 );
double  _mali_sf64_to_double( sf64 );

/* dot product operations */
sf32 _mali_dot4_sf32( const sf32 a[4], const sf32 b[4] );
sf32 _mali_dot3_sf32( const sf32 a[3], const sf32 b[3] );
sf16 _mali_dot4_sf16( const sf16 a[4], const sf16 b[4] );
sf16 _mali_dot3_sf16( const sf16 a[3], const sf16 b[3] );


#if defined __cplusplus
}
#endif


#endif /* SOFTFLOAT_H_INCLUDED */


