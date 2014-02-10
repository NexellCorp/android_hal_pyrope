#undef LOG_TAG
#define LOG_TAG     "HDMIUseMirrorAndVideoImpl"

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
#include "HWCCommonRenderer.h"
#include "HDMIMirrorRenderer.h"

#include "HWCImpl.h"
#include "HDMICommonImpl.h"
#include "HDMIUseMirrorAndVideoImpl.h"

using namespace android;

HDMIUseMirrorAndVideoImpl::HDMIUseMirrorAndVideoImpl(HDMICommonImpl *mirrorImpl, HDMICommonImpl *glAndVideoImpl)
    :mMirrorImpl(mirrorImpl),
     mGLAndVideoImpl(glAndVideoImpl)
{
}

HDMIUseMirrorAndVideoImpl::~HDMIUseMirrorAndVideoImpl()
{
    if (mMirrorImpl)
        delete mMirrorImpl;
    if (mGLAndVideoImpl)
        delete mGLAndVideoImpl;
}

int HDMIUseMirrorAndVideoImpl::enable()
{
    if (!mEnabled) {
        mMirrorImpl->enable();
        // mGLAndVideoImpl->enable();
        mEnabled = true;
    }
    return 0;
}

int HDMIUseMirrorAndVideoImpl::disable()
{
    if (mMirrorImpl->getEnabled())
        mMirrorImpl->disable();
    if (mGLAndVideoImpl->getEnabled())
        mGLAndVideoImpl->disable();
    mEnabled = false;
    return 0;
}
bool HDMIUseMirrorAndVideoImpl::hasVideoLayer(hwc_display_contents_1_t *contents)
{
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET)
            continue;

        if (layer.compositionType == HWC_BACKGROUND)
            continue;

        if (canOverlay(layer))
            return true;
    }

    return false;
}

int HDMIUseMirrorAndVideoImpl::prepare(hwc_display_contents_1_t *contents)
{
    if (hasVideoLayer(contents)) {
        // use GLAndVideoImpl
        if (mUseMirror) {
            if (mMirrorImpl->getEnabled())
                mMirrorImpl->disable();
            if (!mGLAndVideoImpl->getEnabled())
                mGLAndVideoImpl->enable();
            mUseMirror = false;
        }
        return mGLAndVideoImpl->prepare(contents);
    } else {
        if (!mUseMirror) {
            if (mGLAndVideoImpl->getEnabled())
                mGLAndVideoImpl->disable();
            if (!mMirrorImpl->getEnabled())
                mMirrorImpl->enable();
            mUseMirror = true;
        }
        return mMirrorImpl->prepare(contents);
    }
}

int HDMIUseMirrorAndVideoImpl::set(hwc_display_contents_1_t *contents, void *handle)
{
    if (mUseMirror)
        return mMirrorImpl->set(contents, handle);
    else
        return mGLAndVideoImpl->set(contents, handle);
}

private_handle_t const *HDMIUseMirrorAndVideoImpl::getRgbHandle()
{
    if (mUseMirror)
        return mMirrorImpl->getRgbHandle();
    else
        return mGLAndVideoImpl->getRgbHandle();
}

private_handle_t const *HDMIUseMirrorAndVideoImpl::getVideoHandle()
{
    if (mUseMirror)
        return mMirrorImpl->getVideoHandle();
    else
        return mGLAndVideoImpl->getVideoHandle();
}

int HDMIUseMirrorAndVideoImpl::render()
{
    if (mUseMirror)
        return mMirrorImpl->render();
    else
        return mGLAndVideoImpl->render();
}
