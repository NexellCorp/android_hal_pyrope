#undef LOG_TAG
#define LOG_TAG     "HDMIUseGLAndVideoImpl"

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
#include <NXPrimitive.h>

#include <cutils/log.h>

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>

#include <gralloc_priv.h>

#include "HWCRenderer.h"
#include "HWCCommonRenderer.h"

#include "HWCImpl.h"
#include "HDMICommonImpl.h"
#include "HDMIUseGLAndVideoImpl.h"

using namespace android;

HDMIUseGLAndVideoImpl::HDMIUseGLAndVideoImpl(int rgbID, int videoID)
    :HDMICommonImpl(rgbID, videoID),
    mRGBLayerIndex(-1),
    mVideoLayerIndex(-1),
    mRGBRenderer(NULL),
    mVideoRenderer(NULL),
    mRGBHandle(NULL),
    mVideoHandle(NULL),
    mVideoLayer(NULL)
{
    init();
}

HDMIUseGLAndVideoImpl::HDMIUseGLAndVideoImpl(int rgbID, int videoID, int width, int height)
    :HDMICommonImpl(rgbID, videoID, width, height, width, height),
    mRGBLayerIndex(-1),
    mVideoLayerIndex(-1),
    mRGBRenderer(NULL),
    mVideoRenderer(NULL),
    mRGBHandle(NULL),
    mVideoHandle(NULL),
    mVideoLayer(NULL)
{
    init();
}

HDMIUseGLAndVideoImpl::~HDMIUseGLAndVideoImpl()
{
    if (mRGBRenderer)
        delete mRGBRenderer;
    if (mVideoRenderer)
        delete mVideoRenderer;
}

void HDMIUseGLAndVideoImpl::init()
{
    mRGBRenderer = new HWCCommonRenderer(mRgbID, 3, 1);
    if (!mRGBRenderer)
        ALOGE("FATAL: can't create RGBRenderer");

    mVideoRenderer = new HWCCommonRenderer(mVideoID, 4, 3);
    if (!mVideoRenderer)
        ALOGE("FATAL: can't create VideoRenderer");
}

int HDMIUseGLAndVideoImpl::enable()
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

int HDMIUseGLAndVideoImpl::disable()
{
    // if (mEnabled) {
        ALOGD("Disable");
        mVideoRenderer->stop();
        mRGBRenderer->stop();
        unConfigRgb();
        unConfigVideo();
        unConfigHDMI();
        v4l2_unlink(mSubID, nxp_v4l2_hdmi);
        mEnabled = false;
    // }
    return 0;
}

int HDMIUseGLAndVideoImpl::prepare(hwc_display_contents_1_t *contents)
{
    // if (unlikely(!mEnabled))
    //     return 0;

    mRGBLayerIndex = -1;
    mVideoLayerIndex = -1;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            mRGBLayerIndex = i;
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND)
            continue;

        if (mVideoLayerIndex == -1 && canOverlay(layer)) {
            layer.compositionType = HWC_OVERLAY;
            layer.hints |= HWC_HINT_CLEAR_FB;
            mVideoLayerIndex = i;
            continue;
        }

        layer.compositionType = HWC_FRAMEBUFFER;
    }

    ALOGV("prepare");
    return 0;
}

bool HDMIUseGLAndVideoImpl::checkVideoConfigChanged()
{
    uint32_t width = mVideoHandle->width;
    uint32_t height = mVideoHandle->height;

    return    (width != mVideoWidth    ||
               height != mVideoHeight  ||
               mVideoLeft != mVideoLayer->displayFrame.left ||
               mVideoTop  != mVideoLayer->displayFrame.top ||
               mVideoRight != mVideoLayer->displayFrame.right ||
               mVideoBottom != mVideoLayer->displayFrame.bottom);
}

int HDMIUseGLAndVideoImpl::set(hwc_display_contents_1_t *contents, void *unused)
{
    // if (unlikely(!mEnabled))
    //     return 0;

    mRGBHandle = NULL;
    mVideoHandle = NULL;
    mVideoLayer = NULL;

    ALOGV("set: rgb %d, video %d", mRGBLayerIndex, mVideoLayerIndex);

    configHDMI(mWidth, mHeight);

    mRGBLayerIndex = -1;
    mVideoLayerIndex = -1;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            mRGBLayerIndex = i;
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND)
            continue;

        if (mVideoLayerIndex == -1 && canOverlay(layer)) {
            mVideoLayerIndex = i;
            mVideoLayer = &layer;
            continue;
        }

        if (mRGBLayerIndex >= 0 && mVideoLayerIndex >= 0)
            break;
    }


    if (mRGBLayerIndex >= 0) {
        mRGBHandle = reinterpret_cast<private_handle_t const *>(contents->hwLayers[mRGBLayerIndex].handle);
        mRGBRenderer->setHandle(mRGBHandle);
        configRgb(contents->hwLayers[mRGBLayerIndex]);
    } else {
        unConfigRgb();
        mRGBRenderer->stop();
    }

    if (mVideoLayerIndex >= 0) {
        mVideoHandle = reinterpret_cast<private_handle_t const *>(contents->hwLayers[mVideoLayerIndex].handle);
        mVideoRenderer->setHandle(mVideoHandle);
        bool videoConfigChanged = checkVideoConfigChanged();
        if (videoConfigChanged) {
            unConfigVideo();
            mVideoRenderer->stop();
        }
        configVideo(contents->hwLayers[mVideoLayerIndex], mVideoHandle);
        configVideoCrop(contents->hwLayers[mVideoLayerIndex]);
    } else {
        unConfigVideo();
        mVideoRenderer->stop();
    }

    return 0;
}

private_handle_t const *HDMIUseGLAndVideoImpl::getRgbHandle()
{
    return mRGBHandle;
}

private_handle_t const *HDMIUseGLAndVideoImpl::getVideoHandle()
{
    return mVideoHandle;
}

int HDMIUseGLAndVideoImpl::render()
{
    if (mVideoHandle) {
        int syncFd = mVideoLayer->acquireFenceFd;
        mVideoRenderer->render(&syncFd);
        mVideoLayer->releaseFenceFd = syncFd;
        ALOGV("acquirefd: %d, releasefd: %d", mVideoLayer->acquireFenceFd, syncFd);
    }
    if (mRGBHandle)
        mRGBRenderer->render();
    return 0;
}
