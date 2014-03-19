/*
** =============================================================================
** Copyright (C) 2010-2012  Immersion Corporation.
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
 * Declares the constants, macros, types, and functions that are common between
 * the Haptic C API and the Haptic Transport.
 *
 * \version 1.0.10.0
 */

/* Prevent this file from being processed more than once. */
#ifndef _HAPTICCOMMON_H
#define _HAPTICCOMMON_H

/* Include platform-dependent definitions. */
#include <hapticos.h>

/**
 * \defgroup    ResultDefs  Return Status Constants and Macros
 */
/*@{*/

/**
 * Indicates true condition (no error).
 */
#define HAPTIC_S_TRUE                   1

/**
 * Indicates false condition (no error).
 */
#define HAPTIC_S_FALSE                  0

/**
 * No error.
 */
#define HAPTIC_S_OK                     0

/**
 * General error not covered by more specific codes.
 */
#define HAPTIC_E_FAIL                   -1

/**
 * Internal error. Should not arise. Possibly due to a programming error in the
 * Haptic Transport or Haptic C API.
 */
#define HAPTIC_E_INTERNAL               -2

/**
 * System error. A system call returned an error.
 */
#define HAPTIC_E_SYSTEM                 -3

/**
 * Memory allocation error. Unable to allocate required memory.
 */
#define HAPTIC_E_MEMORY                 -4

/**
 * Null or invalid argument.
 */
#define HAPTIC_E_ARGUMENT               -5

/**
 * The Haptic C API is already initialized. The application has already called
 * ImHapticInitialize() without an intervening call to ImHapticTerminate().
 */
#define HAPTIC_E_ALREADY_INITIALIZED    -6

/**
 * The Haptic C API is not initialized. The application must first call
 * ImHapticInitialize().
 */
#define HAPTIC_E_NOT_INITIALIZED        -7

/**
 * The Haptic C API is not installed on this device.
 */
#define HAPTIC_E_UNSUPPORTED            -8

/**
 * Tests a return status code for success.
 */
#define HAPTIC_SUCCEEDED(r)              ((r) >= HAPTIC_S_OK)

/**
 * Tests a return status code for failure.
 */
#define HAPTIC_FAILED(r)                 ((r) < HAPTIC_S_OK)

/*@}*/

/**
 * \addtogroup  DebugDefs Debugging Constants and Macros
 */
/*@{*/

/**
 * \def     HAPTIC_VERIFY(x)
 *                      Similar to the platform-dependent HAPTIC_ASSERT() but
 *                      the expression is always evaluated.
 */
#if HAPTIC_DEBUG
#define HAPTIC_VERIFY(x)                 HAPTIC_ASSERT(x)
#else
#define HAPTIC_VERIFY(x)                 (void)(x)
#endif

/*@}*/

/**
 * \addtogroup  Types       Types
 */
/*@{*/

/**
 * Native integer.
 */
typedef int             HapticInt;

/**
 * Return status code.
 *
 * All Haptic C API functions return a status code.
 */
typedef HapticInt       HapticResult;

/**
 * Thread mutex.
 *
 * Used within the Haptic C API for inter-thread synchronization.
 */
typedef void*           HapticThreadMutex;

/*@}*/

#endif
