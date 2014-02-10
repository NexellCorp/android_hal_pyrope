/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include "audio_route.h"

#if	(0)
#define LOGD(msg...)	ALOGI(msg)
#else
#define LOGD(msg...) 	do { } while(0)
#endif

#include <audio_utils/resampler.h>
#include <tinyalsa/asoundlib.h>

//#define	DUMP_PLAYBACK
#ifdef  DUMP_PLAYBACK
#define DUMP_PLYA_ENABLE	"/data/pcm/play"
#define DUMP_PLYA_PATH		"/data/pcm/play.wav"
#endif

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
    int active_out;
    int active_in;
    pthread_mutex_t lock;
    float voice_volume;
    bool mic_mute;
    struct audio_route *ar;
};

struct stream_out {
    struct audio_stream_out stream;
    pthread_mutex_t lock;
	struct audio_device *dev;
    struct resampler_itfe *resampler;
    struct pcm *pcm;
    struct pcm_config config;
	audio_format_t format;
    audio_channel_mask_t channel_mask;
    bool standby;
    char *buffer;
	/* sound card */
	struct snd_card_dev *card;
	FILE *file;
};

struct stream_in {
    struct audio_stream_in stream;
    pthread_mutex_t lock;
	struct audio_device *dev;
    struct resampler_itfe *resampler;
    struct pcm *pcm;
    struct pcm_config config;
	audio_format_t format;
    uint32_t channel_mask;
    bool standby;
    char *buffer;
	/* sound card */
	struct snd_card_dev *card;
};

static struct pcm_config *pcm_config_in = NULL;
#define	get_input_pcm_config()		(pcm_config_in)
#define	set_input_pcm_config(c)		(pcm_config_in = c)

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

static int pcm_params_setup(struct snd_card_dev *card, struct pcm_config *pcm)
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

static int pcm_output_open(struct stream_out *out)
{
    struct snd_card_dev *card = out->card;
    int type;

	LOGD("%s %s pcmC%dD%d%s open ref=%d\n", __FUNCTION__, card->name,
		card->card, card->device, card->flags & PCM_IN ? "c" : "p", card->refcount);
	if (card->refcount)
		return -1;

	out->pcm = pcm_open(card->card, card->device,
				card->flags, &out->config);

	if (out->pcm && ! pcm_is_ready(out->pcm)) {
    	ALOGE("%s pcmC%dD%d%s open failed: %s", __FUNCTION__,
        	card->card, card->device, card->flags & PCM_IN ? "c" : "p",
        	pcm_get_error(out->pcm));
		return -EINVAL;
	}

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
	card->refcount++;
    return 0;
}

static int pcm_input_open(struct stream_in *in)
{
    struct snd_card_dev *card = in->card;
    int type;

	LOGD("%s pcmC%dD%d%s open ref=%d\n", __FUNCTION__,
		card->card, card->device, card->flags & PCM_IN ? "c" : "p", card->refcount);

	if (card->refcount)
		return -1;

	in->pcm = pcm_open(card->card, card->device,
				card->flags, &in->config);

	if (in->pcm && ! pcm_is_ready(in->pcm)) {
    	ALOGE("%s pcmC%dD%d%s open failed: %s", __FUNCTION__,
        	card->card, card->device, card->flags & PCM_IN ? "c" : "p",
        	pcm_get_error(in->pcm));
		return -EINVAL;
	}
	card->refcount++;

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate, audio_format_t format, int channels)
{
	struct pcm_config *config = get_input_pcm_config();
    size_t size;
    size_t device_rate;
    size_t bitperframe = 16;

	if (NULL == config) {
    	ALOGE("%s not opened input device, set default config !!!", __FUNCTION__);
		struct snd_card_dev *card = &pcm_in;
		int ret = pcm_params_setup(card, &card->config);
		if (ret)
			return 0;
    	config = &card->config;
	}

    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames */
	LOGD("%s (period_size=%d, sample_rate=%d, rate=%d, bitperframe=%d)\n",
		__FUNCTION__, config->period_size, sample_rate, config->rate, bitperframe);

    size = (config->period_size * sample_rate) / config->rate;
    size = ((size + (bitperframe-1)) / bitperframe) * bitperframe;

    return size * channels * sizeof(short);
}

static int do_output_standby(struct stream_out *out)
{
    struct snd_card_dev *card = out->card;
    LOGD("%s %s ref=%d, standby=%d\n", __FUNCTION__, card->name, card->refcount, out->standby);

    if (!out->standby) {
    	if (1 == card->refcount)
        	pcm_close(out->pcm);

#ifdef	DUMP_PLAYBACK
		if (out->file)
			fclose(out->file);
#endif
       	card->refcount--;
        out->pcm = NULL;
        out->standby = true;
    }
    return 0;
}

