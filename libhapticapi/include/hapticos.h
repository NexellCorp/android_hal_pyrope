/*
** =============================================================================
** Copyright (C) 2012  Immersion Corporation.
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
 * Declares the platform-dependent constants, macros, types, and functions of
 * the Haptic C API and the Haptic Transport on Linux platforms.
 *
 * \version 1.0.10.0
 */

/* Prevent this file from being processed more than once. */
#ifndef _HAPTICOS_H
#define _HAPTICOS_H

/*
** Display a compile-time error if this file is used on a non-Linux platform.
*/
#ifndef __linux__
#error This file is for Linux platforms.
#endif

/* System includes. */
#include <assert.h>
#include <stddef.h>

#ifndef HAPTICAPI_EXTERN
    /**
     * Linkage specification for the Haptic C API, may be overridden by client
     * project settings.
     */
    #define HAPTICAPI_EXTERN extern
#endif

#ifndef HAPTICAPI_CALL
    /**
     * Calling convention for the Haptic C API, may be overridden by client
     * project settings.
     */
    #define HAPTICAPI_CALL
#endif

#ifndef HAPTICTRANSPORT_EXTERN
    /**
     * Linkage specification for the Immersion Haptic Transport, may be
     * overridden by client project settings.
     */
    #define HAPTICTRANSPORT_EXTERN extern
#endif

#ifndef HAPTICTRANSPORT_CALL
    /**
     * Calling convention for the Immersion Haptic Transport (used by the
     * Haptic C API), may be overridden by client project settings.
     */
    #define HAPTICTRANSPORT_CALL
#endif

/**
 * \addtogroup  DebugDefs   Debugging Constants and Macros
 */
/*@{*/

#ifndef NDEBUG
    /**
     * Used within the Haptic C API to determine whether building for debugging
     * purposes.
     */
    #define HAPTIC_DEBUG                1
#endif

/**
 * Used within the Haptic C API for assertions.
 */
#define HAPTIC_ASSERT(x)                assert(x)

/*@}*/


/**
 * \addtogroup  Types       Types
 */
/*@{*/

/**
 * 8-bit unsigned integer.
 */
typedef unsigned char                   HapticUInt8;

/**
 * Data size.
 */
typedef size_t                          HapticSize;

/*@}*/

#endif
