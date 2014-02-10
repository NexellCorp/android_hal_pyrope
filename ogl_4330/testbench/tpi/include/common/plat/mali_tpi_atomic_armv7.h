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
 * @file mali_tpi_atomic_armv7.h
 * armv7-specific atomic functions for the TPI module
 */

#ifndef _TPI_ATOMIC_ARMV7_H_
#define _TPI_ATOMIC_ARMV7_H_

#if CSTD_TOOLCHAIN_RVCT_GCC_MODE
	/* ancient armcc doesn't support ARMv6+ instructions in inline assembly. revert to ARMv5 atomics. */
#	if __ARMCC_VERSION < 410000
#		include "mali_tpi_atomic_armv5.h"
#	else
#		define dmb() 	__asm { dmb }
#	endif

#else /* CSTD_TOOLCHAIN_RVCT_GCC_MODE */
#	define dmb()	__asm__ volatile("dmb" : : : "memory")
#endif

#endif /* _TPI_ATOMIC_ARMV7_H_ */
