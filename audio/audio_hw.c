/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "audio_hw_primary"
/*#define LOG_NDEBUG 0*/

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <audio_utils/resampler.h>
#include <tinyalsa/asoundlib.h>
#include "audio_route.h"

#if	(0)
#define DLOGI(msg...)	ALOGI(msg)
#else
#define DLOGI(msg...)	do { } while(0)
#endif

//#define	DUMP_PLAYBACK
#ifdef  DUMP_PLAYBACK
#define DUMP_PLYA_ENABLE	"/data/pcm/play"
#define DUMP_PLYA_PATH		"/data/pcm/play.wav"
#endif

#define PCM_DEVICE 0
#define PCM_DEVICE_DEEP 1

#define MIXER_CARD 0

/* duration in ms of volume ramp applied when starting capture to remove plop */
#define CAPTURE_START_RAMP_MS 100

/* maximum number of channel mask configurations supported. Currently the primary
 * output only supports 1 (stereo) and the multi channel HDMI output 2 (5.1 and 7.1) */
#define MAX_SUPPORTED_CHANNEL_MASKS 2

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

enum output_type {
    OUTPUT_DEEP_BUF,      // deep PCM buffers output stream
    OUTPUT_LOW_LATENCY,   // low latency output stream
    OUTPUT_HDMI,          // HDMI multi channel
    OUTPUT_TOTAL
};

struct snd_card_dev {
	const char *name;
	int card;
	int device;
	unsigned int flags;
	unsigned int refcount;
	struct pcm_config config;
};

static struct snd_card_dev pcm_out = {
	.name		= "PCM OUT",
	.card		= 0,
	.device		= 0,
	.flags		= PCM_OUT,
	.config		= {
		.channels		= 2,
		.rate			= 48000,
    	.period_size 	= 1024,
    	.period_count 	= 4,
    	.format 		= PCM_FORMAT_S16_LE,
	},
};

static struct snd_card_dev pcm_in = {
	.name		= "PCM IN",
	.card		= 0,
	.device		= 0,
	.flags		= PCM_IN,
	.config		= {
		.channels		= 2,
		.rate			= 48000,
    	.period_size 	= 1024,
    	.period_count 	= 4,
    	.format 		= PCM_FORMAT_S16_LE,
	},
};

static struct snd_card_dev spdif_out = {
	.name		= "SPDIF OUT",
	.card		= 1,
	.device		= 0,
	.flags		= PCM_OUT,
	.config		= {
		.channels		= 2,
		.rate			= 48000,
    	.period_size 	= 1024,
    	.period_count 	= 4,
    	.format 		= PCM_FORMAT_S16_LE,
	},
};

struct audio_device {
    struct audio_hw_device device;
    audio_devices_t out_device; /* "or" of stream_out.device for all active output streams */
    audio_devices_t in_device;
    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    float voice_volume;
    bool mic_mute;
    struct audio_route *ar;
    audio_source_t input_source;
    int cur_route_id;     /* current route ID: combination of input source
                           * and output device IDs */
    audio_channel_mask_t in_channel_mask;
    struct stream_out *outputs[OUTPUT_TOTAL];
};

struct stream_out {
    struct audio_stream_out stream;
    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct audio_device *dev;
	unsigned int request_rate;
    struct resampler_itfe *resampler;
    struct pcm *pcm;
    struct pcm_config config;
	audio_format_t format;
    unsigned int pcm_device;
    audio_devices_t device;
    /* FIXME: when HDMI output is active, other outputs must be disabled as
     * HDMI and RT5631 share the same I2S. This means that notifications and other sounds are
     * silent when watching a hdmi video. */
    bool disabled;
    audio_channel_mask_t channel_mask;
    bool standby; /* true if all PCMs are inactive */
    char *buffer;
    /* Array of supported channel mask configurations. +1 so that the last entry is always 0 */
    audio_channel_mask_t supported_channel_masks[MAX_SUPPORTED_CHANNEL_MASKS + 1];
    bool muted;
    uint64_t written; /* total frames written, not cleared when entering standby */
	FILE *file;
	/* sound card */
	struct snd_card_dev *card;
};

struct stream_in {
    struct audio_stream_in stream;
    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct audio_device *dev;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider rsmp_buf_provider;
    struct pcm *pcm;
    unsigned int request_rate;
    struct pcm_config config;
	audio_format_t format;
    uint32_t channel_mask;
    bool standby;
    char *buffer;
    size_t frames_in;
    int read_status;
    audio_source_t input_source;
    audio_io_handle_t io_handle;
    audio_devices_t device;
    uint16_t ramp_vol;
    uint16_t ramp_step;
    size_t  ramp_frames;
    FILE *file;
	/* sound card */
	struct snd_card_dev *card;
};

static int stream_in_refcount = 0; //  add refcount for CTS testRecordingAudioInRawFormats

static struct pcm_config *pcm_config_in = NULL;
#define	get_in_pcm_config_gptr(c)		(c = (struct pcm_config *)pcm_config_in)
#define	set_in_pcm_config_gptr(c)		(pcm_config_in = (struct pcm_config *)c)

static unsigned int __pcm_format_to_bits(enum pcm_format format)
{
    switch (format) {
    case PCM_FORMAT_S32_LE:
        return 32;
    case PCM_FORMAT_S8:
        return 8;
    case PCM_FORMAT_S24_LE:
        return 34;
    default:
    case PCM_FORMAT_S16_LE:
        return 16;
    };
}

#define	pcm_format_to_bytes(fmt)		(__pcm_format_to_bits(fmt)/8)

static enum pcm_format pcm_bits_to_format(int bits)
{
    switch (bits) {
    case 32:
    	return PCM_FORMAT_S32_LE;
    case 8:
    	return PCM_FORMAT_S8;
    case 24:
    	return PCM_FORMAT_S24_LE;
    default:
    	return PCM_FORMAT_S16_LE;
    };
}

