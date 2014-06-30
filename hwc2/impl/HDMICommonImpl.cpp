#undef LOG_TAG
#define LOG_TAG     "HDMICommonImpl"

#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <linux/ion.h>
#include <linux/videodev2_nxp_media.h>

#include <ion/ion.h>
#include <nxp-v4l2.h>
#include <NXPrimitive.h>

#include <cutils/log.h>

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>

#include "HWCImpl.h"
#include "HDMICommonImpl.h"

using namespace android;

HDMICommonImpl::HDMICommonImpl(int rgbID, int videoID)
    :HWCImpl(rgbID, videoID),
    mRgbConfigured(false),
    mVideoConfigured(false),
    mHDMIConfigured(false)
{
    init();
}

HDMICommonImpl::HDMICommonImpl(int rgbID, int videoID, int width, int height, int srcWidth, int srcHeight)
    :HWCImpl(rgbID, videoID, width, height, srcWidth, srcHeight),
    mRgbConfigured(false),
    mVideoConfigured(false),
    mHDMIConfigured(false)
{
    init();
}

void HDMICommonImpl::init()
{
    if (mRgbID == nxp_v4l2_mlc0_rgb)
        mSubID = nxp_v4l2_mlc0;
    else
        mSubID = nxp_v4l2_mlc1;
}

int HDMICommonImpl::configRgb(struct hwc_layer_1 &layer)
{
    if (likely(mRgbConfigured))
        return 0;

    int width = layer.sourceCrop.right - layer.sourceCrop.left;
    int height = layer.sourceCrop.bottom - layer.sourceCrop.top;

    int ret = v4l2_set_format(mRgbID, width, height, V4L2_PIX_FMT_RGB32);
    if (ret < 0) {
        ALOGE("configRgb(): failed to v4l2_set_format()");
        return ret;
    }

    width = layer.displayFrame.right - layer.displayFrame.left;
    height = layer.displayFrame.bottom - layer.displayFrame.top;

    ret = v4l2_set_crop(mRgbID, layer.displayFrame.left, layer.displayFrame.top, width, height);
    if (ret < 0) {
        ALOGE("configRgb(): failed to v4l2_set_crop()");
        return ret;
    }

    ret = v4l2_reqbuf(mRgbID, 3);
    if (ret < 0) {
        ALOGE("configRgb(): failed to v4l2_reqbuf()");
        return ret;
    }

    mRgbConfigured = true;
    return 0;
}

int HDMICommonImpl::configVideo(struct hwc_layer_1 &layer, const private_handle_t *h)
{
    if (likely(mVideoConfigured))
        return 0;

    bool configChanged = false;

    int width;
    int height;

    if (h) {
        width = h->width;
        height = h->height;
    } else {
        width = layer.sourceCrop.right - layer.sourceCrop.left;
        height = layer.sourceCrop.bottom - layer.sourceCrop.top;
    }

    mVideoWidth = width;
    mVideoHeight = height;

    int ret = v4l2_set_format(mVideoID,
            width,
            height,
            V4L2_PIX_FMT_YUV420);
    ALOGV("configVideo: %d:%d - %d:%d", layer.sourceCrop.left, layer.sourceCrop.top, layer.sourceCrop.right, layer.sourceCrop.bottom);
    if (ret < 0) {
        ALOGE("configVideo(): failed to v4l2_set_format()");
        return ret;
    }

    mVideoLeft = layer.displayFrame.left;
    mVideoTop  = layer.displayFrame.top;
    mVideoRight = layer.displayFrame.right;
    mVideoBottom = layer.displayFrame.bottom;

    ret = v4l2_set_crop(mVideoID, mVideoLeft, mVideoTop, mVideoRight - mVideoLeft, mVideoBottom - mVideoTop);
    if (ret < 0) {
        ALOGE("configVideo(): failed to v4l2_set_crop()");
        return ret;
    }

    ret = v4l2_reqbuf(mVideoID, 4);
    if (ret < 0) {
        ALOGE("configVideo(): failed to v4l2_reqbuf()");
        return ret;
    }

    ret = v4l2_set_ctrl(mVideoID, V4L2_CID_MLC_VID_PRIORITY, 1);
    if (ret < 0) {
        ALOGE("configVideo(): failed to v4l2_set_ctrl()");
        return ret;
    }

    mVideoConfigured = true;
    return 0;
}

int HDMICommonImpl::configHDMI(uint32_t preset)
{
    if (likely(mHDMIConfigured))
        return 0;

    int ret = v4l2_set_preset(nxp_v4l2_hdmi, preset);
    if (ret < 0) {
        ALOGE("configHDMI(): failed to v4l2_set_preset(0x%x)", preset);
        return ret;
    }
    mHDMIConfigured = true;
    return 0;
}

int HDMICommonImpl::configHDMI(int width, int height)
{
    if (likely(mHDMIConfigured))
        return 0;

    uint32_t preset;

    if (width <= 1280)
        preset = V4L2_DV_720P60;
    else
        preset = V4L2_DV_1080P60;

    return configHDMI(preset);
}

int HDMICommonImpl::configVideoCrop(struct hwc_layer_1 &layer)
{
    if (mVideoLeft != layer.displayFrame.left ||
        mVideoTop  != layer.displayFrame.top  ||
        mVideoRight != layer.displayFrame.right ||
        mVideoBottom != layer.displayFrame.bottom) {
        mVideoLeft = layer.displayFrame.left;
        mVideoTop  = layer.displayFrame.top;
        mVideoRight = layer.displayFrame.right;
        mVideoBottom = layer.displayFrame.bottom;

        int ret = v4l2_set_crop(mVideoID, mVideoLeft, mVideoTop, mVideoRight - mVideoLeft, mVideoBottom - mVideoTop);
        if (ret < 0)
            ALOGE("failed to v4l2_set_crop()");

        return ret;
    } else {
        return 0;
    }
}
