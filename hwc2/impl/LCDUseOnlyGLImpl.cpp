#undef LOG_TAG
#define LOG_TAG     "LCDUseOnlyGLImpl"

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

#include "HWCImpl.h"
#include "LCDCommonImpl.h"
#include "LCDUseOnlyGLImpl.h"

using namespace android;

LCDUseOnlyGLImpl::LCDUseOnlyGLImpl(int rgbID)
    :LCDCommonImpl(rgbID, -1),
    mRGBRenderer(NULL),
    mRGBLayerIndex(-1),
    mRGBHandle(NULL)
{
    init();
}

LCDUseOnlyGLImpl::LCDUseOnlyGLImpl(int rgbID, int width, int height)
    :LCDCommonImpl(rgbID, -1, width, height),
    mRGBRenderer(NULL),
    mRGBLayerIndex(-1),
    mRGBHandle(NULL)
{
    init();
}

LCDUseOnlyGLImpl::~LCDUseOnlyGLImpl()
{
    if (mRGBRenderer)
        delete mRGBRenderer;
}

void LCDUseOnlyGLImpl::init()
{
    mRGBRenderer = new LCDRGBRenderer(mRgbID);
    if (!mRGBRenderer)
        ALOGE("FATAL: can't create RGBRenderer");
}

int LCDUseOnlyGLImpl::prepare(hwc_display_contents_1_t *contents)
{
    //mRGBLayerIndex = -1;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            //mRGBLayerIndex = i;
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND)
            continue;

        layer.compositionType = HWC_FRAMEBUFFER;
    }

    return 0;
}

int LCDUseOnlyGLImpl::set(hwc_display_contents_1_t *contents, void *unused)
{
    mRGBHandle = reinterpret_cast<private_handle_t const *>(contents->hwLayers[contents->numHwLayers - 1].handle);
    return mRGBRenderer->setHandle(mRGBHandle);
    return 0;
}

private_handle_t const *LCDUseOnlyGLImpl::getRgbHandle()
{
    return mRGBHandle;
}

private_handle_t const *LCDUseOnlyGLImpl::getVideoHandle()
{
    return NULL;
}

int LCDUseOnlyGLImpl::render()
{
    if (mRGBHandle)
        return mRGBRenderer->render();
    else
        return 0;
}
