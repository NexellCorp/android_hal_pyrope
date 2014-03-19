/*
** =============================================================================
** Copyright (c) 2010-2012  Immersion Corporation.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** =============================================================================
*/
/**
 * \file
 *
 * This file defines the Linux mutex implementation.
 *
 * \version 1.0.10.0
 */

/* Haptic API includes */
#include <haptictransport.h>

/* OS includes */
#include <pthread.h>
#include <stdlib.h>     /* for malloc/free */

/*
** ===========================================================================
** Private Types.
** ===========================================================================
*/

typedef struct
{
    pthread_mutex_t  mutex;
} ThreadMutexInternal;

/*
** =============================================================================
** Functions
** =============================================================================
*/

HapticResult HAPTICTRANSPORT_CALL ImTransportCreateThreadMutex(
    HapticThreadMutex* mutex)
{
    HapticResult            retVal = HAPTIC_S_OK;
    ThreadMutexInternal*    intern; /* Initialized below. */

    /* The API must provide a non-NULL output argument. */
    if (!mutex)
    {
        HAPTIC_ASSERT(0);
        return HAPTIC_E_INTERNAL;
    }

    /* Initialize the output argument in case there's an error. */
    *mutex = HAPTIC_INVALID_THREAD_MUTEX;

    /* Allocate the thread mutex object. */
    intern = malloc(sizeof(ThreadMutexInternal));
    if (!intern)
    {
        return HAPTIC_E_MEMORY;
    }

    /* Initialize the mutex. */
    if (0 != pthread_mutex_init(&(intern->mutex), NULL))
    {
        free(intern);
        intern = NULL;
        retVal = HAPTIC_E_SYSTEM;
    }

    *mutex = intern;
    return retVal;
}

HapticResult HAPTICTRANSPORT_CALL ImTransportDestroyThreadMutex(
    HapticThreadMutex mutex)
{
    HapticResult            retVal = HAPTIC_S_OK;
    ThreadMutexInternal*    intern; /* Initialized below. */

    /* The API must have created the thread mutex. */
    if (HAPTIC_INVALID_THREAD_MUTEX == mutex)
    {
        HAPTIC_ASSERT(0);
        return HAPTIC_E_INTERNAL;
    }

    /* Get the thread mutex object. */
    intern = (ThreadMutexInternal*)mutex;


    if ((retVal = ImTransportReleaseThreadMutex(mutex)) > 0)
    {
        /* Delete the mutex. */
        if (0 != pthread_mutex_destroy(&(intern->mutex)))
        {
            retVal = HAPTIC_E_SYSTEM;
        }

        /* Free the thread mutex object. */
        free(mutex);
    } 

    return retVal;
}

HapticResult HAPTICTRANSPORT_CALL ImTransportAcquireThreadMutex(
    HapticThreadMutex mutex)
{
    HapticResult            retVal = HAPTIC_S_OK;
    ThreadMutexInternal*    intern; /* Initialized below. */

    /* The API must have created the thread mutex. */
    if (HAPTIC_INVALID_THREAD_MUTEX == mutex)
    {
        HAPTIC_ASSERT(0);
        return HAPTIC_E_INTERNAL;
    }

    /* Get the thread mutex object. */
    intern = (ThreadMutexInternal*)mutex;

    /* Lock the mutex. */
    if (0 != pthread_mutex_lock(&(intern->mutex)))
    {
        retVal = HAPTIC_E_SYSTEM;
    }

    return retVal;
}

HapticResult HAPTICTRANSPORT_CALL ImTransportReleaseThreadMutex(
    HapticThreadMutex mutex)
{
    HapticResult            retVal = HAPTIC_S_OK;
    ThreadMutexInternal*    intern; /* Initialized below. */

    /* The API must have created the thread mutex. */
    if (HAPTIC_INVALID_THREAD_MUTEX == mutex)
    {
        HAPTIC_ASSERT(0);
        return HAPTIC_E_INTERNAL;
    }

    /* Get the thread mutex object. */
    intern = (ThreadMutexInternal*)mutex;

    /* Unlock the mutex. */
    if (0 != pthread_mutex_unlock(&(intern->mutex)))
    {
        retVal = HAPTIC_E_SYSTEM;
    }

    return retVal;
}
