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
 * @file mali_tpi_atomic_helpers.h
 * Private helpers for TPI atomic access
 */
#ifndef _TPI_ATOMIC_HELPERS_H_
#define _TPI_ATOMIC_HELPERS_H_

/** @brief Public type of atomic variables.
 *
 * This is public for allocation on stack. It always implements exactly 32 bits
 * of unsigned integer storage.
 */
typedef struct
{
	struct
	{
		volatile u32 val;
	} tpi_priv_internal_struct;
} mali_tpi_atomic;

#endif /* _TPI_ATOMIC_HELPERS_H_ */
