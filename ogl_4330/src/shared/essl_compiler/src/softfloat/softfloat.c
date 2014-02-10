/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include "softfloat.h"

#define SOFTFLOAT_INLINE

/******************************************
  helper functions and their lookup tables
 ******************************************/




/*
count leading zeroes functions. Only used when the input is nonzero.
*/

#if defined(__GNUC__) && (defined(__i386) || defined(__amd64))
#elif defined(__arm__) && defined(__ARMCC_VERSION)
#elif defined(__arm__) && defined(__GNUC__) && !defined(__thumb__)
#else
/*
table used for the slow default versions.
*/
static const uint8_t clz_table[256] =
    {
    8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };
#endif


/*
32-bit count-leading-zeroes function: use the Assembly instruction
whenever possible.
*/
static SOFTFLOAT_INLINE uint32_t clz32( uint32_t inp )
    {
#if defined(__GNUC__) && (defined(__i386) || defined(__amd64))
    uint32_t bsr;
    __asm__( "bsrl %1, %0" : "=r"(bsr) : "r"(inp | 1) );
    return 31-bsr;
#else
#if defined(__arm__) && defined(__ARMCC_VERSION)
	return __clz(inp); /* armcc builtin */
#else
#if defined(__arm__) && defined(__GNUC__) && !defined(__thumb__)
    uint32_t lz;
    __asm__( "clz %0, %1" : "=r"(lz) : "r"(inp) );
    return lz;
#else
    /* slow default version */
    uint32_t summa = 24;
    if( inp >= UINT32_C(0x10000) ) { inp >>= 16; summa -= 16; }
    if( inp >= UINT32_C(0x100) ) { inp >>= 8; summa -= 8; }
    return summa + clz_table[inp];
#endif
#endif
#endif
    }


/*
64-bit count-leading-zeroes. Use platform-specific assembly instructions
when possible.
*/
static SOFTFLOAT_INLINE uint32_t clz64( uint64_t inp )
    {
#if defined (__GNUC__) && (defined(__amd64))
    uint64_t bsr;
    __asm__( "bsrq %1, %0" : "=r"(bsr) : "r"(inp | UINT64_C(1)) );
    return 63-(uint32_t)bsr;
#else
#if defined(__i386) || defined(__arm__)
    if( inp >> 32 )
        return clz32( inp >> 32 );
    return clz32( inp ) + 32;
#else
    /* slow default version */
    uint32_t summa = 56;
    uint32_t inp2;
    if( inp >= UINT64_C(0x100000000) ) { inp >>= 32; summa -= 32; }
    inp2 = (uint32_t)inp;
    if( inp2 >= UINT32_C(0x10000) ) { inp2 >>= 16; summa -= 16; }
    if( inp2 >= UINT32_C(0x100) ) { inp2 >>= 8; summa -= 8; }
    return summa + clz_table[inp2];    
#endif
#endif
    }




/* round-to-nearest-even shift. */
static SOFTFLOAT_INLINE uint64_t rtne_shift64( uint64_t inp, uint32_t shamt )
    {
    uint64_t vl1 = UINT64_C(1) << shamt;
    uint64_t inp2 = inp + (vl1>>1); /* added 0.5 ulp */
    uint64_t msk = (inp | UINT64_C(1)) & vl1; /* nonzero if odd. '| 1' forces it to 1 if the shamt is 0. */
    msk--; /* negative if even, nonnegative if odd. */
    inp2 -= (msk >> 63); /* subtract epsilon before shift if even. */
    inp2 >>= shamt;
    return inp2;
    } 


static SOFTFLOAT_INLINE uint32_t rtne_shift32( uint32_t inp, uint32_t shamt )
    {
    uint32_t vl1 = UINT32_C(1) << shamt;
    uint32_t inp2 = inp + (vl1>>1); /* added 0.5 ulp */
    uint32_t msk = (inp | UINT32_C(1)) & vl1; /* nonzero if odd. '| 1' forces it to 1 if the shamt is 0. */
    msk--; /* negative if even, nonnegative if odd. */
    inp2 -= (msk >> 31); /* subtract epsilon before shift if even. */
    inp2 >>= shamt;
    return inp2;
    } 



/* round-to-nearest-away shift */
static SOFTFLOAT_INLINE uint64_t rtna_shift64( uint64_t inp, uint32_t shamt )
    {
    uint64_t vl1 = (UINT64_C(1) << shamt) >> 1;
    inp += vl1;
    inp >>= shamt;
    return inp;
    }


static SOFTFLOAT_INLINE uint32_t rtna_shift32( uint32_t inp, uint32_t shamt )
    {
    uint32_t vl1 = (UINT32_C(1) << shamt) >> 1;
    inp += vl1;
    inp >>= shamt;
    return inp;
    }



/* round-up shift. */    
static SOFTFLOAT_INLINE uint64_t rtup_shift64(uint64_t inp, uint32_t shamt)
    {
    uint64_t vl1 = UINT64_C(1) << shamt;
    inp += vl1;
    inp--;
    inp >>= shamt;
    return inp;
    }

static SOFTFLOAT_INLINE uint32_t rtup_shift32(uint32_t inp, uint32_t shamt)
    {
    uint32_t vl1 = UINT32_C(1) << shamt;
    inp += vl1;
    inp--;
    inp >>= shamt;
    return inp;
    }



/* sticky right-shifts */
static SOFTFLOAT_INLINE uint32_t sticky_urshift32(uint32_t inp, uint32_t shamt)
    {
    uint32_t vl1 = (UINT32_C(1) << shamt);
	uint32_t vl2;
    vl1--; /* all 1s in the lower bits. */
    vl2 = (inp & vl1); /* the low-order bits of inp */
    vl2 += vl1; /* propagate a carry to bit position [shamt] unless the low-order bits are 0. */
    inp |= vl2; /* logic-OR the carry-bit (if set) into bit position [shamt]. */
    return inp >> shamt;
    }


static SOFTFLOAT_INLINE uint64_t sticky_urshift64(uint64_t inp, uint32_t shamt)
    {
    uint64_t vl1 = (UINT64_C(1) << shamt);
	uint64_t vl2;
    vl1--; /* all 1s in the lower bits. */
    vl2 = (inp & vl1); /* the low-order bits of inp */
    vl2 += vl1; /* propagate a carry to bit position [shamt] unless the low-order bits are 0. */
    inp |= vl2; /* logic-OR the carry-bit (if set) into bit position [shamt]. */
    return inp >> shamt;
    }



/******************************
  Negate and absolute value
******************************/

/*
These operations only manipulate the sign bit of their argument.
*/


sf16 _mali_neg_sf16( sf16 inp ) 
    { return inp ^ UINT32_C(0x8000); }
    
sf32 _mali_neg_sf32( sf32 inp )
    { return inp ^ UINT32_C(0x80000000); }

sf64 _mali_neg_sf64( sf64 inp )
    { return inp ^ UINT64_C(0x8000000000000000); }


sf16 _mali_abs_sf16( sf16 inp )
    { return inp & UINT32_C(0x7FFF); }
    
sf32 _mali_abs_sf32( sf32 inp )
    { return inp & UINT32_C(0x7FFFFFFF); }
    
sf64 _mali_abs_sf64( sf64 inp ) 
    { return inp & UINT64_C(0x7FFFFFFFFFFFFFFF); }


/*********************************
  is-NEG test functions 
*********************************/

int _mali_isneg_sf16( sf16 inp )
    { return (inp & UINT32_C(0x8000)) == UINT32_C(0x8000); }

int _mali_isneg_sf32( sf32 inp )
    { return (inp & UINT32_C(0x80000000)) == UINT32_C(0x80000000); }

int _mali_isneg_sf64( sf64 inp )
    { return (inp & UINT64_C(0x8000000000000000)) == UINT64_C(0x8000000000000000); }


/*********************************
  is-Denormal test functions 
*********************************/

int  _mali_isdenormal_sf16( sf16 inp )
	{ return (((inp & UINT32_C(0x7C00)) == UINT32_C(0)) && ((inp & UINT32_C(0x007FFFFF)) != UINT32_C(0)));}

int  _mali_isdenormal_sf32( sf32 inp )
	{ return (((inp & UINT32_C(0x7F800000)) == UINT32_C(0)) && ((inp & UINT32_C(0x007FFFFF)) != UINT32_C(0)));}

int  _mali_isdenormal_sf64( sf64 inp )
	{ return (((inp & UINT64_C(0x7FF0000000000000)) == UINT64_C(0)) && ((inp & UINT64_C(0x000FFFFFFFFFFFFF)) != UINT64_C(0)));}



/*********************************
  clear mantissa of denormal
*********************************/
sf16  _mali_denormal_flushtozero_sf16( sf16 inp )
{
	if ((inp & UINT32_C(0x7C00)) == UINT32_C(0)) 
	{
		return (inp &  UINT32_C(0x8000));
	}
	else
	{
		return (inp);
	}
}

sf32  _mali_denormal_flushtozero_sf32( sf32 inp )
{
	if ((inp & UINT32_C(0x7F800000)) == UINT32_C(0)) 
	{
		return (inp &  UINT32_C(0x80000000));
	}
	else
	{
		return (inp);
	}
}

sf64  _mali_denormal_flushtozero_sf64( sf64 inp )
{
	if ((inp & UINT64_C(0x7FF0000000000000)) == UINT64_C(0)) 
	{
		return (inp &  UINT64_C(0x8000000000000000));
	}
	else
	{
		return (inp);
	}
}


/*********************************
  Get exponent
*********************************/
int  _mali_exponent_sf16( sf16 inp )
{
	return  (int)(((inp & UINT32_C(0x7C00))) >> 10);
}

int  _mali_exponent_sf32( sf32 inp )
{
	return (int)((inp & UINT32_C(0x7F800000)) >> 23); 
}

int  _mali_exponent_sf64( sf64 inp )
{
	return (int)((inp & UINT64_C(0x7FF0000000000000)) >> 52); 
}




/*********************************
  is-NaN and is-INF test functions 
*********************************/

int _mali_isnan_sf16( sf16 inp )  
    { return (inp & UINT32_C(0x7FFF)) > UINT32_C(0x7C00); }

int _mali_isnan_sf32( sf32 inp )
    { return (inp & UINT32_C(0x7FFFFFFF)) > UINT32_C(0x7F800000); }

int _mali_isnan_sf64( sf64 inp )
    { return (inp & UINT64_C(0x7FFFFFFFFFFFFFFF)) > UINT64_C(0x7FF0000000000000); }


int _mali_isinf_sf16( sf16 inp )
    { return (inp & UINT32_C(0x7FFF)) == UINT32_C(0x7C00); }

int _mali_isinf_sf32( sf32 inp )
    { return (inp & UINT32_C(0x7FFFFFFF)) == UINT32_C(0x7F800000); }

int _mali_isinf_sf64( sf64 inp )
    { return (inp & UINT64_C(0x7FFFFFFFFFFFFFFF)) == UINT64_C(0x7FF0000000000000); }



/***********************************
  Floating-point comparisons
***********************************/

/* these are:
    - IEEE-754r equality
    - Less-than
    - Less-than-or-equal
    - TotalOrder
    - Testbench-equality. This type of equalityt differs from IEEE-754r equality in that:
       * +0 and -0 don't compare equal
       * all NaNs compare equal to each other.
*/


/*
Equality tests can for the most part compare numerical equality, except:
 - If both operands are NaN, then they compare NOT EQUAL
 - If both operands are Zero, then the compare EQUAL
*/

int _mali_equal_sf32( sf32 a, sf32 b )
    {
    /*
    first, mask off the sign bit, and add 2^23 minus 1.
    For NaNs, this cases ap and bp to become negative numbers.
    If a is Zero, then ap becomes 0x7FFFFF; for other values of a, then ap becomes a larger value.
    */
    int32_t ap = (a & UINT32_C(0x7FFFFFFF)) + UINT32_C(0x7FFFFF); 
    int32_t bp = (b & UINT32_C(0x7FFFFFFF)) + UINT32_C(0x7FFFFF);
    /*
    This logical-OR operation will, if either input is a NaN, produce a negative value.
    If both inputs are Zero, then it will produce 0x7FFFFF; if any input is greater than zero,
    then it will produce a positive number of 0x800000 or greater.
    */
    int32_t det = ap | bp;
    if( det >= 0x800000 )
        {
        /*
        if we reach this point, the logic-OR resulted in a value that is 0x800000 or greater;
        we can compare our two inputs directly, and we are done.
        */
        return a == b;
        }
    else
        {
        /*
        If we are sent here, we know that the input either contained NaN or was two zeroes;
        in this case, 'det' is negative in case of NaNs and positive in case of Zero.
        */
        return det >= 0;
        }
    }


int _mali_equal_sf16( sf16 a, sf16 b )
    {
    /* see comments for _mali_equal_sf32. */
    uint32_t ap = (a & UINT32_C(0x7FFF)) + UINT32_C(0x3FF);
    uint32_t bp = (b & UINT32_C(0x7FFF)) + UINT32_C(0x3FF);
    int16_t det = (int16_t)(ap | bp);
    if( det >= 0x400 )
        return a == b;
    else
        return det >= 0;
    }


int _mali_equal_sf64( sf64 a, sf64 b )
    {
    /* see comments for _mali_equal_sf64 */
    int64_t ap = (a & UINT64_C(0x7FFFFFFFFFFFFFFF)) + UINT64_C(0xFFFFFFFFFFFFF);
    int64_t bp = (b & UINT64_C(0x7FFFFFFFFFFFFFFF)) + UINT64_C(0xFFFFFFFFFFFFF);
    int64_t det = ap | bp;
    if( det >= INT64_C(0x10000000000000) )
        return a == b;
    else
        return det >= 0;
    }



/* Less-than tests.
   If either input is a NaN, or both inputs are zero,
   then the less-than compare is false. Else, we can XOR
   all non-sign bits with the sign-bit and compare the result.
   */
   
int _mali_lt_sf32( sf32 a, sf32 b )
    {
    /*
    compute a determinant as for equality test; the less-than
    relation is true only if the determinant is 2^23 or greater.
    */
    int32_t ap = (a & UINT32_C(0x7FFFFFFF)) + 0x7FFFFF;
    int32_t bp = (b & UINT32_C(0x7FFFFFFF)) + 0x7FFFFF;
    int32_t det = ap | bp;
    /* For both inputs, XOR all non-sign bits with the sign bit. */
    uint32_t a_ext = (int32_t)a >> 31; /* signed right-shift; replicates sign bit to all bit lanes */
    uint32_t b_ext = (int32_t)b >> 31;
	int32_t ax;
	int32_t bx;
    a_ext >>= 1; /* unsigned shift; fill top bit with '0' */
    b_ext >>= 1;
    ax = a ^ a_ext;
    bx = b ^ b_ext;
    /*  finally, perform the compares. */
    return det >= 0x800000 && ax < bx; /* important: both compares are SIGNED. */
    }

   
int _mali_lt_sf16( sf16 a, sf16 b )
    {
    /*
    compute a determinant as for equality test; the less-than
    relation is true only if the determinant is 2^10 or greater.
    */
    uint32_t ap = (a & UINT32_C(0x7FFF)) + 0x3FF;
    uint32_t bp = (b & UINT32_C(0x7FFF)) + 0x3FF;
    int16_t det = (int16_t)(ap | bp);
    /* For both inputs, XOR all non-sign bits with the sign bit. */
    uint32_t a_ext = ((int32_t)a << 16) >> 31;
    uint32_t b_ext = ((int32_t)b << 16) >> 31;
	int16_t ax;
	int16_t bx;
    a_ext >>= 17;
    b_ext >>= 17;
    ax = (int16_t)(a ^ a_ext);
    bx = (int16_t)(b ^ b_ext);
    /*  finally, perform the compares. */
    return det >= 0x400 && ax < bx;
    }



int _mali_lt_sf64( sf64 a, sf64 b )
    {
    /*
    compute a determinant as for equality test; the less-than
    relation is true only if the determinant is 2^52 or greater.
    */
    int64_t ap = (a & UINT64_C(0x7FFFFFFFFFFFFFFF)) + UINT64_C(0xFFFFFFFFFFFFF);
    int64_t bp = (b & UINT64_C(0x7FFFFFFFFFFFFFFF)) + UINT64_C(0xFFFFFFFFFFFFF);
    int64_t det = ap | bp;
    /* For both inputs, XOR all non-sign bits with the sign bit. */
    uint64_t a_ext = (int64_t)a >> 63; 
    uint64_t b_ext = (int64_t)b >> 63;
	int64_t ax;
	int64_t bx;
    a_ext >>= 1; 
    b_ext >>= 1;
    ax = a ^ a_ext;
    bx = b ^ b_ext;
    /*  finally, perform the compares. */
    return det >= INT64_C(0x10000000000000) && ax < bx;
    }


/*
Less-than-or-equal test.
Ths is essentially identical to the less-than test, except for the final test.
The final test forks in two paths depending on the value of the determinant.
*/
int _mali_le_sf32( sf32 a, sf32 b )
    {
    /*
    compute a determinant as for equality test; the less-than
    relation is true only if the determinant is 2^23 or greater.
    */
    int32_t ap = (a & UINT32_C(0x7FFFFFFF)) + UINT32_C(0x7FFFFF);
    int32_t bp = (b & UINT32_C(0x7FFFFFFF)) + UINT32_C(0x7FFFFF);
    int32_t det = ap | bp;
    /* For both inputs, XOR all non-sign bits with the sign bit */
    uint32_t a_ext = (int32_t)a >> 31; /* signed right-shift; replicates sign bit to all bit lanes */
    uint32_t b_ext = (int32_t)b >> 31;
	int32_t ax;
	int32_t bx;
    a_ext >>= 1; /* unsigned shift; fill top bit with '0' */
    b_ext >>= 1;
    ax = a ^ a_ext;
    bx = b ^ b_ext;
    /* finally, perform the compares. */
    if( det >= INT32_C(0x800000) )
        return bx >= ax;
    else
        return det >= 0; /* both inputs zero: OK */   
    }


int _mali_le_sf16( sf16 a, sf16 b )
    {
    /* 
    compute a determinant as for equality test; the less-than
    relation is true only if the determinant is 2^10 or greater.
    */
    uint32_t ap = (a & UINT32_C(0x7FFF)) + UINT32_C(0x3FF);
    uint32_t bp = (b & UINT32_C(0x7FFF)) + UINT32_C(0x3FF);
    int16_t det = (int16_t)(ap | bp);
    /* For both inputs, XOR all non-sign bits with the sign bit. */
    uint32_t a_ext = ((int32_t)a << 16) >> 31;
    uint32_t b_ext = ((int32_t)b << 16) >> 31;
	int16_t ax;
	int16_t bx;
    a_ext >>= 17;
    b_ext >>= 17;
    ax = (int16_t)(a ^ a_ext);
    bx = (int16_t)(b ^ b_ext);
    /* finally, perform the compares. */
    if( det >= INT32_C(0x400) )
        return bx >= ax;
    else
        return det >= 0;
    }



int _mali_le_sf64( sf64 a, sf64 b )
    {
    /* 
    compute a determinant as for equality test; the less-than
    relation is true only if the determinant is 2^52 or greater. 
    */
    int64_t ap = (a & UINT64_C(0x7FFFFFFFFFFFFFFF)) + UINT64_C(0xFFFFFFFFFFFFF);
    int64_t bp = (b & UINT64_C(0x7FFFFFFFFFFFFFFF)) + UINT64_C(0xFFFFFFFFFFFFF);
    int64_t det = ap | bp;
    /* For both inputs, XOR all non-sign bits with the sign bit. */
    uint64_t a_ext = (int64_t)a >> 63; 
    uint64_t b_ext = (int64_t)b >> 63;
	int64_t ax;
	int64_t bx;
    a_ext >>= 1; 
    b_ext >>= 1;
    ax = a ^ a_ext;
    bx = b ^ b_ext;
    /*  finally, perform the compares. */
    if( det >= INT64_C(0x10000000000000) )
        return bx >= ax;
    else
        return det >= 0;
    }


/* TotalOrdering. This compare imposes a total ordering over all representable
floating-point values; for the most part (except +/- zero and NaN) it behaves
like a less-than-or-equal compare.

At the bit-level, the operation is a sign-magnitude compare
except that -0 counts as less than +0. As such, what we do, is to
XOR all non-sign bits with the sign bit, then perform a simple
signed compare.

*/

int _mali_ord_sf32( sf32 a, sf32 b )
    {
    uint32_t a_ext = (int32_t)a >> 31; /* signed shift: copies sign bit to all bit positions. */
    uint32_t b_ext = (int32_t)b >> 31; /* signed shift */
	int32_t ax;
	int32_t bx;
    a_ext >>= 1; /* unsigned shift: clears the sign bit */
    b_ext >>= 1; /* unsigned shift */
    ax = a ^ a_ext;
    bx = b ^ b_ext;
    return ax <= bx; /* signed compare! */
    }

int _mali_ord_sf16( sf16 a, sf16 b )
    {
    uint32_t a_ext = ((int32_t)a << 16) >> 31; /* signed shift */
    uint32_t b_ext = ((int32_t)b << 16) >> 31; /* signed shift */
	int16_t ax;
	int16_t bx;
    a_ext >>= 17; /* unsigned shift */
    b_ext >>= 17; /* unsigned shift */
    ax = a ^ a_ext;
    bx = b ^ b_ext;
    return ax <= bx; /* signed compare! */
    }

int _mali_ord_sf64( sf64 a, sf64 b )
    {
    uint64_t a_ext = (int64_t)a >> 63; /* signed shift */
    uint64_t b_ext = (int64_t)b >> 63; /* signed shift */
	int64_t ax;
	int64_t bx;
    a_ext >>= 1; /* unsigned shift */
    b_ext >>= 1; /* unsigned shift */
    ax = a ^ a_ext;
    bx = b ^ b_ext;
    return ax <= bx; /* signed compare! */
    }



/*****************************************************
  float->float widening conversion routines.
*****************************************************/  

/*
Routines are supplied for FP16->FP32 and FP32->FP64 conversions;
an FP16->FP64 conversion can safely be assembled from these two conversions.
*/


/* convert from FP16 to FP32. */
sf32 _mali_sf16_to_sf32( sf16 inp )
    {
    uint32_t inpx = inp;

    /*
    This table contains, for every FP16 sign/exponent value combination, the difference
    between the input FP16 value and the value obtained by shifting the correct FP32 result
    right by 13 bits. This table allows us to handle every case except denormals and NaN
    with just 1 table lookup, 2 shifts and 1 add.
    */    
        
    static const int32_t tbl[64] = {
        INT32_C(0x80000000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), 
        INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), 
        INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), 
        INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x1C000), INT32_C(0x80038000), 
        INT32_C(0x80038000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000),
        INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000),
        INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000),
        INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x54000), INT32_C(0x80070000) };    
    int32_t res = tbl[inpx >> 10];
    res += inpx;
    
    /* the normal cases: the MSB of 'res' is not set. */
    if( res >= 0 ) /* signed compare */
        return res << 13;
    
    /* Infinity and Zero: the bottom 10 bits of 'res' are clear. */
    if( (res & UINT32_C(0x3FF) ) == 0)
        return res << 13;
    
    /* NaN: the exponent field of 'inp' is not zero;
       NaNs must be quitened. */
    if( (inpx & 0x7C00) != 0)
        return (res << 13) | UINT32_C(0x400000);
        
    /* the remaining cases are Denormals. */
		{
		uint32_t sign = (inpx & UINT32_C(0x8000)) << 16;
		uint32_t mskval = inpx & UINT32_C(0x7FFF);
		uint32_t leadingzeroes = clz32( mskval );
		mskval <<= leadingzeroes;
		return (mskval >> 8) + ((0x85 - leadingzeroes) << 23) + sign;
		}    
    }


