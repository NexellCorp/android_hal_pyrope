#undef LOG_TAG
#define LOG_TAG         "HDMIUseRescCommonImpl"

#include <stdio.h>

#include <linux/ion.h>
#include <linux/videodev2.h>
#include <ion/ion.h>

#include <nxp-v4l2.h>
#include <NXPrimitive.h>

#include <cutils/log.h>

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>

#include "HWCImpl.h"
#include "HDMICommonImpl.h"
#include "RescConfigure.h"
#include "HDMIUseRescCommonImpl.h"

using namespace android;

HDMIUseRescCommonImpl::HDMIUseRescCommonImpl(
        int rgbID,
        int videoID,
        int width,
        int height,
        int srcWidth,
        int srcHeight,
        int scaleFactor,
        HDMICommonImpl *impl,
        RescConfigure *rescConfigure
        )
    :HDMICommonImpl(rgbID, videoID, width, height, srcWidth, srcHeight),
    mScaleFactor(scaleFactor),
    mImpl(impl),
    mRescConfigure(rescConfigure),
    mRescConfigured(false)
{
}

HDMIUseRescCommonImpl::~HDMIUseRescCommonImpl()
{
    if (mImpl)
        delete mImpl;
    if (mRescConfigure)
        delete mRescConfigure;
}

int HDMIUseRescCommonImpl::enable()
{
    if (!mEnabled) {
        ALOGD("Enable");
        int ret = v4l2_link(mSubID, nxp_v4l2_resol);
        if (ret < 0) {
            ALOGE("can't link me --> nxp_v4l2_resol");
            return -EINVAL;
        }
        ret = v4l2_link(nxp_v4l2_resol, nxp_v4l2_hdmi);
        if (ret < 0) {
            ALOGE("can't link nxp_v4l2_resol --> nxp_v4l2_hdmi");
            return -EINVAL;
        }
        mEnabled = true;
    }
    return 0;
}

int HDMIUseRescCommonImpl::disable()
{
    if (mEnabled) {
        ALOGD("Disable");
        mImpl->disable();
        v4l2_unlink(nxp_v4l2_resol, nxp_v4l2_hdmi);
        v4l2_unlink(mSubID, nxp_v4l2_resol);
        mRescConfigured = false;
        mEnabled = false;
    }
    return 0;
}

int HDMIUseRescCommonImpl::prepare(hwc_display_contents_1_t *contents)
{
    ALOGV("prepare");
    return mImpl->prepare(contents);
}

int HDMIUseRescCommonImpl::set(hwc_display_contents_1_t *contents, void *handle)
{
    if (!mRescConfigured) {
        ALOGV("set");
        int ret = mRescConfigure->configure(mSrcWidth, mSrcHeight, mWidth, mHeight, V4L2_PIX_FMT_RGB32, mScaleFactor);
        if (ret < 0) {
            ALOGE("set(): failed to RescConfigure->configure()");
            return ret;
        }
        mRescConfigured = true;
    }
    return mImpl->set(contents, handle);
}

private_handle_t const *HDMIUseRescCommonImpl::getRgbHandle()
{
    return mImpl->getRgbHandle();
}

private_handle_t const *HDMIUseRescCommonImpl::getVideoHandle()
{
    return mImpl->getVideoHandle();
}

int HDMIUseRescCommonImpl::render()
{
    return mImpl->render();
}
