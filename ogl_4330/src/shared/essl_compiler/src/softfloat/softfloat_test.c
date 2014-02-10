/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */




/*

This file contains routines to test the various routines present in the softfloat library.

*/


// defining this macro before including inttypes.h
// is necessary if we compile with a C++ compiler.
#define __STDC_FORMAT_MACROS

#include "softfloat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if !defined(_MSC_VER)
	#include <inttypes.h> // this is already included by softfloat.h?
#endif

// including fenv.h fails on the Cygwin platform.
// and on Visual C++
#if !defined(__CYGWIN__) && !defined(_MSC_VER)
	#include <fenv.h>
#endif

#if defined(FE_UPWARD) && defined(FE_DOWNWARD) && defined(FE_TOWARDZERO) && defined(FE_TONEAREST)
	#define HAVE_ROUNDING_MACROS
#else
	// Microsoft does not support #warning.
	#ifndef _MSC_VER
		#warning "<fenv.h> not present or not containing rounding-mode macros; some tests disabled"
	#endif
#endif

// name strings for the various rounding modes:
const char *round_names[5] =
    {
    "UP",
    "DOWN",
    "TOZERO",
    "NEARESTEVEN",
    "NEARESTAWAY"
    };


// two union types that are useful for testing.
typedef union if32_
    {
    float f;
    uint32_t u;
    int32_t s;
    } if32;

typedef union if64_
    {
    double f;
    uint64_t u;
    int64_t s;
    } if64;

#ifdef _MSC_VER
	// Microsoft does not support C99 so #define away problematic keywords.
	#define inline
	#define volatile

	// Microsoft does not support isnan so synthesise it
	int isnan( float f )
	{
		if32 inp;
		inp.f = f;
		
		return ( (inp.u & 0x7F800000U) == 0x7F800000U && (inp.u & 0x7FFFFFU) != 0 );
	}

	// Microsoft does not support llabs so synthesise it
	int64_t llabs( int64_t inp )
	{
		if ( inp < 0 )
			return -inp;
		else
			return inp;
	}

#endif

/* Maximal LFSR stepping functions for 32 and 64 bit LFSRs.

   For each LFSR step, we feed the value from the previous step into the function;
   it will return the value to use in the new step.
   
   They are usable as fast and cheap pseudo-random functions. If left to just run forever,
   they will iterate over all possible bit patterns except the all-0s pattern, with
   a period of 2^N - 1.
   */ 
   
static inline uint32_t lfsr32( uint32_t inp )
    {
    return (inp >> 1) ^ (-(inp & 1U) & 0x80200003U);
    }


static inline uint64_t lfsr64( uint64_t inp )
    {
    return (inp >> 1) ^ (-(inp & 1ULL) & 0xE800000000000000ULL);
    }

/* LFSR stepping functions that take pointers to memory and update
   a memory region as if it is an LFSR. The LFSRs are represented
   as unions; this allows aliasing to work as expected
   
   Memory-based steppers are available for 128, 192 and 256 bit operation;
   as for the 32 and 64 bit variants, they are used as el-cheapo pseudo-random
   number generators.   
*/

typedef union l128_
    {
    uint8_t v8[16];
    uint16_t v16[8];
    uint32_t v32[4];
    uint64_t v64[2];
    } l128;

typedef union l192_
    {
    uint8_t v8[24];
    uint16_t v16[12];
    uint32_t v32[6];
    uint64_t v64[3];
    } l192;

typedef union l256_
    {
    uint8_t v8[32];
    uint16_t v16[16];
    uint32_t v32[8];
    uint64_t v64[4];
    } l256;

static inline void lfsr128( l128 *lfsr )
    {
    uint32_t vl0 = lfsr->v32[0];
    uint32_t vl1 = lfsr->v32[1];
    uint32_t vl2 = lfsr->v32[2];
    uint32_t vl3 = lfsr->v32[3];
    lfsr->v32[0] = (vl0 >> 1) | (vl1 << 31);
    lfsr->v32[1] = (vl1 >> 1) | (vl2 << 31);
    lfsr->v32[2] = (vl2 >> 1) | (vl3 << 31);
    lfsr->v32[3] = (vl3 >> 1) ^ (-(vl0 & 1U) & 0xE1000000U);
    }

static inline void lfsr192( l192 *lfsr )
    {
    uint32_t vl0 = lfsr->v32[0];
    uint32_t vl1 = lfsr->v32[1];
    uint32_t vl2 = lfsr->v32[2];
    uint32_t vl3 = lfsr->v32[3];
    uint32_t vl4 = lfsr->v32[4];
    uint32_t vl5 = lfsr->v32[5];
    lfsr->v32[0] = (vl0 >> 1) | (vl1 << 31);
    lfsr->v32[1] = (vl1 >> 1) | (vl2 << 31);
    lfsr->v32[2] = (vl2 >> 1) | (vl3 << 31);
    lfsr->v32[3] = (vl3 >> 1) | (vl4 << 31);
    lfsr->v32[4] = (vl4 >> 1) | (vl5 << 31);
    lfsr->v32[5] = (vl5 >> 1) ^ (-(vl0 & 1U) & 0xA0030000U);    
    }

static inline void lfsr256( l256 *lfsr )
    {
    uint32_t vl0 = lfsr->v32[0];
    uint32_t vl1 = lfsr->v32[1];
    uint32_t vl2 = lfsr->v32[2];
    uint32_t vl3 = lfsr->v32[3];
    uint32_t vl4 = lfsr->v32[4];
    uint32_t vl5 = lfsr->v32[5];
    uint32_t vl6 = lfsr->v32[6];
    uint32_t vl7 = lfsr->v32[7];
    lfsr->v32[0] = (vl0 >> 1) | (vl1 << 31);
    lfsr->v32[1] = (vl1 >> 1) | (vl2 << 31);
    lfsr->v32[2] = (vl2 >> 1) | (vl3 << 31);
    lfsr->v32[3] = (vl3 >> 1) | (vl4 << 31);
    lfsr->v32[4] = (vl4 >> 1) | (vl5 << 31);
    lfsr->v32[5] = (vl5 >> 1) | (vl6 << 31);
    lfsr->v32[6] = (vl6 >> 1) | (vl7 << 31);
    lfsr->v32[7] = (vl7 >> 1) ^ (-(vl0 & 1U) & 0xA4000000U);    
    }










// code to test whether an sf16->sf32 routine converts correctly.



// basic test-set for sf16-to-sf32
typedef struct sf16_sf32_testval_
    {
    sf16 a;
    sf32 b;
    } sf16_sf32_testval;
    
sf16_sf32_testval testvals[] = {
    { 0x0000U, 0x00000000U },
    { 0x0001U, 0x33800000U },
    { 0x0002U, 0x34000000U },
    { 0x03FFU, 0x387FC000U },
    { 0x0400U, 0x38800000U },
    { 0x0800U, 0x39000000U },
    { 0x3800U, 0x3F000000U },
    { 0x3BFFU, 0x3F7FE000U },
    { 0x3C00U, 0x3F800000U },
    { 0x4000U, 0x40000000U },
    { 0x7BFFU, 0x477FE000U },
    { 0x7C00U, 0x7F800000U },
    { 0x7C01U, 0x7FC02000U },
    { 0x7FFFU, 0x7FFFE000U },

    { 0x8000U, 0x80000000U },
    { 0x8001U, 0xB3800000U },
    { 0x8002U, 0xB4000000U },
    { 0x83FFU, 0xB87FC000U },
    { 0x8400U, 0xB8800000U },
    { 0x8800U, 0xB9000000U },
    { 0xB800U, 0xBF000000U },
    { 0xBBFFU, 0xBF7FE000U },
    { 0xBC00U, 0xBF800000U },
    { 0xC000U, 0xC0000000U },
    { 0xFBFFU, 0xC77FE000U },
    { 0xFC00U, 0xFF800000U },
    { 0xFC01U, 0xFFC02000U },
    { 0xFFFFU, 0xFFFFE000U } };



// alternate implementation of sf16_to_sf32
sf32  alt_sf16_to_sf32( sf16 inp )
    {
    if32 vl;
    uint32_t expo = (inp >> 10) & 0x1F;
    uint32_t mant = inp & 0x3FF;
    uint32_t sign = inp >> 15;
    
    float fsign = sign ? -1.0 : 1.0;
    
    if( expo == 0 )
        {        
        // zero or denormal.
        vl.f = mant * (1.0/16777216.0) * fsign;
        }
    else if( expo == 0x1F )
        {
        // infinity or NaN.
        vl.u = sign << 31; // set the sign bit
        vl.u |= 0x7F800000; // set all exponent bits to all 1s. 
        if( mant != 0) // check if the input value is a NaN.
            {
            vl.u |= 0x400000; // quieten the NaN
            vl.u |= mant << 13; // keep the payload
            }
        }
    else
        {
        // normal number.
        mant += 0x400;
        vl.f = (mant * (1.0/1024.0)) * pow(2, (float)expo - 15) * fsign;
        }
        
    return vl.u;
    }


void test_sf16_to_sf32( void )
    {
    uint32_t i;
    // perform directed testing with a handful of hand-chosen values.
    for(i=0;i< sizeof(testvals)/sizeof(testvals[0]); i++)
        {
        uint32_t p1 = _mali_sf16_to_sf32( testvals[i].a );
        uint32_t p2 = testvals[i].b;
        if( p1 != p2 )
            {
            printf("SF16_to_SF32 error1: index=%" PRId32 ", input=%4.4" PRIX32 ", result=%8.8" PRIX32 ", expected=%8.8" PRIX32 "\n",
                i, testvals[i].a, p1, p2 ); 
            }        
        }
    
    // exhaustive-test the main and the alternate implementations against each other
    for(i=0;i<65536;i++)
        {
        uint32_t p1 = _mali_sf16_to_sf32( i );
        uint32_t p2 = alt_sf16_to_sf32( i );
        if( p1 != p2 )
            {
            printf("SF16_to_SF32 error2: input=%4.4" PRIX32 ", result=%8.8" PRIX32 ", expected=%8.8" PRIX32 "\n",
                i, p1, p2 ); 
            }
        }
    }

// alternate implementation of sf32_to_sf64
sf64  alt_sf32_to_sf64( sf32 inp )
    {
    if32 vl1;
    if64 res;
    
    vl1.u = inp;
    // except for NaN, this cast basically *is* the alternate implementation.
    res.f = (double) vl1.f;
    if( isnan(res.f) )
        {
        // in case of NaN, the expected value needs to retain the NaN's payload,
        // but it also needs to be a quiet NaN.
        uint64_t sign = (inp & 0x80000000U);
        uint64_t mant = inp & 0x7FFFFFU;
        sign <<= 32;
        mant <<= 29;
        res.u = sign | mant | 0x7FF8000000000000ULL;
        }
    return res.u;
    }


// exhaustive-test the main the alternate implementations against each other.
void test_sf32_to_sf64( void )
    {
    uint32_t i = 0;
    do  {
        uint64_t p1 = _mali_sf32_to_sf64( i );
        uint64_t p2 = alt_sf32_to_sf64( i );
        if( p1 != p2 )
            {
            printf("SF32_to_SF64 error: input=%8.8" PRIX32 ", result=%16.16" PRIX64 ", expected=%16.16" PRIX64 "\n",
                i, p1, p2 );            
            }
        }
        while( i++ != 0xFFFFFFFFU );
    }



// test that converting from FP16 to FP32 and back again works correctly.
void test_sf16_up_and_down_rm( roundmode rm )
    {
    int i;

    // test that converting from fp16 to fp32 and back again works.
    // conversion is supposed to give an unchanged value for every possible input
    // except that SNaNs have their top mantissa bit set, thereby becoming QNaNs.
    for(i=0;i<65536;i++)
        {
        uint32_t r1 = _mali_sf16_to_sf32( i );
        uint16_t r2 = _mali_sf32_to_sf16( r1, rm );
        uint16_t r2_expect = _mali_isnan_sf16(i) 
            ? (i | 0x200) 
            : i;
        if( r2 != r2_expect )
            {
            printf("SF16_to_SF32 up-and-down (roundmode=%s) Input: %4.4" PRIX32 " -> Converted to %8.8" PRIX32 " -> Back to %4.4" PRIX32 " (expected %4.4" PRIX32 ")\n",
                round_names[rm], i, r1, r2, r2_expect );
            }
        }   
    }


// iterate the test above for all round modes
void test_sf16_up_and_down( void )
    {
    printf("Test: sf32_to_sf16(sf16_to_sf32()): all round modes\n");
    test_sf16_up_and_down_rm( SF_UP );
    test_sf16_up_and_down_rm( SF_DOWN );
    test_sf16_up_and_down_rm( SF_TOZERO );
    test_sf16_up_and_down_rm( SF_NEARESTEVEN );
    test_sf16_up_and_down_rm( SF_NEARESTAWAY );    
    }


// test that converting from FP32 to FP64 and back again works correctly.
void test_sf32_up_and_down_rm( roundmode rm )
    {
    uint32_t i;
    
    uint32_t inp;
    uint64_t temp;
    uint32_t res, expected;
    i = 0;
    do  {
        inp = i;
        temp = _mali_sf32_to_sf64( inp );
        res = _mali_sf64_to_sf32( temp, rm );
        // expected result is identical to input result except that SNaNs are quietened.
        expected = _mali_isnan_sf32( inp ) 
            ? (inp | 0x400000)
            : inp;
        if( res != expected )
            {
            printf("SF32_to_SF64 up-and-down (roundmode=%s) Input: %8.8" PRIX32 " -> Converted to %16.16" PRIX64 " -> Back to %8.8" PRIX32 " (expected %8.8" PRIX32 ")\n",
                round_names[rm], inp, temp, res, expected );            
            }
            
        }
        while( i++ != 0xFFFFFFFFU );    
    }

// iterate the test above for every round-mode
void test_sf32_up_and_down( void )
    {
    printf("Test: sf64_to_sf32(sf32_to_sf64()): round-mode UP\n");
    test_sf32_up_and_down_rm( SF_UP );
    printf("Test: sf64_to_sf32(sf32_to_sf64()): round-mode DOWN\n");
    test_sf32_up_and_down_rm( SF_DOWN );
    printf("Test: sf64_to_sf32(sf32_to_sf64()): round-mode TOZERO\n");
    test_sf32_up_and_down_rm( SF_TOZERO );
    printf("Test: sf64_to_sf32(sf32_to_sf64()): round-mode NEARESTEVEN\n");
    test_sf32_up_and_down_rm( SF_NEARESTEVEN );
    printf("Test: sf64_to_sf32(sf32_to_sf64()): round-mode NEARESTAWAY\n");
    test_sf32_up_and_down_rm( SF_NEARESTAWAY );    
    }





/*

for FP32->FP16 conversion, we want to check whether rounding is done correctly.

What we do is as follows:
    1: convert from fp32 to fp16.
    2: convert back from fp16 to fp32.
    3: if the fp32 value is equal to the input value
       then the result is correct, else:
    4: if the fp32 value is greater than the input value, we compute the
       NextBelow() value, else we compute the NextAbove() value.
    5: The result value and the NextBelow/NextAbove value form an interval.
       If the input value is not within this interval, we have failed altogether.
    6: Do roundoff-mode dependent checks:
        SF_UP: result value must be greater than or equal to input value
        SF_DOWN: result value must be less than or equal to input value
        SF_TRUNC: abs(resultvalue) must be smaller than or equal to input value.
        SF_NEARESTAWAY:
            if the input value is not at the center of the interval, then
            it must be closer to the input-value end of the interval. If the input
            value is at the center of the interval, then the result must be greater than
            the input value.
        SF_NEARESTEVEN:
            if the input value is not at the center of the interval, then
            it must be closer to the input-value end of the interval. If the
            input value is at the center of the interval, then the LSB of the FP16 value
            must be 0.


The same procedure needs to be done for FP32->FP16 conversion
            
*/