/* convert from FP32 to FP64. */
sf64 _mali_sf32_to_sf64( sf32 inp )
    {

    /*
    This table contains, for every FP32 sign/exponent value combination, the difference
    between the value that results from just left-shifting the FP32 by 29 places
    and the correct FP64 value. Because these actual values are very large, and have all zeroes except
    in their top 8 bits, the actual table elements are right-shifted by 56 bits so that they each fit into an uint8_t.
    
    This table allows us to handle every case except denormals and NaN
    with just 1 table lookup, 3 shifts and 1 add.
    */    
        
    static const uint8_t tbl[512] = {
        0x00, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,     
        0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x70,     
        
        0x70, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 
        0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xE0
        }; 
    uint8_t tblval = tbl[inp >> 23];
    uint64_t res = ((uint64_t)inp << 29) + ((uint64_t)tblval << 56);
    
    /* normal numbers: tblval AND 8 is nonzero. */
    if( (tblval & 0x8) != 0 )
        return res;
        
    /* Inf/Zero: all mantissa bits are zero. */
    if( (inp & UINT32_C(0x7FFFFF)) == 0 )
        return res;
    
    /* NaN: quieten the NeN. */
    if( (inp & UINT32_C(0x7F800000)) != 0 )
        return res | UINT64_C(0x8000000000000);
    
    /* remaining cases are Denormals. */
		{
		uint32_t mskval = inp & UINT32_C(0x7FFFFFFF);
		uint32_t leadingzeroes = clz32( mskval );
		uint32_t sign = inp & UINT32_C(0x80000000);
		uint64_t xsign = (uint64_t)sign << 32;
		mskval <<= leadingzeroes;
		return ((uint64_t) mskval << 21)
			+ ((uint64_t) (UINT32_C(0x388) - leadingzeroes) << 52)
			+ xsign;
		}

    }

   


/********************************************
 float->float  narrowing conversion routines
********************************************/

/*
Conversion functions are supplied for
fp32->fp16, fp64->fp32 and fp64->fp16 conversions;

it should be noted that fp64->fp16 is not invariant with
performing a sequence of fp64->fp32 followed by fp32->fp16;
more details are given at the fp64->fp16 conversion
function itself.
*/

 



/* Conversion routine that converts from FP32 to FP16.
   It supports denormals and all rounding modes.
   If a NaN is given as input, it is quietened.
   */


sf16  _mali_sf32_to_sf16( sf32 inp, roundmode rmode )
    {
    /* for each possible sign/exponent combination, store a case index.
       This gives a 512-byte table */
    static const uint8_t tab[512] = {
        0,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
        10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
        10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
        10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
        10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
        10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
        10,10,10,10,10,20,20,20,20,20,20,20,20,20,20,20,
        20,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,
        30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,40,
        40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,
        40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,
        40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,
        40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,
        40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,
        40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,
        40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,50,
    
        5,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,25,25,25,25,25,25,25,25,25,25,25,
        25,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,
        35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,45,
        45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,
        45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,
        45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,
        45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,
        45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,
        45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,
        45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,55,
        };
    
    /* many of the cases below use a case-dependent magic constant.
       So we look up a magic constant before actually performing
       the switch. This table allows us to group cases, thereby
       minimizing code size. */
    static const uint32_t tabx[60] =
        {
        UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0x8000), UINT32_C(0x80000000), UINT32_C(0x8000), UINT32_C(0x8000), UINT32_C(0x8000),
        UINT32_C(1), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0x8000), UINT32_C(0x8001), UINT32_C(0x8000), UINT32_C(0x8000), UINT32_C(0x8000),
        UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0x8000), UINT32_C(0x8000), UINT32_C(0x8000), UINT32_C(0x8000), UINT32_C(0x8000),
        UINT32_C(0xC8001FFF), UINT32_C(0xC8000000), UINT32_C(0xC8000000), UINT32_C(0xC8000FFF), UINT32_C(0xC8001000),
        UINT32_C(0x58000000), UINT32_C(0x38001FFF), UINT32_C(0x58000000), UINT32_C(0x58000FFF), UINT32_C(0x58001000),
        UINT32_C(0x7C00), UINT32_C(0x7BFF), UINT32_C(0x7BFF), UINT32_C(0x7C00), UINT32_C(0x7C00),
        UINT32_C(0xFBFF), UINT32_C(0xFC00), UINT32_C(0xFBFF), UINT32_C(0xFC00), UINT32_C(0xFC00),
        UINT32_C(0x90000000), UINT32_C(0x90000000) ,UINT32_C(0x90000000), UINT32_C(0x90000000), UINT32_C(0x90000000),
        UINT32_C(0x20000000), UINT32_C(0x20000000), UINT32_C(0x20000000), UINT32_C(0x20000000), UINT32_C(0x20000000)
        };
    
    uint32_t p;
    uint32_t idx = rmode + tab[inp >> 23];
    uint32_t vlx = tabx[idx];
    switch( idx )
        {      
        /*
        positive number which may be Infinity or NaN.
        We need to check whether it is NaN; if it is, quieten it
        by setting the top bit of the mantissa. (If we don't do this quieting,
        then a NaN that is distinguished only by having its low-order bits set,
        would be turned into an INF.
        */
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57:
        case 58:
        case 59:
            /*
            the input value is 0x7F800000 or 0xFF800000 if it is INF.
            By subtracting 1, we get 7F7FFFFF or FF7FFFFF, that is,
            bit 23 becomes zero. For NaNs, however, this operation will
            keep bit 23 with the value 1. 
            We can then extract bit 23, and logical-OR bit 9 of the result
            with this bit in order to quieten the NaN (a Quiet NaN 
            is a NaN where the top bit of the mantissa is set.)
            */
            p = (inp-1) & UINT32_C(0x800000); /* zero if INF, nonzero if NaN. */
            return ((inp + vlx) >> 13) | (p >> 14);
        /*
        positive, exponent = 0, round-mode == UP; need to check
        whether number actually is 0. If it is, then return 0,
        else return 1 (the smallest representable nonzero number)
        */
        case 0:
            /*
            -inp will set the MSB if the input number is nonzero.
            Thus (-inp) >> 31 will turn into 0 if the input number is 0
            and 1 otherwise.
            */
            return (uint32_t)(-(int32_t)inp) >> 31;
        
        /*
        negative, exponent = , round-mode == DOWN, need to check
        whether number is actually 0. If it is, return 0x8000 ( float -0.0 )
        Else return the smallest negative number ( 0x8001 )
        */
        case 6:
            /*
            in this case 'vlx' is 0x80000000. By subtracing the input value from it,
            we obtain a value that is 0 if the input value is in fact zero and
            has the MSB set if it isn't. We then right-shift the value by 31 places
            to get a value that is 0 if the input is -0.0 and 1 otherwise.
            */
            return ((vlx-inp)>>31) + UINT32_C(0x8000);
        
        /*
        for all other cases involving underflow/overflow,
        we don't need to do actual tests; we just return 'vlx'.
        */
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
        case 48:
        case 49:
            return vlx;

        /*
        for normal numbers,
        'vlx' is the difference between the FP32 value of a number and the
        FP16 representation of the same number left-shifted by 13 places.
        In addition, a rounding constant is baked into 'vlx':
        for rounding-away-from zero, the constant is 2^13 - 1, causing
        roundoff away from zero.
        for round-to-nearest away, the constant is 2^12, causing roundoff away from zero.
        for round-to-nearest-even, the constant is 2^12 - 1. This causes correct
        round-to-nearest-even except for odd input numbers. For odd input numbers,
        we need to add 1 to the constant.
        */
        
        /* normal number, all rounding modes except round-to-nearest-even: */
        case 30:
        case 31:
        case 32:
        case 34:
        case 35:
        case 36:
        case 37:
        case 39:
            return (inp+vlx) >> 13;
            
        /* normal number, round-to-nearest-even. */
        case 33:
        case 38:
            p = inp + vlx;
            p += (inp >> 13) & 1;
            return p >> 13;
 
        /*
        the various denormal cases. These are not expected to be common,
        so their performance is a bit less important.
        For each of these cases, we need to extract an exponent and a mantissa
        (including the implicit '1'!), and then right-shift the mantissa
        by a shift-amount that depends on the exponent.
        The shift must apply the correct rounding mode.
        'vlx' is used to supply the sign of the resulting denormal number.
        */
        case 21:
        case 22:
        case 25:
        case 27:
            /* denormal, round towards zero. */
            p = 126 - ((inp >> 23) & 0xFF);
            return (((inp & UINT32_C(0x7FFFFF)) + UINT32_C(0x800000)) >> p) | vlx;
        case 20:
        case 26:
            /* denornal, round away from zero. */
            p = 126 - ((inp >> 23) & 0xFF);
            return rtup_shift32((inp & UINT32_C(0x7FFFFF)) + UINT32_C(0x800000), p) | vlx;
        case 24:
        case 29:
            /* denornal, round to nearest-away */
            p = 126 - ((inp >> 23) & 0xFF);
            return rtna_shift32((inp & UINT32_C(0x7FFFFF)) + UINT32_C(0x800000), p) | vlx;
        case 23:
        case 28:
            /* denormal, round to nearest-even. */
            p = 126 - ((inp >> 23) & 0xFF);
            return rtne_shift32((inp & UINT32_C(0x7FFFFF)) + UINT32_C(0x800000), p) | vlx;
        }   
    
    return 0;
    }




/* Conversion routine that converts from FP64 to FP32. */
sf32 _mali_sf64_to_sf32( sf64 inp, roundmode rmode )
    {
    
    /*
    in order to determine which case actually applies, we will use a 2-level table lookup.
    The first level is a 256-entry table that contains offsets into the second table.
    */
    
    static const uint8_t tab1[256] =
        {
        0, 
        1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        8,17,
        32,33,33,33,33,33,33,33,33,33,33,33,33,33,33,34,
        49,49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,49,
        50, 
        
        66, 
        67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,
        74,83,
        98,99,99,99,99,99,99,99,99,99,99,99,99,99,99,100,
        115,115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,115,
        116
        };
    
    static const uint8_t tab2[132] = {
        /* [0] */ 0, /* positive zero or underflow */
        /* [1] */ 10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10, /* positive underflow */
        /* [17] */ 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20, /* positive denormal */
        /* [33] */ 30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* positive normal */
        /* [49] */ 40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40, /* positive overflow */
        /* [65] */ 50,  /* positive infinity or NaN */
        /* [66] */ 5,
        /* [67] */ 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        /* [83] */ 25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
        /* [99] */ 35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,
        /* [115] */ 45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,
        /* [131] */ 55,
        };
    
    /*    
    many of the cases below use a case-dependent magic constant.
    So we look up a magic constant before actually performing
    the switch. This table allows us to group cases, thereby
    minimizing code size.   
    */
    static const uint64_t tabx[60] =
        {
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0x80000000), UINT64_C(0x8000000000000000), UINT64_C(0x80000000), UINT64_C(0x80000000), UINT64_C(0x80000000),
        UINT64_C(1), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0x80000000), UINT64_C(0x80000001), UINT64_C(0x80000000), UINT64_C(0x80000000), UINT64_C(0x80000000),
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0x80000000), UINT64_C(0x80000000), UINT64_C(0x80000000), UINT64_C(0x80000000), UINT64_C(0x80000000),
        UINT64_C(0xC80000001FFFFFFF), UINT64_C(0xC800000000000000), UINT64_C(0xC800000000000000), UINT64_C(0xC80000000FFFFFFF), UINT64_C(0xC800000010000000), 
        UINT64_C(0x5800000000000000), UINT64_C(0x580000001FFFFFFF), UINT64_C(0x5800000000000000), UINT64_C(0x580000000FFFFFFF), UINT64_C(0x5800000010000000), 
        UINT64_C(0x7F800000), UINT64_C(0x7F7FFFFF), UINT64_C(0x7F7FFFFF), UINT64_C(0x7F800000), UINT64_C(0x7F800000),
        UINT64_C(0xFF7FFFFF), UINT64_C(0xFF800000), UINT64_C(0xFF7FFFFF), UINT64_C(0xFF800000), UINT64_C(0xFF800000),
        UINT64_C(0x9000000000000000), UINT64_C(0x9000000000000000), UINT64_C(0x9000000000000000), UINT64_C(0x9000000000000000), UINT64_C(0x9000000000000000),
        UINT64_C(0x2000000000000000), UINT64_C(0x2000000000000000), UINT64_C(0x2000000000000000), UINT64_C(0x2000000000000000), UINT64_C(0x2000000000000000) };
    
    uint64_t p;
    uint32_t idx = tab2[ ((inp >> 52) & 0xF) + tab1[inp >> 56]] + rmode;
    uint64_t vlx = tabx[idx];
       
    switch( idx )
        {
        /*
        positive number which may be Infinity or NaN.
        We need to check whether it is NaN; if it is, quieten it
        by setting the top bit of the mantissa. (If we don't do this quieting,
        then a NaN that is distinguished only by having its low-order bits set,
        would be turned into an INF.
        */
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57:
        case 58:
        case 59:
            /*
            the input value is 0x7FF0_0000_0000_0000 or 0xFFF0_0000_0000_0000 if it is INF.
            By subtracting 1, we get 7FEF_FFFF_FFFF_FFFF or FF7FFFFF, that is,
            bit 52 becomes zero. For NaNs, however, this operation will
            keep bit 52 with the value 1. 
            We can then extract bit 52, and logical-OR bit 22 of the result
            with this bit in order to quieten the NaN (a Quiet NaN 
            is a NaN where the top bit of the mantissa is set.)
            */
            p = (inp-UINT64_C(1)) & UINT64_C(0x10000000000000); /* zero if INF, nonzero if NaN. */
            return (sf32)((inp + vlx) >> 29) | (sf32)(p >> 30);

        /*
        positive, exponent = 0, round-mode == UP; need to check
        whether number actually is 0. If it is, then return 0,
        else return 1 (the smallest representable nonzero number)
        */
        case 0:
            /*
            -inp will set the MSB if the input number is nonzero.
            Thus (-inp) >> 63 will turn into 0 if the input number is 0
            and 1 otherwise.
            */
            return (sf32)((uint64_t)(-(int64_t)inp) >> 63);

        /*
        negative, exponent = , round-mode == DOWN, need to check
        whether number is actually 0. If it is, return 0x80000000 ( float -0.0 )
        Else return the smallest negative number ( 0x80000001
        */
        case 6:
            /*
            in this case 'vlx' is 0x8000_0000_0000_0000. By subtracing the input value from it,
            we obtain a value that is 0 if the input value is in fact zero and
            has the MSB set if it isn't. We then right-shift the value by 31 places
            to get a value that is 0 if the input is -0.0 and 1 otherwise.
            */
            return (sf32)((vlx-inp)>>63) + UINT32_C(0x80000000);

        /*
        for all other cases involving underflow/overflow,
        we don't need to do actual tests; we just return 'vlx'.
        */
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
        case 48:
        case 49:
            return (sf32)vlx;

        /*
        for normal numbers,
        'vlx' is the difference between the FP64 value of a number and the
        FP32 representation of the same number left-shifted by 29 places.
        In addition, a rounding constant is baked into 'vlx':
        for rounding-away-from zero, the constant is 2^29 - 1, causing
        roundoff away from zero.
        for round-to-nearest away, the constant is 2^28, causing roundoff away from zero.
        for round-to-nearest-even, the constant is 2^28 - 1. This causes correct
        round-to-nearest-even except for odd input numbers. For odd input numbers,
        we need to add 1 to the constant.
        */

        /* normal number, all rounding modes except round-to-nearest-even: */
        case 30:
        case 31:
        case 32:
        case 34:
        case 35:
        case 36:
        case 37:
        case 39:
            return (sf32)((inp+vlx) >> 29);

        /* normal number, round-to-nearest-even. */
        case 33:
        case 38:
            p = inp + vlx;
            p += (inp >> 29) & UINT64_C(1);
            return (sf32)(p >> 29);

        /*
        the various denormal cases. These are not expected to be common,
        so their performance is a bit less important.
        For each of these cases, we need to extract an exponent and a mantissa
        (including the implicit '1'!), and then right-shift the mantissa
        by a shift-amount that depends on the exponent.
        The shift must apply the correct rounding mode.
        'vlx' is used to supply the sign of the resulting denormal number.
        */
        case 21:
        case 22:
        case 25:
        case 27:
            /* denormal, round towards zero. */
            p = 926 - ((inp >> 52) & 0x7FF);
            return (sf32)((((inp & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000)) >> p) | vlx);
        case 20:
        case 26:
            /* denornal, round away from zero. */
            p = 926 - ((inp >> 52) & 0x7FF);
            return (sf32)(rtup_shift64((inp & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000), (int32_t)p) | vlx);
        case 24:
        case 29:
            /* denornal, round to nearest-away. */
            p = 926 - ((inp >> 52) & 0x7FF);
            return (sf32)(rtna_shift64((inp & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000), (int32_t)p) | vlx);
        case 23:
        case 28:
            /* denormal, round to nearest-even. */
            p = 926 - ((inp >> 52) & 0x7FF);
            return (sf32)(rtne_shift64((inp & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000), (int32_t)p) | vlx);
        }
    
    return 0;
    }



/* routine to convert an FP64 value to FP16.
   Note that this is NOT invariant with converting from FP64 to FP32,
   and then from there to FP16.

   One counterexample is the value:
     P = 1.0 + pow(2, -11) + pow(2, -25)
   in the Round-to-nearest-even rounding mode.
     
   The two nearest representable FP16 values are:
     P_low = 1.0
     P_high = 1.0 + pow(2, -10)
   As P is closer to P_high than P_low, the correct behavior is to round to P_high.
   
   Now, the two nearest representable FP32 values are:
     P_low' = 1.0 + pow(2,-11)
     P_high' = 1.0 + pow(2,-11) + pow(2,-23)
   In this case, we must round to P_low', resulting in
     P' = 1.0 + pow(2,-11)
   However, if we round this value from FP32 to FP16, we get that P'
   is equally far from both P_low and P_high; in this case, round-to-nearest-even
   causes us to round to P_low.
*/


sf16 _mali_sf64_to_sf16( sf64 inp, roundmode rmode )
    {
    
    static const uint8_t tab1[256] = {
        0, /* exponent values from 0 to 15 */
        /*  61 '1's, representing exponents ranging from 16 to 991. */
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,
        12, /* 992 to 1007; last 11 entries are Denormals in FP16 */
        32, /* 1008 to 1023: first entry is Denormal in FP16, last 15 entries are Normal */
        34, /* 1024 to 1039: first 15 entries are Normal, last is Overflow */
        /*  62 * the '49' entry; exponents ranging from 1040 to 2032 are all overflow */
        49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,
        49,49,49,49,49,49,49,49,49,49,49,49,
        50, /* first 15 entries are overflow, the last one is Infinity. */

        /* the second half ofd the table just repeats data, with a +66 offset */
        66, /* exponent values from 0 to 15 */
        /* 61 '1's, representing exponents ranging from 16 to 991. */
        67,67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,67,
        67,67,67,67,67,67,67,67,67,67,67,
        78, /* 992 to 1007; last 11 entries are Denormals in FP16 */
        98, /* 1008 to 1023: first entry is Denormal in FP16, last 15 entries are Normal */
        100, /* 1024 to 1039: first 15 entries are Normal, last is Overflow */
        /* 62 * the '115' entry; exponents ranging from 1040 to 2032 are all overflow */
        115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,
        115,115,115,115,115,115,115,115,115,115,115,115,
        116, /* first 15 entries are overflow, the last one is Infinity. */
        };        


    /*
    magic constants table, similar in scope to the one for sf64_to_sf32, but with
    different actual values.
    */
    
    static const uint64_t tabx[60] = {
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0x8000), UINT64_C(0x8000000000000000), UINT64_C(0x8000), UINT64_C(0x8000), UINT64_C(0x8000),
        UINT64_C(1), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0x8000), UINT64_C(0x8001), UINT64_C(0x8000), UINT64_C(0x8000), UINT64_C(0x8000),
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0x8000), UINT64_C(0x8000), UINT64_C(0x8000), UINT64_C(0x8000), UINT64_C(0x8000),
        UINT64_C(0xC10003FFFFFFFFFF), UINT64_C(0xC100000000000000), UINT64_C(0xC100000000000000), UINT64_C(0xC10001FFFFFFFFFF), UINT64_C(0xC100020000000000),
        UINT64_C(0x4300000000000000), UINT64_C(0x430003FFFFFFFFFF), UINT64_C(0x4300000000000000), UINT64_C(0x430001FFFFFFFFFF), UINT64_C(0x4300020000000000),
        UINT64_C(0x7C00), UINT64_C(0x7BFF), UINT64_C(0x7BFF), UINT64_C(0x7C00), UINT64_C(0x7C00),
        UINT64_C(0xFBFF), UINT64_C(0xFC00), UINT64_C(0xFBFF), UINT64_C(0xFC00), UINT64_C(0xFC00),
        UINT64_C(0x8200000000000000), UINT64_C(0x8200000000000000), UINT64_C(0x8200000000000000), UINT64_C(0x8200000000000000), UINT64_C(0x8200000000000000),
        UINT64_C(0x0400000000000000), UINT64_C(0x0400000000000000), UINT64_C(0x0400000000000000), UINT64_C(0x0400000000000000), UINT64_C(0x0400000000000000) };
        
        
    /* this table is the same as for sf64_to_sf32 */
    static const uint8_t tab2[132] = {
        /* [0] */ 0, /* positive zero or underflow */
        /* [1] */ 10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10, /* positive underflow */
        /* [17] */ 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20, /* positive denormal */
        /* [33] */ 30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* positive normal */
        /* [49] */ 40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40, /* positive overflow */
        /* [65] */ 50,  /* positive infinity or NaN */
        /* [66] */ 5,
        /* [67] */ 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        /* [83] */ 25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
        /* [99] */ 35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,
        /* [115] */ 45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,
        /* [131] */ 55,
        };
                        
    /* the table lookup algorithm is the same as for sf64_to_sf32 */
    uint64_t p;
    uint32_t idx = tab2[ ((inp >> 52) & 0xF) + tab1[inp >> 56]] + rmode;
    uint64_t vlx = tabx[idx];

    switch( idx )
        {
        /*
        positive number which may be Infinity or NaN.
        We need to check whether it is NaN; if it is, quieten it
        by setting the top bit of the mantissa. (If we don't do this quieting,
        then a NaN that is distinguished only by having its low-order bits set,
        would be turned into an INF.
        */
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 56:
        case 57:
        case 58:
        case 59:
            /*
            the input value is 0x7FF0_0000_0000_0000 or 0xFFF0_0000_0000_0000 if it is INF.
            By subtracting 1, we get 7FEF_FFFF_FFFF_FFFF or FF7FFFFF, that is,
            bit 52 becomes zero. For NaNs, however, this operation will
            keep bit 52 with the value 1. 
            We can then extract bit 52, and logical-OR bit 22 of the result
            with this bit in order to quieten the NaN (a Quiet NaN 
            is a NaN where the top bit of the mantissa is set.)
            */
            p = (inp-UINT64_C(1)) & UINT64_C(0x10000000000000); /* zero if INF, nonzero if NaN. */
            return (sf16)(((inp + vlx) >> 42) | (p >> 43));

        /*
        positive, exponent = 0, round-mode == UP; need to check
        whether number actually is 0. If it is, then return 0,
        else return 1 (the smallest representable nonzero number)
        */
        case 0:
            /*
            -inp will set the MSB if the input number is nonzero.
            Thus (-inp) >> 63 will turn into 0 if the input number is 0
            and 1 otherwise.
            */
            return (sf16)((uint64_t)(-(int64_t)inp) >> 63);

        /*
        negative, exponent = , round-mode == DOWN, need to check
        whether number is actually 0. If it is, return 0x80000000 ( float -0.0 )
        Else return the smallest negative number ( 0x80000001
        */
        case 6:
            /*
            in this case 'vlx' is 0x8000_0000_0000_0000. By subtracing the input value from it,
            we obtain a value that is 0 if the input value is in fact zero and
            has the MSB set if it isn't. We then right-shift the value by 31 places
            to get a value that is 0 if the input is -0.0 and 1 otherwise.
            */
            return (sf16)(((vlx-inp)>>63) + UINT32_C(0x8000));

        /*
        for all other cases involving underflow/overflow,
        we don't need to do actual tests; we just return 'vlx'.
        */
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
        case 48:
        case 49:
            return (sf16)vlx;

        /*
        for normal numbers,
        'vlx' is the difference between the FP64 value of a number and the
        FP16 representation of the same number left-shifted by 42 places.
        In addition, a rounding constant is baked into 'vlx':
        for rounding-away-from zero, the constant is 2^42 - 1, causing
        roundoff away from zero.
        for round-to-nearest away, the constant is 2^41, causing roundoff away from zero.
        for round-to-nearest-even, the constant is 2^41 - 1. This causes correct
        round-to-nearest-even except for odd input numbers. For odd input numbers,
        we need to add 1 to the constant.
        */

        /* normal number, all rounding modes except round-to-nearest-even: */
        case 30:
        case 31:
        case 32:
        case 34:
        case 35:
        case 36:
        case 37:
        case 39:
            return (sf16)((inp+vlx) >> 42);

        /* normal number, round-to-nearest-even. */
        case 33:
        case 38:
            p = inp + vlx;
            p += (inp >> 42) & UINT64_C(1);
            return (sf16)(p >> 42);

 
        /*
        the various denormal cases. These are not expected to be common,
        so their performance is a bit less important.
        For each of these cases, we need to extract an exponent and a mantissa
        (including the implicit '1'!), and then right-shift the mantissa
        by a shift-amount that depends on the exponent.
        The shift must apply the correct rounding mode.
        'vlx' is used to supply the sign of the resulting denormal number.
        */
        case 21:
        case 22:
        case 25:
        case 27:
            /* denormal, round towards zero. */
            p = 1051 - ((inp >> 52) & 0x7FF);
            return (sf16)((((inp & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000)) >> p) | vlx);
        case 20:
        case 26:
            /* denornal, round away from zero. */
            p = 1051 - ((inp >> 52) & 0x7FF);
            return (sf16)(rtup_shift64((inp & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000), (int32_t)p) | vlx);
        case 24:
        case 29:
            /* denornal, round to nearest-away */
            p = 1051 - ((inp >> 52) & 0x7FF);
            return (sf16)(rtna_shift64((inp & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000), (int32_t)p) | vlx);
        case 23:
        case 28:
            /* denormal, round to nearest-even. */
            p = 1051 - ((inp >> 52) & 0x7FF);
            return (sf16)(rtne_shift64((inp & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000), (int32_t)p) | vlx);
        }
  
    return 0;
    }
    

   






