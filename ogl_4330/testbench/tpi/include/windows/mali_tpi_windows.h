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

#ifndef _MALI_TPI_WINDOWS_H_
#define _MALI_TPI_WINDOWS_H_

#include <windows.h>

/* **DO NOT** include this header directly; include via root mali_tpi.h. */

#define MALI_TPI_FILE_STDIO     1
typedef FILE                    mali_tpi_file;

typedef HANDLE                  mali_tpi_thread;
typedef CRITICAL_SECTION        mali_tpi_mutex;
typedef mali_tpi_mutex			mali_tpi_barrier;	/* TODO to be implemented according to windows API */

#endif /* End (_MALI_TPI_WINDOWS_H_) */
