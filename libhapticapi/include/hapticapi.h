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
 * Declares the constants, macros, types, and functions of the Haptic C API.
 *
 * \version 1.0.10.0
 *
 * \mainpage
 *
 * \section     whatis      What is TouchSense 2200 &mdash; Fundamental Principles
 *
 * TouchSense 2200 is Immersion's haptic solution for mobile computing devices.
 * The solution comprises mechanical, electrical, firmware, and software
 * guidelines, designs, and components.
 *
 * This documentation describes Immersion's Haptic C API, a software component
 * of the TouchSense 2200 solution. The following diagram shows where the C API
 * fits in relative to other software components.
 *
 * \image       html        hapticapi.png "TouchSense 2200 Software Stack"
 *
 * The Haptic C API and Transport run in the application's process space. The
 * C API is platform-independent. The C API interface is the same on all
 * platforms.
 *
 * The Transport implements the platform-dependent services that the C API
 * requires. The Transport typically uses device drivers to communicate with the
 * haptic hardware and synchronize access from different processes.
 *
 * Immersion provides application developers with Haptic C API header files.
 * Immersion provides partners (OEMs/ODMs) with reference C API and Transport
 * source code and binaries. Immersion's partners (OEMs/ODMs) provide end users,
 * and therefore application developers, with C API and Transport binaries for
 * specific platforms.
 *
 * \section     howtouse    How to Use This API
 *
 * Your target device should come with Haptic C API and Transport run-time
 * binaries pre-installed. Immersion provides additional header files and
 * libraries that you may need to build your applications with haptic
 * capabilities.
 *
 * To use the Haptic C API,
 *  -   call #ImHapticInitialize once, typically when your application
 *      initializes
 *  -   call #ImHapticPlayEffect whenever you want to play an effect from your
 *      application
 *  -   call #ImHapticPlayEffectSequence whenever you want to play a sequence
 *      of effects from your application
 *  -   Call #ImHapticPlayPreDefinedEffectSequence whenever you want to play a
 *      pre-defined sequence of effects from your application. Call
 *      #ImHapticGetPreDefinedEffectSequenceIndexMax to get the maximum index for
 *      pre-defined sequences of effects.
 *  -   call #ImHapticTerminate once, typically when your application
 *      terminates
 *
 * Whevener you start playing a new haptic effect, if there is a previous effect
 * still playing, the old effect stops playing before the new one starts. If you
 * want to stop any playing effect without starting a new one, call
 * ImHapticStopPlayingEffect().
 *
 * The following diagram illustrates the order in which the various Haptic C API
 * functions may be called. For brevity, the \c ImHaptic prefix has been removed
 * from the function names in the diagram.
 *
 * \image       html        hapticapifunctions.png "API Function Call Sequence"
 */

/* Prevent this file from being processed more than once. */
#ifndef _HAPTICAPI_H
#define _HAPTICAPI_H

/* Include platform-common definitions. */
#include <hapticcommon.h>

/*
** API functions may be called from C. Prevent C++ compilers from mangling
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
 * Maximum effect index for ImHapticPlayEffect() and in buffer passed to
 * ImHapticPlayEffectSequence().
 */
#define HAPTIC_EFFECT_INDEX_MAX                      123

/**
 * Maximum inter-effect delay in multiples of 10 milliseconds in buffer passed to
 * ImmHapticPlayEffectSequence().
 */
#define HAPTIC_INTER_EFFECT_DELAY_10MS_MAX           128

/**
 * Maximum size of buffer passed to ImHapticPlayEffectSequence().
 */
#define HAPTIC_PLAY_EFFECT_SEQUENCE_BUFFER_SIZE_MAX  8

/**
 * Maximum effect duration passed to ImHapticPlayTimedEffect().
 */
#define HAPTIC_TIMED_EFFECT_DURATION_MS_MAX          10000

/*
** -----------------------------------------------------------------------------
** Public Functions
** -----------------------------------------------------------------------------
*/