static audio_format_t pcm_format_to_android(enum pcm_format format)
{
    switch (format) {
    case PCM_FORMAT_S32_LE:
        return AUDIO_FORMAT_PCM_32_BIT;
    case PCM_FORMAT_S8:
        return AUDIO_FORMAT_PCM_8_BIT;
    case PCM_FORMAT_S24_LE:
        return AUDIO_FORMAT_PCM_8_24_BIT;
    default:
    case PCM_FORMAT_S16_LE:
        return AUDIO_FORMAT_PCM_16_BIT;
    };
}

static audio_channel_mask_t	pcm_channels_to_android(int channels)
{
	switch (channels) {
	case 1:	return AUDIO_CHANNEL_OUT_MONO;
	case 4:	return AUDIO_CHANNEL_OUT_QUAD;
	case 6:	return AUDIO_CHANNEL_OUT_5POINT1;
	default:
	case 2:	return AUDIO_CHANNEL_OUT_STEREO;
	}
}

static int pcm_config_setup(struct snd_card_dev *card, struct pcm_config *pcm)
{
    struct pcm_config *config = &card->config;
	struct pcm_params *params;
    unsigned int min;
    unsigned int max;
	unsigned int bits;
	int ret = 0;

	if (! pcm)
		return -EINVAL;

	params = pcm_params_get(card->card, card->device, card->flags);
    if (params == NULL) {
    	ALOGE("%s pcmC%dD%d%s device does not exist.", __FUNCTION__,
        	card->card, card->device, card->flags & PCM_IN ? "c" : "p");
        return -EINVAL;
	}
   	ALOGI("%s %s, card=%p [pcmC%dD%d%s]\n", __FUNCTION__, card->name,
   		card, card->card, card->device, card->flags & PCM_IN ? "c" : "p");

    min = pcm_params_get_min(params, PCM_PARAM_CHANNELS);
    max = pcm_params_get_max(params, PCM_PARAM_CHANNELS);
    pcm->channels = (config->channels >= min && max >= config->channels) ?
    				config->channels : min;
	ALOGI("    Channels:\tmin=%u\t\tmax=%u\t\tch=%u\n", min, max, pcm->channels);

    min = pcm_params_get_min(params, PCM_PARAM_RATE);
    max = pcm_params_get_max(params, PCM_PARAM_RATE);
    pcm->rate = (config->rate >= min && max >= config->rate) ?
    				config->rate : min;
    ALOGI("        Rate:\tmin=%uHz\tmax=%uHz\trate=%uHz\n", min, max, pcm->rate);

    min = pcm_params_get_min(params, PCM_PARAM_PERIOD_SIZE);
    max = pcm_params_get_max(params, PCM_PARAM_PERIOD_SIZE);
    pcm->period_size = (config->period_size >= min && max >= config->period_size) ?
    				config->period_size : min;
    ALOGI(" Period size:\tmin=%u\t\tmax=%u\tsize=%u\n", min, max, pcm->period_size);

    min = pcm_params_get_min(params, PCM_PARAM_PERIODS);
    max = pcm_params_get_max(params, PCM_PARAM_PERIODS);
    pcm->period_count = (config->period_count >= min && max >= config->period_count) ?
    				config->period_count : min;
    ALOGI("Period count:\tmin=%u\t\tmax=%u\t\tcount=%u\n", min, max, pcm->period_count);

	min = pcm_params_get_min(params, PCM_PARAM_SAMPLE_BITS);
    max = pcm_params_get_max(params, PCM_PARAM_SAMPLE_BITS);
	bits = __pcm_format_to_bits(config->format);
	pcm->format = (bits >= min && max >= bits) ?
					pcm_bits_to_format(bits) : pcm_bits_to_format(min);
    ALOGI(" Sample bits:\tmin=%u\t\tmax=%u\t\tbits=%u\n", min, max, __pcm_format_to_bits(pcm->format));

    pcm_params_free(params);
    return ret;
}

#define STRING_TO_ENUM(string) { #string, string }

struct string_to_enum {
    const char *name;
    uint32_t value;
};

const struct string_to_enum out_channels_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
};

enum {
    OUT_DEVICE_SPEAKER,
    OUT_DEVICE_HEADSET,
    OUT_DEVICE_HEADPHONES,
    OUT_DEVICE_SPEAKER_AND_HEADSET,
    OUT_DEVICE_TAB_SIZE,           /* number of rows in route_configs[][] */
    OUT_DEVICE_NONE,
    OUT_DEVICE_CNT
};

enum {
    IN_SOURCE_MIC,
    IN_SOURCE_CAMCORDER,
    IN_SOURCE_VOICE_RECOGNITION,
    IN_SOURCE_VOICE_COMMUNICATION,
    IN_SOURCE_TAB_SIZE,            /* number of lines in route_configs[][] */
    IN_SOURCE_NONE,
    IN_SOURCE_CNT
};


int get_output_device_id(audio_devices_t device)
{
    if (device == AUDIO_DEVICE_NONE)
        return OUT_DEVICE_NONE;

    if (popcount(device) == 2) {
        if ((device == (AUDIO_DEVICE_OUT_SPEAKER |
                       AUDIO_DEVICE_OUT_WIRED_HEADSET)) ||
                (device == (AUDIO_DEVICE_OUT_SPEAKER |
                        AUDIO_DEVICE_OUT_WIRED_HEADPHONE)))
            return OUT_DEVICE_SPEAKER_AND_HEADSET;
        else
            return OUT_DEVICE_NONE;
    }

    if (popcount(device) != 1)
        return OUT_DEVICE_NONE;

    switch (device) {
    case AUDIO_DEVICE_OUT_SPEAKER:
        return OUT_DEVICE_SPEAKER;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        return OUT_DEVICE_HEADSET;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        return OUT_DEVICE_HEADPHONES;
    default:
        return OUT_DEVICE_NONE;
    }
}

