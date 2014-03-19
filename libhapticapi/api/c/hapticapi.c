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
**     hapticapi.c
**
** Description:
**     C API for DRV2605
**
** =============================================================================
*/

/**
 * \file
 *
 * This file contains the implementation of the Haptic C API.
 *
 */

/* Haptic API/Transport includes */
#include <hapticapi.h>
#include <haptictransport.h>

/* OS includes */
#include <stdlib.h>     /* for malloc/free */
#include <string.h>     /* for memcpy, etc. */

#include <drv2605.h>
#include <android/log.h>
/*
** -----------------------------------------------------------------------------
** Private Constants
** -----------------------------------------------------------------------------
*/

/* API States */
#define HAPTIC_STATE_TERMINATING                3
#define HAPTIC_STATE_INITIALIZING               2
#define HAPTIC_STATE_INITIALIZED                1
#define HAPTIC_STATE_NOT_INITIALIZED            0

/* Chip Type */
#define TYPE_DRV2605 5
#define TYPE_DRV2604 4

/* Diagnostic Result */
#define DIAGNOSTIC_PASSED 0
#define DIAGNOSTIC_FAILED 1

/*
** -----------------------------------------------------------------------------
** Private Variables
** -----------------------------------------------------------------------------
*/

/* API state. */
static HapticInt            sAPIState           = HAPTIC_STATE_NOT_INITIALIZED;

/* Inter-thread sychronization mutex. */
static HapticThreadMutex    sAPIMutex           = HAPTIC_INVALID_THREAD_MUTEX;

/*
** -----------------------------------------------------------------------------
** Private Functions
** -----------------------------------------------------------------------------
*/

HapticResult ImHapticLockAPI();
HapticResult ImHapticUnlockAPI();

/*
** -----------------------------------------------------------------------------
** Public Functions
** -----------------------------------------------------------------------------
*/

HapticResult HAPTICAPI_CALL ImHapticInitialize()
{
    HapticResult    retVal;     /* Initialized below. */
    HapticResult    retVal2;    /* Initialized below. */

    /*
    ** Check if the API is already initialized and make sure only one thread
    ** initializes it at a time.
    */
    retVal = ImTransportTestAndSetInt(&sAPIState,
                                      HAPTIC_STATE_INITIALIZING,
                                      HAPTIC_STATE_NOT_INITIALIZED);
    if (HAPTIC_FAILED(retVal))
    {
        return retVal;
    }
    if (HAPTIC_S_TRUE != retVal)
    {
        return HAPTIC_E_ALREADY_INITIALIZED;
    }

    /*
    ** Connect to the haptic hardware and establish communication with the
    ** firmware.
    */
    retVal = ImTransportOpen();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        /*
        ** Create a mutex to synchronize the API from threads within this
        ** process.
        */
        retVal = ImTransportCreateThreadMutex(&sAPIMutex);
        if (HAPTIC_SUCCEEDED(retVal))
        {
            /*
            ** If the Transport succeeded in creating the mutex, it must be
            ** valid.
             */
            if (HAPTIC_INVALID_THREAD_MUTEX == sAPIMutex)
            {
                HAPTIC_ASSERT(0);
                retVal = HAPTIC_E_INTERNAL;
            }
        }

        /* Clean up on failure. */
        if (HAPTIC_FAILED(retVal))
        {
            ImTransportClose();
        }
    }

    /*
    ** Update the API state according to whether the initialization
    ** succeeded.
    */
    retVal2 = ImTransportTestAndSetInt(&sAPIState,
                                       HAPTIC_SUCCEEDED(retVal)
                                       ? HAPTIC_STATE_INITIALIZED
                                       : HAPTIC_STATE_NOT_INITIALIZED,
                                       HAPTIC_STATE_INITIALIZING);
    if (HAPTIC_S_FALSE == retVal2)
    {
        HAPTIC_ASSERT(0);
        retVal2 = HAPTIC_E_INTERNAL;
    }
    /* Don't overwrite previous error. */
    if ((HAPTIC_S_TRUE != retVal2) && HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}