/*****************************************
 floating-point round-to-integer routines
*****************************************/



/*
32-bit round-to-integer.

The Round-to-integer operation subdivides into cases as follows:

 * Input value is less than 1.0. In this case, we must return 0 or 1;
   we compare the input value against a round-mode dependent constant to choose.
 * Input value is 2^23 or greater. In this case, we just return the input vaoue unchanged.
 * Other input values:
    - add a round-mode dependent value
    - then AND with an AND-mask.

The output of the round-to-integer routines in this library is known to
disagree with glibc's rint() implementation; this is a confirmed glibc bug.

*/ 
    
    
sf32  _mali_round_sf32( sf32 inp, roundmode rmode )
    {
    
    uint32_t ainp = inp & UINT32_C(0x7FFFFFFF);
    uint32_t expo = ainp >> 23;
    uint32_t tabidx = (inp >> 31) + (rmode << 1);  
    uint32_t shamt = 149 - expo;
    uint32_t ANDmask;
    int32_t sANDmask;
    
    if( shamt >= 23 )
        {
        /* first subcase: check if the absolute value of the input value is less than 1.0 */
        if( shamt < UINT32_C(0x80000000) )
            {
            /*
            in this case, the input value has an absolute value less than 1.0
            The result is then either 0.0 or 1.0; we determine which one it is by
            comparing the absolute value against a rounding-mode dependent constant. 
            */
            uint32_t trh_table[10] =
                {
                UINT32_C(0x00000000), /* round up, positive */
                UINT32_C(0x7FFFFFFF), /* round up, negative */
                UINT32_C(0x7FFFFFFF), /* round down, positive */
                UINT32_C(0x00000000), /* round down, negative */
                UINT32_C(0x7FFFFFFF), /* truncate, positive */
                UINT32_C(0x7FFFFFFF), /* truncate, negative */
                UINT32_C(0x3F000000), /* round-to-nearest-even, positive */
                UINT32_C(0x3F000000), /* round-to-nearest-even, negative */
                UINT32_C(0x3EFFFFFF), /* round-to-nearest-away, positive */
                UINT32_C(0x3EFFFFFF), /* round-to-nearest-away, negative */
                };
            uint32_t res = ainp > trh_table[tabidx] ? 0x3F800000 : 0;
            res |= inp & 0x80000000;
            return res;
            }
        else
        /* second subcase: is the absolute value of the input value 2^23 or greater. */
            {
            /*
            in this case, the input value has an absolute value of 2^23 or greater
            or else is a NaN; quieten the input value if it is a NaN, else return
            it unchanged.
            */
            return (ainp > UINT32_C(0x7F800000)) 
                ? (inp | UINT32_C(0x400000))
                : inp;
            }
        }
                
    ANDmask = (~1) << shamt;
    sANDmask = (int32_t)ANDmask;
    switch( tabidx )
        {
        /* round away from zero: */
        case 0: /* round up, positive */
        case 3: /* round down, negative */
            inp += ~ANDmask;
            break;

        /* round towards zero: */
        case 1: /* round up, negative */
        case 2: /* round down, positive */
        case 4: /* truncate, positive */
        case 5: /* truncate, negative */
            break;
                   
        /* round to nearest-even: */
        case 6:
        case 7:
            /*
            If the integer fraction of the number is an even number, then
            we subtract 1 ULP; after this, we perform the same computation as if
            we perform round-to-nearest-away.
            */
            inp -= ~( inp >> (shamt + 1)) & 1;
            /* no break */
        
        /* round to nearest-away: */
        case 8:
        case 9:
            inp -= (sANDmask >> 1);
            break;
        }
    
    return inp & ANDmask;
    }



/*
the round_sf16() function.
It works in largely the same manner as the sf32_round() function above; only
a few datatypes and hardcoded constants are changed.
*/


sf16  _mali_round_sf16( sf16 inp, roundmode rmode )
    {    
    uint32_t ainp = inp & 0x7FFF;
    uint32_t expo = ainp >> 10;
    uint32_t tabidx = (inp >> 15) + (rmode << 1);  
    uint32_t shamt = 24 - expo;
    uint16_t ANDmask;
    int16_t sANDmask;
    
    if( shamt >= 10 )
        {
        /* first subcase: check the absolute value of the input value is less than 1.0 */
        if( shamt < UINT32_C(0x80000000) )
            {
            /*
            in this case, the input value has an absolute value less than 1.0
            The result is then either 0.0 or 1.0; we determine which one it is by
            comparing the absolute value against a rounding-mode dependent constant. 
            */
            uint16_t trh_table[10] =
                {
                0x0000U, /* round up, positive */
                0x7FFFU, /* round up, negative */
                0x7FFFU, /* round down, positive */
                0x0000U, /* round down, negative */
                0x7FFFU, /* truncate, positive */
                0x7FFFU, /* truncate, negative */
                0x3800U, /* round-to-nearest-even, positive */
                0x3800U, /* round-to-nearest-even, negative */
                0x37FFU, /* round-to-nearest-away, positive */
                0x37FFU, /* round-to-nearest-away, negative */
                };
            uint16_t res = ainp > trh_table[tabidx] ? 0x3C00U : 0;
            res |= inp & 0x8000U;
            return res;
            }
        else
        /* second subcase: the absolute value of the input value is 2^10 or greater. */
            {
            /* in this case, the input value has an absolute value of 2^10 or greater */
            /* or else is a NaN; if it is a NaN, quieten it, else return its input unchanged. */
            return (ainp > 0x7C00U) 
                ? (inp | 0x200U)
                : inp;             
            }
        }
                
    ANDmask = (~1) << shamt;
    sANDmask = (int16_t)ANDmask;
    switch( tabidx )
        {
        /* round away from zero: */
        case 0: /* round up, positive */
        case 3: /* round down, negative */
            inp += ~ANDmask;
            break;

        /* round towards zero: */
        case 1: /* round up, negative */
        case 2: /* round down, positive */
        case 4: /* truncate, positive */
        case 5: /* truncate, negative */
            break;
                   
        /* round to nearest-even: */
        case 6:
        case 7:
            /* 
            If the integer fraction of the number is an even number, then
            we subtract 1 ULP; after this, we perform the same computation as if
            we perform round-to-nearest-away. */
            inp -= ~( inp >> (shamt + 1)) & 1;
            /* no break */
        
        /* round to nearest-away: */
        case 8:
        case 9:
            inp -= (sANDmask >> 1);
            break;
        }
    
    return inp & ANDmask;
    }



/* the round_sf64() function.
  identical with the round_sf32() function above, except that a few constants are modified.
*/
sf64  _mali_round_sf64( sf64 inp, roundmode rmode )
    {
    
    uint64_t ainp = inp & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint32_t expo = (uint32_t)(ainp >> 52);
    uint32_t tabidx = (uint32_t)(inp >> 63) + (rmode << 1);  
    uint32_t shamt = 1074 - expo;
	uint64_t ANDmask;
    int64_t sANDmask;
   
    if( shamt >= 52 )
        {
        /* first subcase: check if the absolute value of the input value is less than 1,0 */
        if( shamt < UINT32_C(0x80000000) )
            {
            /*
            in this case, the input value has an absolute value less than 1.0
            The result is then either 0.0 or 1.0; we determine which one it is by
            comparing the absolute value against a rounding-mode dependent constant. 
            */
            static const uint64_t trh_table[10] =
                {
                UINT64_C(0x0000000000000000), /* round up, positive */
                UINT64_C(0x7FFFFFFFFFFFFFFF), /* round up, negative */
                UINT64_C(0x7FFFFFFFFFFFFFFF), /* round down, positive */
                UINT64_C(0x0000000000000000), /* round down, negative */
                UINT64_C(0x7FFFFFFFFFFFFFFF), /* truncate, positive */
                UINT64_C(0x7FFFFFFFFFFFFFFF), /* truncate, negative */
                UINT64_C(0x3FE0000000000000), /* round-to-nearest-even, positive */
                UINT64_C(0x3FE0000000000000), /* round-to-nearest-even, negative */
                UINT64_C(0x3FDFFFFFFFFFFFFF), /* round-to-nearest-away, positive */
                UINT64_C(0x3FDFFFFFFFFFFFFF), /* round-to-nearest-away, negative */
                };
            uint64_t res = ainp > trh_table[tabidx] ? UINT64_C(0x3FF0000000000000) : 0;
            res |= inp & UINT64_C(0x8000000000000000);
            return res;
            }
        else
        /* second subcase: if the absolute value of the input value 2^52 or greater */
            {
            /*
            in this case, the input value has an absolute value of 2^52 or greater
            or else is a NaN; If it is a NaN, quieten it, else leave it unchanged.
            */
            return (ainp > UINT64_C(0x7FF0000000000000))
                ? (inp | UINT64_C(0x8000000000000))
                : inp;
            }
        }
                
    ANDmask = (~UINT64_C(1)) << shamt;
    sANDmask = (int64_t)ANDmask;
    switch( tabidx )
        {
        /* round away from zero: */
        case 0: /* round up, positive */
        case 3: /* round down, negative */
            inp += ~ANDmask;
            break;

        /* round towards zero: */
        case 1: /* round up, negative */
        case 2: /* round down, positive */
        case 4: /* truncate, positive */
        case 5: /* truncate, negative */
            break;
                   
        /* round to nearest-even: */
        case 6:
        case 7:
            /*
            If the integer fraction of the number is an even number, then
            we subtract 1 ULP; after this, we perform the same computation as if
            we perform round-to-nearest-away.
            */
            inp -= ~( inp >> (shamt + 1)) & UINT64_C(1);
        
        /* round to nearest-away: */
        case 8:
        case 9:
            inp -= (sANDmask >> 1);
            break;
        }
    
    return inp & ANDmask;
    }



/************************************************
 Floating-point conversion to integer routines
************************************************/

/*
signed 16-bit convert to integer.
*/


int16_t _mali_sf16_to_s16( sf16 inp, roundmode rm )
    {
    uint16_t msk = (int16_t)inp >> 15; /* zero if positive, 0xFFFF if negative */
    uint32_t idx = (rm << 1) + (inp >> 15);
    
    uint32_t ainp = inp & UINT32_C(0x7FFF);
    uint32_t shamt = 29 - (ainp >> 10);

    if( shamt >= 15 )
        {
        if( ainp >= UINT32_C(0x7800) )
            {
            /* overflow. return 0 if NaN, else a clamped value. */
            int32_t binp = ainp - UINT32_C(0x7C01); /* negative unless NaN */
            binp >>= 31; /* all 1s for a non-NaN value, all 0s for NaN. */
            return (UINT32_C(0x7FFF) ^ msk) & binp;
            } else {
			/*
			  remaining cases are underflow: the input value has an absolute value
			  less than 1.
			*/
			static const uint32_t tbl1[10] =
				{
				UINT32_C(0x0000),
				UINT32_C(0x7FFF),
				UINT32_C(0x7FFF),
				UINT32_C(0x0000),
				UINT32_C(0x7FFF),
				UINT32_C(0x7FFF),
				UINT32_C(0x3800),
				UINT32_C(0x3800),
				UINT32_C(0x37FF),
				UINT32_C(0x37FF)
				};
			uint32_t vl = tbl1[idx] - ainp; /* MSB = 1 if ainp is greater than the table entry */
			vl >>= 31; /* result is 1 if ainp is greater than table entry. */
			vl ^= msk; /* swap the sign of vl if input was negative. */
			vl -= msk; 
			return vl;
			}
        } else {
    
		/*
		  round-up mode dependent lookup table. The values need to be right-shifted
		  by 31 minus the shift amount.
		*/
		static const uint32_t tbl2[10] =
			{ 
			0x7FFE, /* positive, round UP */
			0x0000, /* negative, round UP */
			0x0000, /* positive, round down */
			0x7FFE, /* negative, round down */
			0x0000, /* positive, round to zero */
			0x0000, /* negative, round to zero */
			0x3FFF, /* positive, round to nearest even */
			0x3FFF, /* negative, round to nearest even */
			0x4000, /* positive, round to nearest away */
			0x4000, /* negative, round to nearest away */
			};

		/*
		  extract the mantissa of the input number, to an integer
		  in the [2^14, 2^15) range
		*/
		uint32_t mant = ((inp << 4) & UINT32_C(0x3FFF)) + UINT32_C(0x4000);
		
		/*
		  add 1 ULP to the addition offset if round-to-nearest-even and the input number is even.
		  Note that some of the entries in the 'tbl2' LUT above has 0 in their lowest bit, so that
		  'tbo' can be added to them and not affect the value they will have after right-shift.
		*/
		uint32_t tbo = (mant >> shamt) & 1;
		
		/*
		  add and offset of 0, 0.5-ulp, 0.5 or 1-ulp dependent on rounding mode.
		  The offset needs to be right-shfited according to round-mode. 
		*/
		mant += (tbl2[idx] + tbo) >> (15-shamt);
		
		mant >>= shamt;
		mant ^= msk;
		mant -= msk;
		
		return mant;
        }
	}


/*
signed 32-bit convert to integer.
*/

int32_t _mali_sf32_to_s32( sf32 inp, roundmode rm )
    {
    uint32_t msk = (int32_t)inp >> 31; /* zero if positive, all 1s (numeric -1) if negative. */
    uint32_t idx = (rm << 1) - msk;
    
    uint32_t ainp = inp & UINT32_C(0x7FFFFFFF);
    uint32_t shamt = 157 - (ainp >> 23);

    if( shamt >= 31 )
        {
        if( ainp >= UINT32_C(0x4F000000) )
            {
            /* overflow. return 0 if NaN, else a clamped value. */
            int32_t binp = ainp - UINT32_C(0x7F800001); /* negative unless NaN */
            binp >>= 31; /* all 1s for a non-NaN value, all 0s for NaN. */
            return (UINT32_C(0x7FFFFFFF) ^ msk) & binp;
            } else {
			/*
			  remaining cases are underflow: the input value has an absolute value
			  less than 1.
			*/
			static const uint32_t tbl1[10] =
				{
					UINT32_C(0x00000000),
					UINT32_C(0x7FFFFFFF),
					UINT32_C(0x7FFFFFFF),
					UINT32_C(0x00000000),
					UINT32_C(0x7FFFFFFF),
					UINT32_C(0x7FFFFFFF),
					UINT32_C(0x3F000000),
					UINT32_C(0x3F000000),
					UINT32_C(0x3EFFFFFF),
					UINT32_C(0x3EFFFFFF)
				};
			uint32_t vl = tbl1[idx] - ainp; /* MSB = 1 if ainp is greater than the table entry */
			vl >>= 31; /* result is 1 if ainp is greater than table entry. */
			vl ^= msk; /* swap the sign of vl if input was negative. */
			vl -= msk; 
			return vl;
			}
        } else {
    
		/*
		  rounding mode dependent lookup table. The values need to be right-shifted
		  by 31 minus the shift amount.
		*/
		static const uint32_t tbl2[10] =
			{ 
				UINT32_C(0x7FFFFFFE), /* positive, round UP */
				UINT32_C(0x00000000), /* negative, round UP */
				UINT32_C(0x00000000), /* positive, round down */
				UINT32_C(0x7FFFFFFE), /* negative, round down */
				UINT32_C(0x00000000), /* positive, round to zero */
				UINT32_C(0x00000000), /* negative, round to zero */
				UINT32_C(0x3FFFFFFF), /* positive, round to nearest even */
				UINT32_C(0x3FFFFFFF), /* negative, round to nearest even */
				UINT32_C(0x40000000), /* positive, round to nearest away */
				UINT32_C(0x40000000), /* negative, round to nearest away */
			};


		/*
		  extract the mantissa of the input number, to an integer
		  in the [2^30, 2^31) range
		*/
		uint32_t mant = ((inp << 7) & UINT32_C(0x3FFFFFFF)) + UINT32_C(0x40000000);
    
		/* 
		   add 1 ULP to the addition offset if round-to-nearest-even and the input number is even.
		   Note that some of the entries in the 'tbl2' LUT above has 0 in their lowest bit, so that
		   'tbo' can be added to them and not affect the value they will have after right-shift.
		*/
		uint32_t tbo = (mant >> shamt) & 1;

		/*
		  add an offset of 0, 0.5-ulp, 0.5 or 1-ulp dependent on rounding mode.
		  The offset needs to be right-shfited according to round-mode. 
		*/
		mant += (tbl2[idx] + tbo) >> (31-shamt);
		
		mant >>= shamt;
		mant ^= msk; /* The XOR and subtract here are used to conditionally negate the number without a branch. */
		mant -= msk;
		
		return mant;
		}
    }


/*
signed 64-bit convert to integer.
*/

int64_t _mali_sf64_to_s64( sf64 inp, roundmode rm )
    {
    uint64_t msk = (int64_t)inp >> 63; /* zero if positive, all 1s (numeric -1) if negative. */
    uint32_t idx = (uint32_t)((rm << 1) - msk);
    
    uint64_t ainp = inp & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint32_t shamt = 1085 - (uint32_t)(ainp >> 52);

    if( shamt >= 63 )
        {
        if( ainp >= UINT64_C(0x43E0000000000000) )
            {
            /* overflow. return 0 if NaN, else a clamped value. */
            int64_t binp = ainp - UINT64_C(0x7FF0000000000001); /* negative unless NaN */
            binp >>= 63; /* all 1s for a non-NaN value, all 0s for NaN. */
            return (UINT64_C(0x7FFFFFFFFFFFFFFF) ^ msk) & binp;
            } else {
			/*
			  remaining cases are underflow: the input value has an absolute value
			  less than 1.
			*/
			static const uint64_t tbl1[10] =
				{
					UINT64_C(0x0000000000000000),
					UINT64_C(0x7FFFFFFFFFFFFFFF),
					UINT64_C(0x7FFFFFFFFFFFFFFF),
					UINT64_C(0x0000000000000000),
					UINT64_C(0x7FFFFFFFFFFFFFFF),
					UINT64_C(0x7FFFFFFFFFFFFFFF),
					UINT64_C(0x3FE0000000000000),
					UINT64_C(0x3FE0000000000000),
					UINT64_C(0x3FDFFFFFFFFFFFFF),
					UINT64_C(0x3FDFFFFFFFFFFFFF)
				};
			uint64_t vl = tbl1[idx] - ainp; /* MSB = 1 if ainp is greater than the table entry */
			vl >>= 63; /* result is 1 if ainp is greater than table entry. */
			vl ^= msk; /* swap the sign of vl if input was negative. */
			vl -= msk; 
			return vl;
			}
        } else {
    
		/*
		  round-up mode dependent lookup table. The values need to be right-shifted
		  by 31 minus the shift amount.
		*/
		static const uint64_t tbl2[10] =
			{ 
				UINT64_C(0x7FFFFFFFFFFFFFFE), /* positive, round UP */
				UINT64_C(0x0000000000000000), /* negative, round UP */
				UINT64_C(0x0000000000000000), /* positive, round down */
				UINT64_C(0x7FFFFFFFFFFFFFFE), /* negative, round down */
				UINT64_C(0x0000000000000000), /* positive, round to zero */
				UINT64_C(0x0000000000000000), /* negative, round to zero */
				UINT64_C(0x3FFFFFFFFFFFFFFF), /* positive, round to nearest even */
				UINT64_C(0x3FFFFFFFFFFFFFFF), /* negative, round to nearest even */
				UINT64_C(0x4000000000000000), /* positive, round to nearest away */
				UINT64_C(0x4000000000000000), /* negative, round to nearest away */
			};

		/*
		  extract the mantissa of the input number, to an integer
		  in the [2^30, 2^31) range
		*/
		uint64_t mant = ((inp << 10) & UINT64_C(0x3FFFFFFFFFFFFFFF)) + UINT64_C(0x4000000000000000);
    
		/*
		  add 1 ULP to the addition offset if round-to-nearest-even and the input number is even.
		  Note that some of the entries in the 'tbl2' LUT above has 0 in their lowest bit, so that
		  'tbo' can be added to them and not affect the value they will have after right-shift.
		*/
		uint64_t tbo = (mant >> shamt) & 1;

		/*
		  add an offset of 0, 0.5-ulp, 0.5 or 1-ulp dependent on rounding mode.
		  The offset needs to be right-shfited according to round-mode. 
		*/
		mant += (tbl2[idx] + tbo) >> (63-shamt);

		mant >>= shamt;
		mant ^= msk;
		mant -= msk;
    
		return mant;
		}
    }



/*
convert FP16 to unsigned INT16
*/

uint16_t _mali_sf16_to_u16( sf16 inp, roundmode rm )
    {
    uint32_t shamt = 29 - (inp >> 10);
    if( shamt > 15 )
        {
        static const uint32_t tbl1[5] =
            {
            UINT32_C(0x0000),
            UINT32_C(0x7FFF),
            UINT32_C(0x7FFF),
            UINT32_C(0x3800),
            UINT32_C(0x37FF),
            };
        if( inp >= 0x7C00 )
            /* overflow, NaN or negative number. */
            return inp >= 0x7C01 ? 0 : 0xFFFF;
        
        else if( inp >= 0x7800 )
            /* number between 2^15 and 2^16 */
            return (inp << 5) | 0x8000;
        
        /* remaining cases are underflow. */
        return (tbl1[rm] - inp) >> 31;
        } else {
    
		/*
		  rounding mode dependent lookup table. The values need to be right-shifted
		  by 31 minus the shift amount.
		*/
		static const uint32_t tbl2[5] =
			{ 
				UINT32_C(0x7FFE), /* round UP */
				UINT32_C(0x0000), /* round down */
				UINT32_C(0x0000), /* round to zero */
				UINT32_C(0x3FFF), /* round to nearest even */
				UINT32_C(0x4000), /* round to nearest away */
			};
		uint32_t mant = ((inp << 4) & UINT32_C(0x3FFF)) + 0x4000;
		uint32_t tbo = (mant >> shamt) & 1;
		mant += (tbl2[rm] + tbo) >> (15-shamt);
		mant >>= shamt;
		return mant;
		}
    }




/*
convert FP32 to unsigned INT32
*/

