/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_tpi_atomic_x86.h
 * x86-specific atomic functions for the tpi module
 */

#ifndef _TPI_ATOMIC_X86_H_
#define _TPI_ATOMIC_X86_H_

#include "mali_tpi_atomic_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE u32 mali_tpi_atomic_get(const mali_tpi_atomic *atom)
{
	return atom->tpi_priv_internal_struct.val;
}

static INLINE void mali_tpi_atomic_set(mali_tpi_atomic *atom, u32 val)
{
	atom->tpi_priv_internal_struct.val = val;
}

static INLINE u32 mali_tpi_atomic_add(mali_tpi_atomic *atom, u32 val)
{
	u32 ret;

	ret = __sync_add_and_fetch( &atom->tpi_priv_internal_struct.val, val );

	return ret;
}

static INLINE u32 mali_tpi_atomic_sub(mali_tpi_atomic *atom, u32 val)
{
	return mali_tpi_atomic_add(atom, -val);
}

static INLINE u32 mali_tpi__atomic_compare_and_swap(mali_tpi_atomic *atom, u32 old_value,
					      u32 new_value)
{
	u32 ret;

	ret = __sync_val_compare_and_swap( &atom->tpi_priv_internal_struct.val, old_value, new_value );

	return ret;
}

#ifdef __cplusplus
}
#endif

#define TPI_PRIV_PROVIDES_ATOMIC 1

#endif /* _TPI_ATOMIC_X86_H_ */