int get_input_source_id(audio_source_t source)
{
    switch (source) {
    case AUDIO_SOURCE_DEFAULT:
        return IN_SOURCE_NONE;
    case AUDIO_SOURCE_MIC:
        return IN_SOURCE_MIC;
    case AUDIO_SOURCE_CAMCORDER:
        return IN_SOURCE_CAMCORDER;
    case AUDIO_SOURCE_VOICE_RECOGNITION:
        return IN_SOURCE_VOICE_RECOGNITION;
    case AUDIO_SOURCE_VOICE_COMMUNICATION:
        return IN_SOURCE_VOICE_COMMUNICATION;
    default:
        return IN_SOURCE_NONE;
    }
}

struct route_config {
    const char * const output_route;
    const char * const input_route;
};

const struct route_config media_speaker = {
    "media-speaker",
    "media-main-mic",
};

const struct route_config media_headphones = {
    "media-headphones",
    "media-main-mic",
};

const struct route_config media_headset = {
    "media-headphones",
    "media-headset-mic",
};

const struct route_config camcorder_speaker = {
    "media-speaker",
    "media-second-mic",
};

const struct route_config camcorder_headphones = {
    "media-headphones",
    "media-second-mic",
};

const struct route_config voice_rec_speaker = {
    "voice-rec-speaker",
    "voice-rec-main-mic",
};

const struct route_config voice_rec_headphones = {
    "voice-rec-headphones",
    "voice-rec-main-mic",
};

const struct route_config voice_rec_headset = {
    "voice-rec-headphones",
    "voice-rec-headset-mic",
};

const struct route_config communication_speaker = {
    "communication-speaker",
    "communication-main-mic",
};

const struct route_config communication_headphones = {
    "communication-headphones",
    "communication-main-mic",
};

const struct route_config communication_headset = {
    "communication-headphones",
    "communication-headset-mic",
};

const struct route_config speaker_and_headphones = {
    "speaker-and-headphones",
    "main-mic",
};

const struct route_config * const route_configs[IN_SOURCE_TAB_SIZE]
                                               [OUT_DEVICE_TAB_SIZE] = {
    {   /* IN_SOURCE_MIC */
        &media_speaker,             /* OUT_DEVICE_SPEAKER */
        &media_headset,             /* OUT_DEVICE_HEADSET */
        &media_headphones,          /* OUT_DEVICE_HEADPHONES */
        &speaker_and_headphones     /* OUT_DEVICE_SPEAKER_AND_HEADSET */
    },
    {   /* IN_SOURCE_CAMCORDER */
        &camcorder_speaker,         /* OUT_DEVICE_SPEAKER */
        &camcorder_headphones,      /* OUT_DEVICE_HEADSET */
        &camcorder_headphones,      /* OUT_DEVICE_HEADPHONES */
        &speaker_and_headphones     /* OUT_DEVICE_SPEAKER_AND_HEADSET */
    },
    {   /* IN_SOURCE_VOICE_RECOGNITION */
        &voice_rec_speaker,         /* OUT_DEVICE_SPEAKER */
        &voice_rec_headset,         /* OUT_DEVICE_HEADSET */
        &voice_rec_headphones,      /* OUT_DEVICE_HEADPHONES */
        &speaker_and_headphones     /* OUT_DEVICE_SPEAKER_AND_HEADSET */
    },
    {   /* IN_SOURCE_VOICE_COMMUNICATION */
        &communication_speaker,     /* OUT_DEVICE_SPEAKER */
        &communication_headset,     /* OUT_DEVICE_HEADSET */
        &communication_headphones,  /* OUT_DEVICE_HEADPHONES */
        &speaker_and_headphones     /* OUT_DEVICE_SPEAKER_AND_HEADSET */
    }
};

static int do_output_standby(struct stream_out *out);

/**
 * NOTE: when multiple mutexes have to be acquired, always respect the
 * following order: hw device > in stream > out stream
 */

/* Helper functions */

static void select_devices(struct audio_device *adev)
{
    int output_device_id = get_output_device_id(adev->out_device);
    int input_source_id = get_input_source_id(adev->input_source);
    const char *output_route = NULL;
    const char *input_route = NULL;
    int new_route_id;

    audio_route_reset(adev->ar);

    new_route_id = (1 << (input_source_id + OUT_DEVICE_CNT)) + (1 << output_device_id);
    if (new_route_id == adev->cur_route_id)
        return;
    adev->cur_route_id = new_route_id;

    if (input_source_id != IN_SOURCE_NONE) {
        if (output_device_id != OUT_DEVICE_NONE) {
            input_route =
                    route_configs[input_source_id][output_device_id]->input_route;
            output_route =
                    route_configs[input_source_id][output_device_id]->output_route;
        } else {
            switch (adev->in_device) {
            case AUDIO_DEVICE_IN_WIRED_HEADSET & ~AUDIO_DEVICE_BIT_IN:
                output_device_id = OUT_DEVICE_HEADSET;
                break;
            default:
                output_device_id = OUT_DEVICE_SPEAKER;
                break;
            }
            input_route =
                    route_configs[input_source_id][output_device_id]->input_route;
        }
    } else {
        if (output_device_id != OUT_DEVICE_NONE) {
            output_route =
                    route_configs[IN_SOURCE_MIC][output_device_id]->output_route;
        }
    }

    ALOGV("select_devices() devices %#x input src %d output route %s input route %s",
          adev->out_device, adev->input_source,
          output_route ? output_route : "none",
          input_route ? input_route : "none");

    if (output_route)
        audio_route_apply_path(adev->ar, output_route);
    if (input_route)
        audio_route_apply_path(adev->ar, input_route);

    audio_route_update_mixer(adev->ar);
}


static void force_non_hdmi_out_standby(struct audio_device *adev)
{
    enum output_type type;
    struct stream_out *out;

    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        out = adev->outputs[type];
        if (type == OUTPUT_HDMI || !out)
            continue;
        pthread_mutex_lock(&out->lock);
        do_output_standby(out);
        pthread_mutex_unlock(&out->lock);
    }
}

