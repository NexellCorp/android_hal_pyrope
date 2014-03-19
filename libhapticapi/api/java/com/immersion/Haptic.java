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
**     Haptic.java
**
** Description:
**     Haptic Java API wrapper class for Android.
**
** =============================================================================
*/

package com.immersion;

/**
 * Haptic Java API.
 */
public class Haptic
{
    static
    {
         System.loadLibrary("hapticapi");
         System.loadLibrary("hapticjavaapi");
    }

    /**
     * Maximum effect index for {@link #playEffect playEffect} and in buffer
     * passed to {@link #playEffectSequence playEffectSequence}.
     */
    public static final int HAPTIC_EFFECT_INDEX_MAX = 254;

    /**
     * Maximum inter-effect delay in multiples of 5 milliseconds in buffer passed to
     * {@link #playEffectSequence playEffectSequence}.
     */
    public static final int HAPTIC_INTER_EFFECT_DELAY_10MS_MAX = 127;

    /**
     * Maximum size of buffer passed to
     * {@link #playEffectSequence playEffectSequence}.
     */
    public static final int HAPTIC_PLAY_EFFECT_SEQUENCE_BUFFER_SIZE_MAX = 8;

    /**
     * Maximum effect duration passed to
     * {@link #playTimedEffect playTimedEffect}.
     */
    public static final int HAPTIC_TIMED_EFFECT_DURATION_MS_MAX = 10000;

    /**
     * Device identifier
     */
    public static final int DEVICE_DRV2605 = 5;
    public static final int DEVICE_DRV2604 = 4;

    /**
     * Revision mask
     */
    public static final int DEVICE_REVISION_MASK = 0x07;

    /**
     * Initializes the Haptic Java API.
     * <p>
     * {@link #initialize initialize} establishes communication with the haptic
     * hardware.
     * <p>
     * Applications must call {@link #initialize initialize}, typically during
     * application initialization, before calling the other functions of the
     * Haptic Java API. If an application calls other functions of the API
     * before calling {@link #initialize initialize}, those functions throw
     * a RuntimeException.
     *
     * @throws  RuntimeException
     *              The detail message of the exception may consist of the
     *              following values:
     *              <dl>
     *                  <dt>"HAPTIC_E_FAIL"</dt>
     *                  <dd>General error not covered by more specific codes.
     *                      Possible causes include:
     *                      <ul>
     *                          <li>Unable to find or establish communication
     *                              with the haptic hardware.</li>
     *                      </ul>
     *                  </dd>
     *                  <dt>"HAPTIC_E_INTERNAL"</dt>
     *                  <dd>Internal error. Should not arise. Possibly due to a
     *                      programming error in the Haptic Transport or Haptic
     *                      C API.</dd>
     *                  <dt>"HAPTIC_E_SYSTEM"</dt>
     *                  <dd>System error. A system call returned an error.</dd>
     *                  <dt>"HAPTIC_E_MEMORY"</dt>
     *                  <dd>Memory allocation error. Unable to allocate required
     *                      memory.</dd>
     *                  <dt>"HAPTIC_E_ALREADY_INITIALIZED"</dt>
     *                  <dd>The application has already called
     *                      {@link #initialize initialize} without a subsequent
     *                      call to {@link #terminate terminate}. Applications
     *                      may call {@link #initialize initialize} more than
     *                      once if they also call {@link #terminate terminate}
     *                      in between calls to
     *                      {@link #initialize initialize}.</dd>
     *              </dl>
     * @see     #terminate terminate
     */
    public static native void initialize();

    /**
     * Terminates the Haptic Java API.
     * <p>
     * {@link #terminate terminate} frees resources such as
     * dynamically-allocated memory, and releases any connection to the haptic
     * hardware.
     * <p>
     * Applications should call {@link #terminate terminate}, typically during
     * application termination, after calling the other functions of the Haptic
     * Java API.
     *
     * @throws  RuntimeException
     *              The detail message of the exception may consist of the
     *              following values:
     *              <dl>
     *                  <dt>"HAPTIC_E_INTERNAL"</dt>
     *                  <dd>Internal error. Should not arise. Possibly due to a
     *                      programming error in the Haptic Transport or Haptic
     *                      C API.</dd>
     *                  <dt>"HAPTIC_E_SYSTEM"</dt>
     *                  <dd>System error. A system call returned an error.</dd>
     *                  <dt>"HAPTIC_E_NOT_INITIALIZED"</dt>
     *                  <dd>The application has not called
     *                      {@link #initialize initialize}. Applications may
     *                      call {@link #initialize initialize} more than once
     *                      if they also call {@link #terminate terminate} in
     *                      between calls to
     *                      {@link #initialize initialize}.</dd>
     *              </dl>
     * @see     #initialize initialize
     */
    public static native void terminate();

