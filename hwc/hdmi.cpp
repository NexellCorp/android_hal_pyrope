#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include <cutils/log.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <linux/videodev2_nxp_media.h>
#include <ion/ion.h>
#include <android-nxp-v4l2.h>
#include <nxp-v4l2.h>

#include <gralloc_priv.h>

#include "hdmi.h"

#define VSYNC_CTL_FILE  "/sys/devices/platform/display/active.1"
#define VSYNC_MON_FILE  "/sys/devices/platform/display/vsync.1"

#define VSYNC_ON        "1"
#define VSYNC_OFF       "0"

//#define DBG_TIMESTAMP

namespace android {

NXHdmiThread::NXHdmiThread(int rgbMaxBufferCount, int videoMaxBufferCount)
    : RgbOn(false),
      VideoOn(false),
      RgbMaxBufferCount(rgbMaxBufferCount),
      RgbOutIndex(0),
      RgbOutQCount(0),
      VideoMaxBufferCount(videoMaxBufferCount),
      VideoOutIndex(0),
      VideoOutQCount(0)
{
    VsyncCtlFd = open(VSYNC_CTL_FILE, O_RDWR);
    if (VsyncCtlFd < 0)
        ALOGE("can't open %s", VSYNC_CTL_FILE);

    VsyncMonFd = open(VSYNC_MON_FILE, O_RDONLY);
    if (VsyncMonFd < 0)
        ALOGE("can't open %s", VSYNC_MON_FILE);

    vsyncMonEnable(true);
    ALOGD("%s", __func__);
}

NXHdmiThread::~NXHdmiThread()
{
    ALOGD("%s", __func__);
}

void NXHdmiThread::vsyncMonEnable(bool enable)
{
    if (enable)
        write(VsyncCtlFd, VSYNC_ON, sizeof(VSYNC_ON));
    else
        write(VsyncCtlFd, VSYNC_OFF, sizeof(VSYNC_OFF));
}

void NXHdmiThread::waitVsync(void)
{
    struct pollfd fd;
    fd.fd = VsyncMonFd;
    fd.events = POLLPRI;

    int err = 0;
    do {
        err = poll(&fd, 1, -1);
    } while (err <= 0);

    // this is debugging code
#ifdef DBG_TIMESTAMP
    err = lseek(VsyncMonFd, 0, SEEK_SET);
    if (err < 0) {
        ALOGE("waitVsync(): error seeking to vsync timestamp");
        return;
    }

    char buf[4096] = {0, };
    err = read(VsyncMonFd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("waitVsync(): reading vsync timestamp!!!");
        return;
    }
    buf[sizeof(buf) - 1] = '\0';
    uint64_t timestamp = strtoull(buf, NULL, 0);
    ALOGD("TimeStamp: %llu", timestamp);
#endif
}

bool NXHdmiThread::processMessage(const NXHdmiMessage &msg)
{
    uint32_t type = msg.type;
    uint32_t ext1 = msg.ext1;
    uint32_t ext2 = msg.ext2;
    ALOGV("Msg: type(%d), ext1(0x%x), ext2(0x%x)", type, ext1, ext2);

    // TODO: BUG
    if (ext1 || ext2) type = NXHdmiMessage::HDMI_PLAY;
    else if (type != NXHdmiMessage::HDMI_EXIT) type = NXHdmiMessage::HDMI_STOP_RGB;

    bool ret = true;
    switch (type) {
    case NXHdmiMessage::HDMI_PLAY:
        ret = handlePlay(ext1, ext2);
        break;
    case NXHdmiMessage::HDMI_STOP_RGB:
        /* TODO: HACK */
#if 0
        if (RgbOn) {
            ALOGD("Stop RGB!!!");
            RgbOn = false;
        }
#else
        ALOGD("Message: HDMI_STOP_RGB");
        // ret = handlePlay(ext1, ext2);
#endif
        break;
    case NXHdmiMessage::HDMI_EXIT:
        ALOGD("Quit HDMI!!!");
        return false;
    default:
        ALOGE("%s: invalid type(0x%x)", __func__, msg.type);
        break;
    }

    return true;
}

bool NXHdmiThread::handlePlay(uint32_t msgVal1, uint32_t msgVal2)
{
    uint32_t ext1 = msgVal1;
    uint32_t ext2 = msgVal2;
    bool ret;

    if (!ext1 && RgbOn) {
        // Stop RGB
        ALOGD("StopRgb");
        ret = handleStopRgb();
        if (!ret) {
            ALOGE("%s: handleStopRgb failed", __func__);
            return false;
        }
        RgbOn = false;
    }

    if (!ext2 && VideoOn) {
        ALOGD("StopVideo");
        ret = handleStopVideo();
        if (!ret) {
            ALOGE("%s: handleStopVideo failed", __func__);
            return false;
        }
        VideoOn = false;
    }

    if (ext1 && !RgbOn) {
        ALOGD("startRgb");
        ret = handleStartRgb(ext1);
        if (ret) {
            RgbOn = true;
            ext1 = 0;
        } else {
            ALOGE("%s: handleStartRgb failed(handle: 0x%x)", __func__, ext1);
            return false;
        }
    }

    if (ext2 && !VideoOn) {
        ALOGD("startVideo");
        ret = handleStartVideo(ext2);
        if (ret) {
            VideoOn = true;
            ext2 = 0;
        } else {
            ALOGE("%s: handleStartVideo failed(handle: 0x%x)", __func__, ext2);
            return false;
        }
    }

    if (ext1 || ext2) {
        waitVsync();
        if (ext1) {
            ALOGV("PlayRgb");
            ret = handlePlayRgb(ext1);
            if (!ret) {
                ALOGE("%s: handlePlayRgb failed", __func__);
                return false;
            }
        }
        if (ext2) {
            ALOGV("PlayVideo");
            ret = handlePlayVideo(ext2);
            if (!ret) {
                ALOGE("%s: handlePlayVideo failed", __func__);
                return false;
            }
        }
    }

    return true;
}

bool NXHdmiThread::handleStartVideo(uint32_t msgVal)
{
    private_handle_t const *buffer = (private_handle_t const *)msgVal;
    if (!buffer) {
        ALOGE("StartVideo: No Buffer!!!");
        return false;
    }

    VideoOutIndex = 0;
    VideoOutQCount = 0;

    int ret = v4l2_qbuf(nxp_v4l2_mlc1_video, 3, VideoOutIndex, buffer, -1, NULL);
    if (ret < 0) {
        ALOGE("StartVideo: error v4l2_qbuf()");
        return false;
    }

    VideoOutIndex++;
    VideoOutQCount++;

    ret = v4l2_streamon(nxp_v4l2_mlc1_video);
    if (ret < 0) {
        ALOGE("StartVideo: error v4l2_streamon()");
        return false;
    }

    return true;
}

bool NXHdmiThread::handleStopVideo()
{
    int ret = v4l2_streamoff(nxp_v4l2_mlc1_video);
    if (ret < 0) {
        ALOGE("StopVideo: error v4l2_streamoff()");
        return false;
    }
    return true;
}

bool NXHdmiThread::handlePlayVideo(uint32_t msgVal)
{
    private_handle_t const *buffer = (private_handle_t const *)msgVal;
    if (!buffer) {
        ALOGE("PlayVideo: No Buffer!!!");
        return false;
    }

    int ret = v4l2_qbuf(nxp_v4l2_mlc1_video, 3, VideoOutIndex, buffer, -1, NULL);
    if (ret < 0) {
        ALOGE("PlayVideo: error v4l2_qbuf()");
        return false;
    }

    VideoOutIndex++;
    VideoOutIndex %= VideoMaxBufferCount;
    VideoOutQCount++;
    if (VideoOutQCount >= VideoMaxBufferCount) {
        int dqIndex;
        ret = v4l2_dqbuf(nxp_v4l2_mlc1_video, 1, &dqIndex, NULL);
        if (ret < 0) {
            ALOGE("PlayVideo: error v4l2_dqbuf()");
            return false;
        }
        VideoOutQCount--;
    }
    return true;
}

bool NXHdmiThread::handleStartRgb(uint32_t msgVal)
{
    private_handle_t const *buffer = (private_handle_t const *)msgVal;
    if (!buffer) {
        ALOGE("StartRgb: No Buffer!!!");
        return false;
    }

    RgbOutIndex = 0;
    RgbOutQCount = 0;

    int ret = v4l2_qbuf(nxp_v4l2_mlc1_rgb, 1, RgbOutIndex, buffer, -1, NULL);
    if (ret < 0) {
        ALOGE("StartRgb: error v4l2_qbuf()");
        return false;
    }

    RgbOutIndex++;
    RgbOutQCount++;

    ret = v4l2_streamon(nxp_v4l2_mlc1_rgb);
    if (ret < 0) {
        ALOGE("StartRgb: error v4l2_streamon()");
        return false;
    }

    return true;
}

bool NXHdmiThread::handleStopRgb()
{
    int ret = v4l2_streamoff(nxp_v4l2_mlc1_rgb);
    if (ret < 0) {
        ALOGE("StopRgb: error v4l2_streamoff()");
        return false;
    }
    return true;
}

bool NXHdmiThread::handlePlayRgb(uint32_t msgVal)
{
    private_handle_t const *buffer = (private_handle_t const *)msgVal;
    ALOGV("rgb buffer: %p", buffer);
    if (!buffer) {
        ALOGE("PlayRgb: No Buffer!!!");
        return false;
    }

    int ret = v4l2_qbuf(nxp_v4l2_mlc1_rgb, 1, RgbOutIndex, buffer, -1, NULL);
    if (ret < 0) {
        ALOGE("PlayRgb: error v4l2_qbuf()");
        return false;
    }

    RgbOutIndex++;
    RgbOutIndex %= RgbMaxBufferCount;
    RgbOutQCount++;
    if (RgbOutQCount >= RgbMaxBufferCount) {
        int dqIndex;
        ret = v4l2_dqbuf(nxp_v4l2_mlc1_rgb, 1, &dqIndex, NULL);
        if (ret < 0) {
            ALOGE("PlayRgb: error v4l2_dqbuf()");
            return false;
        }
        RgbOutQCount--;
    }
    return true;
}

}; // namespace
