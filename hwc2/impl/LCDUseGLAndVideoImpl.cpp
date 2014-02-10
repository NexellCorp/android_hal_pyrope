#undef LOG_TAG
#define LOG_TAG     "LCDUseGLAndVideoImpl"

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
#include "LCDRGBRenderer.h"
#include "HWCCommonRenderer.h"

#include "HWCImpl.h"
#include "LCDCommonImpl.h"
#include "LCDUseGLAndVideoImpl.h"

using namespace android;

LCDUseGLAndVideoImpl::LCDUseGLAndVideoImpl(int rgbID, int videoID)
    :LCDCommonImpl(rgbID, videoID),
    mRGBRenderer(NULL),
    mVideoRenderer(NULL),
    mRGBHandle(NULL),
    mVideoHandle(NULL),
    mOverlayConfigured(false),
    mRGBLayerIndex(-1),
    mOverlayLayerIndex(-1)
{
    init();
}

LCDUseGLAndVideoImpl::LCDUseGLAndVideoImpl(int rgbID, int videoID, int width, int height)
    :LCDCommonImpl(rgbID, videoID, width, height),
    mRGBRenderer(NULL),
    mVideoRenderer(NULL),
    mRGBHandle(NULL),
    mVideoHandle(NULL),
    mOverlayConfigured(false),
    mRGBLayerIndex(-1),
    mOverlayLayerIndex(-1)
{
    init();
}

LCDUseGLAndVideoImpl::~LCDUseGLAndVideoImpl()
{
    if (mRGBRenderer)
        delete mRGBRenderer;
    if (mVideoRenderer)
        delete mVideoRenderer;
}

void LCDUseGLAndVideoImpl::init()
{
    mRGBRenderer = new LCDRGBRenderer(mRgbID);
    if (!mRGBRenderer)
        ALOGE("FATAL: can't create RGBRenderer");

    mVideoRenderer = new HWCCommonRenderer(mVideoID, 4);
    if (!mVideoRenderer)
        ALOGE("FATAL: can't create VideoRenderer");
}

int LCDUseGLAndVideoImpl::configOverlay(struct hwc_layer_1 &layer)
{
    int ret;

    ALOGD("configOverlay");

    ret = v4l2_set_format(mVideoID,
            layer.sourceCrop.right - layer.sourceCrop.left,
            layer.sourceCrop.bottom - layer.sourceCrop.top,
            V4L2_PIX_FMT_YUV420M);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_format()");
        return ret;
    }

    mLeft = layer.displayFrame.left;
    mTop  = layer.displayFrame.top;
    mRight = layer.displayFrame.right;
    mBottom = layer.displayFrame.bottom;

    ret = v4l2_set_crop(mVideoID, mLeft, mTop, mRight - mLeft, mBottom - mTop);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_crop()");
        return ret;
    }

    ret = v4l2_reqbuf(mVideoID, 4);
    if (ret < 0) {
        ALOGE("failed to v4l2_reqbuf()");
        return ret;
    }

    ret = v4l2_set_ctrl(mVideoID, V4L2_CID_MLC_VID_PRIORITY, 1);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_ctrl()");
        return ret;
    }

    mOverlayConfigured = true;
    return 0;
}

int LCDUseGLAndVideoImpl::configCrop(struct hwc_layer_1 &layer)
{
    if (mLeft != layer.displayFrame.left ||
        mTop != layer.displayFrame.top ||
        mRight != layer.displayFrame.right ||
        mBottom != layer.displayFrame.bottom) {
        mLeft = layer.displayFrame.left;
        mTop  = layer.displayFrame.top;
        mRight = layer.displayFrame.right;
        mBottom = layer.displayFrame.bottom;

        int ret = v4l2_set_crop(mVideoID, mLeft, mTop, mRight - mLeft, mBottom - mTop);
        if (ret < 0)
            ALOGE("failed to v4l2_set_crop()");

        return ret;
    } else {
        return 0;
    }
}

int LCDUseGLAndVideoImpl::prepare(hwc_display_contents_1_t *contents)
{
    mRGBLayerIndex = -1;
    mOverlayLayerIndex = -1;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            mRGBLayerIndex = i;
            ALOGV("prepare: rgb %d", i);
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND)
            continue;

        if (mOverlayLayerIndex == -1 && canOverlay(layer)) {
            layer.compositionType = HWC_OVERLAY;
            layer.hints |= HWC_HINT_CLEAR_FB;
            mOverlayLayerIndex = i;
            ALOGV("prepare: overlay %d", i);
            continue;
        }

        layer.compositionType = HWC_FRAMEBUFFER;
    }

    return 0;
}

int LCDUseGLAndVideoImpl::set(hwc_display_contents_1_t *contents, void *unused)
{
    mRGBHandle = NULL;
    mVideoHandle = NULL;

    ALOGV("set: rgb %d, overlay %d", mRGBLayerIndex, mOverlayLayerIndex);
    if (mOverlayLayerIndex >= 0) {
        mVideoHandle = reinterpret_cast<private_handle_t const *>(contents->hwLayers[mOverlayLayerIndex].handle);
        mVideoRenderer->setHandle(mVideoHandle);
        ALOGV("Set Video Handle: %p", mVideoHandle);
    }

    if (mRGBLayerIndex >= 0) {
        mRGBHandle = reinterpret_cast<private_handle_t const *>(contents->hwLayers[mRGBLayerIndex].handle);
        mRGBRenderer->setHandle(mRGBHandle);
        ALOGV("Set RGB Handle: %p", mRGBHandle);
    }

    if (!mOverlayConfigured && mVideoHandle) {
        configOverlay(contents->hwLayers[mOverlayLayerIndex]);
        mVideoOffCount = 0;
    } else if (mOverlayConfigured && !mVideoHandle) {
        mVideoOffCount++;
        if (mVideoOffCount > 1) {
            ALOGD("stop video layer");
            mVideoRenderer->stop();
            mOverlayConfigured = false;
        }
    } else if (mVideoHandle){
        mVideoOffCount = 0;
        configCrop(contents->hwLayers[mOverlayLayerIndex]);
    }
    return 0;
}

private_handle_t const *LCDUseGLAndVideoImpl::getRgbHandle()
{
    return mRGBHandle;
}

private_handle_t const *LCDUseGLAndVideoImpl::getVideoHandle()
{
    return mVideoHandle;
}

int LCDUseGLAndVideoImpl::render()
{
    ALOGV("Render");
    if (mVideoHandle)
        mVideoRenderer->render();
    if (mRGBHandle)
        mRGBRenderer->render();

    return 0;
}