/*
helper functions: given a soft-float value, find the next larger value towards +INF
*/
static sf16 nextabove_sf16( sf16 p )
    {
    if( p == 0x8000U || p == 0U ) return 1U;
    if( p >= 0x8000U ) return p - 1U;
    return p + 1U;
    }

static sf32 nextabove_sf32( sf32 p )
    {
    if( p == 0x80000000U || p == 0U ) return 1U;
    if( p >= 0x80000000U ) return p - 1U;
    return p + 1U;
    }

static sf64 nextabove_sf64( sf64 p )
    {
    if( p == 0x8000000000000000ULL || p == 0ULL ) return 1ULL;
    if( p >= 0x8000000000000000ULL ) return p - 1ULL;
    return p + 1ULL;
    }


/*
helper functions: given a soft-float value, find the next smaller value towards -INF
*/
static sf16 nextbelow_sf16( sf16 p )
    {
    if( p == 0x8000 || p == 0 ) return 0x8001U;
    if( p >= 0x8000 ) return p + 1;
    return p - 1U;
    }

static sf32 nextbelow_sf32( sf32 p )
    {
    if( p == 0x80000000U || p == 0U ) return 0x80000001U;
    if( p >= 0x80000000U ) return p + 1U;
    return p - 1U;
    }

static sf64 nextbelow_sf64( sf64 p )
    {
    if( p == 0x8000000000000000ULL || p == 0ULL ) return 0x8000000000000001ULL;
    if( p >= 0x8000000000000000ULL ) return p + 1ULL;
    return p - 1U;
    }


/*
helper functions: given a soft-float value, find the next value away from Zero.
*/

sf16 away_from_zero_sf16( sf16 p )
    {
    return p + 1U;
    }

sf32 away_from_zero_sf32( sf32 p )
    {
    return p + 1U;
    }

sf64 away_from_zero_sf64( sf64 p )
    {
    return p + 1ULL;
    }



// perform an SF32->SF16 test with rounding-mode UP.
void test_sfup_sf32_to_sf16( sf32 inp )
    {
    if32 vinp, vres, v_nextabove;
    sf16 sval = _mali_sf32_to_sf16( inp, SF_UP );
    sf16 snext = nextabove_sf16( sval );
    vinp.u = inp;
    vres.u = _mali_sf16_to_sf32( sval );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf32( inp ) || _mali_isnan_sf32( vres.u ) )
        {
        if( !(_mali_isnan_sf32( inp ) && _mali_isnan_sf32( vres.u )) )
            {
            printf("SF32_to_SF16, round-mode=UP: IsNan error, input=%8.8" PRIX32 ", result=%8.8" PRIX32 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextabove.u = _mali_sf16_to_sf32( snext );
    // vres.f should be greater than or equal to vinp.f
    // v_nextabove.f should be greater than vinp.f
    
    if( vres.f < vinp.f  ||  v_nextabove.f <= vinp.f )
        printf("SF32_to_SF16: Rounding error on value %8.8" PRIX32 ", round-mode = UP\n", inp);
    }



// perform an SF64->SF32 test with rounding-mode UP.
void test_sfup_sf64_to_sf32( sf64 inp )
    {
    if64 vinp, vres, v_nextabove;
    sf32 sval = _mali_sf64_to_sf32( inp, SF_UP );
    sf32 snext = nextabove_sf32( sval );
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64( sval );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF32, round-mode=UP: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextabove.u = _mali_sf32_to_sf64( snext );
    // vres.f should be greater than or equal to vinp.f
    // v_nextabove.f should be greater than vinp.f
    
    if( vres.f < vinp.f  ||  v_nextabove.f <= vinp.f )
        printf("SF64_to_SF32: Rounding error on value %16.16" PRIX64 ": result = %16.16" PRIX64 ", round-mode = UP\n", inp, vres.u);
    }



// perform an SF64->SF16 test with rounding-mode UP.
void test_sfup_sf64_to_sf16( sf64 inp )
    {
    if64 vinp, vres, v_nextabove;
    sf16 sval = _mali_sf64_to_sf16( inp, SF_UP );
    sf16 snext = nextabove_sf16( sval );
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32(sval) );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF16, round-mode=UP: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextabove.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32(snext) );
    // vres.f should be greater than or equal to vinp.f
    // v_nextabove.f should be greater than vinp.f
    
    if( vres.f < vinp.f  ||  v_nextabove.f <= vinp.f )
        printf("SF64_to_SF16: Rounding error on value %16.16" PRIX64 ": result = %16.16" PRIX64 ", round-mode = UP\n", inp, vres.u);
    }




// perform an FP32->FP16 test with rounding-mode DOWN.
void test_sfdown_sf32_to_sf16( sf32 inp )
    {
    if32 vinp, vres, v_nextbelow;
    sf16 sval = _mali_sf32_to_sf16( inp, SF_DOWN );
    sf16 snext = nextbelow_sf16( sval );
    vinp.u = inp;
    vres.u = _mali_sf16_to_sf32( sval );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf32( inp ) || _mali_isnan_sf32( vres.u ) )
        {
        if( !(_mali_isnan_sf32( inp ) && _mali_isnan_sf32( vres.u )) )
            {
            printf("SF32_to_SF16, round-mode=DOWN: IsNan error, input=%8.8" PRIX32 ", result=%8.8" PRIX32 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextbelow.u = _mali_sf16_to_sf32( snext );
    if( vres.f > vinp.f  ||  v_nextbelow.f >= vinp.f )
        printf("SF32_to_SF16: Rounding error on value %8.8" PRIX32 ", round-mode = DOWN\n", inp);
    }



// perform an FP64->FP32 test with rounding-mode DOWN.
void test_sfdown_sf64_to_sf32( sf64 inp )
    {
    if64 vinp, vres, v_nextbelow;
    sf32 sval = _mali_sf64_to_sf32( inp, SF_DOWN );
    sf32 snext = nextbelow_sf32( sval );
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64( sval );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF32, round-mode=DOWN: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextbelow.u = _mali_sf32_to_sf64( snext );
    if( vres.f > vinp.f  ||  v_nextbelow.f >= vinp.f )
        printf("SF64_to_SF32: Rounding error on value %16.16" PRIX64 ": result=%16.16" PRIX64 " round-mode = DOWN\n", inp, vres.u);
    }



// perform an FP64->FP16 test with rounding-mode DOWN.
void test_sfdown_sf64_to_sf16( sf64 inp )
    {
    if64 vinp, vres, v_nextbelow;
    sf16 sval = _mali_sf64_to_sf16( inp, SF_DOWN );
    sf16 snext = nextbelow_sf16( sval );
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32(sval) );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF16, round-mode=DOWN: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextbelow.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32(snext) );
    if( vres.f > vinp.f  ||  v_nextbelow.f >= vinp.f )
        printf("SF64_to_SF32: Rounding error on value %16.16" PRIX64 ": result=%16.16" PRIX64 " round-mode = DOWN\n", inp, vres.u);
    }



// perform an FP32->FP16 test with rounding-mode TOZERO.
void test_sftozero_sf32_to_sf16( sf32 inp )
    {
    if32 vinp, vres, v_nextaway;
    sf16 sval = _mali_sf32_to_sf16( inp, SF_TOZERO );
    sf16 snext = away_from_zero_sf16( sval );
    vinp.u = inp;
    vres.u = _mali_sf16_to_sf32( sval );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf32( inp ) || _mali_isnan_sf32( vres.u ) )
        {
        if( !(_mali_isnan_sf32( inp ) && _mali_isnan_sf32( vres.u )) )
            {
            printf("SF32_to_SF16, round-mode=TOZERO: IsNan error, input=%8.8" PRIX32 ", result=%8.8" PRIX32 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextaway.u = _mali_sf16_to_sf32( snext );
    // vres.f should have a less-than-or-equal absolute value to vinp.f
    // vinp.f should have a less-than absolute value than v_nextaway.f
    if( fabs(vres.f) > fabs(vinp.f)  ||  fabs(vinp.f) >= fabs(v_nextaway.f) )
        {
        printf("SF32_to_SF16: Rounding error on value %8.8" PRIX32 ", round-mode = TOZERO\n", inp);
        printf("Input: %8.8" PRIX32 ", Actual: %4.4" PRIX32 " (%8.8" PRIX32 "), NextAway: %8.8" PRIX32 "\n",
            inp, sval, vres.u, v_nextaway.u );
        }
    }



// perform an FP64->FP32 test with rounding-mode TOZERO.
void test_sftozero_sf64_to_sf32( sf64 inp )
    {
    if64 vinp, vres, v_nextaway;
    sf32 sval = _mali_sf64_to_sf32( inp, SF_TOZERO );
    sf32 snext = away_from_zero_sf32( sval );
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64( sval );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF32, round-mode=TOZERO: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextaway.u = _mali_sf32_to_sf64( snext );
    // vres.f should have a less-than-or-equal absolute value to vinp.f
    // vinp.f should have a less-than absolute value than v_nextaway.f
    if( fabs(vres.f) > fabs(vinp.f)  ||  fabs(vinp.f) >= fabs(v_nextaway.f) )
        {
        printf("SF64_to_SF32: Rounding error on value %16.16" PRIX64 ", round-mode = TOZERO\n", inp);
        printf("Input: %16.16" PRIX64 ", Actual: %8.8" PRIX32 " (%16.16" PRIX64 "), NextAway: %16.16" PRIX64 "\n",
            inp, sval, vres.u, v_nextaway.u );
        }
    }


// perform an FP64->FP16 test with rounding-mode TOZERO.
void test_sftozero_sf64_to_sf16( sf64 inp )
    {
    if64 vinp, vres, v_nextaway;
    sf16 sval = _mali_sf64_to_sf16( inp, SF_TOZERO );
    sf16 snext = away_from_zero_sf16( sval );
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32(sval) );
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF16, round-mode=TOZERO: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    v_nextaway.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32(snext) );
    // vres.f should have a less-than-or-equal absolute value to vinp.f
    // vinp.f should have a less-than absolute value than v_nextaway.f
    if( fabs(vres.f) > fabs(vinp.f)  ||  fabs(vinp.f) >= fabs(v_nextaway.f) )
        {
        printf("SF64_to_SF16: Rounding error on value %16.16" PRIX64 ", round-mode = TOZERO\n", inp);
        printf("Input: %16.16" PRIX64 ", Actual: %8.8" PRIX32 " (%16.16" PRIX64 "), NextAway: %16.16" PRIX64 "\n",
            inp, sval, vres.u, v_nextaway.u );
        }
    }




