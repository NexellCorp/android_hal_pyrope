/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2009-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

#ifndef _MALI_TPI_H_
#define _MALI_TPI_H_

/* ============================================================================
	Includes
============================================================================ */
#include <stdio.h>
#include "malisw/mali_malisw.h"

#if (1 == CSTD_OS_WINDOWS) || defined(TPI_OS_WINDOWS)
	#include "tpi/include/windows/mali_tpi_windows.h"
#elif (1 == CSTD_OS_LINUX) || defined(TPI_OS_LINUX)
	#include "tpi/include/linux/mali_tpi_linux.h"
#elif (1 == CSTD_OS_ANDROID) || defined(TPI_OS_ANDROID)
	#include "tpi/include/android/mali_tpi_android.h"
#elif (1 == CSTD_OS_SYMBIAN) || defined(TPI_OS_SYMBIAN)
	#include "tpi/include/symbian/mali_tpi_symbian.h"
#elif (1 == CSTD_OS_APPLEOS) || defined(TPI_OS_APPLEOS)
	#include "tpi/include/osx/mali_tpi_osx.h"
#else
	#error "There is no TPI support for your operating system"
#endif

#include "tpi/include/common/mali_tpi_internal.h"

#include "tpi/mali_tpi_pr400.h"

#endif /* End (_MALI_TPI_H_) */