static void out_select_device(struct stream_out *out)
{
    struct audio_device *adev = out->dev;

	if (AUDIO_DEVICE_OUT_AUX_DIGITAL == adev->active_out)
		out->card = &spdif_out;
	else
		out->card = &pcm_out;
}

static int do_input_standby(struct stream_in *in)
{
    struct snd_card_dev *card = in->card;
    LOGD("%s %s ref=%d, standby=%d\n", __FUNCTION__, card->name, card->refcount, in->standby);

    if (!in->standby) {
    	if (1 == card->refcount)
        	pcm_close(in->pcm);

        card->refcount--;
        in->pcm = NULL;
        in->standby = true;
    }
    return 0;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
	LOGD("%s %s (rate=%d)\n", __FUNCTION__, out->card->name, out->config.rate);
    return out->config.rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
	LOGD("%s %s (rate=%d)\n", __FUNCTION__,
		((struct stream_out *)stream)->card->name, rate);
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
	size_t buffer_size = out->config.period_size *
            audio_stream_frame_size((struct audio_stream *)stream);

	LOGD("%s %s (buffer_size=%d)\n", __FUNCTION__, out->card->name, buffer_size);
    return buffer_size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
	LOGD("%s %s (channels=0x%x)\n", __FUNCTION__, out->card->name, out->config.channels);
    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
	struct stream_out *out = (struct stream_out *)stream;
	LOGD("%s %s (format=0x%x)\n", __FUNCTION__, out->card->name, out->format);
    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
	LOGD("%s %s (format=0x%x)\n", __FUNCTION__,
		((struct stream_out *)stream)->card->name, format);
    return -ENOSYS;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    int status;

    LOGD("%s %s\n", __FUNCTION__, out->card->name);

    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    status = do_output_standby(out);

    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);

    return status;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
	LOGD("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;

	LOGD("%s (%s)\n", __FUNCTION__, kvpairs);
    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));

    if (ret >= 0) {
        val = atoi(value);
        pthread_mutex_lock(&adev->lock);
        pthread_mutex_lock(&out->lock);

        if ((val != 0) && ((adev->active_out & AUDIO_DEVICE_OUT_ALL) != val)) {
        	/* close */
			do_output_standby(out);

			/* change device */
           	adev->active_out = val;
			out_select_device(out);
        }

        pthread_mutex_unlock(&out->lock);
        pthread_mutex_unlock(&adev->lock);
    }

    str_parms_destroy(parms);
    return ret;
}
static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
	LOGD("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
	uint32_t latency = (out->config.period_size * out->config.period_count * 1000) /
            		out->config.rate;

	LOGD("%s %s (latency=%d)\n", __FUNCTION__,
		((struct stream_out *)stream)->card->name, latency);
    return latency;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
	LOGD("%s %s (left=%f, right=%f)\n", __FUNCTION__,
		((struct stream_out *)stream)->card->name, left, right);
    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret = 0;
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    int i;

//	LOGD("%s %s\n", __FUNCTION__, out->card->name);

    /*
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the output stream mutex - e.g.
     * executing out_set_parameters() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = pcm_output_open(out);
        if (0 > ret) {
            pthread_mutex_unlock(&adev->lock);
            goto err_write;
        }
        out->standby = false;
    }
    pthread_mutex_unlock(&adev->lock);

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
	LOGD("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	LOGD("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	LOGD("%s %s\n", __FUNCTION__, ((struct stream_out *)stream)->card->name);
    return 0;
}

/*
static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
	LOGD("%s\n", __FUNCTION__);
    return -EINVAL;
}
*/

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
	LOGD("+++ %s (devices=0x%x, flags=0x%x)\n", __FUNCTION__, devices, flags);

    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (!out)
        return -ENOMEM;

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

	// jhkim
	struct snd_card_dev *card = &pcm_out;

	if ((devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) &&
		(flags & AUDIO_OUTPUT_FLAG_DIRECT)) {
		card = &spdif_out;
	}

	ret = pcm_params_setup(card, &out->config);
	if (ret)
		goto err_open;

	out->card = card;
	out->format	= pcm_format_to_android(out->config.format);
	out->channel_mask = pcm_channels_to_android(out->config.channels);

    out->dev = adev;
	out->standby = true;

	config->sample_rate = out->config.rate;
	config->channel_mask = out->channel_mask;
	config->format = out->format;
    //

    *stream_out = &out->stream;
    LOGD("--- %s (devices=0x%x, flags=0x%x)\n", __FUNCTION__, devices, flags);
    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

	LOGD("%s %s\n", __FUNCTION__, out->card->name);

    out_standby(&stream->common);

    if (out->buffer)
        free(out->buffer);
    if (out->resampler)
        release_resampler(out->resampler);

    free(stream);
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
	LOGD("%s (rate=%d)\n", __FUNCTION__, in->config.rate);
    return in->config.rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
	LOGD("%s (rate=%d)\n", __FUNCTION__, rate);
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
	size_t buffer_size = get_input_buffer_size(in->config.rate, in->format,
                                popcount(in->channel_mask));

	LOGD("%s (buffer_size=%d)\n", __FUNCTION__, buffer_size);
	return buffer_size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
	audio_channel_mask_t channels = in->channel_mask;

	LOGD("%s (channels=0x%x)\n", __FUNCTION__, in->config.channels);
    return channels;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
	struct stream_in *in = (struct stream_in *)stream;
	LOGD("%s (format=0x%x)\n", __FUNCTION__, in->format);
    return in->format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
	LOGD("%s (format=0x%x)\n", __FUNCTION__, format);
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    int status;

    LOGD("%s\n", __FUNCTION__);

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);

    status = do_input_standby(in);

    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
	LOGD("%s\n", __FUNCTION__);
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
	LOGD("%s\n", __FUNCTION__);
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
	LOGD("%s\n", __FUNCTION__);
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
	LOGD("%s (gain=%f)\n", __FUNCTION__, gain);
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
   	int ret = 0;
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    size_t frames_rq = bytes / audio_stream_frame_size(&stream->common);

