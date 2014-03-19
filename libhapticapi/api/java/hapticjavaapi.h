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
** File:
**     hapticjavaapi.h
**
** Description:
**    Native functions for TS2200 haptic shim package for Android
**
** =========================================================================
*/
#include <jni.h>

#ifndef __HAPTICJAVAAPI_H__
#define __HAPTICJAVAAPI_H__

/*
 * Class:     com_immersion_Haptic
 * Method:    setAudioHapticEnabled
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_com_immersion_Haptic_setAudioHapticEnabled
  (JNIEnv *, jclass, jboolean enable);

/*
 * Class:     com_immersion_Haptic
 * Method:    isAudioHapticEnabled
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_immersion_Haptic_isAudioHapticEnabled
  (JNIEnv *, jclass);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Class:     com_immersion_Haptic
 * Method:    initialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_immersion_Haptic_initialize
  (JNIEnv *, jclass);

/*
 * Class:     com_immersion_Haptic
 * Method:    terminate
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_immersion_Haptic_terminate
  (JNIEnv *, jclass);

/*
 * Class:     com_immersion_Haptic
 * Method:    playEffect
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_immersion_Haptic_playEffect
  (JNIEnv *, jclass, jint);

/*
 * Class:     com_immersion_Haptic
 * Method:    playTimedEffect
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_immersion_Haptic_playTimedEffect
  (JNIEnv *, jclass, jint);

/*
 * Class:     com_immersion_Haptic
 * Method:    playEffectSequence
 * Signature: ([BII)V
 */
JNIEXPORT void JNICALL Java_com_immersion_Haptic_playEffectSequence
  (JNIEnv *, jclass, jbyteArray, jint);

/*
 * Class:     com_immersion_Haptic
 * Method:    stopPlayingEffect
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_immersion_Haptic_stopPlayingEffect
  (JNIEnv *, jclass);

/*
 * Class:     com_immersion_Haptic
 * Method:    runDiagnostic
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_immersion_Haptic_runDiagnostic
  (JNIEnv *, jclass);

/*
 * Class:     com_immersion_Haptic
 * Method:    getChipRevision
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_immersion_Haptic_getChipRevision
  (JNIEnv *, jclass);

/*
 * Class:     com_immersion_Haptic
 * Method:    getDeviceID
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_immersion_Haptic_getDeviceID
  (JNIEnv *, jclass);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __HAPTICJAVAAPI_H__ */