/**
 * Initializes the Haptic C API.
 *
 * ImHapticInitialize() establishes communication with the haptic hardware.
 *
 * Applications must call ImHapticInitialize(), typically during application
 * initialization, before calling the other functions of the Haptic C API. If an
 * application calls other functions of the API before calling
 * ImHapticInitialize(), those functions return \c #HAPTIC_E_NOT_INITIALIZED.
 *
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_FAIL
 *                          General error not covered by more specific codes.
 *                          Possible causes include:
 *                          -   Unable to find or establish communication with
 *                              the haptic hardware.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_MEMORY
 *                          Memory allocation error. Unable to allocate required
 *                          memory.
 * \retval      HAPTIC_E_ALREADY_INITIALIZED
 *                          The application has already called
 *                          ImHapticInitialize() without a subsequent call to
 *                          ImHapticTerminate(). Applications may call
 *                          ImHapticInitialize() more than once if they also
 *                          call ImHapticTerminate() in between calls to
 *                          ImHapticInitialize().
 * \retval      HAPTIC_E_UNSUPPORTED
 *                          The Haptic C API is not installed on this device.
 * \sa          ImHapticTerminate
 */
HAPTICAPI_EXTERN HapticResult HAPTICAPI_CALL ImHapticInitialize();

/**
 * Terminates the Haptic C API.
 *
 * ImHapticTerminate() frees resources such as dynamically-allocated memory, and
 * releases any connection to the haptic hardware.
 *
 * Applications should call ImHapticTerminate(), typically during application
 * termination, after calling the other functions of the Haptic C API.
 *
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_NOT_INITIALIZED
 *                          The application has not called ImHapticInitialize().
 *                          Applications may call ImHapticTerminate() more than
 *                          once if they also call ImHapticInitialize() in
 *                          between calls to ImHapticTerminate().
 * \retval      HAPTIC_E_UNSUPPORTED
 *                          The Haptic C API is not installed on this device.
 * \sa          ImHapticInitialize
 */
HAPTICAPI_EXTERN HapticResult HAPTICAPI_CALL ImHapticTerminate();

/**
 * Plays a haptic effect.
 *
 * \param[in]   effectIndex Zero-based index of the effect to play. The effect
 *                          index must be between \c 0 and \c
 *                          #HAPTIC_EFFECT_INDEX_MAX, inclusive.
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_ARGUMENT
 *                          \p effectIndex is invalid. For example,
 *                          -   \p effectIndex is negative
 *                          -   \p effectIndex is greater than \c
 *                              #HAPTIC_EFFECT_INDEX_MAX
 * \retval      HAPTIC_E_NOT_INITIALIZED
 *                          The application has not called ImHapticInitialize().
 * \retval      HAPTIC_E_UNSUPPORTED
 *                          The Haptic C API is not installed on this device.
 */
HAPTICAPI_EXTERN HapticResult HAPTICAPI_CALL ImHapticPlayEffect(
    HapticInt effectIndex);

/**
 * Plays a fixed haptic effect for a specified duration.
 *
 * \param[in]   effectDuration
 *                          Duration, in milliseconds, for which to play the
 *                          effect. The effect duration must be between \c 0 and
 *                          \c #HAPTIC_TIMED_EFFECT_DURATION_MS_MAX, inclusive.
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_ARGUMENT
 *                          \p effectDuration is invalid. For example,
 *                          -   \p effectDuration is negative
 *                          -   \p effectDuration is greater than \c
 *                              #HAPTIC_TIMED_EFFECT_DURATION_MS_MAX
 * \retval      HAPTIC_E_NOT_INITIALIZED
 *                          The application has not called ImHapticInitialize().
 * \retval      HAPTIC_E_UNSUPPORTED
 *                          The Haptic C API is not installed on this device.
 */
HAPTICAPI_EXTERN HapticResult HAPTICAPI_CALL ImHapticPlayTimedEffect(
    HapticInt effectDuration);