uint32_t _mali_sf32_to_u32( sf32 inp, roundmode rm )
    {
    uint32_t shamt = 157 - (inp >> 23);
    if( shamt > 31 )
        {
        static const uint32_t tbl1[5] =
            {
            UINT32_C(0x00000000),
            UINT32_C(0x7FFFFFFF),
            UINT32_C(0x7FFFFFFF),
            UINT32_C(0x3F000000),
            UINT32_C(0x3EFFFFFF),
            };
        if( inp >= UINT32_C(0x4F800000) )
            /* overflow, NaN or negative number */
            return inp >= UINT32_C(0x7F800001) ? UINT32_C(0) : UINT32_C(0xFFFFFFFF);
        
        else if( inp >= UINT32_C(0x4F000000) )
            /* number between 2^31 and 2^32 */
            return (inp << 8) | UINT32_C(0x80000000);
        
        /* remaining cases are underflow. */
        return (tbl1[rm] - inp) >> 31;
        } else {
    
		/*
		  rounding mode dependent lookup table. The values need to be right-shifted
		  by 31 minus the shift amount.
		*/
		static const uint32_t tbl2[5] =
			{ 
			UINT32_C(0x7FFFFFFE), /* round UP */
			UINT32_C(0x00000000), /* round down */
			UINT32_C(0x00000000), /* round to zero */
			UINT32_C(0x3FFFFFFF), /* round to nearest even */
			UINT32_C(0x40000000), /* round to nearest away */
			};
		uint32_t mant = ((inp << 7) & UINT32_C(0x3FFFFFFF)) + 0x40000000;
		uint32_t tbo = (mant >> shamt) & 1;
		mant += (tbl2[rm] + tbo) >> (31-shamt);
		mant >>= shamt;
		return mant;
	    }
    }



/*
convert FP64 to unsigned INT64
*/

uint64_t _mali_sf64_to_u64( sf64 inp, roundmode rm )
    {
    uint32_t shamt = 1085 - (uint32_t)(inp >> 52);
    if( shamt > 63 )
        {
        static const uint64_t tbl1[5] =
            {
            UINT64_C(0x0000000000000000),
            UINT64_C(0x7FFFFFFFFFFFFFFF),
            UINT64_C(0x7FFFFFFFFFFFFFFF),
            UINT64_C(0x3FE0000000000000),
            UINT64_C(0x3FDFFFFFFFFFFFFF),
            };
        if( inp >= UINT64_C(0x43F0000000000000) )
            /* overflow, NaN or negative number. */
            return inp >= UINT64_C(0x7FF0000000000001) ? UINT64_C(0) : UINT64_C(0xFFFFFFFFFFFFFFFF);
        
        else if( inp >= UINT64_C(0x43E0000000000000) )
            /* number between 2^62 and 2^63 */
            return (inp << 11) | UINT64_C(0x8000000000000000);
        
        /* remaining cases are underflow. */
        return (tbl1[rm] - inp) >> 63;
        } else {
    
		/*
		  rounding mode dependent lookup table. The values need to be right-shifted
		  by 31 minus the shift amount.
		*/
		static const uint64_t tbl2[5] =
			{ 
			UINT64_C(0x7FFFFFFFFFFFFFFE), /* round UP */
			UINT64_C(0x0000000000000000), /* round down */
			UINT64_C(0x0000000000000000), /* round to zero */
			UINT64_C(0x3FFFFFFFFFFFFFFF), /* round to nearest even */
			UINT64_C(0x4000000000000000), /* round to nearest away */
			};
		uint64_t mant = ((inp << 10) & UINT64_C(0x3FFFFFFFFFFFFFFF)) + UINT64_C(0x4000000000000000);
		uint64_t tbo = (mant >> shamt) & 1;
		mant += (tbl2[rm] + tbo) >> (63-shamt);
		mant >>= shamt;
		return mant;
		}
    }





/**********************************************
  Conversion from integer to floating-point
**********************************************/




/* conversion from integer to float. Rounding-mode applies to this conversion as well.

we do this as follows:
 1: convert the input number to unsigned, keeping the sign bit for later.
 2: perform bit-scan
 3: perform left-shift
 4: pick a constant to add, dependent on round-mode and sign. This constant
    corresponds to 0, 0.5 or 1 ulps in the output format.
 5: add 1 to this constant if round-to-nearest-even and the input number is odd.
 6: handle overflow in the addition
 7: assemble the final number.

*/



sf16 _mali_u16_to_sf16( uint16_t inp, roundmode rm )
    {
    static const uint32_t tbl1[5] =
        {
        UINT32_C(0x1F0000), /* up */
        UINT32_C(0x000000), /* down */
        UINT32_C(0x000000), /* trunc */
        UINT32_C(0x0F0000), /* nearest-even */
        UINT32_C(0x100000), /* nearest-away */
        };
    
    static const uint32_t tbl2[5] =
        { UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0x10000), UINT32_C(0) };
    uint32_t inpx;
    uint32_t lz;
    uint32_t vl;
    uint32_t negexpo;
    uint32_t addvl;
    uint32_t vl2;

    if( inp == 0 ) return 0; /* special case. */
    inpx = inp;
    
    /*
    count leading zeroes and normalize the input number.
    Note that since we are doing 32-bit operations on datatypes that are
    inherently 16-bit, we will be doing our operations in the upper 16 bits.
    */
    lz = clz32( inpx );
    vl = inpx << lz;
    negexpo = lz - (65536+45); /* 1 minus exponent */
    
    /* value that we need to add to 'vl' in order to obtain correct rounding. */
    addvl = tbl1[rm] + ((vl >> 5) & tbl2[rm]);
    vl2 = vl + addvl; /* this addition can overflow for particular input-value/round-mode combos. */
    if( vl2 < vl )  /* this comparison is only true if the addition overflowed. */
        {
        negexpo--;
        vl2 >>= 1;
        vl2 |= UINT32_C(0x80000000);
        }
    return (vl2 >> 21) - (negexpo << 10);
    }


sf32 _mali_u32_to_sf32( uint32_t inp, roundmode rm )
    {
    static const uint32_t tbl1[5] =
        {
        UINT32_C(0xFF), /* up */
        UINT32_C(0x00), /* down */
        UINT32_C(0x00), /* trunc */
        UINT32_C(0x7F), /* nearest-even */
        UINT32_C(0x80), /* nearest-away */
        };
    
    static const uint32_t tbl2[5] =
        { UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(1), UINT32_C(0) };
    uint32_t lz;
    uint32_t vl;
    uint32_t negexpo;
    uint32_t addvl;
    uint32_t vl2;

    if( inp == 0 ) return 0; /* special case. */

    /* count leading zeroes and normalize the input number. */
    lz = clz32( inp );
    vl = inp << lz;
    negexpo = lz - 157; /* 1 minus exponent */
    
    /* value that we need to add to 'vl' in order to obtain correct rounding. */
    addvl = tbl1[rm] + ((vl >> 8) & tbl2[rm]);
    vl2 = vl + addvl; /* this addition can overflow for particular input-value/round-mode combos. */
    if( vl2 < vl )  /* this comparison is only true if the addition overflowed. */
        {
        negexpo--;
        vl2 >>= 1;
        vl2 |= UINT32_C(0x80000000);
        }
    return (vl2 >> 8) - (negexpo << 23);
    }



sf64 _mali_u64_to_sf64( uint64_t inp, roundmode rm )
    {
    static const uint64_t tbl1[5] =
        {
        UINT64_C(0x7FF), /* up */
        UINT64_C(0x000), /* down */
        UINT64_C(0x000), /* trunc */
        UINT64_C(0x3FF), /* nearest-even */
        UINT64_C(0x400), /* nearest-away */
        };
    
    static const uint64_t tbl2[5] =
        { UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(1), UINT64_C(0) };
    uint32_t lz;
    uint64_t vl;
    uint32_t negexpo;
    uint64_t addvl;
    uint64_t vl2;

    if( inp == UINT64_C(0) ) return UINT64_C(0); /* special case. */

    /* count leading zeroes and normalize the input number. */
    lz = clz64( inp );
    vl = inp << lz;
    negexpo = lz - 1085; /* 1 minus exponent */
    
    /* value that we need to add to 'vl' in order to obtain correct rounding. */
    addvl = tbl1[rm] + ((vl >> 11) & tbl2[rm]);
    vl2 = vl + addvl; /* this addition can overflow for particular input-value/round-mode combos. */
    if( vl2 < vl )  /* this comparison is only true if the addition overflowed. */
        {
        negexpo--;
        vl2 >>= 1;
        vl2 |= UINT64_C(0x8000000000000000);
        }
    return (vl2 >> 11) - ((uint64_t)negexpo << 52);
    }




sf16 _mali_s16_to_sf16( int16_t inp, roundmode rm )
    {
    static const uint32_t tbl1[10] =
        {
        UINT32_C(0x1F0000), /* positive, up */
        UINT32_C(0x000000), /* negative, up */
        UINT32_C(0x000000), /* positive, down */
        UINT32_C(0x1F0000), /* negative, down */
        UINT32_C(0x000000), /* positive, truncate */
        UINT32_C(0x000000), /* negative, truncate */
        UINT32_C(0x0F0000), /* positive, nearest-even */
        UINT32_C(0x0F0000), /* negative, nearest-even */
        UINT32_C(0x100000), /* positive, nearest-away */
        UINT32_C(0x100000), /* negative, nearest-away */
        };
    
    static const uint32_t tbl2[10] =
        { UINT32_C(0x0),UINT32_C(0x0),UINT32_C(0x0),UINT32_C(0x0),UINT32_C(0x0),UINT32_C(0x0),UINT32_C(0x10000),UINT32_C(0x10000),UINT32_C(0x0),UINT32_C(0x0) };
    uint32_t inpx;
    uint32_t msk;
    uint32_t idx;
    uint32_t lz;
    uint32_t vl;
    uint32_t negexpo;
    uint32_t addvl;
    uint32_t vl2;
    
    if( inp == 0 ) return 0; /* special case */
    
    inpx = inp;
    msk = (int32_t)inpx >> 31;
    idx = (rm << 1) - msk;
    inpx ^= msk;
    inpx -= msk;
    
    lz = clz32( inpx );
    vl = inpx << lz;
    negexpo = lz - (65536 + 45);
    
    addvl = tbl1[idx] + ((vl >> 5) & tbl2[idx]);
    vl2 = vl + addvl;
    if( vl2 < vl )
        {
        negexpo--;
        vl2 >>= 1;
        vl2 |= UINT32_C(0x80000000);
        }
    return (msk & 0x8000U) + (vl2 >> 21) - (negexpo << 10);
    }




sf32 _mali_s32_to_sf32( int32_t inp, roundmode rm )
    {
    static const uint32_t tbl1[10] =
        {
        0xFF, /* positive, up */
        0x00, /* negative, up */
        0x00, /* positive, down */
        0xFF, /* negative, down */
        0x00, /* positive, truncate */
        0x00, /* negative, truncate */
        0x7F, /* positive, nearest-even */
        0x7F, /* negative, nearest-even */
        0x80, /* positive, nearest-away */
        0x80, /* negative, nearest-away */
        };
    
    static const uint32_t tbl2[10] =
        { 0,0,0,0,0,0,1,1,0,0 };
    uint32_t msk;
    uint32_t idx;
    uint32_t lz;
    uint32_t vl;
    uint32_t negexpo;
    uint32_t addvl;
    uint32_t vl2;
    
    if( inp == 0 ) return 0; /* special case */
    
    msk = (int32_t)inp >> 31;
    idx = (rm << 1) - msk;
    inp ^= msk;
    inp -= msk;
    
    lz = clz32( inp );
    vl = inp << lz;
    negexpo = lz - 157;
    
    addvl = tbl1[idx] + ((vl >> 8) & tbl2[idx]);
    vl2 = vl + addvl;
    if( vl2 < vl )
        {
        negexpo--;
        vl2 >>= 1;
        vl2 |= UINT32_C(0x80000000);
        }
    return (msk << 31) + (vl2 >> 8) - (negexpo << 23);
    }




sf64 _mali_s64_to_sf64( int64_t inp, roundmode rm )
    {
    static const uint64_t tbl1[10] =
        {
        UINT64_C(0x7FF), /* positive, up */
        UINT64_C(0x000), /* negative, up */
        UINT64_C(0x000), /* positive, down */
        UINT64_C(0x7FF), /* negative, down */
        UINT64_C(0x000), /* positive, truncate */
        UINT64_C(0x000), /* negative, truncate */
        UINT64_C(0x3FF), /* positive, nearest-even */
        UINT64_C(0x3FF), /* negative, nearest-even */
        UINT64_C(0x400), /* positive, nearest-away */
        UINT64_C(0x400), /* negative, nearest-away */
        };
    
    static const uint64_t tbl2[10] =
        { UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(1), UINT64_C(1), UINT64_C(0), UINT64_C(0) };
    uint64_t msk;
    uint32_t idx;
    uint32_t lz;
    uint64_t vl;
    uint32_t negexpo;
    uint64_t addvl;
    uint64_t vl2;
    
    if( inp == UINT64_C(0) ) return UINT64_C(0); /* special case */
    
    msk = (int64_t)inp >> 63;
    idx = (rm << 1) - (uint32_t)msk;
    inp ^= msk;
    inp -= msk;
    
    lz = clz64( inp );
    vl = inp << lz;
    negexpo = lz - 1085;
    
    addvl = tbl1[idx] + ((vl >> 11) & tbl2[idx]);
	vl2 = vl + addvl;
    if( vl2 < vl )
        {
        negexpo--;
        vl2 >>= 1;
        vl2 |= UINT64_C(0x8000000000000000);
        }
    return (msk << 63) + (vl2 >> 11) - ((uint64_t)negexpo << 52);
    }




/**************************************
  Widening floating-point multiplies
**************************************/

/*
Widening multiply operations produce a result of double
the width of an ordinary multiply. They cannot overflow, underflow
or produce denormals, even when they receive denormals as input.
The result is always exact; no rounding is performed
*/



/* 16-bit to 32-bit widening multiply. */


sf32 _mali_widen_mul_sf16( sf16 a, sf16 b, uint32_t nan_payload )
    {
    /* table used to assist special-input-value detection. */
    static const uint8_t det_tab[64] = {
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
        };
    
    uint16_t abs_a = a & 0x7FFFU;
    uint16_t abs_b = b & 0x7FFFU;
    uint32_t signvl = (uint32_t)((a ^ b) & 0x8000U) << 16; /* sign bit. */
    uint32_t det = det_tab[a >> 10] | det_tab[b >> 10]; /* 1 if either input is special, else 0. */
    
    uint32_t a_mant, b_mant;
    uint32_t res_exp_m1;
    uint32_t res_mant;
    int32_t rm2;
    int32_t a_exp = abs_a >> 10;
    int32_t b_exp = abs_b >> 10;

    if( det )
        {
        if( abs_a > 0x7C00U )
            {
            if( abs_b > 0x7C00U )
                {
                /* A and B are both NaNs */
                uint16_t  a_nanval = a;
                uint16_t  b_nanval = b;
                uint16_t retval = _mali_ord_sf16( a_nanval, b_nanval ) ? b_nanval : a_nanval;
                return _mali_sf16_to_sf32( retval | 0x200);
                }
            /* A is NaN; return A quietened. */            
            return ((uint32_t)abs_a << 13) | UINT32_C(0x7FC00000) | (((uint32_t)a & 0x8000) << 16);
            }
        
        if( abs_b > 0x7C00U )
            {
            /* B is NaN; return B quietened */
            return ((uint32_t)abs_b << 13) | UINT32_C(0x7FC00000) | (((uint32_t)b & 0x8000) << 16);
            }
            
        if( abs_b == 0x7C00U )
            {
            /* B is infinity. */
            if( abs_a == 0 )
                /* A is Zero and B is Inf, return NaN */
                return signvl | UINT32_C(0x7FC00000) | nan_payload;
            /* otherwise return Infinity */
            return UINT32_C(0x7F800000) | signvl;            
            }
        
        if( abs_a == 0x7C00U )
            {
            /* A is infinity */
            if( abs_b == 0 )
                /* B is zero and A is Inf, return NaN */
                return signvl | UINT32_C(0x7FC00000) | nan_payload;
            /* otherwise return Infinity */
            return UINT32_C(0x7F800000) | signvl;            
            }
        
        
        if( abs_a == 0 || abs_b == 0 )
            /* one of the inputs is Zero, return Zero */
            return signvl;
        
        /* A or B may be denormal. */
        if( abs_a <= 0x3FFU )
            {
            uint32_t lz = clz32( abs_a );
            a_exp = 22 - lz;
            a_mant = abs_a << (lz - 21);
            }
        else
            a_mant = (abs_a & 0x3FFU) | 0x400U;

        if( abs_b <= 0x3FFU )
            {
            uint32_t lz = clz32( abs_b );
            b_exp = 22 - lz;
            b_mant = abs_b << (lz - 21);
            }
        else
            b_mant = (abs_b & 0x3FFU) | 0x400U;
        }
    else
        {
        a_mant = (abs_a & 0x3FFU) | 0x400U;
        b_mant = (abs_b & 0x3FFU) | 0x400U;
        }
    
    res_exp_m1 = a_exp + b_exp + 97;
    
    /* at this point, a_mant and b_mant are in the range [2^10, 2^11) */
    res_mant = a_mant * (b_mant << 2);
    /* at this point, res_mant is in the range [2^22, 2^24). */
    
    /*
    If it is less than 2^23, then it must be left-shifted 1 place
    and the exponent must be decremented
    */
    rm2 = res_mant - UINT32_C(0x800000);
    rm2 >>= 31;
    res_exp_m1 += rm2;
    res_mant += (res_mant & rm2);
    
    return signvl + res_mant + (res_exp_m1 << 23);    
    }





/* 32-bit to 64-bit widening floating-point multiply */

sf64 _mali_widen_mul_sf32( sf32 a, sf32 b, uint64_t nan_payload )
    {
    uint32_t abs_a = a & UINT32_C(0x7FFFFFFF);
    uint32_t abs_b = b & UINT32_C(0x7FFFFFFF);
    int32_t a_exp = abs_a >> 23;
    int32_t b_exp = abs_b >> 23;
    
    /*
    determinant turns into negative value iff at least one of the inputs
    is a Zero/Inf/NaN/Denormal.
    */
    int32_t det = (a_exp - 1) | (b_exp - 1) | (0xFE - a_exp) | (0xFE - b_exp);
    uint32_t signvl = (a ^ b) & UINT32_C(0x80000000);
    
    uint32_t res_exp_m1;
    uint64_t res_mant;
    int64_t rm2;
    uint32_t a_mant, b_mant;
    
    if( det < 0 )
        {
        if( abs_a > UINT32_C(0x7F800000) )
            {
            if( abs_b > UINT32_C(0x7F800000) )
                {
                /* A and B are both NaNs */
                uint32_t  a_nanval = a;
                uint32_t  b_nanval = b;
                uint32_t retval = _mali_ord_sf32( a_nanval, b_nanval ) ? b_nanval : a_nanval;
                return _mali_sf32_to_sf64( retval | UINT32_C(0x400000) );
                }
            /* A is NaN; return a quietened and widened version of A. */
            return ((uint64_t)abs_a << 29) | UINT64_C(0x7FF8000000000000) | ((uint64_t)(a & UINT32_C(0x80000000)) << 32);            
            }

        if( abs_b > UINT32_C(0x7F800000) )
            {
            /* B is NaN; return a quietened and widened version of B. */
            return ((uint64_t)abs_b << 29) | UINT64_C(0x7FF8000000000000) | ((uint64_t)(b & UINT32_C(0x80000000)) << 32);            
            }

        if( abs_b == UINT32_C(0x7F800000) )
            {
            /* B is INF */
            if( abs_a == 0U )
                /* B is INF and A is Zero, return NaN */
                return UINT64_C(0x7FF8000000000000) | nan_payload | ((uint64_t)signvl << 32);
            /* otherwise, return INF */
            return UINT64_C(0x7FF0000000000000) | ((uint64_t)signvl << 32);
            }

        if( abs_a == UINT32_C(0x7F800000) )
            {
            /* A is INF */
            if( abs_b == 0U )
                /* A is INF and B is Zero, return NaN */
                return UINT64_C(0x7FF8000000000000) | nan_payload | ((uint64_t)signvl << 32);
            /* otherwise, return INF */
            return UINT64_C(0x7FF0000000000000) | ((uint64_t)signvl << 32);            
            }

        if( abs_a == 0 )
            {
            /* A is zero; return sign-bit */
            return ((uint64_t)signvl << 32);
            }

        if( abs_b == 0 )
            {
            /* B is zero; return sign-bit */
            return ((uint64_t)signvl << 32);
            }
        
        /* for remaining cases, one or both inputs may be denormal. */
        if( abs_a <= UINT32_C(0x7FFFFF) )
            {
            uint32_t lz = clz32( abs_a );
            a_exp = 9 - lz;
            a_mant = abs_a << (lz-8);
            }
        else
            a_mant = (abs_a & UINT32_C(0x7FFFFF)) | UINT32_C(0x800000);
        
        if( abs_b <= UINT32_C(0x7FFFFF) )
            {
            uint32_t lz = clz32( abs_b );
            b_exp = 9 - lz;
            b_mant = abs_b << (lz-8);
            }
        else
            b_mant = (abs_b & UINT32_C(0x7FFFFF)) | UINT32_C(0x800000);
        
        }
    else
        {
        a_mant = (abs_a & UINT32_C(0x7FFFFF)) | UINT32_C(0x800000);
        b_mant = (abs_b & UINT32_C(0x7FFFFF)) | UINT32_C(0x800000);
        }
    
    
    res_exp_m1 = a_exp + b_exp + 769; 
    
    /* at this point, a_mant and b_mant are in range [2^23, 2^24). */
    
    /* res_mant is in range [2^51, 2^53) */
    res_mant = (uint64_t)a_mant * (b_mant << 5);
    
    /*
    if res_mant is less than 2^52, then left-shift by 1 place
    and decrement the exponent.
    */
    rm2 = res_mant - UINT64_C(0x10000000000000);
    rm2 >>= 63;
    res_exp_m1 += (uint32_t)rm2;
    res_mant += res_mant & rm2;


    return ((uint64_t)signvl << 32) + ((uint64_t)res_exp_m1 << 52) + res_mant;
    }
    

/*************************************************
  Non-widening floating-point multiplications
*************************************************/

/*
For FP16 and FP32 multiplications, we implement non-widening multiplies
by simply doing a widening multiply followed by narrowing conversion.
*/

sf16 _mali_mul_sf16( sf16 a, sf16 b, roundmode rm, uint16_t nan_payload )
    {
    return _mali_sf32_to_sf16( _mali_widen_mul_sf16( a, b, (uint32_t)nan_payload << 13 ), rm );
    }


sf32 _mali_mul_sf32( sf32 a, sf32 b, roundmode rm, uint32_t nan_payload )
    {
    return _mali_sf64_to_sf32( _mali_widen_mul_sf32( a,b, (uint64_t)nan_payload << 29 ), rm );    
    }


/*
For FP64 multiplications, we cannot do such a simple trick. Instead,
we have a much more involved procedure, which includes as a subpart
a widening 64-bit integer multiply.
*/



/*
data structure to represent 128-bit integers for the helper functions below.
vl[0] is the bottom 64 bits, vl[1] is the top 64 bits.
*/
typedef struct v128_
    {
    uint64_t vl[2];
    } v128;

