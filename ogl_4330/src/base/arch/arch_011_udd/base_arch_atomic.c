/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "mali_osu.h"
#include <stddef.h> /* offsetof */

/**
 * Atomic integer increment
 * @param atomic Pointer to mali_atomic_int to atomically increment
 */
void _mali_base_arch_sys_atomic_inc(mali_atomic_int * atomic_in)
{
	_mali_osu_atomic_t * atomic = MALI_REINTERPRET_CAST(_mali_osu_atomic_t *)atomic_in;
	_mali_osu_atomic_inc_and_return( MALI_REINTERPRET_CAST(_mali_osu_atomic_t*)atomic );
}

/**
 * Atomic integer increment and return post-increment value
 * @param atomic Pointer to mali_atomic_int to atomically increment
 * @return Value of val after increment
 */
u32 _mali_base_arch_sys_atomic_inc_and_return(mali_atomic_int * atomic_in)
{
	_mali_osu_atomic_t * atomic = MALI_REINTERPRET_CAST(_mali_osu_atomic_t *)atomic_in;
	return _mali_osu_atomic_inc_and_return( MALI_REINTERPRET_CAST(_mali_osu_atomic_t*)atomic );
}

/**
 * Atomic integer decrement
 * @param atomic Pointer to mali_atomic_int to atomically decrement
 */
void _mali_base_arch_sys_atomic_dec(mali_atomic_int * atomic_in)
{
	_mali_osu_atomic_t * atomic = MALI_REINTERPRET_CAST(_mali_osu_atomic_t *)atomic_in;
	_mali_osu_atomic_dec_and_return( MALI_REINTERPRET_CAST(_mali_osu_atomic_t*)atomic );
}

/**
 * Atomic integer decrement and return post-decrement value
 * @param atomic Pointer to mali_atomic_int to atomically decrement
 * @return Value of val after decrement
 */
u32 _mali_base_arch_sys_atomic_dec_and_return(mali_atomic_int * atomic_in)
{
	_mali_osu_atomic_t * atomic = MALI_REINTERPRET_CAST(_mali_osu_atomic_t *)atomic_in;
	return _mali_osu_atomic_dec_and_return( MALI_REINTERPRET_CAST(_mali_osu_atomic_t*)atomic );
}

/**
 * Atomic integer get. Read value stored in the atomic integer.
 * @param atomic Pointer to mali_atomic_int to read contents of
 * @return Value currently in mali_atomic_int
 * @note If an increment/decrement/set is currently in progress you'll get either
 * the pre or post value.
 */
u32 _mali_base_arch_sys_atomic_get(mali_atomic_int * atomic_in)
{
	_mali_osu_atomic_t * atomic = MALI_REINTERPRET_CAST(_mali_osu_atomic_t *)atomic_in;
	return _mali_osu_atomic_read( MALI_REINTERPRET_CAST(_mali_osu_atomic_t*)atomic );
}

/**
 * Atomic integer set. Used to explicitly set the contents of an atomic
 * integer to a specific value.
 * @param atomic Pointer to mali_atomic_int to set
 * @param value Value to write to the atomic int
 * @note If increment or decrement is currently in progress the mali_atomic_int will contain either the value set, or the value after increment/decrement.
 * @note If another set is currently in progress it's uncertain which will
 * persist when the function returns.
 */
void _mali_base_arch_sys_atomic_set(mali_atomic_int * atomic_in, u32 value)
{
	_mali_osu_atomic_t * atomic = MALI_REINTERPRET_CAST(_mali_osu_atomic_t *)atomic_in;
	_mali_osu_atomic_write( MALI_REINTERPRET_CAST(_mali_osu_atomic_t*)atomic, value );
}

/**
 * Initialize an atomic integer. Used to initialize an atomic integer to
 * a sane value when created.
 * @param atomic Pointer to mali_atomic_int to set
 * @param value Value to initialize the atomic integer to
 * @note This function has to be called when a mali_atomic_int is
 * created/before first use
 */

void _mali_base_arch_sys_atomic_init(mali_atomic_int * atomic_in, u32 value)
{
	_mali_osu_errcode_t err;
	_mali_osu_atomic_t * atomic = MALI_REINTERPRET_CAST(_mali_osu_atomic_t *)atomic_in;

	/* Assert that our assembly routine is using the correct offset for u.val. This must
	 * be maintained with the .S file. */
	MALI_DEBUG_ASSERT( 0 == offsetof( _mali_osu_atomic_t, u.val ),
					   ("mali_osu_atomics.S will require recoding to use different structure offset"));

	err = _mali_osu_atomic_init( MALI_REINTERPRET_CAST(_mali_osu_atomic_t*)atomic, value );

#ifdef __SYMBIAN32__
	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Failed to initialize atomic, err=%08X\n", err) );
#else
	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Failed to initialize atomic, err=%.8X\n", err) );
#endif

	MALI_IGNORE( err );
}
