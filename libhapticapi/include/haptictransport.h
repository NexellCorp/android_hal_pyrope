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
 * Declares the constants, macros, types, and functions of Immersion's Haptic
 * Transport.
 *
 * \version 1.0.10.0
 *
 * \mainpage
 *
 * The Haptic Transport is used by the Haptic C API and provides the
 * platform-dependent services that the C API needs, such as communication with
 * the hardware, inter-thread synchronization, and dynamic memory allocation.
 *
 * The Haptic Transport assumes that it is called only by the Haptic C API. If
 * applications call the Transport directly without going through the C API, the
 * behavior is undefined.
 *
 * While the Haptic Transport provides inter-thread synchronization services,
 * the Haptic C API contains all the logic using those services to support
 * multi-threading and inter-thread mutual exclusion. The Transport assumes that
 * the C API calls only one function at a time within a process. It is possible
 * for different processes to call Transport functions at the same time. The
 * Transport is responsible for handling any inter-process synchronization that
 * may affect its operation.
 */

/* Prevent this file from being processed more than once. */
#ifndef _HAPTICTRANSPORT_H
#define _HAPTICTRANSPORT_H

/* Include platform-common definitions. */
#include <hapticcommon.h>

/*
** Transport functions may be called from C. Prevent C++ compilers from mangling
** function names.
*/
#ifdef __cplusplus
extern "C" {
#endif

/*
** -----------------------------------------------------------------------------
** Constants
** -----------------------------------------------------------------------------
*/

/**
 * Invalid thread mutex.
 */
#define HAPTIC_INVALID_THREAD_MUTEX                 NULL

/*
** -----------------------------------------------------------------------------
** Public Functions
** -----------------------------------------------------------------------------
*/

/**
 * Opens the connection to the haptic hardware and establishes communication
 * with the firmware.
 *
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_FAIL
 *                          General error not covered by more specific codes.
 *                          Possible causes include:
 *                          -   Unable to find or establish communication with
 *                              suitable device.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_MEMORY
 *                          Memory allocation error. Unable to allocate required
 *                          memory.
 * \sa          ImTransportClose
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportOpen();

/**
 * Closes the connection to the haptic hardware.
 *
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \sa          ImTransportOpen
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportClose();

/**
 * Transmits a message to the haptic firmware.
 *
 * \param[in]   buffer      Buffer containing the message to transmit.
 * \param       bufferLength
 *                          Number of bytes in message to transmit.
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \sa          ImTransportOpen
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportWrite(
    const HapticUInt8*  buffer,
    HapticSize          bufferLength);

/**
 * Receives a message from the haptic firmware.
 *
 * \param[in]   buffer      Buffer to receive bytes from the firmware.
 * \param       bufferLength
 *                          Number of bytes to receive from the firmware.
 *
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \sa          ImTransportOpen
 */
HapticResult HAPTICTRANSPORT_CALL ImTransportRead(
    HapticUInt8*    buffer,
    HapticSize      bufferLength);

/**
 * Tests and conditionally sets an integer value in one atomic operation that
 * cannot be interrupted by other threads, and which is executed sequentially
 * relative to other memory-access operations.
 *
 * Implements the following logic in one atomic operation:
 * \code
 * if (*target == testValue) *destination = setValue;
 * \endcode
 *
 * \param[in,out]
 *              target      Value to test and conditionally set.
 * \param[in]   setValue    Value to set \p *target to if the test passes.
 * \param[in]   testValue   Value to compare \p *target against for the test.
 * \retval      HAPTIC_S_TRUE
 *                          Test passed. \p *target was equal to
 *                          \p testValue and therefore updated to \p setValue.
 * \retval      HAPTIC_S_FALSE
 *                          Test failed. \p *target was not equal to
 *                          \p testValue and therefore left unchanged.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportTestAndSetInt(
    HapticInt*  target,
    HapticInt   setValue,
    HapticInt   testValue);

/**
 * Tests an integer value in one atomic operation that cannot be interrupted by
 * other threads, and which is executed sequentially relative to other
 * memory-access operations.
 *
 * \param[in]   target      Value to test.
 * \param[in]   testValue   Value to compare \p *target against for the test.
 * \retval      HAPTIC_S_TRUE
 *                          Test passed. \p *target is equal to \p testValue.
 * \retval      HAPTIC_S_FALSE
 *                          Test failed. \p *target is not equal to
 *                          \p testValue.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportTestInt(
    HapticInt*  target,
    HapticInt   testValue);

/**
 * Creates a mutex for inter-thread synchronization.
 *
 * The mutex cannot be used for inter-process synchronization.
 *
 * \param[out]  mutex       Mutex object to initialize.
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_MEMORY
 *                          Memory allocation error. Unable to allocate required
 *                          memory.
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportCreateThreadMutex(
    HapticThreadMutex* mutex);

/**
 * Destroys a mutex used for inter-thread synchronization.
 *
 * \param[in]   mutex       Mutex object to destroy.
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportDestroyThreadMutex(
    HapticThreadMutex mutex);

/**
 * Locks a mutex used for inter-thread synchronization.
 *
 * \param[in]   mutex       Mutex object to lock.
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportAcquireThreadMutex(
    HapticThreadMutex mutex);

/**
 * Unlocks a mutex used for inter-thread synchronization.
 *
 * \param[in]   mutex       Mutex object to unlock.
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 */
HAPTICTRANSPORT_EXTERN HapticResult HAPTICTRANSPORT_CALL ImTransportReleaseThreadMutex(
    HapticThreadMutex mutex);

#ifdef __cplusplus
}
#endif

#endif
