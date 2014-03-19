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
**     hapticjavaapi.c
**
** Description:
**    Native functions for TS2200 haptic shim package for Android
**
** =========================================================================
*/

/* OS Includes */
#include <stdlib.h>
#include <stdio.h>

/* Haptic Includes */
#include "hapticapi.h"
#include "hapticjavaapi.h"

/*
** -----------------------------------------------------------------------------
** Private Constants
** -----------------------------------------------------------------------------
*/

#define MAX_ERROR_STRING_LEN          64

/*
** -----------------------------------------------------------------------------
** Private Data
** -----------------------------------------------------------------------------
*/

static char g_szExceptionDesc[MAX_ERROR_STRING_LEN];

/*
** -----------------------------------------------------------------------------
** Private Functions
** -----------------------------------------------------------------------------
*/

static void JNI_ThrowByName(JNIEnv *env, const char *name, const char *msg)
{
    jclass cls = (*env)->FindClass(env, name);
    /* if cls is NULL, an exception has already been thrown */
    if (cls != NULL) {
        (*env)->ThrowNew(env, cls, msg);
    }
    /* free the local ref */
    (*env)->DeleteLocalRef(env, cls);
}

static void GetErrorString(HapticResult result, char *szErr, int iSize)
{
    switch (result)
    {
       case HAPTIC_E_FAIL:                 strncpy(szErr, "HAPTIC_E_FAIL", iSize); break;
       case HAPTIC_E_INTERNAL:             strncpy(szErr, "HAPTIC_E_INTERNAL", iSize); break;
       case HAPTIC_E_SYSTEM:               strncpy(szErr, "HAPTIC_E_SYSTEM", iSize); break;
       case HAPTIC_E_MEMORY:               strncpy(szErr, "HAPTIC_E_MEMORY", iSize); break;
       case HAPTIC_E_ARGUMENT:             strncpy(szErr, "HAPTIC_E_ARGUMENT", iSize); break;
       case HAPTIC_E_ALREADY_INITIALIZED:  strncpy(szErr, "HAPTIC_E_ALREADY_INITIALIZED", iSize); break;
       case HAPTIC_E_NOT_INITIALIZED:      strncpy(szErr, "HAPTIC_E_NOT_INITIALIZED", iSize); break;
       case HAPTIC_E_UNSUPPORTED:          strncpy(szErr, "HAPTIC_E_UNSUPPORTED", iSize); break;
       default:                            strncpy(szErr, "Unknown Error", iSize);
    }
}

/*
** -----------------------------------------------------------------------------
** Private Macros
** -----------------------------------------------------------------------------
*/

#define HAPTIC_START_TRY_BLOCK        do {
#define HAPTIC_END_TRY_BLOCK          } while (0);

#define HAPTIC_THROW_EXCEPTION(env, result) \
        GetErrorString(result, g_szExceptionDesc, MAX_ERROR_STRING_LEN); \
        JNI_ThrowByName(env, "java/lang/RuntimeException", g_szExceptionDesc); \
        break;

#define HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result) \
    if (HAPTIC_FAILED(result)) \
    { \
        HAPTIC_THROW_EXCEPTION(env, result) \
    }

/*
** -----------------------------------------------------------------------------
** Functions
** -----------------------------------------------------------------------------
*/

/*
    private native void initialize();
*/
void  Java_com_immersion_Haptic_initialize(JNIEnv *env, jclass cls)
{
    HapticResult result = HAPTIC_E_FAIL;

    HAPTIC_START_TRY_BLOCK

    result = ImHapticInitialize();

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK
}

/*
    private native void terminate();
*/
void  Java_com_immersion_Haptic_terminate(JNIEnv *env, jclass cls)
{
    HapticResult result = HAPTIC_E_FAIL;

    HAPTIC_START_TRY_BLOCK

    result = ImHapticTerminate();

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK
}

