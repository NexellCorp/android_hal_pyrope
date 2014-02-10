/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_tpi_atomic_arm.h
 * arm-specific atomic functions for the TPI module
 */

#ifndef _TPI_ATOMIC_ARM_H_
#define _TPI_ATOMIC_ARM_H_

#include <malisw/mali_malisw.h>

/* Mali-400 DDK supports ARMv6 also*/
#if defined(TPI_PLATFORM_ARM_ARCH) && (TPI_PLATFORM_ARM_ARCH >= 5)
#include "mali_tpi_atomic_armv7.h"
#else
#include "mali_tpi_atomic_armv5.h"
#endif

#include "mali_tpi_atomic_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE u32 mali_tpi_atomic_get(const mali_tpi_atomic *atom)
{
	u32 tmp;

	dmb();
	tmp = atom->tpi_priv_internal_struct.val;
	dmb();
	return tmp;
}

/* Use GCC intrinsics on newer GCC where they are efficient */
#if CSTD_TOOLCHAIN_RVCT_GCC_MODE || __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR_ >= 5)
#if __ARMCC_VERSION < 400000
/* these are implicitly defined for armcc 4.x onwards */
extern u32 __sync_bool_compare_and_swap (volatile u32 *ptr, u32 oldval, u32 newval, ...);
extern u32  __sync_add_and_fetch (volatile u32 *ptr, u32 value, ...);
extern u32  __sync_sub_and_fetch (volatile u32 *ptr, u32 value, ...);
extern u32 __sync_val_compare_and_swap (volatile u32 *ptr, u32 oldval, u32 newval, ...);
#endif

static INLINE void mali_tpi_atomic_set(mali_tpi_atomic *atom, u32 val)
{
	volatile u32 *aval;
	mali_bool written;

	do
	{
		aval = &atom->tpi_priv_internal_struct.val;
		written = __sync_bool_compare_and_swap(aval, *aval, val);
	}
	while(!written);
}

static INLINE u32 mali_tpi_atomic_add(mali_tpi_atomic *atom, u32 val)
{
	volatile u32 *aval;

	aval = &atom->tpi_priv_internal_struct.val;

	return __sync_add_and_fetch(aval, val);
}

static INLINE u32 mali_tpi_atomic_sub(mali_tpi_atomic *atom, u32 val)
{
	volatile u32 *aval;

	aval = &atom->tpi_priv_internal_struct.val;

	return __sync_sub_and_fetch(aval, val);
}

static INLINE u32 mali_tpi_atomic_compare_and_swap(mali_tpi_atomic *atom, u32 old_value,
					      u32 new_value)
{
	volatile u32 *aval;

	aval = &atom->tpi_priv_internal_struct.val;

	return __sync_val_compare_and_swap (aval, old_value, new_value);
}

#else /* Old GCC */
static INLINE void mali_tpi_atomic_set(mali_tpi_atomic *atom, u32 val)
{
	u32 tmp;
	volatile u32 *aval;

	aval = &atom->tpi_priv_internal_struct.val;

	dmb();
	__asm__ volatile("1:	ldrex	%[tmp], [%[addr]]\n"
			 "	strex	%[tmp], %[val], [%[addr]]\n"
			 "	cmp	%[tmp], #0\n"
			 "	bne	1b"
			 : [tmp] "=&r" (tmp), "+Qo" (*aval)
			 : [val] "r" (val), [addr] "r" (aval)
			 : "cc");
	dmb();
}

static INLINE u32 mali_tpi_atomic_add(mali_tpi_atomic *atom, u32 val)
{
	u32 res;
	u32 tmp;
	volatile u32 *aval;

	aval = &atom->tpi_priv_internal_struct.val;

	dmb();
	__asm__ volatile("1:	ldrex	%[res], [%[addr]]\n"
			 "	add	%[res], %[res], %[val]\n"
			 "	strex	%[tmp], %[res], [%[addr]]\n"
			 "	cmp	%[tmp], #0\n"
			 "	bne	1b"
			 : [tmp] "=&r" (tmp), [res] "=&r" (res), "+Qo" (*aval)
			 : [addr] "r" (aval), [val] "r" (val)
			 : "cc");
	dmb();
	return res;
}

static INLINE u32 mali_tpi_atomic_sub(mali_tpi_atomic *atom, u32 val)
{
	u32 res;
	u32 tmp;
	volatile u32 *aval;

	aval = &atom->tpi_priv_internal_struct.val;

	dmb();
	__asm__ volatile("1:	ldrex	%[res], [%[addr]]\n"
			 "	sub	%[res], %[res], %[val]\n"
			 "	strex	%[tmp], %[res], [%[addr]]\n"
			 "	cmp	%[tmp], #0\n"
			 "	bne	1b"
			 : [tmp] "=&r" (tmp), [res] "=&r" (res), "+Qo" (*aval)
			 : [addr] "r" (aval), [val] "r" (val)
			 : "cc");
	dmb();
	return res;
}

/* Note: the kernel provided VDSO to perform CAS returns zero for
 * success/non-zero for failure. TPI expects the return value to be the value
 * that was in memory  */
static INLINE u32 mali_tpi_atomic_compare_and_swap(mali_tpi_atomic *atom, u32 old_value,
					      u32 new_value)
{
	u32 res;
	u32 tmp;
	volatile u32 *aval;

	aval = &atom->tpi_priv_internal_struct.val;

	dmb();
	__asm__ volatile("1:	ldrex	%[res], [%[addr]]\n"
			 "	mov     %[tmp], #0\n"
			 "	cmp	%[res], %[old_value]\n"
			 "      it	eq\n"
			 "	strexeq	%[tmp], %[new_value], [%[addr]]\n"
			 "	cmp	%[tmp], #0\n"
			 "	bne	1b\n"
			 : [tmp] "=&r" (tmp), [res] "=&r" (res), "+Qo" (*aval)
			 : [addr] "r" (aval), [new_value] "r" (new_value), [old_value] "r" (old_value)
			 : "cc");
	dmb();
	return res;
}

#endif /* Old GCC */

#ifdef __cplusplus
}
#endif

#define TPI_PRIV_PROVIDES_ATOMIC 1

#endif /* _TPI_ATOMIC_ARM_H_ */