/*
Widening multiply that takes two unsigned 64-bit inputs and produces a 128-bit output.
For x86-64, there exists a very convenient assembly instruction that does all the job for us.
Otherwise, perform slow multiprecision arithmetic.
*/
static SOFTFLOAT_INLINE v128 widening_mul( uint64_t inp1, uint64_t inp2)
    {
    v128 res;
    uint64_t resL;
    uint64_t resH;    
#if defined(__GNUC__) && defined (__x86_64__)
    __asm__( "mulq %2": "=a"(resL), "=d"(resH): "rm"(inp1), "a"(inp2) );
#else
    uint32_t inp1L = (uint32_t)inp1;
    uint32_t inp1H = (uint32_t)(inp1 >> 32);
    uint32_t inp2L = (uint32_t)inp2;
    uint32_t inp2H = (uint32_t)(inp2 >> 32);
    uint64_t prod1 = (uint64_t)inp1L * inp2L;
    uint64_t prod2a = (uint64_t)inp1L * inp2H;
    uint64_t prod2b = (uint64_t)inp2L * inp1H;
    uint64_t prod3 = (uint64_t)inp1H * inp2H;
    
    uint64_t prod2aL = prod2a << 32;
    uint64_t prod2aH = prod2a >> 32;
    uint64_t prod2bL = prod2b << 32;
    uint64_t prod2bH = prod2b >> 32;
        
    resL = prod1;
    resH = prod3;
    resL += prod2aL;
    /*
    if the result of an unsigned addition is smaller than either of its inputs,
    we know that the addition produced a carry-out. Propagate the carry.
    */
    if( resL < prod2aL )
        resH++;
    resH += prod2aH;
    resL += prod2bL;
    if( resL < prod2bL )
        resH++;
    resH += prod2bH;
#endif    
    res.vl[0] = resL;
    res.vl[1] = resH;
    return res;
    }




/*
64-bit soft-float multiply. 

A fairly elaborate block of code, both due to the number of special cases
and the need to use multi-precision arithmetic.

WARNING: when running on 32-bit x86, the results do not match the results
of a multiply using the C 'double' data type. This is due to double-rounding in 
the x87 FPU and is NOT a defect in this routine! This problem can be mitigated by
compiling 32-bit x86 code by adding "-msse2 -mfpmath=sse" on the compiler commandline,
at least with gcc.


*/

sf64 _mali_mul_sf64( sf64 a, sf64 b, roundmode rm, uint64_t nan_payload )
    {
    
    uint64_t abs_a = a & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t abs_b = b & UINT64_C(0x7FFFFFFFFFFFFFFF);
    
    int32_t a_exp = (int32_t)(abs_a >> 52);
    int32_t b_exp = (int32_t)(abs_b >> 52);
    uint64_t a_mant, b_mant;
    int32_t res_exp_m1;
    v128 multres;
    uint64_t res_mant;
    int64_t rm2;
    uint64_t signvl = (a ^ b) & UINT64_C(0x8000000000000000); /* result sign unless we generate/propagate NaN. */
    uint32_t idx = (rm << 1) + (uint32_t)(signvl >> 63); /* {roundmode,sign} index */

    /*
    compute a determinant: if either exponent indicates zero/denorm/inf/nan,
    then at least one of the subtractions will go negative, setting the MSB,
    thereby making sure that the logicOR result will itself be negative.
    */
    int32_t det = ((a_exp - 1) | (b_exp - 1)) | ((0x3FE - a_exp) | (0x3FE - b_exp));

    if( det < 0 )
        {
        /* special-case path if one or both of the inputs are Zero/Inf/NaN/denorm */
        
        if( abs_a > UINT64_C(0x7FF0000000000000) )
            {
            if( abs_b > UINT64_C(0x7FF0000000000000) )
                {
                /* A and B are both NaNs */
                uint64_t  a_nanval = a;
                uint64_t  b_nanval = b;
                uint64_t retval = _mali_ord_sf64( a_nanval, b_nanval ) ? b_nanval : a_nanval;
                return retval | UINT64_C(0x8000000000000);
                }

            /* A is NaN; return a quietened version of A. */
            return a | UINT64_C(0x8000000000000);
            }
        if( abs_b > UINT64_C(0x7FF0000000000000) )
            /* B is NaN; return a quietened version of B. */
            return b | UINT64_C(0x8000000000000);

        if( abs_b == UINT64_C(0x7FF0000000000000) )
            {
            /* B is INF */
            if( abs_a == UINT64_C(0) )
                /* B is INF and A is Zero, return NaN */
                return UINT64_C(0x7FF8000000000000) | signvl | nan_payload;
            /* B is Inf and A is nonzero */
            return UINT64_C(0x7FF0000000000000) | signvl;
            }
        
        if( abs_a == UINT64_C(0x7FF0000000000000) )
            {
            /* A is INF */
            if( abs_b == UINT64_C(0) )
                /* A is INF and B is Zero, return NaN */
                return UINT64_C(0x7FF8000000000000) | signvl | nan_payload;
            /* A is Inf and B is nonzero */
            return UINT64_C(0x7FF0000000000000) | signvl;
            }   
        
        
        if( abs_a == 0 )
            /* A is zero; return zero. */
            return signvl;
            
        if( abs_b == 0 )
            /* B is zero; return zero. */
            return signvl;
        
        /*
        if we come here, then neither input can possibly be zero/inf/nan;
        they may be denormal.
        */
        
        /* if a is denormal, extract it and normalize it. */
        if( a_exp == 0 )
            {
            uint32_t lz = clz64( abs_a );
            a_mant = (abs_a << lz) >> 11;
            a_exp = 12 - lz;
            }
        else
            a_mant = (a & UINT64_C(0xFFFFFFFFFFFFF)) | UINT64_C(0x10000000000000);

        /* if b is denormal, extract it and normalize it. */
        if( b_exp == 0 )
            {
            uint32_t lz = clz64( abs_b );
            b_mant = (abs_b << lz) >> 11;
            b_exp = 12 - lz;
            }
        else
            b_mant = (b & UINT64_C(0xFFFFFFFFFFFFF)) | UINT64_C(0x10000000000000);
        }
    else
        {
        /*
        if neither special case was triggered, both inputs are normal
        and we extract mant acordingly.
        */
        a_mant = (a & UINT64_C(0xFFFFFFFFFFFFF)) | UINT64_C(0x10000000000000);
        b_mant = (b & UINT64_C(0xFFFFFFFFFFFFF)) | UINT64_C(0x10000000000000);        
        }
    
    

    res_exp_m1 = a_exp + b_exp - 1023;
    
    /* mantissa: after this shift, a_mant is within the range [2^62, 2^63) */
    a_mant <<= 10;
    b_mant <<= 10;
    
    /* perform a widening 64->128 bit multiply */
    multres = widening_mul( a_mant, b_mant );
    
    /* reduce the result back to 64-bit with sticky-bit */
    res_mant = multres.vl[1];
    if( multres.vl[0] != 0 )
        res_mant |= 1;
    /* at this point, res_mant is within the range [2^60, 2^62) */

    
    /*
    If it is less than 2^61, then we must decrement the exponent
    and left-shift the mantissa 1 place
    */
    rm2 = res_mant - UINT64_C(0x2000000000000000);
    rm2 >>= 63; /* all 1s if res_mant is less than 2^61. */
    res_exp_m1 += (int32_t)rm2;
    res_mant += (res_mant & rm2);

    
    /* check for overflow/underflow. */
    if((uint32_t) res_exp_m1 >= 2046 )
        {    

        /* table containing values to return if the result is too small to be representable. */
        static const uint64_t small_tab[10] =
            {
            UINT64_C(0x0000000000000001), /* positive, round up */
            UINT64_C(0x8000000000000000), /* negative, round up */
            UINT64_C(0x0000000000000000), /* positive, round down */
            UINT64_C(0x8000000000000001), /* negative, round down */
            UINT64_C(0x0000000000000000), /* positive, round to zero */
            UINT64_C(0x8000000000000000), /* negative, round to zero */
            UINT64_C(0x0000000000000000), /* positive, round to nearest-even */
            UINT64_C(0x8000000000000000), /* negative, round to nearest-even */
            UINT64_C(0x0000000000000000), /* positive, round to nearest-away */
            UINT64_C(0x8000000000000000)  /* negative, round to nearest-away */
            };
 
        /* table containing values to return if the result is too large to be representable. */
        static const uint64_t large_tab[10] =
            {
            UINT64_C(0x7FF0000000000000), /* positive, round up */
            UINT64_C(0xFFEFFFFFFFFFFFFF), /* negative, round up */
            UINT64_C(0x7FEFFFFFFFFFFFFF), /* positive, round down */
            UINT64_C(0xFFF0000000000000), /* negative, round down */
            UINT64_C(0x7FEFFFFFFFFFFFFF), /* positive, round to zero */
            UINT64_C(0xFFEFFFFFFFFFFFFF), /* negative, round to zero */
            UINT64_C(0x7FF0000000000000), /* positive, round to nearest-even */
            UINT64_C(0xFFF0000000000000), /* negative, round to nearest-even */
            UINT64_C(0x7FF0000000000000), /* positive, round to nearest-away */
            UINT64_C(0xFFF0000000000000)  /* negative, round to nearest-away */
            };
		uint32_t shamt;
        /* if we have an overflow, return an overflow value. */
        if( res_exp_m1 >= 2046 )
            return large_tab[idx];

        /*
        if we reach this point, we have underflow. This should
        be rounded to a zero or denormal.
        */
        shamt = 9 - res_exp_m1;
        if( shamt > 63 )
            return small_tab[idx];

        switch( idx )
            {
            case 0: /* round UP, positive */
            case 3: /* round DOWN, negative */
                return rtup_shift64( res_mant, shamt ) | signvl;
            case 1: /* round UP, negative */
            case 2: /* round DOWN, positive */
            case 4: /* round TOZERO, positive */
            case 5: /* round TOZERO, negative */
                return (res_mant >> shamt) | signvl;
            case 6:
            case 7: /* round to nearest even */
                return rtne_shift64( res_mant, shamt) | signvl;
            case 8:
            case 9: /* round to nearest away */
                return rtna_shift64( res_mant, shamt) | signvl;
            }
         }
    
    /* if we reach this location, then we know that we have a normal number. */
		{
		/* a table containing rounding-mode dependent constants to add to the input number. */
		static const uint64_t addtab[10] =
			{
			UINT64_C(0x1FF), /* positive, round UP */
			UINT64_C(0x000), /* negative, round UP */
			UINT64_C(0x000), /* positive, round DOWN */
			UINT64_C(0x1FF), /* negative, round DOWN */
			UINT64_C(0x000), /* positive, round to ZERO */
			UINT64_C(0x000), /* negative, round to ZERO */
			UINT64_C(0x0FF), /* positive, round to nearest-even */
			UINT64_C(0x0FF), /* negative, round to nearest-even */
			UINT64_C(0x100), /* positive, round to nearest-away */
			UINT64_C(0x100), /* negative, round to nearest-away */
			};
		static const uint64_t masktab[10] = {0,0,0,0,0,0,1,1,0,0};
			
		res_mant += (res_mant >> 9) & masktab[idx]; /* adjustment for round-to-nearest-even */
		res_mant += addtab[idx]; /* add a constant for rounding */
		res_mant >>= 9;

		/*
		  the addition performed for roundoff may cause a carry overflow.
		  therefore, when combining exponent and mantissa, we must perform
		  an addition, not just logical-OR.
		*/
		return res_mant + ((uint64_t)res_exp_m1 << 52) + signvl;
		}
    }



/*******************************************
  Non-narrowing floating-point addition
******************************************/


/*
16-bit floating-point add
*/

