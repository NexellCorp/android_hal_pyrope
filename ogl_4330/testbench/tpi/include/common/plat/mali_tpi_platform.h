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
 * @file mali_tpi_platform.h
 * Platform-specific header files for the TPI module
 *
 * There is one mali_tpi_platform.h for each platform
 */
#ifndef _TPI_PLATFORM_H_
#define _TPI_PLATFORM_H_

/* Add includes here for this platform's TPI inline functions */
#ifdef __GNUC__

/* GCC specific atomic ops */

#if defined(__x86_64__) || defined(__i486__) || defined(__i386__)
#include "mali_tpi_atomic_x86.h"
#else
#include "mali_tpi_atomic_arm.h"
#endif

#endif /* __GNUC__ */

#ifndef TPI_PRIV_PROVIDES_ATOMIC
#error "No atomic support, please fix your TPI platform support"
#endif

#endif /* _TPI_PLATFORM_H_ */
