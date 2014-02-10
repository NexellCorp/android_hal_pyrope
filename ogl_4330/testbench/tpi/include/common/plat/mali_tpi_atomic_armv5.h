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
 * @file mali_tpi_atomic_armv5.h
 * armv5-specific atomic functions for the TPI module
 */

#ifndef _TPI_ATOMIC_ARMV5_H_
#define _TPI_ATOMIC_ARMV5_H_

#if CSTD_TOOLCHAIN_RVCT_GCC_MODE && __ARMCC_VERSION < 410000
	/* __memory_changed() merely informs the compiler, doesn't generate any code */
#	define dmb() 	do { int r0 = 0; __memory_changed(); __asm { mcr	p15, 0, r0, c7, c10, 5 } } while(0)
#else	
#	define dmb()	__asm__ volatile("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory")
#endif

#endif /* _TPI_ATOMIC_ARMV5_H_ */

