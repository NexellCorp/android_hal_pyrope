/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2010-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

/* ============================================================================
	Includes
============================================================================ */
#define MALI_TPI_API MALI_TPI_EXPORT
#include "tpi/mali_tpi.h"

#include <assert.h>

/* ============================================================================
	Private Functions
============================================================================ */
/**
 * @brief Generate a new state block full of random untempered numbers.
 *
 * @param state    The state structure containing the state to generate.
 */
static void _mali_tpi_rand_generate_block( mali_tpi_rand_state* state )
{
	unsigned int index;
	static const mali_tpi_uint32 magic[] = {0x0, 0x9908b0df};

	for( index = 0; index < MALI_TPI_RNG_STATE_LEN; index++ )
	{
		mali_tpi_uint32 value;
		unsigned int index_p1m;
		unsigned int index_p397m;

		index_p1m   = (index + 1)   % MALI_TPI_RNG_STATE_LEN;
		index_p397m = (index + 397) % MALI_TPI_RNG_STATE_LEN;

		value = (state->state[index]     & 0x80000000) |
		        (state->state[index_p1m] & 0x7fffffff);

		state->state[index] =
		    state->state[index_p397m] ^ (value >> 1) ^ magic[value & 0x1];
	}
}

/* ------------------------------------------------------------------------- */
/**
 * @brief Yield a new 4-byte tempered random word.
 *
 * This function will call the internal _mali_tpi_rand_generate_block function
 * if we have run out of random state in the current permute cycle.
 *
 * @param state    The state structure containing the state to yield from.
 */
static mali_tpi_uint32 _mali_tpi_rand_yield_32bit( mali_tpi_rand_state* state )
{
	mali_tpi_uint32 value;

	if( MALI_TPI_RNG_STATE_LEN == state->index )
	{
		_mali_tpi_rand_generate_block( state );
		state->index = 0;
	}

	/* Get raw state value. */
	value = state->state[state->index];
	state->index++;
	state->generated++;

	/* Temper it. */
	value ^= (value >> 11);
	value ^= (value <<  7) & 0x9d2c5680;
	value ^= (value << 15) & 0xefc60000;
	value ^= (value >> 18);

	return value;
}

/* ------------------------------------------------------------------------- */
/**
 * @brief Yield a new 4-byte tempered random word.
 *
 * This function will call the internal _mali_tpi_rand_generate_block function
 * if we have run out of random state in the current permute cycle.
 *
 * @param state    The state structure containing the state to yield from.
 */
static mali_tpi_uint64 _mali_tpi_rand_yield_64bit( mali_tpi_rand_state* state )
{
	mali_tpi_uint64 value;
	value  = ((mali_tpi_uint64)_mali_tpi_rand_yield_32bit( state )) << 32;
	value |= ((mali_tpi_uint64)_mali_tpi_rand_yield_32bit( state ));
	return value;
}