/* must be called with hw device and output stream mutexes locked */
static int pcm_output_start(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    struct snd_card_dev *card = out->card;
    struct pcm_config *pcm = &out->config;
    int type;

	DLOGI("%s %s pcmC%dD%d%s open ref=%d\n", __FUNCTION__, card->name,
		card->card, card->device, card->flags & PCM_IN ? "c" : "p", card->refcount);

    if (out == adev->outputs[OUTPUT_HDMI]) {
        force_non_hdmi_out_standby(adev);
    } else if (adev->outputs[OUTPUT_HDMI] && !adev->outputs[OUTPUT_HDMI]->standby) {
        out->disabled = true;
        return 0;
    }

    out->disabled = false;

	if (card->refcount)
		return -1;

		out->pcm = pcm_open(card->card, card->device, card->flags, pcm);

        if (out->pcm && !pcm_is_ready(out->pcm)) {
    	ALOGE("%s pcmC%dD%d%s open failed: %s", __FUNCTION__,
        	card->card, card->device, card->flags & PCM_IN ? "c" : "p",
        	pcm_get_error(out->pcm));
            pcm_close(out->pcm);
            return -ENOMEM;
        }

	card->refcount++;

    adev->out_device |= out->device;
    select_devices(adev);

#ifdef	DUMP_PLAYBACK
	FILE *fp = fopen(DUMP_PLYA_ENABLE, "r");
	if (fp) {
		out->file = fopen(DUMP_PLYA_PATH, "wb");
		if (NULL == out->file) {
   			ALOGE("%s pcmC%dD%d%s open failed: %s, ERR=%s", __FUNCTION__,
       			card->card, card->device, card->flags & PCM_IN ? "c" : "p",
       			DUMP_PLYA_PATH, strerror(errno));
			ALOGE("UID:%d, GID:%d, EUID:%d, EGID:%d\n", getuid(), getgid(), geteuid(), getegid());
			ALOGE("ACCESS: R_OK=%s, W_OK=%s, X_OK=%s\n",
				access(DUMP_PLYA_PATH, R_OK)?"X":"O", access(DUMP_PLYA_PATH, W_OK)?"X":"O",
				access(DUMP_PLYA_PATH, X_OK)?"X":"O");
		}
		fclose(fp);
	}
#endif

    return 0;
}

/* must be called with hw device and input stream mutexes locked */
static int pcm_input_start(struct stream_in *in)
{
    struct audio_device *adev = in->dev;
    struct snd_card_dev *card = in->card;
    struct pcm_config *pcm = &in->config;
    int type;

	DLOGI("%s pcmC%dD%d%s open ref=%d\n", __FUNCTION__,
		card->card, card->device, card->flags & PCM_IN ? "c" : "p", card->refcount);

	if (card->refcount)
		return -1;

	in->pcm = pcm_open(card->card, card->device, card->flags, pcm);

	if (in->pcm && ! pcm_is_ready(in->pcm)) {
    	ALOGE("%s pcmC%dD%d%s open failed: %s", __FUNCTION__,
        	card->card, card->device, card->flags & PCM_IN ? "c" : "p",
        	pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        return -ENOMEM;
    }
	card->refcount++;

    /* if no supported sample rate is available, use the resampler */
    if (in->resampler)
        in->resampler->reset(in->resampler);

    in->frames_in = 0;
    adev->input_source = in->input_source;
    adev->in_device = in->device;
    adev->in_channel_mask = in->channel_mask;

    select_devices(adev);

    /* initialize volume ramp */
    in->ramp_frames = (CAPTURE_START_RAMP_MS * in->request_rate) / 1000;
    in->ramp_step = (uint16_t)(USHRT_MAX / in->ramp_frames);
    in->ramp_vol = 0;;

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate, audio_format_t format, int channels)
{
	struct pcm_config *pcm = NULL;
    size_t size;
    size_t device_rate;
    size_t bitperframe = 16;

	get_in_pcm_config_gptr(pcm);

	if (NULL == pcm) {
    	ALOGE("%s not opened input device, set default pcm config !!!", __FUNCTION__);
		struct snd_card_dev *card = &pcm_in;
		int ret = pcm_config_setup(card, &card->config);
		if (ret)
			return 0;
    	pcm = &card->config;
	}

    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames */
	DLOGI("%s (period_size=%d, sample_rate=%d, rate=%d, bitperframe=%d)\n",
		__FUNCTION__, pcm->period_size, sample_rate, pcm->rate, bitperframe);

    size = (pcm->period_size * sample_rate) / pcm->rate;
    size = ((size + (bitperframe-1)) / bitperframe) * bitperframe;

    return size * channels * sizeof(short);
}

static int get_input_next_buffer(struct resampler_buffer_provider *provider,
                                   struct resampler_buffer* buffer)
{
	struct pcm_config *pcm = NULL;
    struct stream_in *in;
    size_t i, frames_size;
	int16_t *pbuffer;

    if (provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct stream_in *)((char *)provider - offsetof(struct stream_in, rsmp_buf_provider));

    if (in->pcm == NULL) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

	get_in_pcm_config_gptr(pcm);

	pbuffer = (int16_t *)in->buffer;
	frames_size = pcm_frames_to_bytes(in->pcm, pcm->period_size);

    if (in->frames_in == 0) {
        in->read_status = pcm_read(in->pcm, (void*)in->buffer, frames_size);
        if (0 != in->read_status) {
            ALOGE("%s pcm_read error status (%d)", __FUNCTION__, in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }

        in->frames_in = pcm->period_size;

        /* Do stereo to mono conversion in place by discarding right channel */
        if (in->channel_mask == AUDIO_CHANNEL_IN_MONO) {
            for (i = 1; i < in->frames_in; i++)
                pbuffer[i] = pbuffer[i * 2];
		}
    }

    buffer->frame_count = (buffer->frame_count > in->frames_in) ? in->frames_in : buffer->frame_count;
    buffer->i16 = pbuffer + (pcm->period_size - in->frames_in) * popcount(in->channel_mask);

    return in->read_status;
}

static void release_input_buffer(struct resampler_buffer_provider *provider,
                                  struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (provider == NULL || buffer == NULL)
        return;

    in = (struct stream_in *)((char *)provider - offsetof(struct stream_in, rsmp_buf_provider));
    in->frames_in -= buffer->frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t pcm_read_frames(struct stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_pos = 0;
    size_t  frame_byte = audio_stream_frame_size(&in->stream.common);

    while (frames_pos < frames) {
        size_t frames_cnt = frames - frames_pos;

        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                    	(int16_t *)((char *)buffer + frames_pos * frame_byte),
                    	&frames_cnt);
        } else {
            struct resampler_buffer buf = {
                    { raw : NULL, },
                    frame_count : frames_cnt,
            };

            get_input_next_buffer(&in->rsmp_buf_provider, &buf);

            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                           frames_pos * frame_byte,
                        buf.raw,
                        buf.frame_count * frame_byte);
                frames_cnt = buf.frame_count;
            }

            release_input_buffer(&in->rsmp_buf_provider, &buf);
        }

        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0)
            return in->read_status;

        frames_pos += frames_cnt;
    }

    return frames_pos;
}