sf16 _mali_add_sf16( sf16 a, sf16 b, roundmode rm, uint16_t nan_payload )
    {
    
    /*    
    pick the larger of the two input values. The computation
    below left-rotates both input values by 1 bit each
    and then conditionally the LSB. 
    Flipping the LSB has the result that if the two input numbers
    are of equal magnitude and opposite sign, the positive number is
    considered "larger". This causes cancellation to result in +0.
    This is correct in most rounding-modes; however in rounding mode DOWN
    such a cancellation should result in -0; this is achieved by abstaining
    from flipping the bit.
    */
    
    static const uint32_t fbittab[5] = {1,0,1,1,1};  
    uint16_t a_lrot = (a << 1) | (a >> 15);
    uint16_t b_lrot = (b << 1) | (b >> 15);
    uint32_t fbit = fbittab[rm];

    uint16_t maxv, minv;
    uint16_t exp_maxv;
    uint16_t exp_minv;
    uint16_t mask;
    uint32_t idx;
    uint32_t mant_maxv;
    uint32_t mant_minv;
    uint32_t shamt;
    uint32_t mant_minv2;
    uint32_t mantres;
    uint32_t lz;
    uint32_t res_exp_m1;

    a_lrot ^= fbit;
    b_lrot ^= fbit;
        
    if( a_lrot > b_lrot )
        {
        maxv = a;
        minv = b;
        }
    else
        {
        maxv = b;
        minv = a;
        }
        
    exp_maxv = (maxv >> 10) & 0x1F;
    exp_minv = (minv >> 10) & 0x1F;

    mask = (int16_t)(maxv ^ minv) >> 15; /* all 1s if operation is eff-subtract, all 0s otherwise. */
    
    if( exp_maxv == 0x1F )
        {
        /* INF/NaN inputs need to be handled specially. */
        
        /*
        If the "largest" number is a NaN, then we must return one of the inputs.
        In this case, if 'a' is NaN, then we return a quietened version of 'a',
        else we return a quietened version of 'b'.
        */
        if( (maxv & UINT32_C(0x7FFF)) > UINT32_C(0x7C00) )
            {
            /* one or both of the inputs was a NaN. */
            int a_nan = _mali_isnan_sf16( a );
            int b_nan = _mali_isnan_sf16( b );
            uint32_t a_nanval = a;
            uint32_t b_nanval = b;
            if( a_nan && b_nan )
                return (_mali_ord_sf16(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x200);
            else if( a_nan )
                return a_nanval | UINT32_C(0x200);
            else
                return b_nanval | UINT32_C(0x200);
            }
        
        /*
        if we come here, the largest input was an INF. Construct a NaN
        if the other input is an Inf of the opposite sign, else return the INF.
        */
        if( exp_maxv == exp_minv && mask != 0)
			{
            return UINT32_C(0x7E00) | nan_payload;
			}
        else
			{
            return maxv;
			}
        }

    /*
    number from 0 to 9, indicating effective rounding mode,
    constructed from input rounding mode and the sign of the maximum vale.
    */
    idx = (rm << 1) + (maxv >> 15); 
    
    /* extract mantissas. */
    mant_maxv = (maxv & UINT32_C(0x3FF)) + UINT32_C(0x400);
    mant_minv = (minv & UINT32_C(0x3FF)) + UINT32_C(0x400);
    
    if( exp_minv == 0 )
        {
        /*
        if either input is a denormal, we need to perform some adjustments:
        set the exponent to 1, and clear the "implied" bit of the mantissa.
        */
        uint32_t exp_maxv_zero = (0x100 - exp_maxv) >> 8;
        uint32_t exp_minv_zero = (0x100 - exp_minv) >> 8;
        exp_maxv += exp_maxv_zero;
        exp_minv += exp_minv_zero;
        mant_maxv -= exp_maxv_zero << 10;
        mant_minv -= exp_minv_zero << 10;
        }

    /*
    shift-amount for the shift before the actual addition.
    shift-amount cannot be greater than 31, so no overflow is possible.
    */
    shamt = exp_maxv - exp_minv;


    /*
    left-shift both inputs 3 places, then right-shift the smaller mantissa.
    */
    mant_maxv <<= 3; /* in the range [2^13, 2^14); 2 leading zeroes. */
    mant_minv <<= 3;
    mant_minv2 = sticky_urshift32( mant_minv, shamt ); /* sticky right-shift. */
    
    /* negate the smaller mantissa */
    mant_minv2 ^= mask;
    
    /* perform the actual addition. */
    mantres = mant_maxv + mant_minv2 - mask;

    if( mantres == 0 && mant_maxv != 0)
        {
        /*
        exact cancellation. Return -0 in the round-DOWN mode and +0 in all other modes.
        */
        static const uint32_t uftab[5] =
            {
            UINT32_C(0x0000), /* round UP */
            UINT32_C(0x8000), /* round DOWN */
            UINT32_C(0x0000), /* round DOWN */
            UINT32_C(0x0000), /* round NEARESTEVEN */
            UINT32_C(0x0000), /* round NEARESTAWAY */
            };
        return uftab[rm];                   
        }

    
    /* normalize the number with a left-shift. */
    lz = clz32( mantres ) - 16;

    res_exp_m1 = exp_maxv + 1 - lz;
    if( res_exp_m1 >= UINT32_C(0x1E) )
        {
        if( (int32_t)res_exp_m1 > 0 )
            {
            /* overflow: return the overflow value. */
            static const uint16_t oftab[10] = {
                UINT32_C(0x7C00), /* positive, round up */
                UINT32_C(0xFBFF), /* negative, round up */
                UINT32_C(0x7BFF), /* positive, round down */
                UINT32_C(0xFC00), /* negative, round down */
                UINT32_C(0x7BFF), /* positive, truncate */
                UINT32_C(0xFBFF), /* negative, truncate */
                UINT32_C(0x7C00), /* positive, round-to-nearest-even */
                UINT32_C(0xFC00), /* negative, round-to-nearest-even */
                UINT32_C(0x7C00), /* positive, round-to-nearest-away */
                UINT32_C(0xFC00), /* negative, round-to-nearest-away */
                };
            return oftab[idx];                
            }
        
        /*    
        the other case is denormals, for which res_exp_m1 is
        essentially negative. In this case, we
        just adjust the leading-zeroes-count and
        the result exponent.
        */
        lz += res_exp_m1;
        res_exp_m1 = 0;
        }
    
    mantres <<= lz; /* ends up in range [2^15, 2^16) for non-denormals. */
        {
		static const uint32_t add_tab[10] = {
			UINT32_C(0x1F), /* positive, round up */
			UINT32_C(0x00), /* negative, round up */
			UINT32_C(0x00), /* positive, round down */
			UINT32_C(0x1F), /* negative, round down */
			UINT32_C(0x00), /* positive, truncate */
			UINT32_C(0x00), /* negative, truncate */
			UINT32_C(0x0F), /* positive, round-to-nearest-even */
			UINT32_C(0x0F), /* negative, round-to-nearest-even */
			UINT32_C(0x10), /* positive, round-to-nearest-away */
			UINT32_C(0x10), /* negative, round-to-nearest-away */
            };
		static const uint32_t rtne_tab[10] = { 0,0,0,0,0,0,1,1,0,0 };
    
		/* value to add for rounding purposes. */
		uint32_t mr_add = add_tab[idx] + (rtne_tab[idx] & (mantres >> 5) );
    
		/*
		  this addition can overflow, in which case we need to increment
		  the exponent.
		*/
		mantres += mr_add;
		if( mantres >= UINT32_C(0x10000) )
            {
			/*
			  if the addition did overflow, the exponent needs to be increased
			  and the mantissa right-shifted.
			  This gives the correct result even if the overflow tips
			  over into infinity.
			*/
				mantres >>= 1;
				res_exp_m1++;
			}
    
		/*
		  finally, combine a result. Note that the MSB of the mantissa is not
		  removed before this calculation, so we need to compute exponent
		  minus 1 (except in the case of a denormal).
		*/
		return (maxv & 0x8000U) + (res_exp_m1 << 10) + (mantres >> 5);
		}
    }



/*
32-bit floating-point add
*/
sf32 _mali_add_sf32( sf32 a, sf32 b, roundmode rm, uint32_t nan_payload )
    {
    
    /*    
    pick the larger of the two input values. The computation
    below left-rotates both input values by 1 bit each
    and then conditionally the LSB. 
    Flipping the LSB has the result that if the two input numbers
    are of equal magnitude and opposite sign, the positive number is
    considered "larger". This causes cancellation to result in +0.
    This is correct in most rounding-modes; however in rounding mode DOWN
    such a cancellation should result in -0; this is achieved by abstaining
    from flipping the bit.
    */
    
    static const uint32_t fbittab[5] = {1,0,1,1,1};    
    uint32_t a_lrot = (a << 1) | (a >> 31);
    uint32_t b_lrot = (b << 1) | (b >> 31);
    uint32_t fbit = fbittab[rm];
    uint32_t maxv, minv;
    uint32_t exp_maxv;
    uint32_t exp_minv;
    uint32_t mask;
    uint32_t idx;
    uint32_t mant_maxv;
    uint32_t mant_minv;
    uint32_t shamt;
    uint32_t mant_minv2;
    uint32_t mantres;
    uint32_t lz;
    uint32_t res_exp_m1;


    a_lrot ^= fbit;
    b_lrot ^= fbit;
        
    if( a_lrot > b_lrot )
        {
        maxv = a;
        minv = b;
        }
    else
        {
        maxv = b;
        minv = a;
        }
        
    exp_maxv = (maxv >> 23) & 0xFF;
    exp_minv = (minv >> 23) & 0xFF;

    mask = (int32_t)(maxv ^ minv) >> 31; /* all 1s if operation is eff-subtract, all 0s otherwise.*/
    
    if( exp_maxv == 0xFF )
        {
        /* INF/NaN inputs need to be handled specially. */
        
        /*
        If the "largest" number is a NaN, then we must return one of the inputs.
        In this case, if 'a' is NaN, then we return a quietened version of 'a',
        else we return a quietened version of 'b'.
        */
        if( (maxv & UINT32_C(0x7FFFFFFF)) > UINT32_C(0x7F800000) )
            {
            /* one or both of the inputs was a NaN. */
            int a_nan = _mali_isnan_sf32( a );
            int b_nan = _mali_isnan_sf32( b );
            uint32_t a_nanval = a;
            uint32_t b_nanval = b;
            if( a_nan && b_nan )
                return (_mali_ord_sf32(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x400000);
            else if( a_nan )
                return a_nanval | UINT32_C(0x400000);
            else
                return b_nanval | UINT32_C(0x400000);
            }
        
        /*
        if we come here, the largest input was an INF. Construct a NaN
        if the other input is an Inf of the opposite sign, else return the INF.
        */
        if( exp_maxv == exp_minv && mask != 0)
            return UINT32_C(0x7FC00000) | nan_payload;
        else
            return maxv;
        }

    /*
    number from 0 to 9, indicating effective rounding mode,
    constructed from input rounding mode and the sign of the maximum vale.
    */
    idx = (rm << 1) + (maxv >> 31); 
    
    /* extract mantissas. */
    mant_maxv = (maxv & UINT32_C(0x7FFFFF)) + UINT32_C(0x800000);
    mant_minv = (minv & UINT32_C(0x7FFFFF)) + UINT32_C(0x800000);
    
    if( exp_minv == 0 )
        {
        /*
        if either input is a denormal, we need to perform some adjustments:
        set the exponent to 1, and clear the "implied" bit of the mantissa.
        */
        uint32_t exp_maxv_zero = (0x100 - exp_maxv) >> 8;
        uint32_t exp_minv_zero = (0x100 - exp_minv) >> 8;
        exp_maxv += exp_maxv_zero;
        exp_minv += exp_minv_zero;
        mant_maxv -= exp_maxv_zero << 23;
        mant_minv -= exp_minv_zero << 23;
        }


    /*
    shift-amount for the shift before the actual addition.
    */
    shamt = exp_maxv - exp_minv;
    if( shamt > 31 ) 
        shamt = 31;


    /* left-shift both inputs 3 places, then right-shift the smaller mantissa. */
    mant_maxv <<= 3; /* in the range [2^26, 2^27); 5 leading zeroes. */
    mant_minv <<= 3;
    mant_minv2 = sticky_urshift32( mant_minv, shamt ); /* sticky right-shift. */
    
    /* negate the smaller mantissa */
    mant_minv2 ^= mask;
    
    /* perform the actual addition. */
    mantres = mant_maxv + mant_minv2 - mask;
    
    if( mantres == 0 && mant_maxv != 0)
        {
        /*
        exact cancellation. Return -0 in the round-DOWN mode and +0 in all other modes.
        */
        static const uint32_t uftab[5] =
            {
            UINT32_C(0x00000000), /* round UP */
            UINT32_C(0x80000000), /* round DOWN */
            UINT32_C(0x00000000), /* round DOWN */
            UINT32_C(0x00000000), /* round NEARESTEVEN */
            UINT32_C(0x00000000), /* round NEARESTAWAY */
            };
        return uftab[rm];                   
        }
    
    
    /* normalize the number with a left-shift. */
    lz = clz32( mantres );

    res_exp_m1 = exp_maxv + 4 - lz;
    if( res_exp_m1 >= UINT32_C(0xFE) )
        {
        if( (int32_t)res_exp_m1 > 0 )
            {
            /* overflow: return the overflow value. */
            static const uint32_t oftab[10] = {
                UINT32_C(0x7F800000), /* positive, round up */
                UINT32_C(0xFF7FFFFF), /* negative, round up */
                UINT32_C(0x7F7FFFFF), /* positive, round down */
                UINT32_C(0xFF800000), /* negative, round down */
                UINT32_C(0x7F7FFFFF), /* positive, truncate */
                UINT32_C(0xFF7FFFFF), /* negative, truncate */
                UINT32_C(0x7F800000), /* positive, round-to-nearest-even */
                UINT32_C(0xFF800000), /* negative, round-to-nearest-even */
                UINT32_C(0x7F800000), /* positive, round-to-nearest-away */
                UINT32_C(0xFF800000), /* negative, round-to-nearest-away */
                };
            return oftab[idx];                
            };
            
        /*
        the other case is denormals, for which res_exp_m1 is
        essentially negative. In this case, we
        just adjust the leading-zeroes-count and
        the result exponent.
        */
        lz += res_exp_m1;
        res_exp_m1 = 0;
        }
    
    mantres <<= lz; /* ends up in range [2^31, 2^32) for non-denormals. */
    
		{
	    static const uint32_t add_tab[10] = {
	        UINT32_C(0xFF), /* positive, round up */
	        UINT32_C(0x00), /* negative, round up */
	        UINT32_C(0x00), /* positive, round down */
	        UINT32_C(0xFF), /* negative, round down */
	        UINT32_C(0x00), /* positive, truncate */
	        UINT32_C(0x00), /* negative, truncate */
	        UINT32_C(0x7F), /* positive, round-to-nearest-even */
	        UINT32_C(0x7F), /* negative, round-to-nearest-even */
	        UINT32_C(0x80), /* positive, round-to-nearest-away */
	        UINT32_C(0x80), /* negative, round-to-nearest-away */
	        };
	    static const uint32_t rtne_tab[10] = { 0,0,0,0,0,0,1,1,0,0 };
	    
	    /* value to add for rounding purposes. */
	    uint32_t mr_add = add_tab[idx] + (rtne_tab[idx] & (mantres >> 8) );
	    
	    /*
	    this addition can overflow, in which case we need to increment
	    the exponent.
	    */
	    mantres += mr_add;
	    if( mantres < mr_add )
	        {
	        /*
	        if the addition did overflow, the exponent needs to be increased
	        and the mantissa right-shifted.
	        This gives the correct result even if the overflow tips
	        over into infinity.
	        */
	        mantres >>= 1;
	        mantres |= UINT32_C(0x80000000);
	        res_exp_m1++;
	        }
	    
	    /*
	    finally, combine a result. Note that the MSB of the mantissa is not
	    removed before this calculation, so we need to compute exponent
	    minus 1 (except in the case of a denormal).
	    */
	    return (maxv & UINT32_C(0x80000000)) + (res_exp_m1 << 23) + (mantres >> 8);
		}
    }




/*
64-bit floating-point add
*/
sf64 _mali_add_sf64( sf64 a, sf64 b, roundmode rm, uint64_t nan_payload )
    {
        
    /*
    pick the larger of the two input values. The computation
    below left-rotates both input values by 1 bit each
    and then conditionally the LSB. 
    Flipping the LSB has the result that if the two input numbers
    are of equal magnitude and opposite sign, the positive number is
    considered "larger". This causes cancellation to result in +0.
    This is correct in most rounding-modes; however in rounding mode DOWN
    such a cancellation should result in -0; this is achieved by abstaining
    from flipping the bit.
    */
    
    static const uint64_t fbittab[5] = {UINT64_C(1),UINT64_C(0),UINT64_C(1),UINT64_C(1),UINT64_C(1)};
    uint64_t a_lrot = (a << 1) | (a >> 63);
    uint64_t b_lrot = (b << 1) | (b >> 63);
    uint64_t fbit = fbittab[rm];
    uint64_t maxv, minv;
    uint32_t exp_maxv;
    uint32_t exp_minv;
    uint64_t mask;
    uint32_t idx;
    uint64_t mant_maxv;
    uint64_t mant_minv;
    uint32_t shamt;
    uint64_t mant_minv2;
    uint64_t mantres;
    uint64_t lz;
    uint64_t res_exp_m1;

    a_lrot ^= fbit;
    b_lrot ^= fbit;
        
    if( a_lrot > b_lrot )
        {
        maxv = a;
        minv = b;
        }
    else
        {
        maxv = b;
        minv = a;
        }
        
    exp_maxv = (uint32_t)(maxv >> 52) & 0x7FF;
    exp_minv = (uint32_t)(minv >> 52) & 0x7FF;

    mask = (int64_t)(maxv ^ minv) >> 63; /* all 1s if operation is eff-subtract, all 0s otherwise. */
    
    if( exp_maxv == 0x7FF )
        {
        /* INF/NaN inputs need to be handled specially. */
        
        /*
        If the "largest" number is a NaN, then we must return one of the inputs.
        In this case, if 'a' is NaN, then we return a quietened version of 'a',
        else we return a quietened version of 'b'.
        */
        if( (maxv & UINT64_C(0x7FFFFFFFFFFFFFFF)) > UINT64_C(0x7FF0000000000000) )
            {

            /* one or both of the inputs was a NaN. */
            int a_nan = _mali_isnan_sf64( a );
            int b_nan = _mali_isnan_sf64( b );
            uint64_t a_nanval = a;
            uint64_t b_nanval = b;
            if( a_nan && b_nan )
                return (_mali_ord_sf64(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT64_C(0x8000000000000);
            else if( a_nan )
                return a_nanval | UINT64_C(0x8000000000000);
            else
                return b_nanval | UINT64_C(0x8000000000000);
            }
        
        /*
        if we come here, the largest input was an INF. Construct a NaN
        if the other input is an Inf of the opposite sign, else return the INF.
        */
        if( exp_maxv == exp_minv && mask != UINT64_C(0))
            return UINT64_C(0x7FF8000000000000) | nan_payload;
        else
            return maxv;
        }

    /*
    number from 0 to 9, indicating effective rounding mode,
    constructed from input rounding mode and the sign of the maximum vale.
    */
    idx = (rm << 1) + (uint32_t)(maxv >> 63); 
    
    /* extract mantissas. */
    mant_maxv = (maxv & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000);
    mant_minv = (minv & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000);
    
    if( exp_minv == 0 )
        {
        /*
        if either input is a denormal, we need to perform some adjustments:
        set the exponent to 1, and clear the "implied" bit of the mantissa.
        */
        uint32_t exp_maxv_zero = (0x800 - exp_maxv) >> 11;
        uint32_t exp_minv_zero = (0x800 - exp_minv) >> 11;
        exp_maxv += exp_maxv_zero;
        exp_minv += exp_minv_zero;
        mant_maxv -= (uint64_t)exp_maxv_zero << 52;
        mant_minv -= (uint64_t)exp_minv_zero << 52;
        }


    /* shift-amount for the shift before the actual addition. */
    shamt = exp_maxv - exp_minv;
    if( shamt > 63 ) 
        shamt = 63;


    /* left-shift both inputs 3 places, then right-shift the smaller mantissa. */
    mant_maxv <<= 3; /* in the range [2^26, 2^27); 5 leading zeroes. */
    mant_minv <<= 3;
    mant_minv2 = sticky_urshift64( mant_minv, shamt ); /* sticky right-shift. */
    
    /* negate the smaller mantissa */
    mant_minv2 ^= mask;
    
    /* perform the actual addition. */
    mantres = mant_maxv + mant_minv2 - mask;

    /* check for exact cancellation */
    if( mantres == UINT64_C(0) && mant_maxv != UINT64_C(0))
        {
        /*
        exact cancellation. Return -0 in the round-DOWN mode and +0 in all other modes.
        */
        static const uint64_t uftab[5] =
            {
            UINT64_C(0x0000000000000000), /* round UP */
            UINT64_C(0x8000000000000000), /* round DOWN */
            UINT64_C(0x0000000000000000), /* round DOWN */
            UINT64_C(0x0000000000000000), /* round NEARESTEVEN */
            UINT64_C(0x0000000000000000), /* round NEARESTAWAY */
            };
        return uftab[rm];                   
        }

    
    /* normalize the number with a left-shift. */
    lz = clz64( mantres );

    res_exp_m1 = exp_maxv + 7 - lz;
    if( res_exp_m1 >= UINT32_C(0x7FE) )
        {
        if( (int32_t)res_exp_m1 > 0 )
            {
            /* overflow: return the overflow value. */
            static const uint64_t oftab[10] = {
                UINT64_C(0x7FF0000000000000), /* positive, round up */
                UINT64_C(0xFFEFFFFFFFFFFFFF), /* negative, round up */
                UINT64_C(0x7FEFFFFFFFFFFFFF), /* positive, round down */
                UINT64_C(0xFFF0000000000000), /* negative, round down */
                UINT64_C(0x7FEFFFFFFFFFFFFF), /* positive, truncate */
                UINT64_C(0xFFEFFFFFFFFFFFFF), /* negative, truncate */
                UINT64_C(0x7FF0000000000000), /* positive, round-to-nearest-even */
                UINT64_C(0xFFF0000000000000), /* negative, round-to-nearest-even */
                UINT64_C(0x7FF0000000000000), /* positive, round-to-nearest-away */
                UINT64_C(0xFFF0000000000000), /* negative, round-to-nearest-away */
                };
            return oftab[idx];                
            }
        
        /*
        the other case is denormals, for which res_exp_m1 is
        essentially negative. In this case, we
        just adjust the leading-zeroes-count and
        the result exponent.
        */
        lz += res_exp_m1;
        res_exp_m1 = 0;
        }
    
    mantres <<= lz; /* ends up in range [2^63, 2^64) for non-denormals. */
        {
	    static const uint64_t add_tab[10] = {
	        UINT64_C(0x7FF), /* positive, round up */
	        UINT64_C(0x000), /* negative, round up */
	        UINT64_C(0x000), /* positive, round down */
	        UINT64_C(0x7FF), /* negative, round down */
	        UINT64_C(0x000), /* positive, truncate */
	        UINT64_C(0x000), /* negative, truncate */
	        UINT64_C(0x3FF), /* positive, round-to-nearest-even */
	        UINT64_C(0x3FF), /* negative, round-to-nearest-even */
	        UINT64_C(0x400), /* positive, round-to-nearest-away */
	        UINT64_C(0x400), /* negative, round-to-nearest-away */
	        };
	    static const uint64_t rtne_tab[10] = { 0,0,0,0,0,0,1,1,0,0 };
	    
	    /* value to add for rounding purposes. */
	    uint64_t mr_add = add_tab[idx] + (rtne_tab[idx] & (mantres >> 11) );
	    
	    /*
	    this addition can overflow, in which case we need to increment
	    the exponent.
	    */
	    mantres += mr_add;
	    if( mantres < mr_add )
	        {
	        /*
	        if the addition did overflow, the exponent needs to be increased
	        and the mantissa right-shifted.
	        This gives the correct result even if the overflow tips
	        over into infinity.
	        */
	        mantres >>= 1;
	        mantres |= UINT64_C(0x8000000000000000);
	        res_exp_m1++;
	        }
	    
	    /*
	    finally, combine a result. Note that the MSB of the mantissa is not
	    removed before this calculation, so we need to compute exponent
	    minus 1 (except in the case of a denormal).
	    */
	    return (maxv & UINT64_C(0x8000000000000000)) + (res_exp_m1 << 52) + (mantres >> 11);
	    }
	}



/******************************************
  Narrowing floating-point additions
******************************************/

/*
These additions are of limited usefulness in isolation;
their main use is as buidling blocks for fused-multiply-add.
Note that their operation is NOT invariant with a
non-narrowing addition followed by a narrowing conversion; such
a procedure performs rounding twice whereas the narrowing
addition by itself only rounds once.
*/


/*
32-bit narrowing floating-point add
*/
sf16 _mali_narrow_add_sf32( sf32 a, sf32 b, roundmode rm, uint16_t nan_payload )
    {
    
    /*    
    pick the larger of the two input values. The computation
    below left-rotates both input values by 1 bit each
    and then conditionally the LSB. 
    Flipping the LSB has the result that if the two input numbers
    are of equal magnitude and opposite sign, the positive number is
    considered "larger". This causes cancellation to result in +0.
    This is correct in most rounding-modes; however in rounding mode DOWN
    such a cancellation should result in -0; this is achieved by abstaining
    from flipping the bit.
    */
    
    static const uint32_t fbittab[5] = {1,0,1,1,1};    
    uint32_t a_lrot = (a << 1) | (a >> 31);
    uint32_t b_lrot = (b << 1) | (b >> 31);
    uint32_t fbit = fbittab[rm];
    uint32_t maxv, minv;
    uint32_t exp_maxv;
    uint32_t exp_minv;
    uint32_t mask;
    uint32_t idx;
    uint32_t mant_maxv;
    uint32_t mant_minv;
    uint32_t shamt;
    uint32_t mant_minv2;
    uint32_t mantres;
    int32_t lz;
    uint32_t res_exp_m1;


    a_lrot ^= fbit;
    b_lrot ^= fbit;
        
    if( a_lrot > b_lrot )
        {
        maxv = a;
        minv = b;
        }
    else
        {
        maxv = b;
        minv = a;
        }
        
    exp_maxv = (maxv >> 23) & 0xFF;
    exp_minv = (minv >> 23) & 0xFF;

    mask = (int32_t)(maxv ^ minv) >> 31; /* all 1s if operation is eff-subtract, all 0s otherwise. */
    
    if( exp_maxv == 0xFF )
        {
        /* INF/NaN inputs need to be handled specially. */
        
        /*
        If the "largest" number is a NaN, then we must return one of the inputs.
        In this case, if 'a' is NaN, then we return a quietened version of 'a',
        else we return a quietened version of 'b'.
        */
        if( (maxv & UINT32_C(0x7FFFFFFF)) > UINT32_C(0x7F800000) )
            {

            /* one or both of the inputs was a NaN. */
            int a_nan = _mali_isnan_sf32( a );
            int b_nan = _mali_isnan_sf32( b );
            uint32_t a_nanval = a;
            uint32_t b_nanval = b;
            uint32_t retval;
            if( a_nan && b_nan )
                retval = (_mali_ord_sf32(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x400000);
            else if( a_nan )
                retval = a_nanval | UINT32_C(0x400000);
            else
                retval = b_nanval | UINT32_C(0x400000);
            return _mali_sf32_to_sf16( retval, rm );
            }
        
        /*
        if we come here, the largest input was an INF. Construct a NaN
        if the other input is an Inf of the opposite sign, else return the INF.
        */
        if( exp_maxv == exp_minv && mask != 0)
            return UINT32_C(0x7E00) | nan_payload;
        else
            return _mali_sf32_to_sf16( maxv, rm );
        }
    
    /*
    number from 0 to 9, indicating effective rounding mode,
    constructed from input rounding mode and the sign of the maximum vale.
    */
    idx = (rm << 1) + (maxv >> 31); 
    
    /* extract mantissas. */
    mant_maxv = (maxv & UINT32_C(0x7FFFFF)) + UINT32_C(0x800000);
    mant_minv = (minv & UINT32_C(0x7FFFFF)) + UINT32_C(0x800000);
    
    if( exp_minv == 0 )
        {
        /*
        if either input is a denormal, we need to perform some adjustments:
        set the exponent to 1, and clear the "implied" bit of the mantissa.
        */
        uint32_t exp_maxv_zero = (0x100 - exp_maxv) >> 8;
        uint32_t exp_minv_zero = (0x100 - exp_minv) >> 8;
        exp_maxv += exp_maxv_zero;
        exp_minv += exp_minv_zero;
        mant_maxv -= exp_maxv_zero << 23;
        mant_minv -= exp_minv_zero << 23;
        }


    /* shift-amount for the shift before the actual addition. */
    shamt = exp_maxv - exp_minv;
    if( shamt > 31 ) 
        shamt = 31;


    /* left-shift both inputs 3 places, then right-shift the smaller mantissa. */
    mant_maxv <<= 3; /* in the range [2^26, 2^27); 5 leading zeroes. */
    mant_minv <<= 3;
    mant_minv2 = sticky_urshift32( mant_minv, shamt ); /* sticky right-shift. */
    
    /* negate the smaller mantissa */
    mant_minv2 ^= mask;
    
    /* perform the actual addition. */
    mantres = mant_maxv + mant_minv2 - mask;


    /* check for exact cancellation */
    if( mantres == 0 && mant_maxv != 0)
        {
        /*
        exact cancellation. Return -0 in the round-DOWN mode and +0 in all other modes.
        */
        static const uint32_t uftab[5] =
            {
            UINT32_C(0x0000), /* round UP */
            UINT32_C(0x8000), /* round DOWN */
            UINT32_C(0x0000), /* round DOWN */
            UINT32_C(0x0000), /* round NEARESTEVEN */
            UINT32_C(0x0000), /* round NEARESTAWAY */
            };
        return uftab[rm];                   
        }

    
    /* normalize the number with a left-shift. */
    lz = clz32( mantres );

    res_exp_m1 = exp_maxv - lz + 4 - 112; /* 112 is the difference between FP32 and FP16 exponent value. */


    if( res_exp_m1 >= UINT32_C(0x1E) )
        {
        if( (int32_t)res_exp_m1 > 0 )
            {
            /* overflow: return the overflow value. */
            static const uint32_t oftab[10] = {
                UINT32_C(0x7C00), /* positive, round up */
                UINT32_C(0xFBFF), /* negative, round up */
                UINT32_C(0x7BFF), /* positive, round down */
                UINT32_C(0xFC00), /* negative, round down */
                UINT32_C(0x7BFF), /* positive, truncate */
                UINT32_C(0xFBFF), /* negative, truncate */
                UINT32_C(0x7C00), /* positive, round-to-nearest-even */
                UINT32_C(0xFC00), /* negative, round-to-nearest-even */
                UINT32_C(0x7C00), /* positive, round-to-nearest-even */
                UINT32_C(0xFC00), /* negative, round-to-nearest-even */
                };
            return oftab[idx];                
            }
        
        /*    
        the other case is denormals, for which res_exp_m1 is
        essentially negative. In this case, we
        just adjust the leading-zeroes-count and
        the result exponent.
        */
        lz += res_exp_m1;
        res_exp_m1 = 0;
        /* since we are doing a narrowing addition, denormals may underflow badly. */
        if( lz < 0 )
            {
            int32_t rlz = -lz;
            if( rlz > 31 )
                rlz = 31;
                
            mantres = sticky_urshift32( mantres, rlz );
            lz = 0;
            }    
        }
    
    mantres <<= lz; /* ends up in range [2^31, 2^32) for non-denormals. */
        {
	    static const uint32_t add_tab[10] = {
	        UINT32_C(0x1FFFFF), /* positive, round up */
	        UINT32_C(0x000000), /* negative, round up */
	        UINT32_C(0x000000), /* positive, round down */
	        UINT32_C(0x1FFFFF), /* negative, round down */
	        UINT32_C(0x000000), /* positive, truncate */
	        UINT32_C(0x000000), /* negative, truncate */
	        UINT32_C(0x0FFFFF), /* positive, round-to-nearest-even */
	        UINT32_C(0x0FFFFF), /* negative, round-to-nearest-even */
	        UINT32_C(0x100000), /* positive, round-to-nearest-away */
	        UINT32_C(0x100000), /* negative, round-to-nearest-away */
	        };
	    static const uint32_t rtne_tab[10] = { 0,0,0,0,0,0,1,1,0,0 };
	    
	    /* value to add for rounding purposes. */
	    uint32_t mr_add = add_tab[idx] + (rtne_tab[idx] & (mantres >> 21) );
	    
	    /*
	    this addition can overflow, in which case we need to increment
	    the exponent.
	    */
	    mantres += mr_add;
	    if( mantres < mr_add )
	        {
	        /*
	        if the addition did overflow, the exponent needs to be increased
	        and the mantissa right-shifted.
	        This gives the correct result even if the overflow tips
	        over into infinity.
	        */
	        mantres >>= 1;
	        mantres |= UINT32_C(0x80000000);
	        res_exp_m1++;
	        }
	    
	    /*
	    finally, combine a result. Note that the MSB of the mantissa is not
	    removed before this calculation, so we need to compute exponent
	    minus 1 (except in the case of a denormal).
	    */
	    return ((maxv >> 16) & 0x8000U) + (res_exp_m1 << 10) + (mantres >> 21);
	    }
	}	



/*
64->32-bit narrowing floating-point add
*/
sf32 _mali_narrow_add_sf64( sf64 a, sf64 b, roundmode rm, uint32_t nan_payload )
    {
    
    /*    
    pick the larger of the two input values. The computation
    below left-rotates both input values by 1 bit each
    and then conditionally the LSB. 
    Flipping the LSB has the result that if the two input numbers
    are of equal magnitude and opposite sign, the positive number is
    considered "larger". This causes cancellation to result in +0.
    This is correct in most rounding-modes; however in rounding mode DOWN
    such a cancellation should result in -0; this is achieved by abstaining
    from flipping the bit.
    */
    
    static const uint64_t fbittab[5] = {UINT64_C(1),UINT64_C(0),UINT64_C(1),UINT64_C(1),UINT64_C(1)};
    uint64_t a_lrot = (a << 1) | (a >> 63);
    uint64_t b_lrot = (b << 1) | (b >> 63);
    uint64_t fbit = fbittab[rm];
    uint64_t maxv, minv;
    uint32_t exp_maxv;
    uint32_t exp_minv;
    uint64_t mask;
    uint32_t idx;
    uint64_t mant_maxv;
    uint64_t mant_minv;
    uint32_t shamt;
    uint64_t mant_minv2;
    uint64_t mantres;
    int64_t lz;
    uint64_t res_exp_m1;


    a_lrot ^= fbit;
    b_lrot ^= fbit;
        
    if( a_lrot > b_lrot )
        {
        maxv = a;
        minv = b;
        }
    else
        {
        maxv = b;
        minv = a;
        }
        
    exp_maxv = (uint32_t)(maxv >> 52) & 0x7FF;
    exp_minv = (uint32_t)(minv >> 52) & 0x7FF;

    mask = (int64_t)(maxv ^ minv) >> 63; /* all 1s if operation is eff-subtract, all 0s otherwise. */
    
    if( exp_maxv == 0x7FF )
        {
        /* INF/NaN inputs need to be handled specially. */
        
        /*
        If the "largest" number is a NaN, then we must return one of the inputs.
        In this case, if 'a' is NaN, then we return a quietened version of 'a',
        else we return a quietened version of 'b'.
        */
        if( (maxv & UINT64_C(0x7FFFFFFFFFFFFFFF)) > UINT64_C(0x7FF0000000000000) )
            {


            /* one or both of the inputs was a NaN. */
            int a_nan = _mali_isnan_sf64( a );
            int b_nan = _mali_isnan_sf64( b );
            uint64_t a_nanval = a;
            uint64_t b_nanval = b;
            uint64_t retval;
            if( a_nan && b_nan )
                retval = (_mali_ord_sf64(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT64_C(0x8000000000000);
            else if( a_nan )
                retval = a_nanval | UINT64_C(0x8000000000000);
            else
                retval = b_nanval | UINT64_C(0x8000000000000);
            return _mali_sf64_to_sf32( retval, rm );
            }
        
        /*
        if we come here, the largest input was an INF. Construct a NaN
        if the other input is an Inf of the opposite sign, else return the INF.
        */
        if( exp_maxv == exp_minv && mask != UINT64_C(0))
            return UINT32_C(0x7FC00000) | nan_payload;
        else
            return _mali_sf64_to_sf32( maxv, rm );
        }

    /*
    number from 0 to 9, indicating effective rounding mode,
    constructed from input rounding mode and the sign of the maximum vale.
    */
    idx = (rm << 1) + (uint32_t)(maxv >> 63); 
    
    /* extract mantissas. */
    mant_maxv = (maxv & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000);
    mant_minv = (minv & UINT64_C(0xFFFFFFFFFFFFF)) + UINT64_C(0x10000000000000);
    
    if( exp_minv == 0 )
        {
        /*
        if either input is a denormal, we need to perform some adjustments:
        set the exponent to 1, and clear the "implied" bit of the mantissa.
        */
        uint32_t exp_maxv_zero = (0x800 - exp_maxv) >> 11;
        uint32_t exp_minv_zero = (0x800 - exp_minv) >> 11;
        exp_maxv += exp_maxv_zero;
        exp_minv += exp_minv_zero;
        mant_maxv -= (uint64_t)exp_maxv_zero << 52;
        mant_minv -= (uint64_t)exp_minv_zero << 52;
        }


    /* shift-amount for the shift before the actual addition. */
    shamt = exp_maxv - exp_minv;
    if( shamt > 63 ) 
        shamt = 63;


    /* left-shift both inputs 3 places, then right-shift the smaller mantissa. */
    mant_maxv <<= 3; /* in the range [2^26, 2^27); 5 leading zeroes. */
    mant_minv <<= 3;
    mant_minv2 = sticky_urshift64( mant_minv, shamt ); /* sticky right-shift. */
    
    /* negate the smaller mantissa */
    mant_minv2 ^= mask;
    
    /* perform the actual addition. */
    mantres = mant_maxv + mant_minv2 - mask;

    /* check for exact cancellation */
    if( mantres == UINT64_C(0) && mant_maxv != UINT64_C(0))
        {
        /*
        exact cancellation. Return -0 in the round-DOWN mode and +0 in all other modes.
        */
        static const uint32_t uftab[5] =
            {
            UINT32_C(0x00000000), /* round UP */
            UINT32_C(0x80000000), /* round DOWN */
            UINT32_C(0x00000000), /* round DOWN */
            UINT32_C(0x00000000), /* round NEARESTEVEN */
            UINT32_C(0x00000000), /* round NEARESTAWAY */
            };
        return uftab[rm];                   
        }
    
    /* normalize the number with a left-shift. */
    lz = clz64( mantres );

    res_exp_m1 = exp_maxv - lz + 7 - 896;
    if( res_exp_m1 >= UINT32_C(0xFE) )
        {
        if( (int32_t)res_exp_m1 > 0 )
            {
            /* overflow: return the overflow value. */
            static const uint32_t oftab[10] = {
                UINT32_C(0x7F800000), /* positive, round up */
                UINT32_C(0xFF7FFFFF), /* negative, round up */
                UINT32_C(0x7F7FFFFF), /* positive, round down */
                UINT32_C(0xFF800000), /* negative, round down */
                UINT32_C(0x7F7FFFFF), /* positive, truncate */
                UINT32_C(0xFF7FFFFF), /* negative, truncate */
                UINT32_C(0x7F800000), /* positive, round-to-nearest-even */
                UINT32_C(0xFF800000), /* negative, round-to-nearest-even */
                UINT32_C(0x7F800000), /* positive, round-to-nearest-even */
                UINT32_C(0xFF800000), /* negative, round-to-nearest-even */
                };
            return oftab[idx];                
            }
            
        /*
        the other case is denormals, for which res_exp_m1 is
        essentially negative. In this case, we
        just adjust the leading-zeroes-count and
        the result exponent.
        */
        lz += res_exp_m1;
        res_exp_m1 = 0;
        if( lz < 0 )
            {
            uint32_t rlz = (uint32_t)-lz;
            if( rlz > 63 )
                rlz = 63;
            mantres = sticky_urshift64( mantres, rlz );
            lz = 0;
            }
        }
    
    mantres <<= lz; /* ends up in range [2^63, 2^64) for non-denormals. */
        {
	    static const uint64_t add_tab[10] = {
	        UINT64_C(0xFFFFFFFFFF), /* positive, round up */
	        UINT64_C(0x0000000000), /* negative, round up */
	        UINT64_C(0x0000000000), /* positive, round down */
	        UINT64_C(0xFFFFFFFFFF), /* negative, round down */
	        UINT64_C(0x0000000000), /* positive, truncate */
	        UINT64_C(0x0000000000), /* negative, truncate */
	        UINT64_C(0x7FFFFFFFFF), /* positive, round-to-nearest-even */
	        UINT64_C(0x7FFFFFFFFF), /* negative, round-to-nearest-even */
	        UINT64_C(0x8000000000), /* positive, round-to-nearest-away */
	        UINT64_C(0x8000000000), /* negative, round-to-nearest-away */
	        };
	    static const uint64_t rtne_tab[10] = { 0,0,0,0,0,0,1,1,0,0 };
	    
	    /* value to add for rounding purposes. */
	    uint64_t mr_add = add_tab[idx] + (rtne_tab[idx] & (mantres >> 40) );
	    
	    /*
	    this addition can overflow, in which case we need to increment
	    the exponent.
	    */
	    mantres += mr_add;
	    if( mantres < mr_add )
	        {
	        /*
	        if the addition did overflow, the exponent needs to be increased
	        and the mantissa right-shifted.
	        This gives the correct result even if the overflow tips
	        over into infinity.
	        */
	        mantres >>= 1;
	        mantres |= UINT64_C(0x8000000000000000);
	        res_exp_m1++;
	        }
	    
	    /*
	    finally, combine a result. Note that the MSB of the mantissa is not
	    removed before this calculation, so we need to compute exponent
	    minus 1 (except in the case of a denormal).
	    */
	    return (sf32)(((maxv >> 32) & UINT64_C(0x80000000)) + (res_exp_m1 << 23) + (mantres >> 40));
	    }
	}



/*****************************************
  Fused Multiply-Add operations
*****************************************/

/*
For 16 and 32 bit fused-multiply-add, we compose the
operation from a widening multiply and a narrowing addition;
as with plain multiply, we cannot perform such a trick
with 64-bit fused-multiply-adds.

These implementations disagree with the glibc fma/fmaf library functions;
this is a bug in glibc, not this library.
*/


sf16 _mali_fma_sf16( sf16 a, sf16 b, sf16 c, roundmode rm, uint16_t nan_payload )
    {
    uint32_t nan_payload2 = (uint32_t)nan_payload << 13;
    return _mali_narrow_add_sf32( 
        _mali_widen_mul_sf16( a,b, nan_payload2), 
        _mali_sf16_to_sf32( c ),
        rm,
        nan_payload );
    }


sf32 _mali_fma_sf32( sf32 a, sf32 b, sf32 c, roundmode rm, uint32_t nan_payload )
    {
    uint64_t nan_payload2 = (uint64_t)nan_payload << 29;
    return _mali_narrow_add_sf64( 
        _mali_widen_mul_sf32( a,b, nan_payload2), 
        _mali_sf32_to_sf64( c ),
        rm,
        nan_payload );
    }










/***************************
  64-bit fused-multiply-add.
*****************************/

/*
This is a highly complex operation, due to rounding requirements, number of cases,
and the large amount of multiprecision arithmetic needed. 
*/

/*
 first, a library of 128-bit arithmetic functions to make things a bit easier.
*/



/* signed right-shift with sticky bit. */
static SOFTFLOAT_INLINE v128 sticky_srshift128( v128 inp, uint32_t shamt )
    {
    v128 res;
    uint32_t det0 = (uint32_t)(-(int32_t)(shamt & 0x3F)); /* zero if low 6 bits are zero, else MSB is set. */
    uint32_t det = (shamt >> 5) | (det0 >> 31); /* shift amount (less bottom 6 bits) concatenated with the sign of 'det'. */
    uint32_t shamt2;
    uint64_t mask;
    switch( det )
        {
        case 0: /* shift amount is zero. */
            res = inp;
            break;
        case 1: /* shift amount in the range [1,63] */
            mask = (UINT64_C(1) << shamt) - 1; 
            res.vl[1] = (int64_t)inp.vl[1] >> shamt;
            res.vl[0] = (inp.vl[0] >> shamt) | (inp.vl[1] << (64-shamt));
            if( (mask & inp.vl[0]) != UINT64_C(0) )
                res.vl[0] |= UINT64_C(1);
            break;
        case 2: /* shift amount is 64. */
            res.vl[1] = (int64_t)inp.vl[1] >> 63;
            res.vl[0] = inp.vl[1];
            if( inp.vl[0] != UINT64_C(0))
                res.vl[0] |= UINT64_C(1);
            break;
        case 3: /* shift amount in the range [65,127] */
            shamt2 = shamt & 0x3F;
            mask = (UINT64_C(1) << shamt2) - 1;
            res.vl[1] = (int64_t)inp.vl[1] >> 63;
            res.vl[0] = (int64_t)inp.vl[1] >> shamt2;
            if( (inp.vl[0] | (inp.vl[1] & mask)) != UINT64_C(0))
                res.vl[0] |= UINT64_C(1);
            break;
        default: /* shift-amount is 128 or more. */
            res.vl[0] = res.vl[1] = (int64_t)inp.vl[1] >> 63;
            if( (inp.vl[0] | inp.vl[1]) != UINT64_C(0) )
                res.vl[0] |= UINT64_C(1);
            break;
        }    
        
        
        
    return res;    
    }


/* left-shift. */
static SOFTFLOAT_INLINE v128 lshift128( v128 inp, uint32_t shamt )
    {
    v128 res;
    uint32_t det0 = (uint32_t)(-(int32_t)(shamt & 0x3F)); /* zero if low 6 bits are zero, else MSB is set. */
    uint32_t det = (shamt >> 5) | (det0 >> 31);  /* shift amount (less bottom 6 bits) concatenated with the sign of 'det'. */ 
    switch( det )
        {
        case 0: /* shift amount is zero. */
            res = inp;
            break;
        case 1: /* shift amount is in range [1,63] */
            res.vl[0] = inp.vl[0] << shamt;
            res.vl[1] = (inp.vl[1] << shamt) | (inp.vl[0] >> (64-shamt));
            break;
        case 2: /* shift amount is 64. */
            res.vl[1] = inp.vl[0];
            res.vl[0] = UINT64_C(0);
            break;
        case 3: /* shift amount is in range [65,127] */
            res.vl[1] = inp.vl[0] << (shamt & 0x3F);
            res.vl[0] = UINT64_C(0);
            break;
        default: /* shift amount is 128 or more. */
            res.vl[0] = res.vl[1] = UINT64_C(0);
            break;        
        }
    
    
    return res;    
    }


/* 128-bit count leading zeroes. */
static SOFTFLOAT_INLINE uint32_t clz128( v128 inp )
    {
    uint32_t res;
    if( inp.vl[1] == UINT64_C(0) )
        {
        res = clz64( inp.vl[0] ) + 64;
        }
    else
        res = clz64( inp.vl[1] );
    
    return res;
    }


/* 128-bit addition */
static SOFTFLOAT_INLINE v128 add128( v128 a, v128 b )
    {
    v128 res;
    uint64_t aL = a.vl[0];
    uint64_t bL = b.vl[0];
    uint64_t aH = a.vl[1];
    uint64_t bH = b.vl[1];
    uint64_t cL = aL + bL;
    uint64_t cH = aH + bH;
    if( cL < aL )
        cH++;
    res.vl[0] = cL;
    res.vl[1] = cH;
    return res;
    }


/* 128-bit negation. */
static SOFTFLOAT_INLINE v128 neg128( v128 inp )
    {
    v128 res;
    uint64_t aL0 = ~inp.vl[0];
    uint64_t aH = ~inp.vl[1];
    uint64_t aL = aL0 + 1;
    if( aL < aL0 )
        aH++;
    res.vl[0] = aL;
    res.vl[1] = aH;
    return res;  
    }



/* 128.bit greater-than comparison. */
static SOFTFLOAT_INLINE int greaterthan128( v128 a, v128 b ) /* a greater than b */
    {
    return (a.vl[1] > b.vl[1] || (a.vl[1] == b.vl[1] && a.vl[0] > b.vl[0]));
    }


/*
then we have the main 64-bit fused-multiply-add function.

Unlike the 16/32-bit versions, it cannot be assembled by a plain widening multiply
followed by an add and a narrowing conversion; we need to perform this one in full.
*/

sf64 _mali_fma_sf64( sf64 a, sf64 b, sf64 c, roundmode rm, uint64_t nan_payload )
    {
    
    /*
    if ANY of the input numbers are Zero, Inf or NaN, then
    FMA is equivalent to multiply followed by add; it produces the
    same special cases.
    */
    
    uint64_t signmask = UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t a_abs = a & signmask;
    uint64_t b_abs = b & signmask;
    uint64_t c_abs = c & signmask;
    
    uint64_t aam1 = (a_abs - UINT64_C(1));
    uint64_t bam1 = (b_abs - UINT64_C(1));
    uint64_t cam1 = (c_abs - UINT64_C(1));
    uint64_t ab_sign;
    uint64_t c_sign;
    
    int32_t a_exp;
    int32_t b_exp;
    int32_t c_exp;
    uint64_t a_mant;
    uint64_t b_mant;
    uint64_t c_mant;
    int32_t ab_exp;
    v128 multres;
    v128 cx;
    uint64_t res_sign;
    int32_t res_exp;
	v128 summa;
    uint32_t lz;
    v128 norm_summa;
	uint64_t mant;
	uint32_t idx;
    if( aam1 >= UINT64_C(0x7FEFFFFFFFFFFFFF)
        || bam1 >= UINT64_C(0x7FEFFFFFFFFFFFFF)
        || cam1 >= UINT64_C(0x7FEFFFFFFFFFFFFF) )
        {
        /* at least one of the input numbers is Inf, NaN or Zero */
        return _mali_add_sf64( _mali_mul_sf64( a, b, rm, nan_payload ), c, rm, nan_payload );
        }
    
    /* split each of the input numbers into exponent and mantissa. Also extract signs. */
    ab_sign = (a^b) & UINT64_C(0x8000000000000000);
    c_sign = c & UINT64_C(0x8000000000000000);
    
    a_exp = (int32_t)(a_abs >> 52);
    b_exp = (int32_t)(b_abs >> 52);
    c_exp = (int32_t)(c_abs >> 52);
    
    /* extract mantissas, taking into account special cases like denormals */
    
    if( a_exp == 0 )
        {
        uint32_t lz = clz64( a_abs );
        a_mant = (a_abs << lz) >> 11;
        a_exp = 12 - lz;
        }
    else
        a_mant = (a & UINT64_C(0xFFFFFFFFFFFFF)) | UINT64_C(0x10000000000000);
    
    if( b_exp == 0 )
        {
        uint32_t lz = clz64( b_abs );
        b_mant = (b_abs << lz) >> 11;
        b_exp = 12 - lz;
        }
    else
        b_mant = (b & UINT64_C(0xFFFFFFFFFFFFF)) | UINT64_C(0x10000000000000);
    
    if( c_exp == 0 )
        {
        uint32_t lz = clz64( c_abs );
        c_mant = (c_abs << lz) >> 11;
        c_exp = 12 - lz;
        }
    else
        c_mant = (c & UINT64_C(0xFFFFFFFFFFFFF)) | UINT64_C(0x10000000000000);
    
    
    a_mant <<= 9; /* number in range [2^61, 2^62) */
    b_mant <<= 9; /* number in range [2^61, 2^62) */
    c_mant <<= 7; /* number in range [2^59, 2^60) */
        
    ab_exp = a_exp + b_exp - 1023; /* exponent of the actual multiply. */
    
    
    /*
    perform the multiply.
    multres is within the range [2^122, 2^124)
    */
    multres = widening_mul( a_mant, b_mant );

    /*
    if multiply result is less than 2^123, left-shift it by 1 bit.
    */
    if( multres.vl[1] < UINT64_C(0x800000000000000) )
        {
        multres.vl[1] = (multres.vl[1] << 1) | (multres.vl[0] >> 63);
        multres.vl[0] <<= 1;
        }
    else
        ab_exp++;
    /* multres now within range [2^123, 2^124) */
    
    /* convert the C mantissa into a 128-bit value. */
    cx.vl[0] = UINT64_C(0);
    cx.vl[1] = c_mant;
    /* cx now within range [2^123, 2^124) */
    
    /* compare the exponents */
    if( ab_exp == c_exp )
        {
        /*
        Same exponent on a*b and c: 
        compare their signs. If they have opposite signs, negate the smaller one.
        */
        res_exp = c_exp;
        if( ab_sign != c_sign )
            {
            if( greaterthan128( multres, cx ) )
                {
                res_sign = ab_sign;
                cx = neg128(cx);
                }
            else
                {
                res_sign = c_sign;
                multres = neg128(multres);
                }
            }
        else
            res_sign = c_sign;
        }
        
    else if( ab_exp > c_exp )
        {
        /*
        a*b has a larger exponent than c. 
        In this case, negate C if its sign is opposite of a*b, 
        then perform sticky-right-shift.
        */
        res_sign = ab_sign;
        if( ab_sign != c_sign )
            cx = neg128( cx );
        cx = sticky_srshift128( cx, ab_exp - c_exp );
        res_exp = ab_exp;
        }
    else
        {
        /*
        c has larger exponent than a*b. In this case,
        negate c if its sign is opposite of a*b
        then perform sticky-right-shift.
        */
        res_sign = c_sign;
        if( ab_sign != c_sign )
            multres = neg128( multres );
        multres = sticky_srshift128( multres, c_exp - ab_exp );
        res_exp = c_exp;
        }
    
    /*
    at this point:
     * add the two input numbers.
     * perform bit-scan and left-shift.
     * this gives us a pair of exponent and normalized mantissa.
    */
    
    summa = add128( multres, cx );
    
    
    if( (summa.vl[0] | summa.vl[1]) == UINT64_C(0) )
        {
        /*
        exact cancellation. Return -0 in the round-DOWN mode and +0 in all other modes.
        */
        static const uint64_t uftab[5] =
            {
            UINT64_C(0x0000000000000000), /* round UP */
            UINT64_C(0x8000000000000000), /* round DOWN */
            UINT64_C(0x0000000000000000), /* round DOWN */
            UINT64_C(0x0000000000000000), /* round NEARESTEVEN */
            UINT64_C(0x0000000000000000), /* round NEARESTAWAY */
            };
        return uftab[rm];                   
        }
            
    /*
    summa is within the range [0, 2^125)
    thus will have zeroes in its top 3 bits. If the result
    has the same exponent as the larger add-input, then it has 4
    leading zeroes. lz will never be smaller than 3.
    */
    
    lz = clz128( summa );
    norm_summa = lshift128( summa, lz - 2 );
    
    /*
    at this point: norm_summa is within the range [2^125, 2^126)
    */
    
    res_exp += 4 - lz;
    
    /*
    reduce the normalized-sum from 128 to 64 bits. This is
    done stickily.    
    */
    mant = norm_summa.vl[1];
    if( norm_summa.vl[0] != UINT64_C(0) )
        mant |= 1;
    
    /* at this point, norm_summa has a value in the range [2^61, 2^62) */    
    
    /* finally perform rounding and package the result into an FP64. */
    idx = (rm << 1) | (uint32_t)(res_sign >> 63);
    if( res_exp >= 2047 )
        {
        /*
        in this case, the final result is an overflow.
        table containing values to return if the result is too large to be representable.
        */
        static const uint64_t large_tab[10] =
            {
            UINT64_C(0x7FF0000000000000), /* positive, round up */
            UINT64_C(0xFFEFFFFFFFFFFFFF), /* negative, round up */
            UINT64_C(0x7FEFFFFFFFFFFFFF), /* positive, round down */
            UINT64_C(0xFFF0000000000000), /* negative, round down */
            UINT64_C(0x7FEFFFFFFFFFFFFF), /* positive, round to zero */
            UINT64_C(0xFFEFFFFFFFFFFFFF), /* negative, round to zero */
            UINT64_C(0x7FF0000000000000), /* positive, round to nearest-even */
            UINT64_C(0xFFF0000000000000), /* negative, round to nearest-even */
            UINT64_C(0x7FF0000000000000), /* positive, round to nearest-away */
            UINT64_C(0xFFF0000000000000)  /* negative, round to nearest-away */
            };
        return large_tab[idx];        
        }
    
    if( res_exp <= -53 )
        {
        /*
        in this case, the final result is an underflow.
        table containing values to return if the result is too small to be representable.
        */
        static const uint64_t small_tab[10] =
            {
            UINT64_C(0x0000000000000001), /* positive, round up */
            UINT64_C(0x8000000000000000), /* negative, round up */
            UINT64_C(0x0000000000000000), /* positive, round down */
            UINT64_C(0x8000000000000001), /* negative, round down */
            UINT64_C(0x0000000000000000), /* positive, round to zero */
            UINT64_C(0x8000000000000000), /* negative, round to zero */
            UINT64_C(0x0000000000000000), /* positive, round to nearest-even */
            UINT64_C(0x8000000000000000), /* negative, round to nearest-even */
            UINT64_C(0x0000000000000000), /* positive, round to nearest-away */
            UINT64_C(0x8000000000000000)  /* negative, round to nearest-away */
            };
        return small_tab[idx];
        }
    
    
    if( res_exp <= 0 )
        {
        /*
        result is a denormal; perform right-shift of the mantissa using one of the
        rounding shift-operations as appropriate for the rounding-mode, then
        OR in the sign-bit.
        */
        uint32_t shamt = 10 - res_exp;
        switch( idx )
            {
            case 0:
            case 3: /* round away from zero */
                return rtup_shift64( mant, shamt ) | res_sign;
            case 1:
            case 2:
            case 4:
            case 5: /* round towards zero */
                return (mant >> shamt) | res_sign;
            case 6:
            case 7: /* round to nearest even */
                return rtne_shift64( mant, shamt ) | res_sign;
            case 8:
            case 9: /* round to nearest away */
                return rtna_shift64( mant, shamt ) | res_sign;
            }
        }
    
    /* result is a normal value. Add a round-mode dependent constant. */
		{
	    static const uint64_t rcc_table[10] =
	        {
	        UINT64_C(0x1FF), /* positive, round up */
	        UINT64_C(0x000), /* negative, round up */
	        UINT64_C(0x000), /* positive, round down */
	        UINT64_C(0x1FF), /* negative, round down */
	        UINT64_C(0x000), /* positive, round to zero */
	        UINT64_C(0x000), /* negative, round to zero */
	        UINT64_C(0x0FF), /* positive, round to nearest-even */
	        UINT64_C(0x0FF), /* negative, round to nearest-even */
	        UINT64_C(0x100), /* positive, round to nearest-away */
	        UINT64_C(0x100), /* negative, round to nearest-away */
	        };
	        
	    static const uint64_t rtne_table[10] =
	        { 0,0,0,0,0,0,1,1,0,0 };    
	    
	    res_exp--; /* compensate for carry-in from the mantissa. */
	    mant += (mant >> 9) & rtne_table[idx];
	    mant += rcc_table[idx];
	    return (mant >> 9) + ((int64_t)res_exp << 52) + res_sign;
	    }
	}



/*
testbench-equal functions:
two NaNs always compare equal, everything else
compares not-equal
*/

int _mali_equal_tb_sf16( sf16 a, sf16 b )
    {
    uint16_t a_abs = a & UINT32_C(0x7FFF);
    uint16_t b_abs = b & UINT32_C(0x7FFF);
    uint16_t det = (a_abs + UINT32_C(0x3FF)) & (b_abs + UINT32_C(0x3FF));
    return (det >= UINT32_C(0x8000) || a == b);
    }

int _mali_equal_tb_sf32( sf32 a, sf32 b )
    {
    uint32_t a_abs = a & UINT32_C(0x7FFFFFFF);
    uint32_t b_abs = b & UINT32_C(0x7FFFFFFF);
    uint32_t det = (a_abs + UINT32_C(0x7FFFFF)) & (b_abs + UINT32_C(0x7FFFFF));
    return (det >= UINT32_C(0x80000000) || a == b);
    }

int _mali_equal_tb_sf64( sf64 a, sf64 b )
    {
    uint64_t a_abs = a & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t b_abs = b & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t det = (a_abs + UINT64_C(0xFFFFFFFFFFFFF)) & (b_abs + UINT64_C(0xFFFFFFFFFFFFF));
    return (det >= UINT64_C(0x8000000000000000) || a == b);
    }


/****************************************
 MinMax functions.
****************************************/

/*
For Min/Max, we first check for NaNs,
then we XOR all non-sign bits with the sign bit, then
we perform a signed compare used to choose a return value.

The 'n' variants will, when given a NaN argument, return the
other argument; the non-'n' variants will, upon encountering
a NaN, return the NaN.
*/


sf16 _mali_min_nan_propagate_sf16( sf16 a, sf16 b )
    {
    uint16_t aabs = a & UINT32_C(0x7FFF);
    uint16_t babs = b & UINT32_C(0x7FFF);
    uint16_t det = (aabs + UINT32_C(0x3FF)) | (babs + UINT32_C(0x3FF));
    int16_t am;
    int16_t bm;
    if( det >= UINT32_C(0x8000) )
        {
        /* one of the operands is a NaN; find out which one and return it.
        If both are NaN, perform TotalOrder. */
        int a_nan = _mali_isnan_sf16( a );
        int b_nan = _mali_isnan_sf16( b );
        uint32_t a_nanval = a;
        uint32_t b_nanval = b;
        if( a_nan && b_nan )
            return (_mali_ord_sf16(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x200);
        else if( a_nan )
            return a_nanval | UINT32_C(0x200);
        else
            return b_nanval | UINT32_C(0x200);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint16_t)((int16_t)a >> 15) >> 1);
    bm = b ^ ( (uint16_t)((int16_t)b >> 15) >> 1);
    return am < bm ? a : b;
    }


sf16 _mali_max_nan_propagate_sf16( sf16 a, sf16 b )
    {
    uint16_t aabs = a & UINT32_C(0x7FFF);
    uint16_t babs = b & UINT32_C(0x7FFF);
    uint16_t det = (aabs + UINT32_C(0x3FF)) | (babs + UINT32_C(0x3FF));
    int16_t am;
    int16_t bm;
    if( det >= UINT32_C(0x8000) )
        {
        /* one of the operands is a NaN; find out which one and return it.
        If both are NaN, perform TotalOrder. */
        int a_nan = _mali_isnan_sf16( a );
        int b_nan = _mali_isnan_sf16( b );
        uint32_t a_nanval = a;
        uint32_t b_nanval = b;
        if( a_nan && b_nan )
            return (_mali_ord_sf16(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x200);
        else if( a_nan )
            return a_nanval | UINT32_C(0x200);
        else
            return b_nanval | UINT32_C(0x200);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint16_t)((int16_t)a >> 15) >> 1);
    bm = b ^ ( (uint16_t)((int16_t)b >> 15) >> 1);
    return am > bm ? a : b;
    }



sf32 _mali_min_nan_propagate_sf32( sf32 a, sf32 b )
    {
    uint32_t aabs = a & UINT32_C(0x7FFFFFFF);
    uint32_t babs = b & UINT32_C(0x7FFFFFFF);
    uint32_t det = (aabs + UINT32_C(0x7FFFFF)) | (babs + UINT32_C(0x7FFFFF));
    int32_t am;
    int32_t bm;
    if( det >= UINT32_C(0x80000000) )
        {
        /* one of the operands is a NaN; find out which one and return it.
        If both are NaN, perform TotalOrder. */
        int a_nan = _mali_isnan_sf32( a );
        int b_nan = _mali_isnan_sf32( b );
        uint32_t a_nanval = a;
        uint32_t b_nanval = b;
        if( a_nan && b_nan )
            return (_mali_ord_sf32(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x400000);
        else if( a_nan )
            return a_nanval | UINT32_C(0x400000);
        else
            return b_nanval | UINT32_C(0x400000);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint32_t)((int32_t)a >> 31) >> 1);
    bm = b ^ ( (uint32_t)((int32_t)b >> 31) >> 1);
    return am < bm ? a : b;
    }


sf32 _mali_max_nan_propagate_sf32( sf32 a, sf32 b )
    {
    uint32_t aabs = a & UINT32_C(0x7FFFFFFF);
    uint32_t babs = b & UINT32_C(0x7FFFFFFF);
    uint32_t det = (aabs + UINT32_C(0x7FFFFF)) | (babs + UINT32_C(0x7FFFFF));
    int32_t am;
    int32_t bm;
    if( det >= UINT32_C(0x80000000) )
        {
        /* one of the operands is a NaN; find out which one and return it.
        If both are NaN, perform TotalOrder. */
        int a_nan = _mali_isnan_sf32( a );
        int b_nan = _mali_isnan_sf32( b );
        uint32_t a_nanval = a;
        uint32_t b_nanval = b;
        if( a_nan && b_nan )
            return (_mali_ord_sf32(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x400000);
        else if( a_nan )
            return a_nanval | UINT32_C(0x400000);
        else
            return b_nanval | UINT32_C(0x400000);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint32_t)((int32_t)a >> 31) >> 1);
    bm = b ^ ( (uint32_t)((int32_t)b >> 31) >> 1);
    return am > bm ? a : b;
    }



sf64 _mali_min_nan_propagate_sf64( sf64 a, sf64 b )
    {
    uint64_t aabs = a & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t babs = b & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t det = (aabs + UINT64_C(0xFFFFFFFFFFFFF)) | (babs + UINT64_C(0xFFFFFFFFFFFFF));
    int64_t am;
    int64_t bm;
    if( det >= UINT64_C(0x8000000000000000) )
        {
        /* one of the operands is a NaN; find out which one and return it.
        If both are NaN, perform TotalOrder. */
        int a_nan = _mali_isnan_sf64( a );
        int b_nan = _mali_isnan_sf64( b );
        uint64_t a_nanval = a;
        uint64_t b_nanval = b;
        if( a_nan && b_nan )
            return (_mali_ord_sf64(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT64_C(0x8000000000000);
        else if( a_nan )
            return a_nanval | UINT64_C(0x8000000000000);
        else
            return b_nanval | UINT64_C(0x8000000000000);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint64_t)((int64_t)a >> 63) >> 1);
    bm = b ^ ( (uint64_t)((int64_t)b >> 63) >> 1);
    return am < bm ? a : b;
    }


sf64 _mali_max_nan_propagate_sf64( sf64 a, sf64 b )
    {
    uint64_t aabs = a & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t babs = b & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t det = (aabs + UINT64_C(0xFFFFFFFFFFFFF)) | (babs + UINT64_C(0xFFFFFFFFFFFFF));
    int64_t am;
    int64_t bm;
    if( det >= UINT64_C(0x8000000000000000) )
        {
        /* one of the operands is a NaN; find out which one and return it.
        If both are NaN, perform TotalOrder. */
        int a_nan = _mali_isnan_sf64( a );
        int b_nan = _mali_isnan_sf64( b );
        uint64_t a_nanval = a;
        uint64_t b_nanval = b;
        if( a_nan && b_nan )
            return (_mali_ord_sf64(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT64_C(0x8000000000000);
        else if( a_nan )
            return a_nanval | UINT64_C(0x8000000000000);
        else
            return b_nanval | UINT64_C(0x8000000000000);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint64_t)((int64_t)a >> 63) >> 1);
    bm = b ^ ( (uint64_t)((int64_t)b >> 63) >> 1);
    return am > bm ? a : b;
    }




sf16 _mali_min_sf16( sf16 a, sf16 b )
    {
    uint16_t aabs = a & UINT32_C(0x7FFF);
    uint16_t babs = b & UINT32_C(0x7FFF);
    uint16_t det = (aabs + UINT32_C(0x3FF)) | (babs + UINT32_C(0x3FF));
    int16_t am;
    int16_t bm;
    if( det >= UINT32_C(0x8000) )
        {
        /* one of the operands is a NaN. */
        uint16_t a_nanval, b_nanval;
        if( aabs <= UINT32_C(0x7C00) )
            return a; /* A is not NaN, return it */ 
        if( babs <= UINT32_C(0x7C00) )
            return b; /* B is not NaN, return it */
        /* at this point, both A and B must be NaN; use TotalOrder to pick one to return. */
        a_nanval = a;
        b_nanval = b;
        return (_mali_ord_sf16(a_nanval, b_nanval) ? b_nanval : a_nanval)  | 0x200;
        }
    /* neither operand is NaN */
    am = a ^ ( (uint16_t)((int16_t)a >> 15) >> 1);
    bm = b ^ ( (uint16_t)((int16_t)b >> 15) >> 1);
    return am < bm ? a : b;
    }



sf16 _mali_max_sf16( sf16 a, sf16 b )
    {
    uint16_t aabs = a & UINT32_C(0x7FFF);
    uint16_t babs = b & UINT32_C(0x7FFF);
    uint16_t det = (aabs + UINT32_C(0x3FF)) | (babs + UINT32_C(0x3FF));
    int16_t am;
    int16_t bm;
    if( det >= UINT32_C(0x8000) )
        {
        /* one of the operands is a NaN. */
        uint16_t a_nanval, b_nanval;
        if( aabs <= UINT32_C(0x7C00) )
            return a; /* A is not NaN, return it */ 
        if( babs <= UINT32_C(0x7C00) )
            return b; /* B is not NaN, return it */
        /* at this point, both A and B must be NaN; use TotalOrder to pick one to return. */
        a_nanval = a;
        b_nanval = b;
        return (_mali_ord_sf16(a_nanval, b_nanval) ? b_nanval : a_nanval) | 0x200;
        }
    /* neither operand is NaN */
    am = a ^ ( (uint16_t)((int16_t)a >> 15) >> 1);
    bm = b ^ ( (uint16_t)((int16_t)b >> 15) >> 1);
    return am > bm ? a : b;
    }



sf32 _mali_min_sf32( sf32 a, sf32 b )
    {
    uint32_t aabs = a & UINT32_C(0x7FFFFFFF);
    uint32_t babs = b & UINT32_C(0x7FFFFFFF);
    uint32_t det = (aabs + UINT32_C(0x7FFFFF)) | (babs + UINT32_C(0x7FFFFF));
    int32_t am;
    int32_t bm;
    if( det >= UINT32_C(0x80000000) )
        {
        /* one of the operands is a NaN. */
        uint32_t a_nanval, b_nanval;
        if( aabs <= UINT32_C(0x7F800000) )
            return a; /* A is not NaN, return it */ 
        if( babs <= UINT32_C(0x7F800000) )
            return b; /* B is not NaN, return it */
        /* at this point, both A and B must be NaN; use TotalOrder to pick one to return. */
        a_nanval = a;
        b_nanval = b;
        return (_mali_ord_sf32(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x400000);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint32_t)((int32_t)a >> 31) >> 1);
    bm = b ^ ( (uint32_t)((int32_t)b >> 31) >> 1);
    return am < bm ? a : b;
    }



sf32 _mali_max_sf32( sf32 a, sf32 b )
    {
    uint32_t aabs = a & UINT32_C(0x7FFFFFFF);
    uint32_t babs = b & UINT32_C(0x7FFFFFFF);
    uint32_t det = (aabs + UINT32_C(0x7FFFFF)) | (babs + UINT32_C(0x7FFFFF));
    int32_t am;
    int32_t bm;
    if( det >= UINT32_C(0x80000000) )
        {
        /* one of the operands is a NaN. */
        uint32_t a_nanval, b_nanval;
        if( aabs <= UINT32_C(0x7F800000) )
            return a; /* A is not NaN, return it */ 
        if( babs <= UINT32_C(0x7F800000) )
            return b; /* B is not NaN, return it */
        /* at this point, both A and B must be NaN; use TotalOrder to pick one to return. */
        a_nanval = a;
        b_nanval = b;
        return (_mali_ord_sf32(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT32_C(0x400000);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint32_t)((int32_t)a >> 31) >> 1);
    bm = b ^ ( (uint32_t)((int32_t)b >> 31) >> 1);
    return am > bm ? a : b;
    }


sf64 _mali_min_sf64( sf64 a, sf64 b )
    {
    uint64_t aabs = a & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t babs = b & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t det = (aabs + UINT64_C(0xFFFFFFFFFFFFF)) | (babs + UINT64_C(0xFFFFFFFFFFFFF));
    int64_t am;
    int64_t bm;
    if( det >= UINT64_C(0x8000000000000000) )
        {
        /* one of the operands is a NaN. */
        uint64_t a_nanval, b_nanval;
        if( aabs <= UINT64_C(0x7FF0000000000000) )
            return a; /* A is not NaN, return it */ 
        if( babs <= UINT64_C(0x7FF0000000000000) )
            return b; /* B is not NaN, return it */
        /* at this point, both A and B must be NaN; use TotalOrder to pick one to return. */
        a_nanval = a;
        b_nanval = b;
        return (_mali_ord_sf64(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT64_C(0x8000000000000);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint64_t)((int64_t)a >> 63) >> 1);
    bm = b ^ ( (uint64_t)((int64_t)b >> 63) >> 1);
    return am < bm ? a : b;
    }



sf64 _mali_max_sf64( sf64 a, sf64 b )
    {
    uint64_t aabs = a & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t babs = b & UINT64_C(0x7FFFFFFFFFFFFFFF);
    uint64_t det = (aabs + UINT64_C(0xFFFFFFFFFFFFF)) | (babs + UINT64_C(0xFFFFFFFFFFFFF));
    int64_t am;
    int64_t bm;
    if( det >= UINT64_C(0x8000000000000000) )
        {
        /* one of the operands is a NaN. */
        uint64_t a_nanval, b_nanval;
        if( aabs <= UINT64_C(0x7FF0000000000000) )
            return a; /* A is not NaN, return it */ 
        if( babs <= UINT64_C(0x7FF0000000000000) )
            return b; /* B is not NaN, return it */
        /* at this point, both A and B must be NaN; use TotalOrder to pick one to return. */
        a_nanval = a;
        b_nanval = b;
        return (_mali_ord_sf64(a_nanval, b_nanval) ? b_nanval : a_nanval) | UINT64_C(0x8000000000000);
        }
    /* neither operand is NaN */
    am = a ^ ( (uint64_t)((int64_t)a >> 63) >> 1);
    bm = b ^ ( (uint64_t)((int64_t)b >> 63) >> 1);
    return am > bm ? a : b;
    }








/****************************************
 Conversion between the native-float
 and the soft-float types.
****************************************/

/* These conversions are rather trivial;
   they consist of threading the value to convert through a uniform,
   and, if needed, invoke a conversion function.
*/

typedef union if32_
    {
    uint32_t u;
    int32_t s;
    float f;
    } if32;


typedef union if64_
    {
    uint64_t u;
    int64_t s;
    double f;
    } if64;


/* convert from soft-float to native-float */

float _mali_sf16_to_float( sf16 p )
    {
    if32 i;
    i.u = _mali_sf16_to_sf32( p );
    return i.f;
    }

double _mali_sf16_to_double( sf16 p )
    {
    if64 i;
    i.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32 (p) );
    return i.f;
    }

float _mali_sf32_to_float( sf32 p )
    {
    if32 i;
    i.u = p;
    return i.f;
    }
    
double _mali_sf32_to_double( sf32 p )
    {
    if64 i;
    i.u = _mali_sf32_to_sf64( p );
    return i.f;
    }
    
float _mali_sf64_to_float( sf64 p, roundmode rm )
    {
    if32 i;
    i.u = _mali_sf64_to_sf32( p, rm );
    return i.f;
    }
    
double _mali_sf64_to_double( sf64 p )
    {
    if64 i;
    i.u = p;
    return i.f;
    }


/* convert from native-float to softfloat */

sf16 _mali_float_to_sf16( float p, roundmode rm )
    {
    if32 i;
    i.f = p;
    return _mali_sf32_to_sf16( i.u, rm );
    }

sf16 _mali_double_to_sf16( double p, roundmode rm )
    {
    if64 i;
    i.f = p;
    return _mali_sf64_to_sf16( i.u, rm );
    }

sf32 _mali_float_to_sf32( float p )
    {
    if32 i;
    i.f = p;
    return i.u;
    }

sf32 _mali_double_to_sf32( double p, roundmode rm )
    {
    if64 i;
    i.f = p;
    return _mali_sf64_to_sf32(i.u, rm);
    }

sf64 _mali_float_to_sf64( float p )
    {
    if32 i;
    i.f = p;
    return _mali_sf32_to_sf64( i.u );
    }

sf64 _mali_double_to_sf64( double p )
    {
    if64 i;
    i.f = p;
    return i.u;
    }





#define DOT4_MAX(a,b) ((a)>(b)?(a):(b))


sf32 _mali_dot4_sf32( const sf32 a[4], const sf32 b[4] )
{
    int i;
    
    int prodexp[4];
    uint64_t prodmant[4];
	int maxexp;
	int64_t mantsum;
	int res_sign;
	int lead_zeroes;
    uint64_t lsh_mant;
    uint32_t lsh_mant2;
    int res_exp;
    uint32_t lsh_mant3;
   
    int inpnan = 0; /* bitmap of subproducts containing NaN inputs */
    int prodnan = 0; /* bitmaps of subproducts evaulating to NaN. */
    int prodinf = 0; /* bitmaps of subproducts evaulating to INF */
    int prodzero = 0; /* bitmaps of subproducts evaulating to Zero. */
    int prodsigns = 0; /* signs of products */
    
    for(i=0;i<4;i++)
	{
        uint32_t aval = a[i];
        uint32_t bval = b[i];
        uint32_t aabs = aval & 0x7FFFFFFFU;
        uint32_t babs = bval & 0x7FFFFFFFU;
        uint32_t a_zero = aabs == 0;
        uint32_t b_zero = babs == 0;
        uint32_t a_inf = aabs == 0x7F800000U;
        uint32_t a_nan = aabs > 0x7F800000U;
        uint32_t b_inf = babs == 0x7F800000U; 
        uint32_t b_nan = babs > 0x7F800000U;
        uint32_t a_exp = aabs >> 23;
        uint32_t a_mant = aabs & 0x7FFFFFU;
        uint32_t b_exp = babs >> 23;
        uint32_t b_mant = babs & 0x7FFFFFU;
        prodsigns |= ((aval ^ bval) >> 31) << i;
        
        inpnan |= (a_nan | b_nan) << i;
        prodnan |= (a_nan | b_nan | (a_zero & b_inf) | (b_zero & a_inf )) << i;
        prodinf |= (a_inf | b_inf) << i;
        prodzero |= (a_zero | b_zero) << i;
        
        if( a_exp == 0 )
            a_exp = 1;
        else 
            a_mant |= 0x800000U;
        
        if( b_exp == 0 )
            b_exp = 1;
        else
            b_mant |= 0x800000U;
        
        prodexp[i] = a_exp + b_exp;
        prodmant[i] = (uint64_t)a_mant * b_mant;
	}
    
    /* special cases: at least one product was INF or NaN,
       or else all four products are Zero. */
    if( prodnan | prodinf | ((prodzero+1)>>4))
	{    
        if( prodnan != 0 )
		{
			/* one or more products are NaN: return NaN. */
            if( inpnan != 0 )
			{
				/* one or more inputs is NaN; return the one with the largest total-order. */
                uint32_t retval = 0xFFFFFFFF;
                for(i=0;i<4;i++)
                    if( _mali_isnan_sf32(a[i]) && _mali_ord_sf32( retval, a[i]) )
                        retval = a[i];
                for(i=0;i<4;i++)
                    if( _mali_isnan_sf32(b[i]) && _mali_ord_sf32( retval, b[i]) )
                        retval = b[i];
                return retval | UINT32_C(0x400000); /* quieting the NaN */
			}
            else
			{
				/* a NaN arose because of a Zero*Inf multiply. */
                return 0x7FC00000U;
			}
		}
 
        if( prodinf != 0 )
		{
            /* one or more products is Inf: if there are multiple such products
               with conflicting signs, then we must return NaN, else we return Inf. */
            if( (prodinf & prodsigns) == 0 )
                return 0x7F800000U; /* +INF */
            if( (prodinf & prodsigns) == prodinf)
                return 0xFF800000U; /* -INF */
            return 0x7FC00000; /* NaN */
		}
 
        if( prodzero == 0xF )
		{
            /* all of the subproducts are zero. In this case, examine the sign bits
               of the subproducts to decide whether to return +0 or -0: if the number of
               subproducts with the sign bit set is 2 or more, then we return -0, else
               we return +0. */
            static const uint32_t signtab[16] = { 0,0,0,1,0,1,1,1,0,1,1,1,1,1,1,1};
            return signtab[prodsigns] << 31;
		}
	}
    
    
    /* if we come here, then we have a true dot-product, rather than just some
       special-case. */
    
    /* compute the maximum exponent */
    maxexp = DOT4_MAX(DOT4_MAX(prodexp[0], prodexp[1]), DOT4_MAX(prodexp[2], prodexp[3]));
    
    mantsum = 0;
    
    /* for each mantissa-product, left-shift it by 5, then right-shift by difference
       between exponent and max-exponent, then either add it to or subtract it from
       the running mantissa-sum. The right-shift performs rounding; if the most significant
       bit shifted out is '1', then the shift result has 1 added to it. */
    for(i=0;i<4;i++)
	{
        int shamt = (maxexp - prodexp[i]);
        if( shamt <= 63 )
		{
            int64_t mant_increment = (prodmant[i] << 6) >> (maxexp - prodexp[i]);
            mant_increment++;
            mant_increment >>= 1;
            if( prodsigns & (1 << i))
                mantsum -= mant_increment;
            else
                mantsum += mant_increment;
		}
	}
    
    /* if the sum is negative, then negate it. */
    res_sign = 0;
    if( mantsum < 0 )
	{
        res_sign = 1;
        mantsum = -mantsum;
	}
    
    /* if the sum is zero, then return zero. */
    if( mantsum == 0 )
	{
        static const uint32_t signtab[16] = { 0,0,0,1,0,1,1,1,0,1,1,1,1,1,1,1};
        return signtab[prodsigns] << 31;
	}
	
    lead_zeroes = clz64( mantsum );
    
    /* normalize the mantissa, then reduce it to 32 bits, with a sticky bit. */
    lsh_mant = mantsum << lead_zeroes;
    lsh_mant2 = lsh_mant >> 32;
    if( lsh_mant & 0xFFFFFFFFU )
        lsh_mant2 |= 1;
    
    
    /* compute the final exponent value, with no special-casing. */
    res_exp = maxexp - lead_zeroes - 115;


    if( res_exp >= 0xFF )
	{
        /* if even after then normalize, the result exponent is too large;
           return an INF of the appropriate sign. */
        return 0x7F800000U | (res_sign << 31);    
	}    
    if( res_exp < 1 )
	{
        /* denormal: perform a right shift with a sticky-bit */
        int shamt = 1 - res_exp;
        if( shamt <= 31 )
		{
            if( lsh_mant2 & (( 1 << shamt) - 1))
                lsh_mant2 |= 1 << shamt;
            lsh_mant2 >>= shamt;
		}
        else
            lsh_mant2 = 0;
        res_exp = 0;
	}
    
    /* perform round to nearest even modification of the mantissa. */
    lsh_mant3 = lsh_mant2 + 0x80U;
    if( lsh_mant3 < lsh_mant2 )
        res_exp++;
    if( (lsh_mant3 & 0x1FFU) == 0x100U )
        lsh_mant3 &= ~0x100;
        
    return (res_sign << 31) | (res_exp << 23) | ((lsh_mant3 & 0x7FFFFFFF) >> 8);
    
}
    



sf32 _mali_dot3_sf32( const sf32 a[3], const sf32 b[3] )
{
	unsigned i;
	sf32 a4[4], b4[4];
	for(i = 0; i < 3; ++i)
	{
		a4[i] = a[i];
		b4[i] = b[i];
	}
	a4[3] = 0;
	b4[3] = 0;
	return _mali_dot4_sf32(a4, b4);

}

sf16 _mali_dot4_sf16( const sf16 a[4], const sf16 b[4] )
{
	unsigned i;
	sf32 a32[4], b32[4];
	sf32 tmp;
	sf16 res;
	for(i = 0; i < 4; ++i)
	{
		a32[i] = _mali_sf16_to_sf32(a[i]);
		b32[i] = _mali_sf16_to_sf32(b[i]);
	}
	tmp = _mali_dot4_sf32(a32, b32);
	res = _mali_sf32_to_sf16(tmp, SF_NEARESTEVEN);
	return res;
}


sf16 _mali_dot3_sf16( const sf16 a[3], const sf16 b[3] )
{
	unsigned i;
	sf32 a32[3], b32[3];
	sf32 tmp;
	sf16 res;
	for(i = 0; i < 3; ++i)
	{
		a32[i] = _mali_sf16_to_sf32(a[i]);
		b32[i] = _mali_sf16_to_sf32(b[i]);
	}
	tmp = _mali_dot3_sf32(a32, b32);
	res = _mali_sf32_to_sf16(tmp, SF_NEARESTEVEN);
	return res;
}