    /**
     * Plays a haptic effect.
     *
     * @param   effectIndex Zero-based index of the effect to play. The effect
     *                      index must be between <code>0</code> and
     *                      {@link #HAPTIC_EFFECT_INDEX_MAX}, inclusive.
     * @throws  RuntimeException
     *              The detail message of the exception may consist of the
     *              following values:
     *              <dl>
     *                  <dt>"HAPTIC_E_INTERNAL"</dt>
     *                  <dd>Internal error. Should not arise. Possibly due to a
     *                      programming error in the Haptic Transport or Haptic
     *                      C API.</dd>
     *                  <dt>"HAPTIC_E_SYSTEM"</dt>
     *                  <dd>System error. A system call returned an error.</dd>
     *                  <dt>"HAPTIC_E_ARGUMENT"</dt>
     *                  <dd><code>effectIndex</code> is invalid. For example,
     *                      <ul>
     *                          <li><code>effectIndex</code> is negative</li>
     *                          <li><code>effectIndex</code> is greater than
     *                          {@link #HAPTIC_EFFECT_INDEX_MAX}</li>
     *                      </ul>
     *                  </dd>
     *                  <dt>"HAPTIC_E_NOT_INITIALIZED"</dt>
     *                  <dd>The application has not called
     *                      {@link #initialize initialize}.</dd>
     *              </dl>
     */
    public static native void playEffect(int effectIndex);

    /**
     * Plays a fixed haptic effect for a specified duration.
     *
     * @param   effectDuration
     *                      Duration, in milliseconds, for which to play
     *                      the effect. The effect duration must be between
     *                      <code>0</code> and
     *                      {@link #HAPTIC_TIMED_EFFECT_DURATION_MS_MAX},
     *                      inclusive.
     * @throws  RuntimeException
     *              The detail message of the exception may consist of the
     *              following values:
     *              <dl>
     *                  <dt>"HAPTIC_E_INTERNAL"</dt>
     *                  <dd>Internal error. Should not arise. Possibly due to a
     *                      programming error in the Haptic Transport or Haptic
     *                      C API.</dd>
     *                  <dt>"HAPTIC_E_SYSTEM"</dt>
     *                  <dd>System error. A system call returned an error.</dd>
     *                  <dt>"HAPTIC_E_ARGUMENT"</dt>
     *                  <dd><code>effectDuration</code> is invalid. For example,
     *                      <ul>
     *                          <li><code>effectDuration</code> is negative</li>
     *                          <li><code>effectDuration</code> is greater than
     *                          {@link #HAPTIC_TIMED_EFFECT_DURATION_MS_MAX}</li>
     *                      </ul>
     *                  </dd>
     *                  <dt>"HAPTIC_E_NOT_INITIALIZED"</dt>
     *                  <dd>The application has not called
     *                      {@link #initialize initialize}.</dd>
     *              </dl>
     */
    public static native void playTimedEffect(int effectDuration);