/* API functions */

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct pcm_config *pcm = &out->config;

	DLOGI("%s %s (rate=%d)\n", __FUNCTION__, out->card->name, pcm->rate);
    return pcm->rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
	DLOGI("%s %s (rate=%d)\n", __FUNCTION__,
		((struct stream_out *)stream)->card->name, rate);
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct pcm_config *pcm = &out->config;
	size_t buffer_size = pcm->period_size *
            audio_stream_frame_size((struct audio_stream *)stream);

	DLOGI("%s %s (buffer_size=%d)\n", __FUNCTION__, out->card->name, buffer_size);
    return buffer_size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct pcm_config *pcm = &out->config;
	DLOGI("%s %s (channels=0x%x, mask=0x%x)\n",
		__FUNCTION__, out->card->name, pcm->channels, out->channel_mask);
    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
	struct stream_out *out = (struct stream_out *)stream;
	DLOGI("%s %s (format=0x%x)\n", __FUNCTION__, out->card->name, out->format);
    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
	DLOGI("%s %s (format=0x%x)\n", __FUNCTION__,
		((struct stream_out *)stream)->card->name, format);
    return -ENOSYS;
}

static void out_select_sndcard(struct stream_out *out)
{
	if(out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL){
		out->card = &spdif_out;
	}
	else{
		out->card = &pcm_out;
	}
}

/* Return the set of output devices associated with active streams
 * other than out.  Assumes out is non-NULL and out->dev is locked.
 */
static audio_devices_t output_devices(struct stream_out *out)
{
    struct audio_device *dev = out->dev;
    enum output_type type;
    audio_devices_t devices = AUDIO_DEVICE_NONE;

    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        struct stream_out *other = dev->outputs[type];
        if (other && (other != out) && !other->standby) {
            /* safe to access other stream without a mutex,
             * because we hold the dev lock,
             * which prevents the other stream from being closed
             */
            devices |= other->device;
        }
    }

    return devices;
}

static int do_output_standby(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    int i;
    struct snd_card_dev *card = out->card;
    DLOGI("%s %s ref=%d, standby=%d\n", __FUNCTION__, card->name, card->refcount, out->standby);

    if (!out->standby) {
    	if (1 == card->refcount && out->pcm)
        	pcm_close(out->pcm);
#ifdef	DUMP_PLAYBACK
		if (out->file)
			fclose(out->file);
#endif
       	card->refcount--;
        out->pcm = NULL;
        out->standby = true;

        if (out == adev->outputs[OUTPUT_HDMI]) {
            /* force standby on low latency output stream so that it can reuse HDMI driver if
             * necessary when restarted */
            force_non_hdmi_out_standby(adev);	
        }

        /* re-calculate the set of active devices from other streams */
        adev->out_device = output_devices(out);

        /* Skip resetting the mixer if no output device is active */
        if (adev->out_device)
            select_devices(adev);
    }
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    int status;

    DLOGI("%s %s\n", __FUNCTION__, out->card->name);

    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    status = do_output_standby(out);

    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);

    return status;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
	DLOGI("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    unsigned int val;

	DLOGI("%s (%s)\n", __FUNCTION__, kvpairs);
    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);
    if (ret >= 0) {
        val = atoi(value);
        if ((out->device != val) && (val != 0)) {
            /* Force standby if moving to/from SPDIF or if the output
             * device changes when in SPDIF mode */
            if (((val & AUDIO_DEVICE_OUT_AUX_DIGITAL) ^
                 (adev->out_device & AUDIO_DEVICE_OUT_AUX_DIGITAL)) ||
                (adev->out_device & AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
                do_output_standby(out);
            }

            if (!out->standby && (out == adev->outputs[OUTPUT_HDMI] ||
                    !adev->outputs[OUTPUT_HDMI] ||
                    adev->outputs[OUTPUT_HDMI]->standby)) {
                adev->out_device = output_devices(out) | val;
                select_devices(adev);
            }
            out->device = val;

			DLOGI("%s (%d)\n", __FUNCTION__, out->device);
			out_select_sndcard(out);

			DLOGI("%s val %d\n", __FUNCTION__, val);
        }
    }
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&adev->lock);

    str_parms_destroy(parms);
    return ret;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    size_t i, j;
    int ret;
    bool first = true;

	DLOGI("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);

    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value, sizeof(value));
    if (ret >= 0) {
        value[0] = '\0';
        i = 0;
        /* the last entry in supported_channel_masks[] is always 0 */
        while (out->supported_channel_masks[i] != 0) {
            for (j = 0; j < ARRAY_SIZE(out_channels_name_to_enum_table); j++) {
                if (out_channels_name_to_enum_table[j].value == out->supported_channel_masks[i]) {
                    if (!first) {
                        strcat(value, "|");
                    }
                    strcat(value, out_channels_name_to_enum_table[j].name);
                    first = false;
                    break;
                }
            }
            i++;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value);
        str = str_parms_to_str(reply);
    } else {
        str = strdup(keys);
    }

    str_parms_destroy(query);
    str_parms_destroy(reply);
    return str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct pcm_config *pcm = &out->config;
	uint32_t latency = (pcm->period_size * pcm->period_count * 1000) / pcm->rate;

	DLOGI("%s %s (latency=%d)\n", __FUNCTION__,
		((struct stream_out *)stream)->card->name, latency);
    return latency;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;

	DLOGI("%s %s (left=%f, right=%f)\n", __FUNCTION__,
		((struct stream_out *)stream)->card->name, left, right);

    if (out == adev->outputs[OUTPUT_HDMI]) {
        /* only take left channel into account: the API is for stereo anyway */
        out->muted = (left == 0.0f);
        return 0;
    }
    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret = 0;
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    int i;

    /*
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the output stream mutex - e.g.
     * executing out_set_parameters() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = pcm_output_start(out);
        if (0 > ret) {
            pthread_mutex_unlock(&adev->lock);
            goto err_write;
        }
        out->standby = false;
    }
    pthread_mutex_unlock(&adev->lock);

    if (out->disabled) {
        ret = -EPIPE;
	   	ALOGI("%s, out->disabled error ret = %d\n", __FUNCTION__, ret);
        goto err_write;
    }

    if (out->muted)
        memset((void *)buffer, 0, bytes);

    /* Write to all active PCMs (I2S/SPDIF) */
	if (out->pcm) {
		pcm_write(out->pcm, (void *)buffer, bytes);
	#ifdef	DUMP_PLAYBACK
		if (out->file)
			fwrite(buffer, 1, bytes, out->file);
	#endif
	}

