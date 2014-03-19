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
 * This file defines the Android atomic count implementation.
 * Use __atomic_cmpxchg from Android Bionic. Returns 0 if test and swap
 * is successful, 1 if it fails.
 *
 * \version 1.0.10.0
 */

/* Haptic Transport includes */
#include <haptictransport.h>

/* ------------------------------------------------------------------------- */
/* AtomicTestAndSet                                                          */
/* ------------------------------------------------------------------------- */
HapticResult HAPTICTRANSPORT_CALL ImTransportTestAndSetInt(
    HapticInt*  destination,
    HapticInt   setValue,
    HapticInt   testValue)
{
    /* The API must provide a non-NULL input/output argument. */
    if (!destination)
    {
        HAPTIC_ASSERT(0);
        return HAPTIC_E_INTERNAL;
    }

    /*
    ** Perform the operation.
     */
    if (__atomic_cmpxchg(testValue, setValue, destination) == 0)
    {
        return HAPTIC_S_TRUE;
    }
    else
    {
        return HAPTIC_S_FALSE;
    }
}

/* ------------------------------------------------------------------------- */
/* AtomicTest                                                                */
/* ------------------------------------------------------------------------- */
HapticResult HAPTICTRANSPORT_CALL ImTransportTestInt(
    HapticInt*  value,
    HapticInt   testValue)
{
    /* The API must provide a non-NULL input argument. */
    if (!value)
    {
        HAPTIC_ASSERT(0);
        return HAPTIC_E_INTERNAL;
    }

    /*
    ** Perform the operation.
     */
    if (__atomic_cmpxchg(testValue, testValue, value) == 0)
    {
        return HAPTIC_S_TRUE;
    }
    else
    {
        return HAPTIC_S_FALSE;
    }
}