/*
    private native void Java_com_immersion_Haptic_playEffect(int nEffectIndex);
*/
void Java_com_immersion_Haptic_playEffect(JNIEnv *env, jclass cls, jint nEffectIndex)
{
    HapticResult result = HAPTIC_E_FAIL;

    HAPTIC_START_TRY_BLOCK

    result = ImHapticPlayEffect(nEffectIndex);

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK
}

/*
    private native void Java_com_immersion_Haptic_playTimedEffect(int nEffectDuration);
*/
void Java_com_immersion_Haptic_playTimedEffect(JNIEnv *env, jclass cls, jint nEffectDuration)
{
    HapticResult result = HAPTIC_E_FAIL;

    HAPTIC_START_TRY_BLOCK

    result = ImHapticPlayTimedEffect(nEffectDuration);

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK
}

/*
    private native void Java_com_immersion_Haptic_playEffectSequence(byte effectIndexes[], int nBufferSize);
*/
void Java_com_immersion_Haptic_playEffectSequence(JNIEnv *env, jclass cls, jbyteArray pEffectIndexes, jint nBufferSize)
{
    HapticResult result = HAPTIC_E_FAIL;
    jbyte        *pByteArray;
    jboolean     iscopy;

    pByteArray = (*env)->GetByteArrayElements(env, pEffectIndexes, &iscopy);

    HAPTIC_START_TRY_BLOCK

    result = ImHapticPlayEffectSequence(pByteArray, nBufferSize);

    (*env)->ReleaseByteArrayElements(env, pEffectIndexes, pByteArray, 0);

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK
}

/*
    private native void Java_com_immersion_Haptic_stopPlayingEffect();
*/
void Java_com_immersion_Haptic_stopPlayingEffect(JNIEnv *env, jclass cls)
{
    HapticResult result = HAPTIC_E_FAIL;

    HAPTIC_START_TRY_BLOCK

    result = ImHapticStopPlayingEffect();

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK
}

/*
    public native void Java_com_immersion_Haptic_getDeviceID();
*/
jint Java_com_immersion_Haptic_getDeviceID(JNIEnv *env, jclass cls)
{
    HapticResult result = HAPTIC_E_FAIL;
    int dev_id;

    HAPTIC_START_TRY_BLOCK

    result = ImHapticGetDevID(&dev_id);

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK

    return dev_id;
}

/*
    public native void Java_com_immersion_Haptic_getChipRevision();
*/
jint Java_com_immersion_Haptic_getChipRevision(JNIEnv *env, jclass cls)
{
    HapticResult result = HAPTIC_E_FAIL;
    int chip_rev;

    HAPTIC_START_TRY_BLOCK

    result = ImHapticGetChipRev(&chip_rev);

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK

    return chip_rev;
}

/*
    public native void Java_com_immersion_Haptic_runDiagnostic();
*/
jint Java_com_immersion_Haptic_runDiagnostic(JNIEnv *env, jclass cls)
{
    HapticResult result = HAPTIC_E_FAIL;
    int diag_result;

    HAPTIC_START_TRY_BLOCK

    result = ImHapticRunDiagnostic(&diag_result);

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK

    return diag_result;
}

/*
    public native void Java_com_immersion_Haptic_setAudioHapticEnabled(boolean enable);
*/
void Java_com_immersion_Haptic_setAudioHapticEnabled(JNIEnv * env, jclass cls, jboolean enable)
{
    HapticResult result = HAPTIC_E_FAIL;

    HAPTIC_START_TRY_BLOCK

    if(enable)
    {
        result = ImAudioHapticEnable();
    }
    else
    {
        result = ImAudioHapticDisable();
    }

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK
}

jboolean JNICALL Java_com_immersion_Haptic_isAudioHapticEnabled(JNIEnv * env, jclass class)
{
    HapticResult result = HAPTIC_E_FAIL;
    int status_result;

    HAPTIC_START_TRY_BLOCK

    result = ImAudioHapticGetStatus(&status_result);

    HAPTIC_THROW_EXCEPTION_ON_ERROR(env, result);
    HAPTIC_END_TRY_BLOCK

    if(status_result) return JNI_TRUE;
    return JNI_FALSE;
}