err_write:
    pthread_mutex_unlock(&out->lock);

	/* dummy out */
    if (0 != ret) {
    	ALOGI("%s, %s dealy ref %d standby %s\n",
    		__FUNCTION__, out->card->name, out->card->refcount, out->standby?"true":"false");
        usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
               out_get_sample_rate(&stream->common));
	}

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
	DLOGI("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	DLOGI("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	DLOGI("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return 0;
}

/*
static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
	DLOGI("%s\n", __FUNCTION__);
    return -EINVAL;
}
*/

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                   uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct pcm_config *pcm = &out->config;
    int ret = -1;

    pthread_mutex_lock(&out->lock);

    int i;
    // There is a question how to implement this correctly when there is more than one PCM stream.
    // We are just interested in the frames pending for playback in the kernel buffer here,
    // not the total played since start.  The current behavior should be safe because the
    // cases where both cards are active are marginal.
    if (out->pcm) {
        size_t avail;
        if (pcm_get_htimestamp(out->pcm, &avail, timestamp) == 0) {
            size_t kernel_buffer_size = pcm->period_size * pcm->period_count;
            // FIXME This calculation is incorrect if there is buffering after app processor
            int64_t signed_frames = out->written - kernel_buffer_size + avail;
            // It would be unusual for this value to be negative, but check just in case ...
            if (signed_frames >= 0) {
                *frames = signed_frames;
                ret = 0;
            }
        }
    }

    pthread_mutex_unlock(&out->lock);

    return ret;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
	DLOGI("%s (pcm rate=%d, request=%d)\n", __FUNCTION__, in->config.rate, in->request_rate);
    return in->request_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
	DLOGI("%s (rate=%d)\n", __FUNCTION__, rate);
    return 0;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct pcm_config *pcm = &in->config;
	audio_channel_mask_t channel_mask = in->channel_mask;

	DLOGI("%s (channels=0x%x, mask=0x%x)\n", __FUNCTION__, pcm->channels, channel_mask);
    return channel_mask;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct pcm_config *pcm = &in->config;

	size_t buffer_size = get_input_buffer_size(pcm->rate, in->format,
                                popcount(in->channel_mask));

	DLOGI("%s (buffer_size=%d)\n", __FUNCTION__, buffer_size);
	return buffer_size;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
	struct stream_in *in = (struct stream_in *)stream;
	DLOGI("%s (format=0x%x)\n", __FUNCTION__, in->format);
    return in->format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
	DLOGI("%s (format=0x%x)\n", __FUNCTION__, format);
    return -ENOSYS;
}

static int do_input_standby(struct stream_in *in)
{
    struct audio_device *adev = in->dev;
    struct snd_card_dev *card = in->card;
    DLOGI("%s %s ref=%d, standby=%d\n", __FUNCTION__, card->name, card->refcount, in->standby);

    if (false == in->standby) {
    	if (1 == card->refcount && in->pcm)
        	pcm_close(in->pcm);

        card->refcount--;
        in->pcm = NULL;

        in->dev->input_source = AUDIO_SOURCE_DEFAULT;
        in->dev->in_device = AUDIO_DEVICE_NONE;
        in->dev->in_channel_mask = 0;
        select_devices(adev);
        in->standby = true;
    }
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    int status;

    DLOGI("%s\n", __FUNCTION__);

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);

    status = do_input_standby(in);

    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
	DLOGI("%s\n", __FUNCTION__);
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    unsigned int val;
    bool apply_now = false;

    parms = str_parms_create_str(kvpairs);

	DLOGI("%s\n", __FUNCTION__);

    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&in->lock);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE,
                            value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        /* no audio source uses val == 0 */
        if ((in->input_source != val) && (val != 0)) {
            in->input_source = val;
            apply_now = !in->standby;
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    if (ret >= 0) {
        /* strip AUDIO_DEVICE_BIT_IN to allow bitwise comparisons */
        val = atoi(value) & ~AUDIO_DEVICE_BIT_IN;
        /* no audio device uses val == 0 */
        if ((in->device != val) && (val != 0)) {
            in->device = val;
            apply_now = !in->standby;
        }
    }

    if (apply_now) {
        adev->input_source = in->input_source;
        adev->in_device = in->device;
        select_devices(adev);
    }

    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock);

    str_parms_destroy(parms);
    return ret;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
	DLOGI("%s\n", __FUNCTION__);
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
	DLOGI("%s (gain=%f)\n", __FUNCTION__, gain);
    return 0;
}