HapticResult HAPTICAPI_CALL ImHapticTerminate()
{
    HapticResult    retVal;     /* Initialized below. */
    HapticResult    retVal2;    /* Initialized below. */

    /*
    ** Check if the API is not yet initialized and make sure only one thread
    ** terminates the API at a time.
    */
    retVal = ImTransportTestAndSetInt(&sAPIState,
                                      HAPTIC_STATE_TERMINATING,
                                      HAPTIC_STATE_INITIALIZED);
    if (HAPTIC_FAILED(retVal))
    {
        return retVal;
    }
    if (HAPTIC_S_TRUE != retVal)
    {
        return HAPTIC_E_NOT_INITIALIZED;
    }

    /*
    ** If there is a failure, continue trying to terminate the rest of the
    ** API and return the first error code.
    */

    /* Lock the API from other threads of the same process. */
    retVal = ImTransportAcquireThreadMutex(sAPIMutex);
    if (HAPTIC_SUCCEEDED(retVal))
    {
        /*
        ** Don't stop any currently playing effect. Leave it to the application
        ** to explictly stop playing an effect before terminating if necessary.
        ** One advantages of not stopping an effect before terminating is that
        ** a terminating application does not interfere with another application
        ** that may be playing effects.
        */

        /* Disconnect from the haptic hardware. */
        retVal2 = ImTransportClose();
        /* Don't overwrite previous error. */
        if (HAPTIC_SUCCEEDED(retVal))
        {
            retVal = retVal2;
        }

        /*
        ** Destroy the mutex used to synchronize the API from threads within this
        ** process.
        */
        retVal2 = ImTransportDestroyThreadMutex(sAPIMutex);
        sAPIMutex = HAPTIC_INVALID_THREAD_MUTEX;
        /* Don't overwrite previous error. */
        if (HAPTIC_SUCCEEDED(retVal))
        {
            retVal = retVal2;
        }

        /*
        ** Set the API state to HAPTIC_NOT_INITIALIZED even if there was an
        ** error. We don't want to leave the API in a half-initialized state.
        */
        retVal2 = ImTransportTestAndSetInt(&sAPIState,
                                           HAPTIC_STATE_NOT_INITIALIZED,
                                           HAPTIC_STATE_TERMINATING);
        if (HAPTIC_S_FALSE == retVal2)
        {
            HAPTIC_ASSERT(0);
            retVal2 = HAPTIC_E_INTERNAL;
        }
        /* Don't overwrite previous error. */
        if ((HAPTIC_S_TRUE != retVal2) && HAPTIC_SUCCEEDED(retVal))
        {
            retVal = retVal2;
        }
    }

    return retVal;
}


HapticResult HAPTICAPI_CALL ImHapticPlayEffect(HapticInt effectIndex)
{

    /*
    ** Check argument.
    ** Make sure effectIndex < number of effects defined in the FW.
    */
    if ((effectIndex < 0) || (effectIndex > HAPTIC_EFFECT_INDEX_MAX))
    {
        return HAPTIC_E_ARGUMENT;
    }

    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[2] = { HAPTIC_CMDID_PLAY_SINGLE_EFFECT, 0xFF & effectIndex };
    retVal = ImTransportWrite(cmd, 2);

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}

HapticResult HAPTICAPI_CALL ImHapticPlayTimedEffect(HapticInt effectDuration)
{

    /*
    ** Check argument.
    */
    if ((0 > effectDuration) || (HAPTIC_TIMED_EFFECT_DURATION_MS_MAX < effectDuration))
    {
        return HAPTIC_E_ARGUMENT;
    }

    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[3] = { 0 };
    cmd[0] = HAPTIC_CMDID_PLAY_TIMED_EFFECT;
    cmd[1] = 0xFF & effectDuration;
    effectDuration >>= 8;
    cmd[2] = 0xFF & effectDuration;

    retVal += ImTransportWrite(cmd, sizeof(cmd));

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}


HapticResult HAPTICAPI_CALL ImHapticPlayEffectSequence(
    const HapticUInt8*  buffer,
    HapticSize          bufferSize)
{
    HapticSize      i;      /* Initialized below. */
    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    /*
    ** Check arguments.
    ** Make sure buffer is non-NULL, buffer is not too large, and repeat count
    ** is valid.
    */
    if (!buffer
        || (0 == bufferSize)
        || (HAPTIC_PLAY_EFFECT_SEQUENCE_BUFFER_SIZE_MAX < bufferSize))
    {
        return HAPTIC_E_ARGUMENT;
    }

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    // TODO: implement using ioctl
    char *data = (char*) calloc(bufferSize + 1, sizeof(char));
    data[0] = HAPTIC_CMDID_PLAY_EFFECT_SEQUENCE;
    memcpy(&data[1], buffer, bufferSize);

    retVal = ImTransportWrite(data, bufferSize + 1);
    free(data);

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}