    /**
     * Plays a sequence of haptic effects.
     *
     * @param   buffer      Buffer containing one or more effect index,
     *                      inter-effect delay pairs. In detail,
     *                      <ul>
     *                          <li><code>buffer[0]</code> specifies the index
     *                              of the first effect</li>
     *                          <li><code>buffer[1]</code> specifies the delay
     *                              in multiples of 5 milliseconds between the
     *                              end of the first effect and the start of the
     *                              second effect</li>
     *                          <li><code>buffer[2]</code> specifies the index
     *                              of the second effect</li>
     *                          <li><code>buffer[3]</code> specifies the delay
     *                              in multiples of 5 milliseconds between the
     *                              end of the second effect and the start of
     *                              the third effect</li>
     *                          <li>and so on, up to a maximum of buffer size of
     *                              {@link #HAPTIC_PLAY_EFFECT_SEQUENCE_BUFFER_SIZE_MAX},
     *                              which allows up to <code>16</code> effects
     *                              to be sequenced. The last inter-effect delay
     *                              is optional and defaults to <code>0</code>.
     *                      </ul>
     * @param   bufferSize  Number of bytes in the buffer.
     * @param   repeat      Number of times to repeat the effect sequence. A
     *                      value of <code>0</code> causes the effect sequence
     *                      to play once. A value of <code>1</code> causes the
     *                      effect sequence to play twice. And so on. The
     *                      inter-effect delay specified for the last effect in
     *                      the sequence is used between the last and first
     *                      effects when the sequence is repeated.
     * @throws  RuntimeException
     *              The detail message of the exception may consist of the
     *              following values:
     *              <dl>
     *                  <dt>"HAPTIC_E_INTERNAL"</dt>
     *                  <dd>Internal error. Should not arise. Possibly due to a
     *                      programming error in the Haptic Transport or Haptic
     *                      C API.</dd>
     *                  <dt>"HAPTIC_E_SYSTEM"</dt>
     *                  <dd>System error. A system call returned an error.</dd>
     *                  <dt>"HAPTIC_E_ARGUMENT"</dt>
     *                  <dd><code>effectIndex</code> is invalid. For example,
     *                      <ul>
     *                          <li><code>buffer</code> is
     *                              <code>NULL</code></li>
     *                          <li><code>bufferSize</code> is <code>0</code> or
     *                              greater than
     *                              {@link #HAPTIC_PLAY_EFFECT_SEQUENCE_BUFFER_SIZE_MAX}</li>
     *                          <li><code>buffer</code> contains an effect index
     *                              that is greater than
     *                              {@link #HAPTIC_EFFECT_INDEX_MAX}</li>
     *                          <li><code>buffer</code> contains an inter-effect
     *                              delay that is greater than
     *                              {@link #HAPTIC_INTER_EFFECT_DELAY_10MS_MAX}</li>
     *                      </ul>
     *                  </dd>
     *                  <dt>"HAPTIC_E_NOT_INITIALIZED"</dt>
     *                  <dd>The application has not called
     *                      {@link #initialize initialize}.</dd>
     *              </dl>
     */
    public static native void playEffectSequence(byte buffer[], int bufferSize);

    /**
     * Stops playing a haptic effect if one is playing.
     * <p>
     * It is not an error to call {@link #stopPlayingEffect stopPlayingEffect}
     * when an effect is not playing.
     *
     * @throws  RuntimeException
     *              The detail message of the exception may consist of the
     *              following values:
     *              <dl>
     *                  <dt>"HAPTIC_E_INTERNAL"</dt>
     *                  <dd>Internal error. Should not arise. Possibly due to a
     *                      programming error in the Haptic Transport or Haptic
     *                      C API.</dd>
     *                  <dt>"HAPTIC_E_SYSTEM"</dt>
     *                  <dd>System error. A system call returned an error.</dd>
     *                  <dt>"HAPTIC_E_NOT_INITIALIZED"</dt>
     *                  <dd>The application has not called
     *                      {@link #initialize initialize}.</dd>
     *              </dl>
     */
    public static native void stopPlayingEffect();

    /**
     * Returns the current device identifier
     */
    public static native int getDeviceID();

    /**
     * Returns the revision number of the chip
     */
    public static native int getChipRevision();

    /**
     * Returns diagnostic and returns the result
     * Returns 0 if diagnostic succeeds, 1 if fails
     */
    public static native int runDiagnostic();

    /**
     * Enables and disables audio-to-haptic feature of the chip
     *
     * @param   enable  Flag indicating whether to enable or disable the feature
     *
     * @throws  RuntimeException
     *              The detail message of the exception may consist of the
     *              following values:
     *              <dl>
     *                  <dt>"HAPTIC_E_INTERNAL"</dt>
     *                  <dd>Internal error. Should not arise. Possibly due to a
     *                      programming error in the Haptic Transport or Haptic
     *                      C API.</dd>
     *                  <dt>"HAPTIC_E_SYSTEM"</dt>
     *                  <dd>System error. A system call returned an error.</dd>
     *                  <dt>"HAPTIC_E_NOT_INITIALIZED"</dt>
     *                  <dd>The application has not called
     *                      {@link #initialize initialize}.</dd>
     *              </dl>
     */
    public static native void setAudioHapticEnabled(boolean enable);

    /**
     * Checks whether audio-to-haptic feature of the chip is enabled
     *
     * @return   true if audio-to-haptic is enabled, false otherwise
     *
     * @throws  RuntimeException
     *              The detail message of the exception may consist of the
     *              following values:
     *              <dl>
     *                  <dt>"HAPTIC_E_INTERNAL"</dt>
     *                  <dd>Internal error. Should not arise. Possibly due to a
     *                      programming error in the Haptic Transport or Haptic
     *                      C API.</dd>
     *                  <dt>"HAPTIC_E_SYSTEM"</dt>
     *                  <dd>System error. A system call returned an error.</dd>
     *                  <dt>"HAPTIC_E_NOT_INITIALIZED"</dt>
     *                  <dd>The application has not called
     *                      {@link #initialize initialize}.</dd>
     *              </dl>
     */
    public static native boolean isAudioHapticEnabled();
}