/* ============================================================================
	Public Functions
============================================================================ */
MALI_TPI_IMPL void mali_tpi_rand_init(
	mali_tpi_rand_state *state,
	mali_tpi_uint32 seed,
	mali_tpi_uint32 generated )
{
	mali_tpi_uint64 index;

	state->index = 0;
	state->seed = seed;
	state->generated = 0;

	state->state[0] = seed;
	for( index = 1; index < MALI_TPI_RNG_STATE_LEN; index++ )
	{
		mali_tpi_uint32 value = state->state[index-1];
		state->state[index] = (0x6C078965 * (value ^ (value >> 30)) + index);
	}

	/* Skip forwards to the given generated number. */
	for( index = 0; index < generated; index++ )
	{
		_mali_tpi_rand_yield_32bit( state );
	}
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_rand_gen_buffer(
	mali_tpi_rand_state *state,
	void* buf,
	size_t len )
{
	size_t index;
	mali_tpi_uint32 value = 0;
	mali_tpi_uint8* dest = (mali_tpi_uint8*) buf;

	for( index = 0; index < len; index++ )
	{
		if( 0 == (index & 0x3) )
		{
			value = _mali_tpi_rand_yield_32bit( state );
		}
		dest[index] = (mali_tpi_uint8)(value & 0xFF);
		value = value >> 8;
	}
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_uint8 mali_tpi_rand_gen_uint8(
	mali_tpi_rand_state *state,
	mali_tpi_uint8 min,
	mali_tpi_uint8 max )
{
	mali_tpi_uint32 range, value;
	assert( max >= min );

	range = max - min;
	value = _mali_tpi_rand_yield_32bit( state );
	value = min + ( value % ( range + 1 ) );

	return (mali_tpi_uint8)(value & 0xFF);
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_uint16 mali_tpi_rand_gen_uint16(
	mali_tpi_rand_state *state,
	mali_tpi_uint16 min,
	mali_tpi_uint16 max )
{
	mali_tpi_uint32 range, value;
	assert( max >= min );

	range = max - min;
	value = _mali_tpi_rand_yield_32bit( state );
	value = min + ( value % ( range + 1 ) );

	return (mali_tpi_uint16)(value & 0xFFFF);
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_uint32 mali_tpi_rand_gen_uint32(
	mali_tpi_rand_state *state,
	mali_tpi_uint32 min,
	mali_tpi_uint32 max )
{
	mali_tpi_uint32 range, value;
	assert( max >= min );

	range = max - min;
	value = _mali_tpi_rand_yield_32bit( state );
	if( 0xFFFFFFFF != range )
	{
		value = min + ( value % ( range + 1 ) );
	}

	return value;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_uint64 mali_tpi_rand_gen_uint64(
	mali_tpi_rand_state *state,
	mali_tpi_uint64 min,
	mali_tpi_uint64 max )
{
	mali_tpi_uint64 range, value;
	assert( max >= min );

	range = max - min;
	value = _mali_tpi_rand_yield_64bit( state );
	if( ((mali_tpi_uint64)-1) != range )
	{
		value = min + ( value % ( range + 1 ) );
	}

	return value;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_int8 mali_tpi_rand_gen_int8(
	mali_tpi_rand_state *state,
	mali_tpi_int8 min,
	mali_tpi_int8 max )
{
	mali_tpi_uint32 range, value;
	assert( max >= min );

	range = max - min;
	value = _mali_tpi_rand_yield_32bit( state );
	value = value % (range + 1);
	value = min + value;

	return (mali_tpi_int8)(value & 0xFF);
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_int16 mali_tpi_rand_gen_int16(
	mali_tpi_rand_state *state,
	mali_tpi_int16 min,
	mali_tpi_int16 max )
{
	mali_tpi_uint32 range, value;
	assert( max >= min );

	range = max - min;
	value = _mali_tpi_rand_yield_32bit( state );
	value = value % (range + 1);
	value = min + value;

	return (mali_tpi_int16)(value & 0xFFFF);
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_int32 mali_tpi_rand_gen_int32(
	mali_tpi_rand_state *state,
	mali_tpi_int32 min,
	mali_tpi_int32 max )
{
	mali_tpi_uint32 range, value;
	assert( max >= min );

	range = max - min;
	value = _mali_tpi_rand_yield_32bit( state );

	if( 0xFFFFFFFF != range )
	{
		value = value % (range + 1);
	}

	value = min + value;

	return (mali_tpi_int32)value;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_int64 mali_tpi_rand_gen_int64(
	mali_tpi_rand_state *state,
	mali_tpi_int64 min,
	mali_tpi_int64 max )
{
	mali_tpi_uint64 range, value;
	assert( max >= min );

	range = max - min;
	value = _mali_tpi_rand_yield_64bit( state );
	if( ((mali_tpi_uint64)-1) != range )
	{
		value = value % ( range + 1 );
	}

	value = min + value;

	return (mali_tpi_int64)value;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL float mali_tpi_rand_gen_ufloat(
	mali_tpi_rand_state *state )
{
	mali_tpi_uint32 value = _mali_tpi_rand_yield_32bit( state );
	return (float)((double)value / (double)0xFFFFFFFF);
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL float mali_tpi_rand_gen_sfloat(
	mali_tpi_rand_state *state )
{
	mali_tpi_uint32 value = _mali_tpi_rand_yield_32bit( state );
	return (float)(((double)value / (double)0x7FFFFFFF) - 1.0);
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL double mali_tpi_rand_gen_udouble(
	mali_tpi_rand_state *state )
{
	mali_tpi_uint64 value = _mali_tpi_rand_yield_64bit( state );
	return ((double)value / (double)((mali_tpi_uint64)-1));
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL double mali_tpi_rand_gen_sdouble(
	mali_tpi_rand_state *state )
{
	mali_tpi_uint64 value = _mali_tpi_rand_yield_64bit( state );
	return (((double)value / (double)(((mali_tpi_uint64)-1)/2)) - 1.0 );
}
