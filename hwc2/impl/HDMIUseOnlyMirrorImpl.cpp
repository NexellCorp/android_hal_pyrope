#undef LOG_TAG
#define LOG_TAG     "HDMIUseOnlyMirrorImpl"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>

#include <linux/fb.h>
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

#include <cutils/log.h>

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>

#include <gralloc_priv.h>

#include "HWCRenderer.h"
#include "HDMIMirrorRenderer.h"

#include "HWCImpl.h"
#include "HDMICommonImpl.h"
#include "HDMIUseOnlyMirrorImpl.h"

using namespace android;

HDMIUseOnlyMirrorImpl::HDMIUseOnlyMirrorImpl(int rgbID)
    :HDMICommonImpl(rgbID, -1),
    mConfigured(false),
    mMirrorRenderer(NULL)
{
    init();
}

HDMIUseOnlyMirrorImpl::HDMIUseOnlyMirrorImpl(int rgbID, int width, int height, int srcWidth, int srcHeight)
    :HDMICommonImpl(rgbID, -1, width, height, srcWidth, srcHeight),
    mConfigured(false),
    mMirrorRenderer(NULL)
{
    init();
    ALOGD("mWidth %d, mHeight %d, srcWidth %d, srcHeight %d", mWidth, mHeight, srcWidth, srcHeight);
}


HDMIUseOnlyMirrorImpl::~HDMIUseOnlyMirrorImpl()
{
    if (mMirrorRenderer)
        delete mMirrorRenderer;
}

void HDMIUseOnlyMirrorImpl::init()
{
    mMirrorRenderer = new HDMIMirrorRenderer(mRgbID, 3);
    if (!mMirrorRenderer)
        ALOGE("FATAL: can't create MirrorRenderer");
}

int HDMIUseOnlyMirrorImpl::enable()
{
    if (!mEnabled) {
        ALOGD("Enable");
        int ret = v4l2_link(mSubID, nxp_v4l2_hdmi);
        if (ret < 0) {
            ALOGE("can't link %d-->%d", mSubID, nxp_v4l2_hdmi);
            return -EINVAL;
        }
        mEnabled = true;
    }
    return 0;
}

int HDMIUseOnlyMirrorImpl::disable()
{
    // if (mEnabled) {
        ALOGD("Disable");
        mMirrorRenderer->stop();
        v4l2_unlink(mSubID, nxp_v4l2_hdmi);
        unConfigHDMI();
        mConfigured = false;
        mEnabled = false;
    // }
    return 0;
}

int HDMIUseOnlyMirrorImpl::prepare(hwc_display_contents_1_t *contents)
{
    int maxWidth = 0;
    int maxHeight = 0;
    int width, height;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND)
            continue;

        // for portrait debugging
        ALOGV("displayFrame l,t,w,h(%d,%d,%d,%d)", layer.displayFrame.left, layer.displayFrame.top,
                layer.displayFrame.right - layer.displayFrame.left, layer.displayFrame.bottom - layer.displayFrame.top);
        ALOGV("sourceCrop l,t,w,h(%d,%d,%d,%d)", layer.sourceCrop.left, layer.sourceCrop.top,
                layer.sourceCrop.right - layer.sourceCrop.left, layer.sourceCrop.bottom - layer.sourceCrop.top);
        ALOGV("transform %d, blending %d", layer.transform, layer.blending);

        if (layer.displayFrame.left >= 0) {
            width = layer.displayFrame.right - layer.displayFrame.left;
        } else {
            width = layer.displayFrame.right;
        }
        if (layer.displayFrame.top >= 0) {
            height = layer.displayFrame.bottom - layer.displayFrame.top;
        } else {
            height = layer.displayFrame.bottom;
        }
        if (width >= maxWidth && height >= maxHeight) {
            maxWidth = width;
            maxHeight = height;
        }

        layer.compositionType = HWC_OVERLAY;
    }

    ALOGV("maxWidth %d, maxHeight %d", maxWidth, maxHeight);
    if (maxWidth < mWidth && maxWidth < maxHeight) {
        ALOGV("can't render this resolution!!!");
        return -1;
    }

    ALOGV("prepare");
    return 0;
}

int HDMIUseOnlyMirrorImpl::set(hwc_display_contents_1_t *contents, void *hnd)
{
    ALOGV("set");
    if (!mConfigured)
        config();

    private_handle_t const *rgbHandle = (private_handle_t const *)hnd;
    mMirrorRenderer->setHandle(rgbHandle);

    ALOGV("set");
    return -EINVAL;
}

private_handle_t const *HDMIUseOnlyMirrorImpl::getRgbHandle()
{
    return mMirrorRenderer->getHandle();
}

private_handle_t const *HDMIUseOnlyMirrorImpl::getVideoHandle()
{
    return NULL;
}

int HDMIUseOnlyMirrorImpl::render()
{
    return mMirrorRenderer->render();
}

int HDMIUseOnlyMirrorImpl::config()
{
    int ret = configHDMI(mWidth, mHeight);
    if (ret < 0) {
        ALOGE("failed to configHDMI()");
        return ret;
    }

    ALOGD("configRGB: id %d, w %d, h %d", mRgbID, mWidth, mHeight);
    ret = v4l2_set_format(mRgbID, mWidth, mHeight, V4L2_PIX_FMT_RGB32);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_format()");
        return ret;
    }

    ret = v4l2_set_crop(mRgbID, 0, 0, mWidth, mHeight);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_crop()");
        return ret;
    }

    ret = v4l2_reqbuf(mRgbID, 3);
    if (ret < 0) {
        ALOGE("failed to v4l2_reqbuf()");
        return ret;
    }

    mConfigured = true;
    return 0;
}