/**
 * Plays a sequence of haptic effects.
 *
 * \param[in]   buffer      Buffer containing one or more effect index,
 *                          inter-effect delay pairs. In detail,
 *                          -   <tt>buffer[0]</tt> specifies the index of the
 *                              first effect
 *                          -   <tt>buffer[1]</tt> specifies the delay in
 *                              multiples of 5 milliseconds between the end of
 *                              the first effect and the start of the second
 *                              effect
 *                          -   <tt>buffer[2]</tt> specifies the index of the
 *                              second effect
 *                          -   <tt>buffer[3]</tt> specifies the delay in
 *                              multiples of 5 milliseconds between the end of
 *                              the second effect and the start of the third
 *                              effect
 *                          -   and so on, up to a maximum of buffer size of
 *                              \c #HAPTIC_PLAY_EFFECT_SEQUENCE_BUFFER_SIZE_MAX,
 *                              which allows up to \c 16 effects to be
 *                              sequenced. The last inter-effect
 *                              delay is optional and defaults to \c 0.
 * \param[in]   bufferSize  Number of bytes in the buffer.
 * \param[in]   repeat      Number of times to repeat the effect sequence.
 *                          A value of \c 0 causes the effect sequence to play
 *                          once. A value of \c 1 causes the effect sequence to
 *                          play twice. And so on. The inter-effect delay
 *                          specified for the last effect in the sequence is
 *                          used between the last and first effects when the
 *                          sequence is repeated.
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_ARGUMENT
 *                          An argument is invalid. For example,
 *                          -   \p buffer is \c NULL
 *                          -   \p bufferSize is \c 0 or greater than \c
 *                              #HAPTIC_PLAY_EFFECT_SEQUENCE_BUFFER_SIZE_MAX
 *                          -   \p buffer contains an effect index that is
 *                              greater than \c #HAPTIC_EFFECT_INDEX_MAX
 *                          -   \p buffer contains an inter-effect delay that is
 *                              greater than \c
 *                              #HAPTIC_INTER_EFFECT_DELAY_5MS_MAX
 *                          -   \p repeat is negative
 *                          -   \p repeat is greater than \c
 *                              #HAPTIC_REPEAT_COUNT_MAX
 * \retval      HAPTIC_E_NOT_INITIALIZED
 *                          The application has not called ImHapticInitialize().
 * \retval      HAPTIC_E_UNSUPPORTED
 *                          The Haptic C API is not installed on this device.
 */
HAPTICAPI_EXTERN HapticResult HAPTICAPI_CALL ImHapticPlayEffectSequence(
    const HapticUInt8*  buffer,
    HapticSize          bufferSize);

/**
 * Stops playing a haptic effect if one is playing.
 *
 * \note        It is not an error to call ImHapticStopPlayingEffect() when an
 *              effect is not playing.
 *
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_NOT_INITIALIZED
 *                          The application has not called ImHapticInitialize().
 * \retval      HAPTIC_E_UNSUPPORTED
 *                          The Haptic C API is not installed on this device.
 */
HAPTICAPI_EXTERN HapticResult HAPTICAPI_CALL ImHapticStopPlayingEffect();

/**
 * Enables audio-to-haptic feature of the chip
 *
 * \note        It is not an error to call ImAudioHapticEnable() even if
 *              it is already enabled
 *
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_NOT_INITIALIZED
 *                          The application has not called ImHapticInitialize().
 * \retval      HAPTIC_E_UNSUPPORTED
 *                          The Haptic C API is not installed on this device.
 */
HAPTICAPI_EXTERN HapticResult HAPTICAPI_CALL ImAudioHapticEnable();

/**
 * Disables audio-to-haptic feature of the chip
 *
 * \note        It is not an error to call ImAudioHapticDisable() even if
 *              it is already disabled
 *
 * \retval      HAPTIC_S_OK No error.
 * \retval      HAPTIC_E_INTERNAL
 *                          Internal error. Should not arise. Possibly due to a
 *                          programming error in the Haptic Transport or Haptic
 *                          C API.
 * \retval      HAPTIC_E_SYSTEM
 *                          System error. A system call returned an error.
 * \retval      HAPTIC_E_NOT_INITIALIZED
 *                          The application has not called ImHapticInitialize().
 * \retval      HAPTIC_E_UNSUPPORTED
 *                          The Haptic C API is not installed on this device.
 */
HAPTICAPI_EXTERN HapticResult HAPTICAPI_CALL ImAudioHapticDisable();

#ifdef __cplusplus
}
#endif

#endif