// perform an FP32->FP16 rounding-mode test with rounding-mode NEAREST_EVEN
void test_sfnearest_even_sf32_to_sf16( sf32 inp )
    {
    if32 vinp, vres, v_nextoff;
    sf16 sval = _mali_sf32_to_sf16( inp, SF_NEARESTEVEN );
    sf16 snext;
    vinp.u = inp;
    vres.u = _mali_sf16_to_sf32 ( sval );
    if( vres.u == vinp.u ) return; // result is equal, don't do any more tests.
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf32( inp ) || _mali_isnan_sf32( vres.u ) )
        {
        if( !(_mali_isnan_sf32( inp ) && _mali_isnan_sf32( vres.u )) )
            {
            printf("SF32_to_SF16, round-mode=NEAREST-EVEN: IsNan error, input=%8.8" PRIX32 ", result=%8.8" PRIX32 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    
    // find the "next" value, either above or below.
    if( vres.f > vinp.f )
        snext = nextbelow_sf16( sval );
    else
        snext = nextabove_sf16( sval );
    v_nextoff.u = _mali_sf16_to_sf32( snext );    
        
    // check that the actual result value is between the input value and
    // the 'next'-value. The two differences below should in that case
    // have opposite signs
    if( ((v_nextoff.f - vinp.f) * (vres.f - vinp.f)) > 0.0 )
        printf("Rounding error 1 on value %8.8" PRIX32 ", round-mode = NEAREST-EVEN\n", inp);
    else
        {
        // difference to the next-value should be larger than the difference to
        // the result-value.
        float dfvar = fabs( v_nextoff.f - vinp.f ) - fabs( vres.f - vinp.f );
        if( dfvar < 0.0 )
            {
            // one exception is allowed when the result-value is INF.
            if( (sval & 0x7FFF) != 0x7C00)
                printf("Rounding 2 error on value %8.8" PRIX32 ", round-mode = NEAREST-EVEN\n", inp);          
            }
        // if difference is equal, then we msut round to EVEN.
        else if( dfvar == 0.0 )
            {
            if( sval & 1 )
                printf("Rounding 3 error on value %8.8" PRIX32 ", round-mode = NEAREST-EVEN\n", inp);                      
            }
        }
    }



// perform an FP64->FP32 rounding-mode test with rounding-mode NEAREST_EVEN
void test_sfnearest_even_sf64_to_sf32( sf64 inp )
    {
    if64 vinp, vres, v_nextoff;
    sf32 sval = _mali_sf64_to_sf32( inp, SF_NEARESTEVEN );
    sf32 snext;
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64 ( sval );
    if( vres.u == vinp.u ) return; // result is equal, don't do any more tests.
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF32, round-mode=NEAREST-EVEN: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    
    // find the "next" value, either above or below.
    if( vres.f > vinp.f )
        snext = nextbelow_sf32( sval );
    else
        snext = nextabove_sf32( sval );
    v_nextoff.u = _mali_sf32_to_sf64( snext );    
        
    // check that the actual result value is between the input value and
    // the 'next'-value. The two differences below should in that case
    // have opposite signs
    if( ((v_nextoff.f - vinp.f) * (vres.f - vinp.f)) > 0.0 )
        printf("Rounding error 1 on value %16.16" PRIX64 ", round-mode = NEAREST-EVEN\n", inp);
    else
        {
        // difference to the next-value should be larger than the difference to
        // the result-value.
        double dfvar = fabs( v_nextoff.f - vinp.f ) - fabs( vres.f - vinp.f );
        if( dfvar < 0.0 )
            {
            // one exception is allowed when the result-value is INF.
            if( (sval & 0x7FFFFFFFU) != 0x7F800000U)
                printf("Rounding 2 error on value %16.16" PRIX64 ", round-mode = NEAREST-EVEN\n", inp);          
            }
        // if difference is equal, then we must round to EVEN.
        else if( dfvar == 0.0 )
            {
            if( sval & 1 )
                printf("Rounding 3 error on value %16.16" PRIX64 ", round-mode = NEAREST-EVEN\n", inp);                      
            }
        }
    }




// perform an FP64->FP16 rounding-mode test with rounding-mode NEAREST_EVEN
void test_sfnearest_even_sf64_to_sf16( sf64 inp )
    {
    if64 vinp, vres, v_nextoff;
    sf16 sval = _mali_sf64_to_sf16( inp, SF_NEARESTEVEN );
    sf16 snext;
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64 ( _mali_sf16_to_sf32(sval) );
    if( vres.u == vinp.u ) return; // result is equal, don't do any more tests.
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF16, round-mode=NEAREST-EVEN: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    
    // find the "next" value, either above or below.
    if( vres.f > vinp.f )
        snext = nextbelow_sf16( sval );
    else
        snext = nextabove_sf16( sval );
    v_nextoff.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32(snext) );    
        
    // check that the actual result value is between the input value and
    // the 'next'-value. The two differences below should in that case
    // have opposite signs
    if( ((v_nextoff.f - vinp.f) * (vres.f - vinp.f)) > 0.0 )
        printf("Rounding error 1 on value %16.16" PRIX64 ", round-mode = NEAREST-EVEN\n", inp);
    else
        {
        // difference to the next-value should be larger than the difference to
        // the result-value.
        double dfvar = fabs( v_nextoff.f - vinp.f ) - fabs( vres.f - vinp.f );
        if( dfvar < 0.0 )
            {
            // one exception is allowed when the result-value is INF.
            if( (sval & 0x7FFFU) != 0x7C00U)
                printf("Rounding 2 error on value %16.16" PRIX64 ", round-mode = NEAREST-EVEN\n", inp);          
            }
        // if difference is equal, then we must round to EVEN.
        else if( dfvar == 0.0 )
            {
            if( sval & 1 )
                printf("Rounding 3 error on value %16.16" PRIX64 ", round-mode = NEAREST-EVEN\n", inp);                      
            }
        }
    }




// perform an FP32->FP16 rounding-mode test with rounding-mode NEAREST_AWAY
void test_sfnearest_away_sf32_to_sf16( sf32 inp )
    {
    if32 vinp, vres, v_nextoff;
    sf16 sval = _mali_sf32_to_sf16( inp, SF_NEARESTAWAY );
    sf16 snext;
    vinp.u = inp;
    vres.u = _mali_sf16_to_sf32 ( sval );
    if( vres.u == vinp.u ) return; // result is equal, don't do any more tests.
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf32( inp ) || _mali_isnan_sf32( vres.u ) )
        {
        if( !(_mali_isnan_sf32( inp ) && _mali_isnan_sf32( vres.u )) )
            {
            printf("SF32_to_SF16, round-mode=NEAREST-AWAY: IsNan error, input=%8.8" PRIX32 ", result=%8.8" PRIX32 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    
    // find the "next" value, either above or below.
    if( vres.f > vinp.f )
        snext = nextbelow_sf16( sval );
    else
        snext = nextabove_sf16( sval );
    v_nextoff.u = _mali_sf16_to_sf32( snext );    
        
    // check that the actual result value is between the input value and
    // the 'next'-value. The two differences below should in that case
    // have opposite signs
    if( ((v_nextoff.f - vinp.f) * (vres.f - vinp.f)) > 0.0 )
        printf("Rounding error 1 on value %8.8" PRIX32 ", round-mode = NEAREST-AWAY\n", inp);
    else
        {
        // difference to the next-value should be larger than the difference to
        // the result-value.
        float dfvar = fabs( v_nextoff.f - vinp.f ) - fabs( vres.f - vinp.f );
        if( dfvar < 0.0 )
            {
            // one exception is allowed when the result-value is INF.
            if( (sval & 0x7FFF) != 0x7C00)
                printf("Rounding 2 error on value %8.8" PRIX32 ", round-mode = NEAREST-AWAY\n", inp);          
            }
        // if difference is equal, then we must round AWAY from zero
        else if( dfvar == 0.0 )
            {
            if( fabs(vres.f) < fabs(vinp.f) )
                printf("Rounding 3 error on value %8.8" PRIX32 ", round-mode = NEAREST-AWAY\n", inp);                      
            }
        }
    }



// perform an FP64->FP32 rounding-mode test with rounding-mode NEAREST_AWAY
void test_sfnearest_away_sf64_to_sf32( sf64 inp )
    {
    if64 vinp, vres, v_nextoff;
    sf32 sval = _mali_sf64_to_sf32( inp, SF_NEARESTAWAY );
    sf32 snext;
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64 ( sval );
    if( vres.u == vinp.u ) return; // result is equal, don't do any more tests.
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF32, round-mode=NEAREST-AWAY: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    
    // find the "next" value, either above or below.
    if( vres.f > vinp.f )
        snext = nextbelow_sf32( sval );
    else
        snext = nextabove_sf32( sval );
    v_nextoff.u = _mali_sf32_to_sf64( snext );    
        
    // check that the actual result value is between the input value and
    // the 'next'-value. The two differences below should in that case
    // have opposite signs
    if( ((v_nextoff.f - vinp.f) * (vres.f - vinp.f)) > 0.0 )
        printf("Rounding error 1 on value %16.16" PRIX64 ", round-mode = NEAREST-AWAY\n", inp);
    else
        {
        // difference to the next-value should be larger than the difference to
        // the result-value.
        double dfvar = fabs( v_nextoff.f - vinp.f ) - fabs( vres.f - vinp.f );
        if( dfvar < 0.0 )
            {
            // one exception is allowed when the result-value is INF.
            if( (sval & 0x7FFFFFFF) != 0x7F800000)
                printf("Rounding 2 error on value %16.16" PRIX64 ", round-mode = NEAREST-AWAY\n", inp);          
            }
        // if difference is equal, then we must round AWAY from zero
        else if( dfvar == 0.0 )
            {
            if( fabs(vres.f) < fabs(vinp.f) )
                printf("Rounding 3 error on value %16.16" PRIX64 ", round-mode = NEAREST-AWAY\n", inp);                      
            }
        }
    }




// perform an FP64->FP16 rounding-mode test with rounding-mode NEAREST_AWAY
void test_sfnearest_away_sf64_to_sf16( sf64 inp )
    {
    if64 vinp, vres, v_nextoff;
    sf16 sval = _mali_sf64_to_sf16( inp, SF_NEARESTAWAY );
    sf16 snext;
    vinp.u = inp;
    vres.u = _mali_sf32_to_sf64 ( _mali_sf16_to_sf32(sval) );
    if( vres.u == vinp.u ) return; // result is equal, don't do any more tests.
    // if the input is NaN, then thre result should be NaN too.
    if( _mali_isnan_sf64( inp ) || _mali_isnan_sf64( vres.u ) )
        {
        if( !(_mali_isnan_sf64( inp ) && _mali_isnan_sf64( vres.u )) )
            {
            printf("SF64_to_SF16, round-mode=NEAREST-AWAY: IsNan error, input=%16.16" PRIX64 ", result=%16.16" PRIX64 "\n",
                inp, vres.u );            
            }        
        return;
        }    
    
    // find the "next" value, either above or below.
    if( vres.f > vinp.f )
        snext = nextbelow_sf16( sval );
    else
        snext = nextabove_sf16( sval );
    v_nextoff.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32(snext) );    
        
    // check that the actual result value is between the input value and
    // the 'next'-value. The two differences below should in that case
    // have opposite signs
    if( ((v_nextoff.f - vinp.f) * (vres.f - vinp.f)) > 0.0 )
        printf("Rounding error 1 on value %16.16" PRIX64 ", round-mode = NEAREST-AWAY\n", inp);
    else
        {
        // difference to the next-value should be larger than the difference to
        // the result-value.
        double dfvar = fabs( v_nextoff.f - vinp.f ) - fabs( vres.f - vinp.f );
        if( dfvar < 0.0 )
            {
            // one exception is allowed when the result-value is INF.
            if( (sval & 0x7FFF) != 0x7C00)
                printf("Rounding 2 error on value %16.16" PRIX64 ", round-mode = NEAREST-AWAY\n", inp);          
            }
        // if difference is equal, then we must round AWAY from zero
        else if( dfvar == 0.0 )
            {
            if( fabs(vres.f) < fabs(vinp.f) )
                printf("Rounding 3 error on value %16.16" PRIX64 ", round-mode = NEAREST-AWAY\n", inp);                      
            }
        }
    }





// perform exhaustive testing of sf32_to_sf16 with all input values in
// all rounding modes.
void test_sf32_to_sf16( void )
    {
    uint32_t i;
    
    test_sf16_up_and_down();
    
    printf("Test: sf32_to_sf16() : round-mode UP\n");
    i = 0U;
    do  {
        test_sfup_sf32_to_sf16( i );
        } while(i++ != 0xFFFFFFFFU);    
    printf("Test: sf32_to_sf16() : round-mode DOWN\n");
    i = 0U;
    do  {
        test_sfdown_sf32_to_sf16( i );
        } while(i++ != 0xFFFFFFFFU);    
    printf("Test: sf32_to_sf16() : round-mode TOZERO\n");
    i = 0U;
    do  {
        test_sftozero_sf32_to_sf16( i );
        } while(i++ != 0xFFFFFFFFU);    
    printf("Test: sf32_to_sf16() : round-mode NEARESTEVEN\n");
    i = 0U;
    do  {
        test_sfnearest_even_sf32_to_sf16( i );
        } while(i++ != 0xFFFFFFFFU);    
    printf("Test: sf32_to_sf16() : round-mode NEARESTAWAY\n");
    i = 0U;
    do  {
        test_sfnearest_away_sf32_to_sf16( i );
        } while(i++ != 0xFFFFFFFFU);    
    }


static inline void test_sf64_to_sf32_one( uint64_t inp )
    {
    test_sfup_sf64_to_sf32( inp );
    test_sfdown_sf64_to_sf32( inp );
    test_sftozero_sf64_to_sf32( inp );
    test_sfnearest_even_sf64_to_sf32( inp );
    test_sfnearest_away_sf64_to_sf32( inp );
    }


static inline void test_sf64_to_sf16_one( uint64_t inp )
    {
    test_sfup_sf64_to_sf16( inp );
    test_sfdown_sf64_to_sf16( inp );
    test_sftozero_sf64_to_sf16( inp );
    test_sfnearest_even_sf64_to_sf16( inp );
    test_sfnearest_away_sf64_to_sf16( inp );
    }


// perform LFSR-based random-testing of sf64_to_sf32 with the specified number of input
// values in all round modes

void test_sf64_to_sf32_rand( int count )
    {
    uint64_t lfsr = 1U;
    int i;
    for(i=0;i<count;i++)
        {
        test_sf64_to_sf32_one( lfsr );
        lfsr = lfsr64( lfsr );
        test_sfup_sf64_to_sf32( lfsr );
        test_sfdown_sf64_to_sf32( lfsr );
        test_sftozero_sf64_to_sf32( lfsr );
        test_sfnearest_even_sf64_to_sf32( lfsr );
        test_sfnearest_away_sf64_to_sf32( lfsr );        
        }
    }


// perform LFSR-based random-testing of sf64_to_sf16 with the specified number of input
// values in all round modes
void test_sf64_to_sf16_rand( int count )
    {
    uint64_t lfsr = 1U;
    int i;
    for(i=0;i<count;i++)
        {
        test_sf64_to_sf16_one( lfsr );
        lfsr = lfsr64( lfsr );
        test_sfup_sf64_to_sf16( lfsr );
        test_sfdown_sf64_to_sf16( lfsr );
        test_sftozero_sf64_to_sf16( lfsr );
        test_sfnearest_even_sf64_to_sf16( lfsr );
        test_sfnearest_away_sf64_to_sf16( lfsr );        
        }
    }



/*
for sf64_to_sf32, we additionally want to perform more directed testing:
pick random FP32 values (excluding zero), expand them to FP64.
Then test: 
   * value
   * value  +  1 * fp64_ulp
   * value  -  1 * fp64_ulp
   * value  +  0.5 * fp32_ulp
   * value  +  0.5 * fp32_ulp - 1 * fp64_ulp
   * value  +  0.5 * fp32_ulp + 1 * fp64_ulp
This is done to specifically exercise rounding-ambiguities.
*/

static void test_sf64_to_sf32_dir_one( uint64_t inp )
    {
    test_sf64_to_sf32_one( inp );
    test_sf64_to_sf32_one( inp + 1ULL );
    test_sf64_to_sf32_one( inp - 1ULL );
    test_sf64_to_sf32_one( inp + 0x10000000ULL );
    test_sf64_to_sf32_one( inp + 0x10000001ULL );
    test_sf64_to_sf32_one( inp + 0x0FFFFFFFULL );
    }


/*
for sf64_to_sf16, we additionally want to perform more directed testing:
pick random FP16 values (excluding zero), expand them to FP64.
Then test: 
   * value
   * value  +  1 * fp64_ulp
   * value  -  1 * fp64_ulp
   * value  +  0.5 * fp16_ulp
   * value  +  0.5 * fp16_ulp - 1 * fp64_ulp
   * value  +  0.5 * fp16_ulp + 1 * fp64_ulp
This is done to specifically exercise rounding-ambiguities.
*/

static void test_sf64_to_sf16_dir_one( uint64_t inp )
    {
    test_sf64_to_sf16_one( inp );
    test_sf64_to_sf16_one( inp + 1U );
    test_sf64_to_sf16_one( inp - 1U );
    test_sf64_to_sf16_one( inp + 0x20000000000ULL );
    test_sf64_to_sf16_one( inp + 0x20000000001ULL );
    test_sf64_to_sf16_one( inp + 0x1FFFFFFFFFFULL );
    }


void test_sf64_to_sf32_dir_rand( int count )
    {
    uint32_t lfsr = 1U;
    int i;
    for(i=0;i<count;i++)
        {
        uint64_t p = _mali_sf32_to_sf64( lfsr );
        test_sf64_to_sf32_dir_one( p );
        lfsr = lfsr32( lfsr );
        }
    }


void test_sf64_to_sf16_dir_rand( int count )
    {
    uint32_t lfsr = 1U;
    int i;
    for(i=0;i<count;i++)
        {
        uint64_t p = _mali_sf32_to_sf64( lfsr );
        test_sf64_to_sf16_dir_one( p );
        lfsr = lfsr32( lfsr );
        }
    }



// top-level function for testing sf64_to_sf32
void test_sf64_to_sf32( void )
    {
    
    printf("Test: sf64_to_sf32() : directed and directed-random\n");
    // special-values testing.
    test_sf64_to_sf32_dir_one( 0x1ULL ); // zero and small denormals
    test_sf64_to_sf32_dir_one( 0x8000000000000001ULL ); // negative zero and small denormals
    test_sf64_to_sf32_dir_one( 0x7FF0000000000000ULL ); // infinity, largest-smaller-than, NaN 
    test_sf64_to_sf32_dir_one( 0xFFF0000000000000ULL ); // negative infinity, largest-smaller-than, NaN 
    // directed-random testing: 6*5 million tests in each rounding mode.
    test_sf64_to_sf32_dir_rand( 4000000 );
    // random-testing: 30 million values in each rounding mode
    printf("Test: sf64_to_sf32() : random\n");
    test_sf64_to_sf32_rand( 30000000 );

    // conversion fron FP32 to FP64 and back again, with every value in every round-mode
    test_sf32_up_and_down();
    }




// test that converting from FP16 to FP64 and back again works correctly
// for one rounding mode
void test_sf16_to_sf64_and_back_rm( roundmode rm )
    {
    uint32_t i;
    
    uint16_t inp;
    uint64_t temp;
    uint16_t res, expected;
    for(i=0; i < 65536; i++)
        {
        inp = i;
        temp = _mali_sf32_to_sf64( _mali_sf16_to_sf32( inp ) );
        res = _mali_sf64_to_sf16( temp, rm );
        expected = _mali_isnan_sf16( inp )
            ? (inp | 0x200)
            : inp;
        if( res != expected )
            {
            printf("SF16_to_SF64 up-and-down (roundmode=%s) Input: %4.4" PRIX32 " -> Converted to %16.16" PRIX64 " -> Back to %4.4" PRIX32 " (expected %4.4" PRIX32 ")\n",
                round_names[rm], inp, temp, res, expected );            
            }
        }
    }


// test that converting from FP16 to FP64 and back again works correctly
// for all rounding modes
void test_sf16_to_sf64_and_back( void )
    {
    test_sf16_to_sf64_and_back_rm( SF_UP );
    test_sf16_to_sf64_and_back_rm( SF_DOWN );
    test_sf16_to_sf64_and_back_rm( SF_TOZERO );
    test_sf16_to_sf64_and_back_rm( SF_NEARESTEVEN );
    test_sf16_to_sf64_and_back_rm( SF_NEARESTAWAY );    
    }




// top-level function for testing sf64_to_sf32
void test_sf64_to_sf16( void )
    {
    
    printf("Test: sf64_to_sf16() : directed and directed-random\n");
    // special-values testing.
    test_sf64_to_sf16_dir_one( 0x1ULL ); // zero and small denormals
    test_sf64_to_sf16_dir_one( 0x8000000000000001ULL ); // negative zero and small denormals
    test_sf64_to_sf16_dir_one( 0x7FF0000000000000ULL ); // infinity, largest-smaller-than, NaN 
    test_sf64_to_sf16_dir_one( 0xFFF0000000000000ULL ); // negative infinity, largest-smaller-than, NaN 
    // directed-random testing: 6*5 million tests in each rounding mode.
    test_sf64_to_sf16_dir_rand( 4000000 );
    // random-testing: 30 million values in each rounding mode
    printf("Test: sf64_to_sf16() : random\n");
    test_sf64_to_sf16_rand( 30000000 );

    // conversion fron FP32 to FP64 and back again, with every value in every round-mode
    test_sf16_to_sf64_and_back();
    }











// helper functions to perform rounding of floating-point values.
// These are necessary because rint() is broken in glibc.

// round towards zero
double rnd_tozero( double p )
    {
    if( p > 0.0) return floor(p);
    return ceil(p);
    }

// round-to-nearest, but away from zero.
double rnd_nearestaway( double p )
    {
    // don't round if the number is already an integer. This avoids a particular problem:
    // if you have an odd input number in the 2^52 .. 2^53 range, then adding 0.5 will force
    // roundoff, turning the odd integer into the next even integer up.
    if( p == floor(p)) return p;
    // we have another problem as well:
    //  (0.5 minus 1 ulp) + 0.5 is rounded up to 1, giving us an incorrect result
    // for the (0.5 minus 1 ulp) quantity)
    if( fabs(p) < 0.5 )
        {
        if( p > 0.0 ) return floor(p);
        else return ceil(p);
        }
    
    if( p > 0.0) return floor(p + 0.5);
    else return ceil(p-0.5);
    }

// round-to-nearest-even.
double rnd_nearesteven( double p )
    {
    // don't round if the number is already an integer. 
    if( p == floor(p)) return p;
    
    // if the number's absolute value is less than 0.5, then return 0.
    if( fabs(p) < 0.5 )
        {
        if( p > 0.0 ) return floor(p);
        else return ceil(p);
        }
    
    
    // tres_d2 is a temporary result. If tres_d2 is an integer, then p is a half-integer
    // above an odd number. In this case, we need to round down, and tres_d2 * 2 is indeed
    // the correct result.
    
    // for all other cases, floor( p + 0.5 ) returns a good result.
    
    {
		double tres_d2 = (p - 0.5) * 0.5;
		if( tres_d2 == floor( tres_d2 ))
			return tres_d2 * 2.0;
		else
			return floor( p + 0.5 );  
	}
	
    }


// table of pointers to the rounding functions.
double (*roundfunc_table[5])(double) =
    {
    ceil, // SF_UP
    floor, // SF_DOWN
    rnd_tozero, // SF_TOZERO
    rnd_nearesteven, // SF_NEARESTEVEM
    rnd_nearestaway, // SF_NEARESTAWAY
    };
    


// function to test sf16_round with a single value. Discrepancies
// are allowed for NaN, however for NaN, we must define and check NaN behavior.
static inline void test_round_sf16_single( sf16 vl, roundmode rm )
    {
    if32 vl1, vl2;
    uint16_t p;
    
    double (*rfunc)(double) = roundfunc_table[rm];
    vl1.u = _mali_sf16_to_sf32( vl );
    p = _mali_round_sf16( vl, rm );
    vl2.u = _mali_sf16_to_sf32( p );
    if( vl2.f != rfunc( vl1.f ))
        {
        // if both input and result are NaN, check that the result is just
        // a quietened version of the input. 
        if( isnan(vl2.f) && isnan(rfunc(vl1.f)))
            {
            if( p != (vl | 0x200) )
                printf("SF16_ROUND (mode=%s) nan-error: input=%4.4" PRIX32 " (%f) result=%4.4" PRIX32 " (%f) expected=%f\n",
                    round_names[rm],
                    vl, vl1.f, p, vl2.f, rfunc (vl1.f) );
            }
        // if equality failed when NOT both input and result are NaN, then we have a failure
        else
            {        
            printf("SF16_ROUND (mode=%s) error: input=%4.4" PRIX32 " (%f) result=%4.4" PRIX32 " (%f) expected=%f\n",
                round_names[rm],
                vl, vl1.f, p, vl2.f, rfunc (vl1.f) );
            }
        }   
    }



// function to test sf32_round with a single value. Discrepancies
// are allowed for NaN
static inline void test_round_sf32_single( sf32 vl, roundmode rm )
    {
    if32 vl1, vl2;
    
    double (*rfunc)(double) = roundfunc_table[rm];
    vl1.u = vl;
    vl2.u = _mali_round_sf32( vl1.u, rm );
    if( vl2.f != rfunc( vl1.f ))
        {
        // if both input and result are NaN, check that the result is just
        // a quietened version of the input. 
        if( isnan(vl2.f) && isnan(rfunc(vl1.f)))
            {
            if( vl2.u != (vl1.u | 0x400000U))
                printf("SF32_ROUND (mode=%s) nan-error: input=%8.8" PRIX32 " (%f) result=%8.8" PRIX32 " (%f) expected=%f\n",
                    round_names[rm],
                    vl1.u, vl1.f, vl2.u, vl2.f, rfunc(vl1.f) );
            }
        // if equality failed when NOT both input and result are NaN, then we have a failure
        else
            {        
            printf("SF32_ROUND (mode=%s) error: input=%8.8" PRIX32 " (%f) result=%8.8" PRIX32 " (%f) expected=%f\n",
                round_names[rm],
                vl1.u, vl1.f, vl2.u, vl2.f, rfunc (vl1.f) );
            }
        }   
    }



// function to test sf64_round with a single value
static inline void test_round_sf64_single( sf64 vl, roundmode rm )
    {
    if64 vl1, vl2;
    
    double (*rfunc)(double) = roundfunc_table[rm];
    vl1.u = vl;
    vl2.u = _mali_round_sf64( vl1.u, rm );
    if( vl2.f != rfunc( vl1.f ))
        {
        // if both input and result are NaN, check that the result is just
        // a quietened version of the input. 
        if( isnan(vl2.f) && isnan(rfunc(vl1.f)))
            {
            if( vl2.u != (vl1.u | 0x8000000000000ULL))
                printf("SF64_ROUND (mode=%s) nan-error: input=%16.16" PRIX64 " (%f) result=%16.16" PRIX64 " (%f) expected=%f\n",
                    round_names[rm],
                    vl1.u, vl1.f, vl2.u, vl2.f, rfunc(vl1.f) );
            }
        // if equality failed when NOT both input and result are NaN, then we have a failure
        else
            {        
            printf("SF64_ROUND (mode=%s) error: input=%16.16" PRIX64 " (%f) result=%16.16" PRIX64 " (%f) expected=%f\n",
                round_names[rm],
                vl1.u, vl1.f, vl2.u, vl2.f, rfunc (vl1.f) );
            }
        }   
    }



// function to test sf16_round exhaustively.
void test_round_sf16( void )
    {
    uint32_t i;
    for(i=0;i<65536;i++)
        {
        test_round_sf16_single( i, SF_UP );
        test_round_sf16_single( i, SF_DOWN );
        test_round_sf16_single( i, SF_TOZERO );
        test_round_sf16_single( i, SF_NEARESTEVEN );
        test_round_sf16_single( i, SF_NEARESTAWAY );
        }    
    }



// function to test sf32_round exhaustively.
void test_round_sf32( void )
    {
    uint32_t i;
    printf("Test: round_sf32(), round-mode = UP\n");
    i = 0U;
    do  {
        test_round_sf32_single( i, SF_UP ); 
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: round_sf32(), round-mode = DOWN\n");
    i = 0U;
    do  {
        test_round_sf32_single( i, SF_DOWN ); 
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: round_sf32(), round-mode = TOZERO\n");
    i = 0U;
    do  {
        test_round_sf32_single( i, SF_TOZERO ); 
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: round_sf32(), round-mode = NEARESTEVEN\n");
    i = 0U;
    do  {
        test_round_sf32_single( i, SF_NEARESTEVEN ); 
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: round_sf32(), round-mode = NEARESTAWAY\n");
    i = 0U;
    do  {
        test_round_sf32_single( i, SF_NEARESTAWAY ); 
        } while( i++ != 0xFFFFFFFFU );
    }






// function to test sf64_round with six values. The six values
// are:
//  - one selected input value
//  - the value +/-1 ULP
//  - negations of the values above
static void test_round_sf64_set( roundmode rm, uint64_t vl )
    {
    uint64_t ngsign = 0x8000000000000000ULL;
    test_round_sf64_single( vl, rm );
    test_round_sf64_single( vl ^ ngsign, rm);
    test_round_sf64_single( vl + 1ULL, rm);
    test_round_sf64_single( (vl ^ ngsign) + 1ULL, rm);
    test_round_sf64_single( vl - 1ULL, rm);
    test_round_sf64_single( (vl ^ ngsign) - 1ULL, rm);
    }



// function to test sf64_round with pseudo-random numbers.
void test_round_sf64_randomized( roundmode rm, int count )
    {
    uint64_t lfsr = 1ULL;
    int i;
    for(i=0;i<count;i++)
        {
        test_round_sf64_single( lfsr, rm );
        lfsr = lfsr64( lfsr );
        }
    }


// function to test sf64_round with selected values:
// The values are:
//  - S1: any power of 2.
//  - S2: S1 * (1.0 + pow(2,-p)) where p ranges from 1 to 55
//  - S3: S1 * (1.0 + 3.0 * pow(2,-p)) where p ranges from 2 to 56
//  - S4: any value that can be obtained from {S1,S2,S3} by adding or subtracting an ULP.
// These values are selected to exercise rounding-behavior.
// They amount to about 1.3 million tests.


void test_round_sf64_selected( roundmode rm )
    {
    if64 vl1, vl2;
    int i,j;

    vl1.f = pow( 2.0, -1074 );
    for(i=-1074;i<1023;i++)
        {
        double scaler1 = 1.0;
        test_round_sf64_set( rm, vl1.u );
        for(j=0;j<55;j++)
            {
            scaler1 *= 0.5;
            vl2.f = vl1.f * (1.0 + scaler1);
            test_round_sf64_set( rm, vl2.u );
            vl2.f = vl1.f * (1.0 + (scaler1 * 0.75));
            test_round_sf64_set( rm, vl2.u );         
            }
        vl1.f *= 2.0;
        }
    }


void test_round_sf64_one_mode( roundmode rm )
    {
    // perform a few tests with special values.
    test_round_sf64_set( rm, 1ULL ); // tests zero and the two smallest-representable denormal values.
    test_round_sf64_set( rm, 0x7FF0000000000000ULL ); // tests Inf, NaN and largest-value-smaller-than-inf

    // perform randomized test with 200 million random-values.
    test_round_sf64_randomized( rm, 200000000 );

    // perform directed testing with 1.3 million tests.
    test_round_sf64_selected( rm );    
    }

void test_round_sf64( void )
    {
    printf("Test: round_sf64(), round-mode = UP\n");
    test_round_sf64_one_mode( SF_UP );
    printf("Test: round_sf64(), round-mode = DOWN\n");
    test_round_sf64_one_mode( SF_DOWN );
    printf("Test: round_sf64(), round-mode = TOZERO\n");
    test_round_sf64_one_mode( SF_TOZERO );
    printf("Test: round_sf64(), round-mode = NEARESTEVEN\n");
    test_round_sf64_one_mode( SF_NEARESTEVEN );
    printf("Test: round_sf64(), round-mode = NEARESTAWAY\n");
    test_round_sf64_one_mode( SF_NEARESTAWAY );
    }





// tests for conversions from floating-point to integer.
// They assume that the round_sf*() functions are correct.

static void test_sf16_to_s16_one_value( sf16 inp, roundmode rm )
    {
    if32 inp2;
    if32 p;
    int16_t expected;
    int16_t res;
    
    inp2.u = _mali_sf16_to_sf32( inp );
    p.u = _mali_sf16_to_sf32( inp );
    if( isnan( p.f )) expected = 0;
    else if ( fabs( p.f ) >= 32768.0 )
        {
        if( p.f > 0.0 )
            expected = 0x7FFFU;
        else
            expected = 0x8000U;
        }
    else
        {
        p.u = _mali_round_sf32( p.u, rm );
        expected = (int16_t) p.f;
        }
    res = _mali_sf16_to_s16( inp, rm );
    if( res != expected )
        printf("SF16_to_S16 (mode=%s) error: input=%4.4" PRIX32 " (%f)  result=%" PRId32 "  expected=%" PRId32 "\n",
            round_names[rm],
            inp, inp2.f, res, expected );  
    }



static void test_sf32_to_s32_one_value( sf32 inp, roundmode rm )
    {
    if32 inp2;
    if32 p;
    int32_t expected;
    int32_t res;

    inp2.u = inp;
    p.u = inp;
    if( isnan( p.f )) expected = 0;
    else if ( fabs( p.f ) >= 2147483648.0 )
        {
        if( p.f > 0.0 )
            expected = 0x7FFFFFFFU;
        else
            expected = 0x80000000U;
        }
    else
        {
        p.u = _mali_round_sf32( p.u, rm );
        expected = (int32_t) p.f;
        }
    res = _mali_sf32_to_s32( inp, rm );
    if( res != expected )
        printf("SF32_to_S32 (mode=%s) error: input=%8.8" PRIX32 " (%f)  result=%" PRId32 "  expected=%" PRId32 "\n",
            round_names[rm],
            inp, inp2.f, res, expected );
    }



static void test_sf64_to_s64_one_value( sf64 inp, roundmode rm )
    {
    if64 inp2;
    if64 p;
    int64_t expected;
    int64_t res;
    
    inp2.u = inp;
    p.u = inp;
    
    if( isnan( p.f )) expected = 0;
    else if ( fabs( p.f ) >= 9223372036854775808.0 )
        {
        if( p.f > 0.0 )
            expected = 0x7FFFFFFFFFFFFFFFULL;
        else
            expected = 0x8000000000000000ULL;
        }
    else
        {
        p.u = _mali_round_sf64( p.u, rm );
        expected = (int64_t) p.f;
        }
    res = _mali_sf64_to_s64( inp, rm );
    if( res != expected )
        printf("SF64_to_S64 (mode=%s) error: input=%16.16" PRIX64 " (%f)  result=%" PRId64 "  expected=%" PRId64 "\n",
            round_names[rm],
            inp, inp2.f, res, expected );   
    }



static void test_sf64_to_s64_rand_mode( roundmode rm, int count )
    {
    int i;
    uint64_t lfsr = 1ULL;
    for(i=0;i<count;i++)
        {
        test_sf64_to_s64_one_value( lfsr, rm );
        lfsr = lfsr64( lfsr );
        }
    }


// perform 200 million random tests of sf64_to_s64 in each round mode.
void test_sf64_to_s64( void )
    {
    printf("Test: sf64_to_s64, round-mode UP\n");
    test_sf64_to_s64_rand_mode( SF_UP, 200000000 );
    printf("Test: sf64_to_s64, round-mode DOWN\n");
    test_sf64_to_s64_rand_mode( SF_DOWN, 200000000 );
    printf("Test: sf64_to_s64, round-mode TOZERO\n");
    test_sf64_to_s64_rand_mode( SF_TOZERO, 200000000 );
    printf("Test: sf64_to_s64, round-mode NEARESTEVEN\n");
    test_sf64_to_s64_rand_mode( SF_NEARESTEVEN, 200000000 );
    printf("Test: sf64_to_s64, round-mode NEARESTAWAY\n");
    test_sf64_to_s64_rand_mode( SF_NEARESTAWAY, 200000000 );   
    }


// Exhaustive-test sf16_to_s16
void test_sf16_to_s16( void )
    {
    uint32_t i;
    for(i=0;i<65536;i++)
        {
        test_sf16_to_s16_one_value( i, SF_UP );
        test_sf16_to_s16_one_value( i, SF_DOWN );
        test_sf16_to_s16_one_value( i, SF_TOZERO );
        test_sf16_to_s16_one_value( i, SF_NEARESTEVEN );
        test_sf16_to_s16_one_value( i, SF_NEARESTAWAY );
        }
    }


// Exhaustive-test sf32_to_s32
void test_sf32_to_s32( void )
    {
    uint32_t i;
    printf("Test: sf32_to_s32, round-mode UP\n");
    i = 0U;
    do  {
        test_sf32_to_s32_one_value( i, SF_UP );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: sf32_to_s32, round-mode DOWN\n");
    i = 0U;
    do  {
        test_sf32_to_s32_one_value( i, SF_DOWN );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: sf32_to_s32, round-mode TOZERO\n");
    i = 0U;
    do  {
        test_sf32_to_s32_one_value( i, SF_TOZERO );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: sf32_to_s32, round-mode NEARESTEVEN\n");
    i = 0U;
    do  {
        test_sf32_to_s32_one_value( i, SF_NEARESTEVEN );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: sf32_to_s32, round-mode NEARESTAWAY\n");
    i = 0U;
    do  {
        test_sf32_to_s32_one_value( i, SF_NEARESTAWAY );
        } while( i++ != 0xFFFFFFFFU );
          
    }




static void test_sf16_to_u16_one_value( sf16 inp, roundmode rm )
    {
    if32 inp2;
    if32 p;
    uint16_t expected;
    uint16_t res;
    
    inp2.u = _mali_sf16_to_sf32( inp );
    p.u = _mali_sf16_to_sf32( inp );
    if( isnan( p.f ) || p.f < 0.0) 
        expected = 0;
    else if ( p.f >= 65536.0 )
        expected = 65535;
    else
        {
        p.u = _mali_round_sf32( p.u, rm );
        expected = (uint16_t)p.f;
        }
    res = _mali_sf16_to_u16( inp, rm );
    if( res != expected )
        printf("SF16_to_U16 (mode=%s) error: input=%4.4" PRIX32 " (%f)  result=%u  expected=%u\n",
            round_names[rm],
            inp, inp2.f, res, expected );  
    }


static void test_sf32_to_u32_one_value( sf32 inp, roundmode rm )
    {
    if32 inp2;
    if32 p;
    uint32_t expected;
    uint32_t res;

    inp2.u = inp;
    p.u = inp;
    if( isnan( p.f ) || p.f < 0.0) 
        expected = 0;
    else if ( p.f >= 4294967296.0 )
        expected = 0xFFFFFFFFU;
    else
        {
        p.u = _mali_round_sf32( p.u, rm );
        expected = (uint32_t) p.f;
        }
    res = _mali_sf32_to_u32( inp, rm );
    if( res != expected )
        printf("SF32_to_U32 (mode=%s) error: input=%8.8" PRIX32 " (%f)  result=%u  expected=%u\n",
            round_names[rm],
            inp, inp2.f, res, expected );    
    }



static void test_sf64_to_u64_one_value( sf64 inp, roundmode rm )
    {
    if64 inp2;
    if64 p;
    uint64_t expected;
    uint64_t res;
    
    inp2.u = inp;
    p.u = inp;
    
    if( isnan( p.f ) || p.f < 0.0) 
        expected = 0;
    else if ( fabs( p.f ) >= 18446744073709551616.0 )
        expected = 0xFFFFFFFFFFFFFFFFULL;
    else
        {
        p.u = _mali_round_sf64( p.u, rm );
        expected = (uint64_t) p.f;
        }
    res = _mali_sf64_to_u64( inp, rm );
    if( res != expected )
        printf("SF64_to_U64 (mode=%s) error: input=%16.16" PRIX64 " (%f)  result=%" PRIu64 "  expected=%" PRIu64"\n",
            round_names[rm],
            inp, inp2.f, res, expected );        
    }



void test_sf64_to_u64_rand_mode( roundmode rm, int count )
    {
    int i;
    uint64_t lfsr = 1ULL;
    for(i=0;i<count;i++)
        {
        test_sf64_to_u64_one_value( lfsr, rm );
        lfsr = lfsr64( lfsr );
        }
    }



void test_sf64_to_u64( void )
    {
    printf("Test: sf64_to_u64, round-mode UP\n");
    test_sf64_to_u64_rand_mode( SF_UP, 200000000 );
    printf("Test: sf64_to_u64, round-mode DOWN\n");
    test_sf64_to_u64_rand_mode( SF_DOWN, 200000000 );
    printf("Test: sf64_to_u64, round-mode TOZERO\n");
    test_sf64_to_u64_rand_mode( SF_TOZERO, 200000000 );
    printf("Test: sf64_to_u64, round-mode NEARESTEVEN\n");
    test_sf64_to_u64_rand_mode( SF_NEARESTEVEN, 200000000 );
    printf("Test: sf64_to_u64, round-mode NEARESTAWAY\n");
    test_sf64_to_u64_rand_mode( SF_NEARESTAWAY, 200000000 );   
    }






void test_sf16_to_u16( void )
    {
    uint32_t i;
    for(i=0;i<65536;i++)
        {
        test_sf16_to_u16_one_value( i, SF_UP );
        test_sf16_to_u16_one_value( i, SF_DOWN );
        test_sf16_to_u16_one_value( i, SF_TOZERO );
        test_sf16_to_u16_one_value( i, SF_NEARESTEVEN );
        test_sf16_to_u16_one_value( i, SF_NEARESTAWAY );
        }
    }


void test_sf32_to_u32( void )
    {
    uint32_t i;
    printf("Test: sf32_to_u32, round-mode UP\n");
    i = 0U;
    do  {
        test_sf32_to_u32_one_value( i, SF_UP );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: sf32_to_u32, round-mode DOWN\n");
    i = 0U;
    do  {
        test_sf32_to_u32_one_value( i, SF_DOWN );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: sf32_to_u32, round-mode TOZERO\n");
    i = 0U;
    do  {
        test_sf32_to_u32_one_value( i, SF_TOZERO );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: sf32_to_u32, round-mode NEARESTEVEN\n");
    i = 0U;
    do  {
        test_sf32_to_u32_one_value( i, SF_NEARESTEVEN );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: sf32_to_u32, round-mode NEARESTAWAY\n");
    i = 0U;
    do  {
        test_sf32_to_u32_one_value( i, SF_NEARESTAWAY );
        } while( i++ != 0xFFFFFFFFU );          
    }


/*
for 16/32 bit data types, our test routines
for int->float conversions rely on the float->float routines
to have been properly tested. What we do is:

 - widen the input value so that it can be converted without rounding error
 - use a C cast to perform conversion from integer to floating-point in the widened format
 - then convert from the wide format down to the narrower format again with
   our own (previosuly-tested) conversion routines.
   
 - The result is then compared to our actual int->float conversion routines.  
*/

static inline void test_s16_to_sf16_one_value( int16_t inp, roundmode rm )
    {
    int32_t vl1 = (int32_t) inp;
    if32 p;
    uint16_t res;
    uint16_t expected;
    if32 res_x;
    if32 expected_x;
    p.f = (float) vl1;
    expected = _mali_sf32_to_sf16( p.u, rm );
    res = _mali_s16_to_sf16( inp, rm );
    if( res != expected )
        {
        res_x.u = _mali_sf16_to_sf32( res );
        expected_x.u = _mali_sf16_to_sf32( expected );
        printf("S16_to_SF16 (mode=%s) error: input=%4.4" PRIX32 "  result=%f (%4.4" PRIX32 ")  expected=%f (%4.4" PRIX32 ")\n",
            round_names[rm],
            inp, res_x.f, res, expected_x.f, expected );
        }
    } 

static inline void test_u16_to_sf16_one_value( uint16_t inp, roundmode rm )
    {
    int32_t vl1 = (int32_t)inp & 0xFFFF;
    if32 p;
    uint16_t res;
    uint16_t expected;
    if32 res_x;
    if32 expected_x;
    p.f = (float) vl1;
    expected = _mali_sf32_to_sf16( p.u, rm );
    res = _mali_u16_to_sf16( inp, rm );
    if( res != expected )
        {
        res_x.u = _mali_sf16_to_sf32( res );
        expected_x.u = _mali_sf16_to_sf32( expected );
        printf("U16_to_SF16 (mode=%s) error: input=%4.4" PRIX32 "  result=%f (%4.4" PRIX32 ")  expected=%f (%4.4" PRIX32 ")\n",
            round_names[rm],
            inp, res_x.f, res, expected_x.f, expected );
        }
    } 


static inline void test_s32_to_sf32_one_value( int32_t inp, roundmode rm )
    {
    int64_t vl1 = (int64_t) inp;
    if64 p;
    if32 res;
    if32 expected;
    p.f = (double) vl1;
    expected.u = _mali_sf64_to_sf32( p.u, rm );
    res.u = _mali_s32_to_sf32( inp, rm );
    if( res.u != expected.u )
        printf("S32_to_SF32 (mode=%s) error: input=%8.8" PRIX32 "  result=%f (%8.8" PRIX32 ")  expected=%f (%8.8" PRIX32 ")\n",
            round_names[rm],
            inp, res.f, res.u, expected.f, expected.u );    
    }

static inline void test_u32_to_sf32_one_value( uint32_t inp, roundmode rm )
    {
    int64_t vl1 = (int64_t)inp & 0xFFFFFFFF;
    if64 p;
    if32 res;
    if32 expected;
    p.f = (double) vl1;
    expected.u = _mali_sf64_to_sf32( p.u, rm );
    res.u = _mali_u32_to_sf32( inp, rm );
    if( res.u != expected.u )
        printf("U32_to_SF32 (mode=%s) error: input=%8.8" PRIX32 "  result=%f (%8.8" PRIX32 ")  expected=%f (%8.8" PRIX32 ")\n",
            round_names[rm],
            inp, res.f, res.u, expected.f, expected.u );    
    }


/*
for 64-bit integer->float conversion routines, we must perform tests differently,
at least for numbers greater than 2^53:
 - first, convert from integer to float using our own routine.
 - Then convert back, using a C cast.
Also, take the float-value, add/subtract 1 ULP, then
convert those back using C casts as well. We can then perform comparisons
between these three values and the input value to check whether rounding is done correctly.
*/



void test_u64_to_sf64_one_value( uint64_t inp, roundmode rm )
    {
    if64 p;
    if64 p_up;
    if64 p_down;
    uint64_t res;
    uint64_t res_up;
    uint64_t res_down;
    int error = 0;
    
    p.u = _mali_u64_to_sf64( inp, rm );
    res = (uint64_t)p.f;
    
    if( res == inp ) return; // exact result is always correct.
    if( inp <= 0x20000000000000ULL )
        {
        // if we received an input number of 2^53 or smaller,then anything but an
        // exact result is intolerable. This is an error. 
        printf("U64_to_SF64 (mode=%s) error1: input=%16.16" PRIX64 " (%" PRIu64 ")  result=%f (%16.16" PRIX64 ")\n",
            round_names[rm], inp, inp, p.f, res );    
        return;
        }    

    // compute the results that arise from taking the FP64 conversion result,
    // adding/subtracting 1 ULP, then converting the result back to integer.
    p_up.u = p.u + 1ULL;
    p_down.u = p.u - 1ULL;
    res_up = (uint64_t)p_up.f;
    res_down = (uint64_t)p_down.f;
        
    if( res_down > inp || res_up < inp )
        {
        // if the actual result is not within the computed result +/-1 ULP, then we
        // know that we have an error. 
        printf("U64_to_SF64 (mode=%s) error2: input=%16.16" PRIX64 " (%" PRIu64 ")  result=%f (%16.16" PRIX64 ")\n",
            round_names[rm], inp, inp, p.f, res );    
        return;
        }
    
    // for remaining cases, we need to do a rounding-mode specific test.
    switch( rm )
        {
        case SF_UP:
            // error if result was rounded down.
            if( res < inp )
                error = 3;
            break;
        case SF_DOWN:
        case SF_TOZERO:
            // error if result was rounded up
            if( res > inp )
                error = 4;
            break;
                    
        case SF_NEARESTEVEN:
            // for nearest-even, testing for correct rounding is much more involved.
            if( res < inp )
                {
                // result was rounded down.
                int64_t diff1 = res - inp;
                int64_t diff2 = inp - res_down;
                if( diff1 > diff2 )
                    // error: rounded away from nearest.
                    error = 5;
                else if(diff1 == diff2)
                    {
                    // round to nearest ambiguity: decide whether rounding down
                    // was in fact appropriate for round-to-nearest-even.
                    // This is the case when 'res' has more trailing zeroes than
                    // 'res_down'.
                    // In order to check this quickly, we exploit the bit-hack that
                    // the expression (v & -v) on an integer evaluates to 1 << ctz(v),
                    // where ctz is the count-trailing-zeroes function.
                    // Because of this, we get
                    //  ctz(v1) > ctz(v2)  =>
                    //  1 << ctz(v1) > 1 << ctz(v2) =>
                    //  (v1 & -v1) > (v2 & -v2)
                    if( (res & -res) < (res_down & -res_down) )
                        error = 6;                                       
                    }
                }
            else
                {
                // result was rounded up
                int64_t diff1 = inp - res;
                int64_t diff2 = res_up - inp;
                if( diff1 > diff2 )
                    // error: rounded away from nearest.
                    error = 1;
                else if( diff1 == diff2 )
                    {
                    // round to nearest ambiguity: decide whether rounding up
                    // was in fact appropriate for round-to-nearest-even.
                    if( (res & -res) < (res_up & -res_up) )
                        error = 7;
                    }
                
                }
            break;
        
        case SF_NEARESTAWAY:
            // for nearest-away, testing for correct rounding is also quite involved.
            if( res < inp )
                {
                // result was rounded down.
                int64_t diff1 = res - inp;
                int64_t diff2 = inp - res_down;
                if( diff1 > diff2 )
                    // error: rounded away from nearest.                    
                    error = 8;
                else if(diff1 == diff2)
                    // round to nearest ambiguity: round-to-nearest-away that rounds
                    // an unsigned number down in case of ambiguous rounding is incorrect.              
                    error = 9;
                }
            else
                {
                // result was rounded up
                int64_t diff1 = inp - res;
                int64_t diff2 = res_up - inp;
                if( diff1 > diff2 )
                    // error: rounded away from nearest.
                    error = 10;         
                }
            break;
        }
    if( error )
        // print error message  
        printf("U64_to_SF64 (mode=%s) error%" PRId32 ": input=%16.16" PRIX64 " (%" PRIu64 ")  result=%f (%16.16" PRIX64 ")\n",
            round_names[rm], error,
            inp, inp, p.f, res );    
    
    }



void test_s64_to_sf64_one_value( int64_t inp, roundmode rm )
    {
    if64 p;
    if64 p_up;
    if64 p_down;
    int64_t res;
    int64_t res_up;
    int64_t res_down;
    int error = 0;
    
    p.u = _mali_s64_to_sf64( inp, rm );
    res = (int64_t)p.f;
    
    if( res == inp ) return; // exact result is always correct.
    if( llabs(inp) <= 0x20000000000000LL )
        {
        // if we received an input number with absolute value of 2^53 or smaller,
        // then anything but an
        // exact result is intolerable. This is an error. 
        printf("S64_to_SF64 (mode=%s) error1: input=%16.16" PRIX64 " (%" PRId64 ")  result=%f (%16.16" PRIX64 ")\n",
            round_names[rm], inp, inp, p.f, res );    
        return;
        }    

    // compute the results that arise from taking the FP64 conversion result,
    // adding/subtracting 1 ULP, then converting the result back to integer.
    p_up.u = nextabove_sf64(p.u);
    p_down.u = nextbelow_sf64(p.u);
    res_up = (int64_t)p_up.f;
    res_down = (int64_t)p_down.f;
        
    if( res_down > inp || res_up < inp )
        {
        // if the actual result is not within the computed result +/-1 ULP, then we
        // know that we have an error. 
        printf("S64_to_SF64 (mode=%s) error2: input=%16.16" PRIX64 " (%" PRId64 ")  result=%f (%16.16" PRIX64 ")\n",
            round_names[rm], inp, inp, p.f, res );    
        return;
        }
    
    // for remaining cases, we need to do a rounding-mode specific test.
    switch( rm )
        {
        case SF_UP:
            // error if result was rounded down.
            if( res < inp )
                error = 3;
            break;
        case SF_DOWN:
            // error if result was rounded up
            if( res > inp )
                error = 4;
            break;
        case SF_TOZERO:
            // error if result is farther from zero than the input.
            if( llabs(res) > llabs(inp) )
                error = 5;
            break;
        
                    
        case SF_NEARESTEVEN:
            // for nearest-even, testing for correct rounding is much more involved.
            if( res < inp )
                {
                // result was rounded down.
                int64_t diff1 = res - inp;
                int64_t diff2 = inp - res_down;
                if( diff1 > diff2 )
                    // error: rounded away from nearest.
                    error = 6;
                else if(diff1 == diff2)
                    {
                    // round to nearest ambiguity: decide whether rounding down
                    // was in fact appropriate for round-to-nearest-even.
                    // This is the case if res has more trailing zeroes than res_down.
                    // In order to check this quickly, we exploit the bit-hack that
                    // the expression (v & -v) on an integer evaluates to 1 << ctz(v),
                    // where ctz is the count-trailing-zeroes function.
                    // Because of this, we get
                    //  ctz(v1) > ctz(v2)  =>
                    //  1 << ctz(v1) > 1 << ctz(v2) =>
                    //  (v1 & -v1) > (v2 & -v2)
                    if( (res & -res) < (res_down & -res_down) )
                        error = 7;
                    }
                }
            else
                {
                // result was rounded up
                int64_t diff1 = inp - res;
                int64_t diff2 = res_up - inp;
                if( diff1 > diff2 )
                    // error: rounded away from nearest.
                    error = 8;
                else if( diff1 == diff2 )
                    {
                    // round to nearest ambiguity: decide whether rounding up
                    // was in fact appropriate for round-to-nearest-even.
                    if( (res & -res) < (res_up & -res_up) )
                        error = 9;                     
                    }
                
                }
            break;
        
        case SF_NEARESTAWAY:
            // for nearest-away, testing for correct rounding is also quite involved.
            if( res < inp )
                {
                // result was rounded down.
                int64_t diff1 = res - inp;
                int64_t diff2 = inp - res_down;
                if( diff1 > diff2 )
                    // error: rounded away from nearest.                    
                    error = 10;
                else if(diff1 == diff2)
                    // round to nearest ambiguity: round-to-nearest-away that rounds
                    // an unsigned number down is an error if the input number is positive.
                    if( inp > 0LL )
                        error = 11;
                }
            else
                {
                // result was rounded up
                int64_t diff1 = inp - res;
                int64_t diff2 = res_up - inp;
                if( diff1 > diff2 )
                    // error: rounded away from nearest.
                    error = 12;         
                else if(diff1 == diff2)
                    // round to nearest ambiguity: round-to-nearest-away that rounds
                    // an unsigned number up is an error if the input number is negative.
                    if( inp < 0LL )
                        error = 13;
                }
            break;
        }
    if( error )
        // print error message  
        printf("S64_to_SF64 (mode=%s) error%" PRId32 ": input=%16.16" PRIX64 " (%" PRId64 ")  result=%f (%16.16" PRIX64 ")\n",
            round_names[rm], error,
            inp, inp, p.f, res );    
    }



void test_u64_to_sf64_one_mode( roundmode rm, int count )
    {
    uint64_t lfsr = 1ULL;
    int i;
    for(i=0;i<count;i++)
        {
        test_u64_to_sf64_one_value( lfsr, rm );
        lfsr = lfsr64( lfsr );
        }
    }

void test_s64_to_sf64_one_mode( roundmode rm, int count )
    {
    uint64_t lfsr = 1ULL;
    int i;
    for(i=0;i<count;i++)
        {
        test_s64_to_sf64_one_value( lfsr, rm );
        lfsr = lfsr64( lfsr );
        }
    }


void test_u64_to_sf64( void )
    {
    printf("Test: u64_to_sf64, round-mode UP\n");
    test_u64_to_sf64_one_mode( SF_UP, 200000000 );
    printf("Test: u64_to_sf64, round-mode DOWN\n");
    test_u64_to_sf64_one_mode( SF_DOWN, 200000000 );
    printf("Test: u64_to_sf64, round-mode TOZERO\n");
    test_u64_to_sf64_one_mode( SF_TOZERO, 200000000 );
    printf("Test: u64_to_sf64, round-mode NEARESTEVEN\n");
    test_u64_to_sf64_one_mode( SF_NEARESTEVEN, 200000000 );
    printf("Test: u64_to_sf64, round-mode NEARESTAWAY\n");
    test_u64_to_sf64_one_mode( SF_NEARESTAWAY, 200000000 );
    }


void test_s64_to_sf64( void )
    {
    printf("Test: s64_to_sf64, round-mode UP\n");
    test_s64_to_sf64_one_mode( SF_UP, 200000000 );
    printf("Test: s64_to_sf64, round-mode DOWN\n");
    test_s64_to_sf64_one_mode( SF_DOWN, 200000000 );
    printf("Test: s64_to_sf64, round-mode TOZERO\n");
    test_s64_to_sf64_one_mode( SF_TOZERO, 200000000 );
    printf("Test: s64_to_sf64, round-mode NEARESTEVEN\n");
    test_s64_to_sf64_one_mode( SF_NEARESTEVEN, 200000000 );
    printf("Test: s64_to_sf64, round-mode NEARESTAWAY\n");
    test_s64_to_sf64_one_mode( SF_NEARESTAWAY, 200000000 );
    }




void test_u16_to_sf16( void )
    {
    uint32_t i;
    for(i=0;i<65536;i++)
        {
        test_u16_to_sf16_one_value( i, SF_UP );
        test_u16_to_sf16_one_value( i, SF_DOWN );
        test_u16_to_sf16_one_value( i, SF_TOZERO );
        test_u16_to_sf16_one_value( i, SF_NEARESTEVEN );
        test_u16_to_sf16_one_value( i, SF_NEARESTAWAY );
        }
    }

void test_s16_to_sf16( void )
    {
    uint32_t i;
    for(i=0;i<65536;i++)
        {
        test_s16_to_sf16_one_value( (int16_t)i, SF_UP );
        test_s16_to_sf16_one_value( (int16_t)i, SF_DOWN );
        test_s16_to_sf16_one_value( (int16_t)i, SF_TOZERO );
        test_s16_to_sf16_one_value( (int16_t)i, SF_NEARESTEVEN );
        test_s16_to_sf16_one_value( (int16_t)i, SF_NEARESTAWAY );
        }
    }



void test_u32_to_sf32( void )
    {
    uint32_t i;
    printf("Test: u32_to_sf32, round-mode UP\n");
    i = 0U;
    do  {
        test_u32_to_sf32_one_value( i, SF_UP );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: u32_to_sf32, round-mode DOWN\n");
    i = 0U;
    do  {
        test_u32_to_sf32_one_value( i, SF_DOWN );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: u32_to_sf32, round-mode TOZERO\n");
    i = 0U;
    do  {
        test_u32_to_sf32_one_value( i, SF_TOZERO );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: u32_to_sf32, round-mode NEARESTEVEN\n");
    i = 0U;
    do  {
        test_u32_to_sf32_one_value( i, SF_NEARESTEVEN );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: u32_to_sf32, round-mode NEARESTAWAY\n");
    i = 0U;
    do  {
        test_u32_to_sf32_one_value( i, SF_NEARESTAWAY );
        } while( i++ != 0xFFFFFFFFU );
          
    }


void test_s32_to_sf32( void )
    {
    uint32_t i;
    printf("Test: s32_to_sf32, round-mode UP\n");
    i = 0U;
    do  {
        test_s32_to_sf32_one_value( (int32_t)i, SF_UP );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: s32_to_sf32, round-mode DOWN\n");
    i = 0U;
    do  {
        test_s32_to_sf32_one_value( (int32_t)i, SF_DOWN );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: s32_to_sf32, round-mode TOZERO\n");
    i = 0U;
    do  {
        test_s32_to_sf32_one_value( (int32_t)i, SF_TOZERO );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: s32_to_sf32, round-mode NEARESTEVEN\n");
    i = 0U;
    do  {
        test_s32_to_sf32_one_value( (int32_t)i, SF_NEARESTEVEN );
        } while( i++ != 0xFFFFFFFFU );
    printf("Test: s32_to_sf32, round-mode NEARESTAWAY\n");
    i = 0U;
    do  {
        test_s32_to_sf32_one_value( (int32_t)i, SF_NEARESTAWAY );
        } while( i++ != 0xFFFFFFFFU );
          
    }



/*
 Perform tests of FP32 multiply.
 This test must be done on an IEEE-754 compliant platform
 where the fesetround() function is available.
*/ 

void test_mul_sf32_roundmode( roundmode rm, int count )
    {
#ifdef HAVE_ROUNDING_MACROS
    int i;
    uint64_t lfsr = 1ULL;
    if32 p1, p2, res, expected;
    
    int old_roundmode = fegetround();
    switch( rm )
        {
        case SF_UP: fesetround( FE_UPWARD ); break;
        case SF_DOWN: fesetround( FE_DOWNWARD ); break;
        case SF_TOZERO: fesetround( FE_TOWARDZERO ); break;
        case SF_NEARESTEVEN: fesetround( FE_TONEAREST ); break;
        case SF_NEARESTAWAY: fesetround( FE_TONEAREST ); break; // warning: may be off-by-1.       
        };
    
    for(i=0;i<count;i++)
        {
        p1.u = lfsr;
        p2.u = lfsr >> 32;
        res.u = _mali_mul_sf32( p1.u, p2.u, rm, 0ULL );
        expected.f = p1.f * p2.f;
        if( res.u != expected.u && !(isnan(res.f) && isnan(expected.f))
            && !(rm == SF_NEARESTAWAY && res.u == expected.u + 1))
            {
            printf("MUL_SF32 (mode=%s) error: input1=%g(%8.8" PRIX32 ") input2=%g(%8.8" PRIX32 ")\n  result=%g(%8.8" PRIX32 ") expected=%g(%8.8" PRIX32 ")\n",
                round_names[rm], p1.f, p1.u, p2.f, p2.u, res.f, res.u, expected.f, expected.u );
            }
        lfsr = lfsr64( lfsr );
        }        
    fesetround( old_roundmode );
#else
    printf("mul_sf32 (roundmode=%s): not run\n", round_names[rm] );
#endif
    }




void test_mul_sf64_roundmode( roundmode rm, int count )
    {
#ifdef HAVE_ROUNDING_MACROS
    int i;
    l128 lfsr;
    lfsr.v32[0] = 1;
    lfsr.v32[1] = 0;
    lfsr.v32[2] = 0;
    lfsr.v32[3] = 0;
    
    if64 p1, p2, res, expected;

    int old_roundmode = fegetround();
    switch( rm )
        {
        case SF_UP: fesetround( FE_UPWARD ); break;
        case SF_DOWN: fesetround( FE_DOWNWARD ); break;
        case SF_TOZERO: fesetround( FE_TOWARDZERO ); break;
        case SF_NEARESTEVEN: fesetround( FE_TONEAREST ); break;
        case SF_NEARESTAWAY: fesetround( FE_TONEAREST ); break; // warning: may be off-by-1.       
        };

    
    for(i=0;i<count;i++)
        {
        p1.u = lfsr.v64[0];
        p2.u = lfsr.v64[1];
        res.u = _mali_mul_sf64( p1.u, p2.u, rm, 0ULL );
        expected.f = p1.f * p2.f;
        if( res.u != expected.u && !(isnan(res.f) && isnan(expected.f))
            && !(rm == SF_NEARESTAWAY && res.u == expected.u + 1))
            {
            printf("MUL_SF64 (mode=%s) error: input1=%g(%16.16" PRIX64 ") input2=%g(%16.16" PRIX64 ")\n  result=%g(%16.16" PRIX64 ") expected=%g(%16.16" PRIX64 ")\n",
                round_names[rm], p1.f, p1.u, p2.f, p2.u, res.f, res.u, expected.f, expected.u );
            }
        lfsr128( &lfsr );
        }        

    fesetround( old_roundmode );
#else
    printf("mul_sf64 (roundmode=%s): not run\n", round_names[rm] );
#endif
    }


/* we use the same approach for additions as for multiplications */

void test_add_sf32_roundmode( roundmode rm, int count )
    {
#ifdef HAVE_ROUNDING_MACROS
    int i;
    uint64_t lfsr = 1ULL;
    if32 p1, p2, res, expected;
    
    int old_roundmode = fegetround();
    switch( rm )
        {
        case SF_UP: fesetround( FE_UPWARD ); break;
        case SF_DOWN: fesetround( FE_DOWNWARD ); break;
        case SF_TOZERO: fesetround( FE_TOWARDZERO ); break;
        case SF_NEARESTEVEN: fesetround( FE_TONEAREST ); break;
        case SF_NEARESTAWAY: fesetround( FE_TONEAREST ); break; // warning: may be off-by-1.       
        };
       
    for(i=0;i<count;i++)
        {
        p1.u = lfsr;
        p2.u = lfsr >> 32;
        res.u = _mali_add_sf32( p1.u, p2.u, rm, 0ULL );
        expected.f = p1.f + p2.f;
        if( res.u != expected.u && !(isnan(res.f) && isnan(expected.f))
            && !(rm == SF_NEARESTAWAY && res.u == expected.u + 1))
            {
            printf("ADD_SF32 (mode=%s) error: input1=%g(%8.8" PRIX32 ") input2=%g(%8.8" PRIX32 ")\n  result=%g(%8.8" PRIX32 ") expected=%g(%8.8" PRIX32 ")\n",
                round_names[rm], p1.f, p1.u, p2.f, p2.u, res.f, res.u, expected.f, expected.u );
            }
        lfsr = lfsr64( lfsr );
        }        
    fesetround( old_roundmode );
#else
    printf("add_sf32 (roundmode=%s): not run\n", round_names[rm] );
#endif
    }






void test_add_sf64_roundmode( roundmode rm, int count )
    {
#ifdef HAVE_ROUNDING_MACROS
    int i;
    l128 lfsr;
    lfsr.v32[0] = 1;
    lfsr.v32[1] = 0;
    lfsr.v32[2] = 0;
    lfsr.v32[3] = 0;
    
    if64 p1, p2, res, expected;

    int old_roundmode = fegetround();
    switch( rm )
        {
        case SF_UP: fesetround( FE_UPWARD ); break;
        case SF_DOWN: fesetround( FE_DOWNWARD ); break;
        case SF_TOZERO: fesetround( FE_TOWARDZERO ); break;
        case SF_NEARESTEVEN: fesetround( FE_TONEAREST ); break;
        case SF_NEARESTAWAY: fesetround( FE_TONEAREST ); break; // warning: may be off-by-1.       
        };

    
    for(i=0;i<count;i++)
        {
        p1.u = lfsr.v64[0];
        p2.u = lfsr.v64[1];
        res.u = _mali_add_sf64( p1.u, p2.u, rm, 0ULL );
        expected.f = p1.f + p2.f;
        if( res.u != expected.u && !(isnan(res.f) && isnan(expected.f))
            && !(rm == SF_NEARESTAWAY && res.u == expected.u + 1))
            {
            printf("ADD_SF64 (mode=%s) error: input1=%g(%16.16" PRIX64 ") input2=%g(%16.16" PRIX64 ")\n  result=%g(%16.16" PRIX64 ") expected=%g(%16.16" PRIX64 ")\n",
                round_names[rm], p1.f, p1.u, p2.f, p2.u, res.f, res.u, expected.f, expected.u );
            }
        lfsr128( &lfsr );
        }        

    fesetround( old_roundmode );
#else
    printf("add_sf64 (roundmode=%s): not run\n", round_names[rm] );
#endif
    }





void test_mul_sf64( void )
    {
    printf("Test: mul_sf64, round-mode UP\n");
    test_mul_sf64_roundmode( SF_UP, 200000000 );
    printf("Test: mul_sf64, round-mode DOWN\n");
    test_mul_sf64_roundmode( SF_DOWN, 200000000 );
    printf("Test: mul_sf64, round-mode TOZERO\n");
    test_mul_sf64_roundmode( SF_TOZERO, 200000000 );
    printf("Test: mul_sf64, round-mode NEAREST-EVEN\n");
    test_mul_sf64_roundmode( SF_NEARESTEVEN, 200000000 );
    printf("Test: mul_sf64, round-mode NEAREST-AWAY\n");
    test_mul_sf64_roundmode( SF_NEARESTAWAY, 200000000 );
    }


void test_mul_sf32( void )
    {
    printf("Test: mul_sf32, round-mode UP\n");
    test_mul_sf32_roundmode( SF_UP, 200000000 );
    printf("Test: mul_sf32, round-mode DOWN\n");
    test_mul_sf32_roundmode( SF_DOWN, 200000000 );
    printf("Test: mul_sf32, round-mode TOZERO\n");
    test_mul_sf32_roundmode( SF_TOZERO, 200000000 );
    printf("Test: mul_sf32, round-mode NEAREST-EVEN\n");
    test_mul_sf32_roundmode( SF_NEARESTEVEN, 200000000 );
    printf("Test: mul_sf32, round-mode NEAREST-AWAY\n");
    test_mul_sf32_roundmode( SF_NEARESTAWAY, 200000000 );
    }


void test_add_sf64( void )
    {
    printf("Test: add_sf64, round-mode UP\n");
    test_add_sf64_roundmode( SF_UP, 200000000 );
    printf("Test: add_sf64, round-mode DOWN\n");
    test_add_sf64_roundmode( SF_DOWN, 200000000 );
    printf("Test: add_sf64, round-mode TOZERO\n");
    test_add_sf64_roundmode( SF_TOZERO, 200000000 );
    printf("Test: add_sf64, round-mode NEAREST-EVEN\n");
    test_add_sf64_roundmode( SF_NEARESTEVEN, 200000000 );
    printf("Test: add_sf64, round-mode NEAREST-AWAY\n");
    test_add_sf64_roundmode( SF_NEARESTAWAY, 200000000 );
    }


void test_add_sf32( void )
    {
    printf("Test: add_sf32, round-mode UP\n");
    test_add_sf32_roundmode( SF_UP, 200000000 );
    printf("Test: add_sf32, round-mode DOWN\n");
    test_add_sf32_roundmode( SF_DOWN, 200000000 );
    printf("Test: add_sf32, round-mode TOZERO\n");
    test_add_sf32_roundmode( SF_TOZERO, 200000000 );
    printf("Test: add_sf32, round-mode NEAREST-EVEN\n");
    test_add_sf32_roundmode( SF_NEARESTEVEN, 200000000 );
    printf("Test: add_sf32, round-mode NEAREST-AWAY\n");
    test_add_sf32_roundmode( SF_NEARESTAWAY, 200000000 );
    }



void test_widen_mul_sf16( void )
    {
    if32 p1, p2, res, expected;
    int i,j;
    for(i=0;i<65536;i++)
        for(j=0;j<65536;j++)
            {
            p1.u = _mali_sf16_to_sf32( i );
            p2.u = _mali_sf16_to_sf32( j );
            expected.f = p1.f * p2.f;
            res.u = _mali_widen_mul_sf16( i, j, 0 );
            if( res.u != expected.u && !(isnan(res.f) && isnan(expected.f)))
                {
                printf("WIDEN_MUL_SF16 error: input1=%g(%4.4" PRIX32 ") input2=%g(%4.4" PRIX32 ")\n  result=%g(%4.4" PRIX32 ") expected=%g(%4.4" PRIX32 ")\n",
                    p1.f, p1.u, p2.f, p2.u, res.f, res.u, expected.f, expected.u );
                }
            }
    }


// For FP16 multiply, we perform testing as follows:
// Widen both inputs to 32 bits, perform a 32-bit multiply.
// then narrow the result down to 16 bits.
// Since widen->multiply cannot introduce rounding errors, there will only
// be a single instance of rounding just as in the actual multiply;
// thus, the widen->multiply->narrow sequence is invariant to
// the direct multiply.

void test_mul_sf16_one_mode( roundmode rm )
    {
    if32 p1, p2, expected_x;
    uint16_t res, expected;
    int i,j;
    for(i=0;i<65536;i++)
        for(j=0;j<65536;j++)
            {
            p1.u = _mali_sf16_to_sf32( i );
            p2.u = _mali_sf16_to_sf32( j );
            expected_x.f = p1.f * p2.f; // roundoff-free.
            expected = _mali_sf32_to_sf16( expected_x.u, rm );
            res = _mali_mul_sf16( i, j, rm, 0 );
            if( res != expected && !(_mali_isnan_sf16(expected) && _mali_isnan_sf16(res)))
                {
                printf("MUL_SF16 error: input1=%g(%4.4" PRIX32 ") input2=%g(%4.4" PRIX32 ")\n  result=%4.4" PRIX32 " expected=%g(%4.4" PRIX32 ")\n",
                    p1.f, p1.u, p2.f, p2.u, res, expected_x.f, expected_x.u );
                }
            
            }
    }


// For FP16 additions, we perform testing as follows:
// Widen both inputs to 64 bits, perform a 64-bit multiply.
// then narrow the result down to 16 bits.
// Since widen->add cannot introduce rounding errors, there will only
// be a single instance of rounding just as in the actual add;
// thus, the widen->add->narrow sequence is invariant to
// the direct multiply.

void test_add_sf16_one_mode( roundmode rm )
    {
    if64 p1, p2, expected_x;
    uint16_t res, expected;
    int i,j;
    for(i=0;i<65536;i++)
        for(j=0;j<65536;j++)
            {
            p1.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32( i ) );
            p2.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32( j ) );
            expected_x.f = p1.f + p2.f; // roundoff-free.
            expected = _mali_sf64_to_sf16( expected_x.u, rm );
            res = _mali_add_sf16( i, j, rm, 0 );
            if( res != expected && !(_mali_isnan_sf16(expected) && _mali_isnan_sf16(res)))
                {
                printf("ADD_SF16 error: input1=%g(%4.4" PRIX64 ") input2=%g(%4.4" PRIX64 ")\n  result=%4.4" PRIX32 " expected=%g(%4.4" PRIX64 ")\n",
                    p1.f, p1.u, p2.f, p2.u, res, expected_x.f, expected_x.u );
                }
            
            }
    }



void test_mul_sf16( void )
    {
    printf("Test: mul_sf16, round-mode UP\n");
    test_mul_sf16_one_mode( SF_UP );
    printf("Test: mul_sf16, round-mode DOWN\n");
    test_mul_sf16_one_mode( SF_DOWN );
    printf("Test: mul_sf16, round-mode TOZERO\n");
    test_mul_sf16_one_mode( SF_TOZERO );
    printf("Test: mul_sf16, round-mode NEARESTEVEN\n");
    test_mul_sf16_one_mode( SF_NEARESTEVEN );
    printf("Test: mul_sf16, round-mode NEARESTAWAY\n");
    test_mul_sf16_one_mode( SF_NEARESTAWAY );
    }


void test_add_sf16( void )
    {
    printf("Test: add_sf16, round-mode UP\n");
    test_add_sf16_one_mode( SF_UP );
    printf("Test: add_sf16, round-mode DOWN\n");
    test_add_sf16_one_mode( SF_DOWN );
    printf("Test: add_sf16, round-mode TOZERO\n");
    test_add_sf16_one_mode( SF_TOZERO );
    printf("Test: add_sf16, round-mode NEARESTEVEN\n");
    test_add_sf16_one_mode( SF_NEARESTEVEN );
    printf("Test: add_sf16, round-mode NEARESTAWAY\n");
    test_add_sf16_one_mode( SF_NEARESTAWAY );
    }




void test_fma_sf16_one_mode( roundmode rm, int count )
    {
#ifdef HAVE_ROUNDING_MACROS
    int i;
    uint64_t lfsr = 1;
    uint16_t a,b,c,res, expected;
    if64 a_ext, b_ext, c_ext, res_ext;
    
    int old_roundmode = fegetround();
    switch( rm )
        {
        case SF_UP: fesetround( FE_UPWARD ); break;
        case SF_DOWN: fesetround( FE_DOWNWARD ); break;
        case SF_TOZERO: fesetround( FE_TOWARDZERO ); break;
        case SF_NEARESTEVEN: fesetround( FE_TONEAREST ); break;
        case SF_NEARESTAWAY: fesetround( FE_TONEAREST ); break; // warning: may be off-by-1.       
        };
    
    for(i=0;i<count;i++)
        {
        a = lfsr & 0xFFFF;
        b = (lfsr >> 16) & 0xFFFF;
        c = (lfsr >> 32) & 0xFFFF;
        a_ext.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32( a ));
        b_ext.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32( b ));
        c_ext.u = _mali_sf32_to_sf64( _mali_sf16_to_sf32( c ));
        res = _mali_fma_sf16( a,b,c, rm, 0 );
        res_ext.f = a_ext.f * b_ext.f + c_ext.f;
        expected = _mali_sf64_to_sf16( res_ext.u, rm );
        if( res != expected  && !(_mali_isnan_sf16(res) && _mali_isnan_sf16(expected)))
            {
            if32 resf;
            resf.u = _mali_sf16_to_sf32( res );
            printf("FMA_SF16 error: inputs a=%4.4" PRIX32 "(%g) b=%4.4" PRIX32 "(%g) c=%4.4" PRIX32 "(%g) result %4.4" PRIX32 "(%g)  expected %4.4" PRIX32 "(%g)\n",
                a, a_ext.f,
                b, b_ext.f,
                c, c_ext.f,
                res, resf.f,
                expected, res_ext.f );
            }
        
        lfsr = lfsr64( lfsr );
        }
    
    fesetround( old_roundmode );    
#else
    printf("fma_sf16 (roundmode=%s): not run\n", round_names[rm] );
#endif
    }




void test_fma_sf32_one_mode( roundmode rm, int count )
    {
#ifdef HAVE_ROUNDING_MACROS
    int i;

    l128 lfsr;
    lfsr.v32[0] = 1;
    lfsr.v32[1] = 0;
    lfsr.v32[2] = 0;
    lfsr.v32[3] = 0;
    
    if32 a,b,c,res;
    if32 expected;
    
    int old_roundmode = fegetround();
    switch( rm )
        {
        case SF_UP: fesetround( FE_UPWARD ); break;
        case SF_DOWN: fesetround( FE_DOWNWARD ); break;
        case SF_TOZERO: fesetround( FE_TOWARDZERO ); break;
        case SF_NEARESTEVEN: fesetround( FE_TONEAREST ); break;
        case SF_NEARESTAWAY: fesetround( FE_TONEAREST ); break; // warning: may be off-by-1.       
        };
    
    for(i=0;i<count;i++)
        {
        a.u = lfsr.v32[0];
        b.u = lfsr.v32[1];
        c.u = lfsr.v32[2];
        res.u = _mali_fma_sf32( a.u, b.u, c.u, rm, 0 );
        // I'd like to use fmaf() here, but the implementation in glibc is broken.
        expected.f = (double)a.f * b.f + c.f;
        if( res.u != expected.u && (res.u < expected.u - 1 || res.u > expected.u + 1)  
            && !(_mali_isnan_sf32(res.u) && _mali_isnan_sf32(expected.u)) )
            {
            printf("FMA_SF32 error: inputs a=%8.8" PRIX32 "(%g) b=%8.8" PRIX32 "(%g) c=%8.8" PRIX32 "(%g) result %8.8" PRIX32 "(%g)  expected %8.8" PRIX32 "(%g)\n",
                a.u, a.f,
                b.u, b.f,
                c.u, c.f,
                res.u, res.f,
                expected.u, expected.f );
            }
        
        lfsr128( &lfsr );
        }
    
    fesetround( old_roundmode );    
#else
    printf("fma_sf16 (roundmode=%s): not run\n", round_names[rm] );
#endif
    }




void test_fma_sf64_one_mode( roundmode rm, int count )
    {
#ifdef HAVE_ROUNDING_MACROS
    int i;

    l192 lfsr;
    lfsr.v64[0] = 1;
    lfsr.v64[1] = 0;
    lfsr.v64[2] = 0;
    
    if64 a,b,c,res;
    if64 lowbound, highbound;
    
    for(i=0;i<count;i++)
        {
        a.u = lfsr.v64[0];
        b.u = lfsr.v64[1];
        c.u = lfsr.v64[2];
        res.u = _mali_fma_sf64( a.u, b.u, c.u, rm, 0 );
        lowbound.u = _mali_add_sf64( _mali_mul_sf64( a.u, b.u, SF_DOWN, 0 ), c.u, SF_DOWN, 0 );
        highbound.u = _mali_add_sf64( _mali_mul_sf64( a.u, b.u, SF_UP, 0 ), c.u, SF_UP, 0 );
        if( ( res.f < lowbound.f || res.f > highbound.f )
            && !(_mali_isnan_sf64(res.u) && (_mali_isnan_sf64(lowbound.u) || _mali_isnan_sf64(highbound.u) )))
            {
            printf("FMA_SF64 error: inputs a=%16.16" PRIX64 "(%g) b=%16.16" PRIX64 "(%g) c=%16.16" PRIX64 "(%g) result %16.16" PRIX64 "(%g)  low %16.16" PRIX64 "(%g)  high %16.16" PRIX64 "(%g)\n",
                a.u, a.f,
                b.u, b.f,
                c.u, c.f,
                res.u, res.f,
                lowbound.u, lowbound.f,
                highbound.u, highbound.f );
            }
        
        lfsr192( &lfsr );
        }
#else
    printf("fma_sf16 (roundmode=%s): not run\n", round_names[rm] );
#endif
    }



void test_fma_sf16( void )
    {
    printf("Test: fma_sf16, round-mode UP\n");
    test_fma_sf16_one_mode( SF_UP, 200000000 );
    printf("Test: fma_sf16, round-mode DOWN\n");
    test_fma_sf16_one_mode( SF_DOWN, 200000000 );
    printf("Test: fma_sf16, round-mode TOZERO\n");
    test_fma_sf16_one_mode( SF_TOZERO, 200000000 );
    printf("Test: fma_sf16, round-mode NEARESTEVEN\n");
    test_fma_sf16_one_mode( SF_NEARESTEVEN, 200000000 );
    printf("Test: fma_sf16, round-mode NEARESTAWAY\n");
    test_fma_sf16_one_mode( SF_NEARESTAWAY, 200000000 );    
    }



void test_fma_sf32( void )
    {
    printf("Test: fma_sf32, round-mode UP\n");
    test_fma_sf32_one_mode( SF_UP, 200000000 );
    printf("Test: fma_sf32, round-mode DOWN\n");
    test_fma_sf32_one_mode( SF_DOWN, 200000000 );
    printf("Test: fma_sf32, round-mode TOZERO\n");
    test_fma_sf32_one_mode( SF_TOZERO, 200000000 );
    printf("Test: fma_sf32, round-mode NEARESTEVEN\n");
    test_fma_sf32_one_mode( SF_NEARESTEVEN, 200000000 );
    printf("Test: fma_sf32, round-mode NEARESTAWAY\n");
    test_fma_sf32_one_mode( SF_NEARESTAWAY, 200000000 );    
    }


void test_fma_sf64( void )
    {
    printf("Test: fma_sf64, round-mode UP\n");
    test_fma_sf64_one_mode( SF_UP, 200000000 );
    printf("Test: fma_sf64, round-mode DOWN\n");
    test_fma_sf64_one_mode( SF_DOWN, 200000000 );
    printf("Test: fma_sf64, round-mode TOZERO\n");
    test_fma_sf64_one_mode( SF_TOZERO, 200000000 );
    printf("Test: fma_sf64, round-mode NEARESTEVEN\n");
    test_fma_sf64_one_mode( SF_NEARESTEVEN, 200000000 );
    printf("Test: fma_sf64, round-mode NEARESTAWAY\n");
    test_fma_sf64_one_mode( SF_NEARESTAWAY, 200000000 );    
    }




/*
test of less-than, less-than-or-equal and equal
*/

void test_compares_sf16_32_64( void )
    {
    if32 ix, jx;
    if64 ixx, jxx;
    int i,j;
    printf("Tests: (equal|lt|le)_sf(16|32|64)\n");
    for(i=0;i<=0xFFFF;i++)
        for(j=0;j<=0xFFFF;j++)
            {
            ix.u = _mali_sf16_to_sf32( i );
            jx.u = _mali_sf16_to_sf32( j );
            ixx.u = _mali_sf32_to_sf64( ix.u );
            jxx.u = _mali_sf32_to_sf64( jx.u );
            if( _mali_equal_sf16(i,j) != (ix.f == jx.f) )
                printf("SF16_EQUAL error: %4.4" PRIX32 " %4.4" PRIX32 "\n",
                    i, j );
            if( _mali_lt_sf16(i,j) != (ix.f < jx.f) )
                printf("SF16_LESSTHAN error: %4.4" PRIX32 " %4.4" PRIX32 "\n",
                    i, j );
            if( _mali_le_sf16(i,j) != (ix.f <= jx.f) )
                printf("SF16_LESSEQUAL error: %4.4" PRIX32 " %4.4" PRIX32 "\n",
                    i, j );         

            if( _mali_equal_sf32(ix.u, jx.u) != (ix.f == jx.f) )
                printf("SF32_EQUAL error: %8.8" PRIX32 " %8.8" PRIX32 "\n",
                    ix.u, jx.u );
            if( _mali_lt_sf32(ix.u, jx.u) != (ix.f < jx.f) )
                printf("SF32_LESSTHAN error: %8.8" PRIX32 " %8.8" PRIX32 "\n",
                    ix.u, jx.u );
            if( _mali_le_sf32(ix.u, jx.u) != (ix.f <= jx.f) )
                printf("SF32_LESSEQUAL error: %8.8" PRIX32 " %8.8" PRIX32 "\n",
                    ix.u, jx.u );         

            if( _mali_equal_sf64(ixx.u, jxx.u) != (ixx.f == jxx.f) )
                printf("SF64_EQUAL error: %16.16" PRIX64 " %16.16" PRIX64 "\n",
                    ixx.u, jxx.u );
            if( _mali_lt_sf64(ixx.u, jxx.u) != (ixx.f < jxx.f) )
                printf("SF64_LESSTHAN error: %16.16" PRIX64 " %16.16" PRIX64 "\n",
                    ixx.u, jxx.u );
            if( _mali_le_sf64(ixx.u, jxx.u) != (ixx.f <= jxx.f) )
                printf("SF64_LESSEQUAL error: %16.16" PRIX64 " %16.16" PRIX64 "\n",
                    ixx.u, jxx.u );         
            }
    }

/*
test of minmax
*/

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif


void test_minmax_sf16_32_64( void )
    {
    if32 ix, jx, rx;
    if64 ixx, jxx, rxx;
    int i,j;
    printf("Tests: (min|max)(n)?_(16|32|64)\n");

    for(i=0;i<=65535;i++)
        for(j=0;j<=65535;j++)
            {
            if( _mali_isnan_sf16(i) || _mali_isnan_sf16(j)) continue;
            if( ((i | j) & 0x7FFF) == 0) continue;
            ix.u = _mali_sf16_to_sf32( i );
            jx.u = _mali_sf16_to_sf32( j );
            ixx.u = _mali_sf32_to_sf64( ix.u );
            jxx.u = _mali_sf32_to_sf64( jx.u );
            
            rx.f = MIN( ix.f, jx.f );
            rxx.f = MIN( ixx.f, jxx.f );
            
            if( _mali_sf16_to_sf32( _mali_min_nan_propagate_sf16(i,j) ) != rx.u )
                printf("SF16_MIN_nan_propagate error: %4.4" PRIX32 " %4.4" PRIX32 "\n", i, j );
            if( _mali_sf16_to_sf32( _mali_min_sf16(i,j) ) != rx.u )
                printf("SF16_MIN error: %4.4" PRIX32 " %4.4" PRIX32 "\n", i, j );

            if( _mali_min_nan_propagate_sf32(ix.u,jx.u) != rx.u)
                printf("SF32_MIN_nan_propagate error: %8.8" PRIX32 " %8.8" PRIX32 "\n", ix.u, jx.u );
            if( _mali_min_sf32(ix.u,jx.u) != rx.u )
                printf("SF32_MIN error: %8.8" PRIX32 " %8.8" PRIX32 "\n", ix.u, jx.u );

            if( _mali_min_nan_propagate_sf64(ixx.u,jxx.u) != rxx.u)
                printf("SF64_MIN_nan_propagate error: %16.16" PRIX64 " %16.16" PRIX64 "\n", ixx.u, jxx.u );
            if( _mali_min_sf64(ixx.u,jxx.u) != rxx.u )
                printf("SF64_MIN error: %16.16" PRIX64 " %16.16" PRIX64 "\n", ixx.u, jxx.u );

            rx.f = MAX( ix.f, jx.f );
            rxx.f = MAX( ixx.f, jxx.f );
            
            if( _mali_sf16_to_sf32( _mali_max_nan_propagate_sf16(i,j) ) != rx.u )
                printf("SF16_MAX_nan_propagate error: %4.4" PRIX32 " %4.4" PRIX32 "\n", i, j );
            if( _mali_sf16_to_sf32( _mali_max_sf16(i,j) ) != rx.u )
                printf("SF16_MAX error: %4.4" PRIX32 " %4.4" PRIX32 "\n", i, j );
            
            if( _mali_max_nan_propagate_sf32(ix.u,jx.u) != rx.u)
                printf("SF32_MAX_nan_propagate error: %8.8" PRIX32 " %8.8" PRIX32 "\n", ix.u, jx.u );
            if( _mali_max_sf32(ix.u,jx.u) != rx.u )
                printf("SF32_MAX error: %8.8" PRIX32 " %8.8" PRIX32 "\n", ix.u, jx.u );

            if( _mali_max_nan_propagate_sf64(ixx.u,jxx.u) != rxx.u)
                printf("SF64_MAX_nan_propagate error: %16.16" PRIX64 " %16.16" PRIX64 "\n", ixx.u, jxx.u );
            if( _mali_max_sf64(ixx.u,jxx.u) != rxx.u )
                printf("SF64_MAX error: %16.16" PRIX64 " %16.16" PRIX64 "\n", ixx.u, jxx.u );            
            }
    
    }





void test_add16_invar( roundmode rm )
    {
    int i,j;
    for(i=0;i<65536;i++)
        {
        if( _mali_isnan_sf16( i ) )
            continue;
        for(j=0;j<65536;j++)
            {
            if( _mali_isnan_sf16( j ) )
                continue;
            {
				sf16 res = _mali_add_sf16( i, j, rm, 0 );
				sf16 res2 = _mali_sf32_to_sf16( 
					_mali_add_sf32 (
						_mali_sf16_to_sf32( i ),
						_mali_sf16_to_sf32( j ),
						rm,
						0 ),
					rm );
				if( res != res2 )
					printf("%4.4X %4.4X -> %4.4X  %4.4X\n", i, j, res, res2 );
				}
            }
        }
    }




/*
The platform_sanity_test() function performs a series
of basic tests in order to detect some commonly found 
IEEE-754 noncompliance issues.
Many of the tests for the softfloat library use
system floating-point functions for a comparison basis;
if these functions are not compliant, the tests are unusable.

The most common problem is the extended-precision operation
of x87; if running on a 32-bit x86 platform, the compiler should
be redirected to use SSE for all floating-point math.
(under GCC, this requires adding "-mfpmath=sse -msse2" to the
compiler commandline).

*/

void platform_sanity_test( void )
    {
    
    // test 1: double rounding involving FP80 format.
    // This catches x87 idiocy.
    {
		volatile double d1 = 36893488147419103232.0; // 2^65
		volatile double d2 = 4095.0; // 2^12 - 2^0
		volatile double dsum = d1 + d2;
		if( dsum != d1 )
			{
			printf(
				"Sanity test failure: FP80 double-rounding present.\n"
				);
			exit(1);
			}
	}
	    
    // test 2: test for excessive intermediate precision, FP32.
    {
		volatile float s1 = 16777216.0f;
		volatile float s2 = 1.0f;
		float p = s1;
		p += s2;
		p += s2;
		{
			volatile float res = p;
			if( res != s1 )
				{
				printf("Sanity test failure: FP32 calculation done in >FP32 precision\n");
				exit(1);
				}
		}
	}
	    
    // test 3: FP64 overflow handling
    {
		volatile double d4 = 1e300;
		volatile double d5 = 1e-300;
		double prod = d4 * d4;
		prod *= d5;
		if( prod < 1e301 )
			{
			printf("Sanity test failure: FP64 overflow failure; intermediate calculation with too much range\n");
			exit(1);
			}
	}
	        
    // test 4: FP32: suspicious use of FMAC 
    {
		volatile float s3 = 18446744073709551616.0f; // 2^64
		volatile float s4 = -340282346638528859811704183484516925440.0f; // 2^104 - 2^128
		float s3a = s3;
		float s4a = s4;
		float fprod = s3a * s3a + s4a;
		if( fprod == 20282409603651670423947251286016.0f /* 2^104 */ )
			{
			printf("Sanity test failure: FP32: Multiply-Add carries excessive range");
			exit(1);
			}
	}
	    
    // test 5: FP32: suspicious use of FMAC
	{
		volatile double d6 = 1.34078079299425970995740249982e154; // 2^512
		volatile double d7 = -1.79769313486231550856124328384e308; // 2^971 - 2^1024
		double d6a = d6;
		double d7a = d7;
		double dprod = d6a * d6a + d7a;
		if( dprod < 1e301 )
			{
			printf("Sanity test failure: FP64: Multiply-Add carries excessive range\n");
			exit(1);
			}
	}
	    
    // test 6: FP32 denormal support.
    {
		volatile float s5 = (1.0f / 1125899906842624.0f) ; // 2^-50
		volatile float s6 = 1125899906842624.0f; // 2^50
		volatile float s7;
		s7 = 2.0f;
		s7 *= s5;
		s7 *= s5;
		s7 *= s5; // at this point, we reach 2^-149, the smallest representable denormal
		s7 *= s6;
		s7 *= s6;
		s7 *= s6;
		if( s7 != 2.0f )
			{
			printf("Sanity test failure: FP32: denormal support missing\n");
			exit(1);
			}
	}
	
    // test 7: look for divisions that have been replaced with reciprocals.
	{
		volatile float s8 = 1e-40f; // denormal; its reciprocal overflows.
		volatile float s9 = 1e-20f;
		volatile float s10 = 1e-21f;
		volatile float s11 = 1e-22f;
	    
		float s8a = s8;
		float s9a = s9;
		float s10a = s10;
		float s11a = s11;
		s9a = s9a / s8a; // doing three divisions to tease out CSE.
		s10a = s10a / s8a;
		s11a = s11a / s8a;
		if( s9a > 1e22 || s10a > 1e22 || s11a > 1e22 )
			{
			printf("Sanity test failure: FP32: reciprocal used in place of real division\n");
			exit(1);
			}
	}    
    
    // test 8: test that NaN works properly.
    {
		volatile float s12 = 1e35f;
		volatile float s13 = 0.0f;
		volatile float s14 = s12;
		s14 *= s12; // overflow to INF
		s14 *= s13; // Inf * 0 => NAN
		if( s14 == 0.0 || s14 == s14 || !isnan(s14) )
			{
			printf("Sanity test failure: FP32: NaN not supported\n");
			exit(1);
			}
	}
		    
}



int main(void)
    {
    platform_sanity_test();

    /*
    printf("---------\n");
    test_add16_invar( SF_NEARESTEVEN );
    printf("---------\n");
    test_add16_invar( SF_UP );
    printf("---------\n");
    test_add16_invar( SF_DOWN );
    printf("---------\n");
    test_add16_invar( SF_TOZERO );
    printf("---------\n");
    test_add16_invar( SF_NEARESTAWAY );
    return 0;
    */
   
    printf("Test: sf16_to_sf32()\n");
    test_sf16_to_sf32();

    printf("Test: sf32_to_sf64()\n");
    test_sf32_to_sf64();

    printf("Test: sf32_to_sf16()\n");
    test_sf32_to_sf16();

    printf("Test: sf64_to_sf32()\n");
    test_sf64_to_sf32();

    printf("Test: round_sf16()\n");
    test_round_sf16();

    printf("Test: round_sf32()\n");
    test_round_sf32();

    printf("Test: round_sf64()\n");
    test_round_sf64();

    test_sf32_to_s32();
    test_sf16_to_s16();
    test_sf64_to_s64();
    test_sf32_to_u32();
    test_sf16_to_u16();
    test_sf64_to_u64();
    test_s32_to_sf32();
    test_s16_to_sf16();
    test_u16_to_sf16();
    test_u32_to_sf32();
    test_u64_to_sf64();
    test_s64_to_sf64();
    test_mul_sf32();
    test_mul_sf64();
    test_mul_sf16();
    test_add_sf16();
    test_add_sf32();
    test_add_sf64();
    test_sf64_to_sf16();
    test_fma_sf16();
    test_fma_sf32();
    test_fma_sf64();

    test_compares_sf16_32_64();
    test_minmax_sf16_32_64();
    


    printf("Testing done\n");
    return 0;
    }

