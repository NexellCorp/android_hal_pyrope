/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_osu.h"


/**
 * Atomic integer increment and return post increment value
 * @param atomic Pointer to _mali_osu_atomic_t to atomically increment
 * @return Value of val after increment
 */
u32 _mali_osu_atomic_inc_and_return(_mali_osu_atomic_t * atomic)
{
	u32 res = __sync_add_and_fetch(&(atomic->u.val), 1);
	return res;
}

/**
 * Atomic integer decrement and return post decrement value
 * @param atomic Pointer to _mali_osu_atomic_t to atomically decrement
 * @return Value of val after decrement
 */
u32 _mali_osu_atomic_dec_and_return(_mali_osu_atomic_t * atomic)
{
	u32 res = __sync_sub_and_fetch(&(atomic->u.val), 1);
	return res;
}

/**
 * Atomic integer get. Read value stored in the atomic integer.
 * @param atomic Pointer to _mali_osu_atomic_t to read contents of
 * @return Value currently in _mali_osu_atomic_t
 * @note If a inc/dec/set is currently in progress you'll get either
 * the pre or post value.
 */
u32 _mali_osu_atomic_read(_mali_osu_atomic_t * atomic)
{
	return atomic->u.val;
}

/**
 * Atomic integer set. Used to explicitly set the contents of an atomic
 * integer to a specific value.
 * @param atomic Pointer to _mali_osu_atomic_t to set
 * @param value Value to write to the atomic int
 * @note If inc/dec is currently in progess the atomic int will contain
 * either the value or value +/- 1.
 * @note If another set is currently in progress it's uncertain which will
 * persist when the function returns.
 */
void _mali_osu_atomic_write(_mali_osu_atomic_t * atomic, u32 value)
{
	atomic->u.val = value;
}

/**
 * Initialize an atomic integer. Used to initialize an atomic integer to a sane value when created.
 * @param atomic Pointer to _mali_osu_atomic_t to set
 * @param value Value to initialize the atomic integer to
 * @note This function has to be called when a _mali_osu_atomic_t is created/before first use
 */
_mali_osu_errcode_t _mali_osu_atomic_init(_mali_osu_atomic_t * atomic, u32 value)
{
	atomic->u.val = value;
	return MALI_ERR_NO_ERROR;
}