static void in_apply_ramp(struct stream_in *in, int16_t *buffer, size_t frames)
{
    size_t i;
    uint16_t vol = in->ramp_vol;
    uint16_t step = in->ramp_step;

    frames = (frames < in->ramp_frames) ? frames : in->ramp_frames;

    if (in->channel_mask == AUDIO_CHANNEL_IN_MONO)
        for (i = 0; i < frames; i++)
        {
            buffer[i] = (int16_t)((buffer[i] * vol) >> 16);
            vol += step;
        }
    else
        for (i = 0; i < frames; i++)
        {
            buffer[2*i] = (int16_t)((buffer[2*i] * vol) >> 16);
            buffer[2*i + 1] = (int16_t)((buffer[2*i + 1] * vol) >> 16);
            vol += step;
        }


    in->ramp_vol = vol;
    in->ramp_frames -= frames;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
   	int ret = 0;
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
	ssize_t frames = bytes / audio_stream_frame_size(&stream->common);

//	DLOGI("%s %s bytes = %d:frames = %d\n", __FUNCTION__, in->card->name, bytes, frames);

    /*
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the input stream mutex - e.g.
     * executing in_set_parameters() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&in->lock);
    if (in->standby) {
        ret = pcm_input_start(in);
        if (0 > ret) {
            pthread_mutex_unlock(&adev->lock);
            goto err_read;
        }
        in->standby = false;
    }
    pthread_mutex_unlock(&adev->lock);

    if (NULL == in->resampler && in->pcm)
        ret = pcm_read(in->pcm, buffer, bytes);
	else
    	ret = pcm_read_frames(in, buffer, frames);

    if (ret > 0)
        ret = 0;

    if (in->ramp_frames > 0)
        in_apply_ramp(in, buffer, frames);

    /*
     * Instead of writing zeroes here, we could trust the hardware
     * to always provide zeroes when muted.
     */
    if (ret == 0 && adev->mic_mute)
        memset(buffer, 0, bytes);

err_read:
    if (0 > ret) {
    	ALOGI("%s, %s dealy ref %d standby %s\n",
    		__FUNCTION__, in->card->name, in->card->refcount, in->standby?"true":"false");
        usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
               in_get_sample_rate(&stream->common));
	}

	pthread_mutex_unlock(&in->lock);
    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
	DLOGI("%s\n", __FUNCTION__);
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream,
                               effect_handle_t effect)
{
    struct stream_in *in = (struct stream_in *)stream;
    effect_descriptor_t descr;

	DLOGI("%s\n", __FUNCTION__);

    if ((*effect)->get_descriptor(effect, &descr) == 0) {

        pthread_mutex_lock(&in->dev->lock);
        pthread_mutex_lock(&in->lock);

        pthread_mutex_unlock(&in->lock);
        pthread_mutex_unlock(&in->dev->lock);
    }

    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    struct stream_in *in = (struct stream_in *)stream;

	DLOGI("%s\n", __FUNCTION__);

    effect_descriptor_t descr;
    if ((*effect)->get_descriptor(effect, &descr) == 0) {

        pthread_mutex_lock(&in->dev->lock);
        pthread_mutex_lock(&in->lock);

        pthread_mutex_unlock(&in->lock);
        pthread_mutex_unlock(&in->dev->lock);
    }

    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int ret;
    enum output_type type;

	DLOGI("*** %s (devices=0x%x, flags=0x%x) ***\n", __FUNCTION__, devices, flags);

    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (!out)
        return -ENOMEM;

    if (devices == AUDIO_DEVICE_NONE)
        devices = AUDIO_DEVICE_OUT_SPEAKER;
    out->device = devices;

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
//  out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
  out->stream.get_presentation_position = out_get_presentation_position;

	// jhkim
	struct snd_card_dev *card = &pcm_out;
	struct pcm_config *pcm = &out->config;

	if ((devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) &&
		(flags & AUDIO_OUTPUT_FLAG_DIRECT)) {
        pthread_mutex_lock(&adev->lock);
        pthread_mutex_unlock(&adev->lock);
		card = &spdif_out;
        out->pcm_device = PCM_DEVICE;
        type = OUTPUT_HDMI;
    } else if (flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER) {
		card = &pcm_out;
        out->pcm_device = PCM_DEVICE_DEEP;
        type = OUTPUT_DEEP_BUF;
    } else {
		card = &pcm_out;
        out->pcm_device = PCM_DEVICE;
        type = OUTPUT_LOW_LATENCY;
    }

	ret = pcm_config_setup(card, pcm);
	if (ret)
		goto err_open;

	out->card = card;
	out->format	= pcm_format_to_android(pcm->format);
	out->channel_mask = pcm_channels_to_android(pcm->channels);

    out->dev = adev;
	out->standby = true;
    /* out->muted = false; by calloc() */
    /* out->written = 0; by calloc() */

	config->sample_rate = pcm->rate;
	config->channel_mask = out->channel_mask;
	config->format = out->format;

    pthread_mutex_lock(&adev->lock);
    if (adev->outputs[type]) {
        pthread_mutex_unlock(&adev->lock);
        ret = -EBUSY;
        goto err_open;
    }
    adev->outputs[type] = out;
    pthread_mutex_unlock(&adev->lock);

    *stream_out = &out->stream;
    DLOGI("--- %s (devices=0x%x, flags=0x%x)\n", __FUNCTION__, devices, flags);
    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct audio_device *adev;
    enum output_type type;

	struct stream_out *out = (struct stream_out *)stream;

	DLOGI("%s %s\n", __FUNCTION__, out->card->name);

    out_standby(&stream->common);
    adev = (struct audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        if (adev->outputs[type] == (struct stream_out *) stream) {
            adev->outputs[type] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&adev->lock);

    if (out->buffer)
        free(out->buffer);
    if (out->resampler)
        release_resampler(out->resampler);

    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
	DLOGI("%s\n", __FUNCTION__);
    return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms = str_parms_create_str(keys);
    char value[32];
    int ret = str_parms_get_str(parms, "ec_supported", value, sizeof(value));
    char *str;

	DLOGI("%s\n", __FUNCTION__);

    str_parms_destroy(parms);
    if (ret >= 0) {
        parms = str_parms_create_str("ec_supported=yes");
        str = str_parms_to_str(parms);
        str_parms_destroy(parms);
        return str;
    }
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
	DLOGI("%s\n", __FUNCTION__);
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    struct audio_device *adev = (struct audio_device *)dev;

    adev->voice_volume = volume;
    DLOGI("%s (volume=%f)\n", __FUNCTION__, volume);
	return 0;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
	DLOGI("%s\n", __FUNCTION__);
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
	DLOGI("%s (mode=%d)\n", __FUNCTION__, mode);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    adev->mic_mute = state;
	DLOGI("%s (state=%d)\n", __FUNCTION__, state);
    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev->mic_mute;
	DLOGI("%s (state=%d)\n", __FUNCTION__, *state);
    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
	int channel_count = popcount(config->channel_mask);
	DLOGI("%s (sample_rate=%d, format=0x%x, channel=%d)\n",
		__FUNCTION__, config->sample_rate, config->format, popcount(config->channel_mask));

    return get_input_buffer_size(config->sample_rate, config->format, channel_count);
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;
    int ret;

	stream_in_refcount++;

    *stream_in = NULL;

	DLOGI("*** %s (devices=0x%x, request rate=%d, channel_mask=0x%x) ***\n",
		__FUNCTION__, devices, config->sample_rate, config->channel_mask);

    /* Respond with a request for mono if a different format is given. */
    if (config->channel_mask != AUDIO_CHANNEL_IN_MONO &&
        config->channel_mask != AUDIO_CHANNEL_IN_FRONT_BACK) {
        config->channel_mask  = AUDIO_CHANNEL_IN_MONO;
        return -EINVAL;
    }

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (!in)
        return -ENOMEM;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

	// jhkim
	struct snd_card_dev *card = &pcm_in;
	struct pcm_config *pcm = &in->config;

	ret = pcm_config_setup(card, pcm);
	if (ret)
		goto err_open;

	in->card = card;
    in->dev = adev;
	in->standby = true;

	in->channel_mask = config->channel_mask;			/* set with input parametes */
	in->request_rate = config->sample_rate;				/* set with input parametes */
   	in->format = pcm_format_to_android(pcm->format);	/* set with device parametes */

    in->input_source = AUDIO_SOURCE_DEFAULT;
    /* strip AUDIO_DEVICE_BIT_IN to allow bitwise comparisons */
    in->device = devices & ~AUDIO_DEVICE_BIT_IN;
    in->io_handle = handle;

	/* resampler */
    if (in->request_rate != pcm->rate) {
    	int format_byte = pcm_format_to_bytes(pcm->format);
		int length = pcm->period_size * pcm->channels * format_byte;

		/*
		 * allocate buffer for resampler
		 * period size * ch * byte per sample
		 */
   		in->buffer = malloc(length);
    	if (NULL == in->buffer || 0 == length) {
    		ALOGE("%s Allocat failed for resampler (%dHZ) buffer[%d: %d,%d ch,%d bits]",
    			__FUNCTION__, in->request_rate, length,
    			pcm->period_size, pcm->channels, format_byte*8);

        	ret = -ENOMEM;
        	goto err_open;
    	}
		memset(in->buffer, 0x0, length);

        ret = create_resampler(pcm->rate, in->request_rate,
							popcount(in->channel_mask),
                          	RESAMPLER_QUALITY_DEFAULT,
							&in->rsmp_buf_provider, &in->resampler);
        if (ret) {
            ret = -EINVAL;
            goto err_resampler;
        }

        in->rsmp_buf_provider.get_next_buffer = get_input_next_buffer;
        in->rsmp_buf_provider.release_buffer = release_input_buffer;

	   	ALOGI("%s %s", __FUNCTION__, card->name);
	   	ALOGI("Create Resampler: rate %d->%d, ch %d->%d, buffer[%d: %d, %dch, %dbits]",
   			pcm->rate, in->request_rate, pcm->channels, popcount(in->channel_mask),
   			length, pcm->period_size, pcm->channels, format_byte*8);
    }

	set_in_pcm_config_gptr(pcm);
    *stream_in = &in->stream;

    return 0;

err_resampler:
	free(in->buffer);
err_open:
    free(in);
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

	DLOGI("%s\n", __FUNCTION__);

	if (stream_in_refcount == 1) {
		in_standby(&stream->common);

		if (in->resampler) {
		    release_resampler(in->resampler);
		    in->resampler = NULL;
		}

		if (in->buffer) {
			free(in->buffer);
			in->buffer = NULL;
		}

		free(stream);

		set_in_pcm_config_gptr(NULL);
	}

	stream_in_refcount--;

    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;

	audio_route_free(adev->ar);

    free(device);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct audio_device *adev;
    int ret;
	DLOGI("+++%s (name=%s)\n", __FUNCTION__, name);

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct audio_device));
    if (!adev)
        return -ENOMEM;

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    adev->ar = audio_route_init(MIXER_CARD, NULL);
    adev->input_source = AUDIO_SOURCE_DEFAULT;
    /* adev->cur_route_id initial value is 0 and such that first device
     * selection is always applied by select_devices() */

    *device = &adev->device.common;

	DLOGI("---%s\n", __FUNCTION__);

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Nexell audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
