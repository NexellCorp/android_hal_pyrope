/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2005-2010, 2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

/* ============================================================================
   Includes
============================================================================ */
#define MALI_TPI_API MALI_TPI_EXPORT
#include "tpi/mali_tpi.h"

#include <assert.h>
#include <string.h>

void mali_tpi_abort(void)
{
	*MALI_REINTERPRET_CAST(u32*)0 = 0;
	/* FIXME: Temporary disabled to pass buildbot builds */
	/* abort(); */
} 

void * mali_tpi_memcpy(void * destination, const void * source, u32 num)
{
	return memcpy(destination, source, num);
}
