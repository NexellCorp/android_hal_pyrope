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

#ifndef _MALI_TPI_OSX_H_
#define _MALI_TPI_OSX_H_

#include <pthread.h>

/* **DO NOT** include this header directly; include via root mali_tpi.h. */

#define MALI_TPI_FILE_STDIO     1
typedef FILE                    mali_tpi_file;

typedef pthread_t               mali_tpi_thread;
typedef pthread_mutex_t         mali_tpi_mutex;

typedef struct mali_tpi_barrier
{
    int needed_threads;
    int called;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} mali_tpi_barrier;

#endif /* End (_MALI_TPI_OSX_H_) */
