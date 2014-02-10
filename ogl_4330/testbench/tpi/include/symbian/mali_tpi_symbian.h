/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2009-2010, 2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

#ifndef _MALI_TPI_SYMBIAN_H_
#define _MALI_TPI_SYMBIAN_H_

/* **DO NOT** include this header directly; include via root mali_tpi.h. */

#define MALI_TPI_FILE_STDIO     1
typedef FILE                    mali_tpi_file;

/* Include this early - we need some of the definition for later functions. */
#include "tpi/include/common/mali_tpi_internal.h"

/* ============================================================================
	Function Definitions
============================================================================ */
/**
 * @defgroup core_symbian_extensions Symbian OS Extensions
 * @{
 */

/**
 * @brief Public initialization function
 *
 * @param dir        The directory type to set to path of
 * @param path       The directory path to set the directory to
 *
 * @note This function is not guaranteed to be reentrant; users of the TPI
 * library should ensure that this function is only called from a single thread
 * context.
 */
MALI_TPI_API void mali_tpi_arm_set_dir(
	mali_tpi_dir dir,
	const char *path );

/**
 * @}
 */

#endif /* End (_MALI_TPI_SYMBIAN_H_) */