HapticResult HAPTICAPI_CALL ImHapticGetDevID(int *devid)
{
    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[] = { HAPTIC_CMDID_GET_DEV_ID };

    retVal = ImTransportWrite(cmd, sizeof(cmd));
    ImTransportRead(cmd, 1);

    if (devid)
        *devid = (cmd[0] >> 4);

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}

HapticResult HAPTICAPI_CALL ImHapticGetChipRev(int *chip_rev)
{
    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[] = { HAPTIC_CMDID_GET_DEV_ID };

    retVal = ImTransportWrite(cmd, sizeof(cmd));
    ImTransportRead(cmd, 1);

    if (chip_rev)
    {
        *chip_rev = (cmd[0] & SILICON_REVISION_MASK);
    }

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}

HapticResult HAPTICAPI_CALL ImHapticRunDiagnostic(int *result)
{
    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[] = { HAPTIC_CMDID_RUN_DIAG };

    retVal = ImTransportWrite(cmd, sizeof(cmd));
    ImTransportRead(cmd, sizeof(cmd));

    if (result)
        *result = cmd[0];

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}


HapticResult HAPTICAPI_CALL ImHapticStopPlayingEffect(void)
{
    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[] = { HAPTIC_CMDID_STOP };

    retVal = ImTransportWrite(cmd, sizeof(cmd));

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}

HapticResult HAPTICAPI_CALL ImAudioHapticEnable()
{
    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[] = { HAPTIC_CMDID_AUDIOHAPTIC_ENABLE };

    retVal = ImTransportWrite(cmd, sizeof(cmd));

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}

HapticResult HAPTICAPI_CALL ImAudioHapticDisable()
{
    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2; /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[] = { HAPTIC_CMDID_AUDIOHAPTIC_DISABLE };
    retVal = ImTransportWrite(cmd, sizeof(cmd));

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}

HapticResult HAPTICAPI_CALL ImAudioHapticGetStatus(int *audioHapticStatus)
{
    HapticResult    retVal; /* Initialized below. */
    HapticResult    retVal2;    /* Initialized below. */

    if (HAPTIC_FAILED(retVal = ImHapticLockAPI()))
    {
        return retVal;
    }

    char cmd[] = { HAPTIC_CMDID_AUDIOHAPTIC_GETSTATUS };

    retVal = ImTransportWrite(cmd, sizeof(cmd));
    ImTransportRead(cmd, 1);

    if (audioHapticStatus)
        *audioHapticStatus = cmd[0];

    /* Unlock the API. */
    retVal2 = ImHapticUnlockAPI();
    if (HAPTIC_SUCCEEDED(retVal))
    {
        retVal = retVal2;
    }

    return retVal;
}

HapticResult ImHapticLockAPI()
{
    HapticResult    retVal; /* Initialized below. */
    /* Make sure the API is initialized. */
    retVal = ImTransportTestInt(&sAPIState, HAPTIC_STATE_INITIALIZED);
    if (HAPTIC_FAILED(retVal))
    {
        return retVal;
    }
    if (HAPTIC_S_TRUE != retVal)
    {
        return HAPTIC_E_NOT_INITIALIZED;
    }

    /* Lock the API from other threads of the same process. */
    retVal = ImTransportAcquireThreadMutex(sAPIMutex);
    if (HAPTIC_SUCCEEDED(retVal))
    {
        /* Make sure the API is still initialized. */
        retVal = ImTransportTestInt(&sAPIState, HAPTIC_STATE_INITIALIZED);
        if(HAPTIC_FAILED(retVal))
        {
            /* Unlock the API. */
            ImHapticUnlockAPI();
        }
    }
    return retVal;
}

HapticResult ImHapticUnlockAPI()
{
    /* Unlock the API. */
    return ImTransportReleaseThreadMutex(sAPIMutex);
}
