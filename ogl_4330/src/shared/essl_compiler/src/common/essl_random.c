/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_random.h"


#ifdef RANDOMIZED_COMPILATION


static int s_seed = 42;

/* generator swiped from src/openvg/tessellator/lcg_random.c */
static int lcg_rand( void )
{
	/* This algorithm is mentioned in the ISO C standard.
	 * The only adaptation done is extending it to 32 bits. */
	unsigned int next = s_seed;
	int result;

	next *= 1103515245;
	next += 12345;
	result = (unsigned int)(next / 65536) % 2048;

	next *= 1103515245;
	next += 12345;
	result <<= 11;
	result ^= (unsigned int)(next / 65536) % 1024;

	next *= 1103515245;
	next += 12345;
	result <<= 10;
	result ^= (unsigned int)(next / 65536) % 1024;
	
	s_seed = next;

	return result;
}


void _essl_set_random_seed(int seed)
{
	if ( seed <= 0 ) seed = 1;
	s_seed = seed;
}

/** return an integer in the range [lower_bound, upper_bound) */
int _essl_get_random_int(int lower_bound, int upper_bound)
{
	double range = (double)upper_bound - (double)lower_bound;
	int divisor = 4294967296.0/range;
	int offset = lower_bound - -range*0.5;
	return lcg_rand()/divisor + offset;
}


#endif