//	LOGD("%s %s bytes=%d\n", __FUNCTION__, in->card->name);

    /* acquiring hw device mutex systematically is useful if a low priority thread is waiting
     * on the input stream mutex - e.g. executing select_mode() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&in->lock);
    if (in->standby) {
        ret = pcm_input_open(in);
        if (0 > ret) {
            pthread_mutex_unlock(&adev->lock);
            goto err_read;
        }
        in->standby = false;
    }
    pthread_mutex_unlock(&adev->lock);

    if (in->pcm)
        ret = pcm_read(in->pcm, buffer, bytes);

    if (ret > 0)
        ret = 0;

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
	LOGD("%s\n", __FUNCTION__);
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	LOGD("%s\n", __FUNCTION__);
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	LOGD("%s\n", __FUNCTION__);
    return 0;
}


static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
	int channel_count = popcount(config->channel_mask);
	LOGD("%s (sample_rate=%d, format=%d, channel=%d)\n",
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
	LOGD("%s (devices=0x%x, request rate=%d) \n", __FUNCTION__, devices, config->sample_rate);

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

	ret = pcm_params_setup(card, &in->config);
	if (ret)
		goto err_open;

	in->card = card;
	in->format	= pcm_format_to_android(in->config.format);
	in->channel_mask = pcm_channels_to_android(in->config.channels);
	set_input_pcm_config(&in->config);

    in->dev = adev;
	in->standby = true;

	config->sample_rate = in->config.rate;
	config->channel_mask = in->channel_mask;
	config->format = in->format;
    //

    *stream_in = &in->stream;
    return 0;

err_open:
    free(in);
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

	LOGD("%s\n", __FUNCTION__);

    in_standby(&stream->common);

    if (in->resampler)
        release_resampler(in->resampler);

    if (in->buffer)
    	free(in->buffer);

    free(stream);
    return;
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
	LOGD("%s\n", __FUNCTION__);
    return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
	LOGD("%s\n", __FUNCTION__);
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
	LOGD("%s\n", __FUNCTION__);
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    struct audio_device *adev = (struct audio_device *)dev;

    adev->voice_volume = volume;
    LOGD("%s (volume=%f)\n", __FUNCTION__, volume);

	return 0;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
	LOGD("%s\n", __FUNCTION__);
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
	LOGD("%s (mode=%d)\n", __FUNCTION__, mode);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    adev->mic_mute = state;
	LOGD("%s (state=%d)\n", __FUNCTION__, state);
    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev->mic_mute;
	LOGD("%s (state=%d)\n", __FUNCTION__, *state);
    return 0;
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
	LOGD("+++%s (name=%s)\n", __FUNCTION__, name);

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
#if 0
  	adev->device.get_master_volume = adev_get_master_volume;
  	adev->device.set_master_mute = adev_set_master_mute;
  	adev->device.get_master_mute = adev_get_master_mute;
#endif
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

    /* hw */
    adev->active_out = AUDIO_DEVICE_OUT_SPEAKER;
    adev->active_in  = AUDIO_DEVICE_IN_BUILTIN_MIC & ~AUDIO_DEVICE_BIT_IN;

	adev->ar = audio_route_init();

    *device = &adev->device.common;

	LOGD("---%s\n", __FUNCTION__);

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
        .name = "NXP4330 audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
