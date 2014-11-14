#undef LOG_TAG
#define LOG_TAG     "LCDCommonImpl"

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

#include "HWCImpl.h"
#include "LCDCommonImpl.h"

using namespace android;

LCDCommonImpl::LCDCommonImpl()
    :mFBFd(-1)
{
    init();
}

LCDCommonImpl::LCDCommonImpl(int rgbID, int videoID)
    :HWCImpl(rgbID, videoID),
    mFBFd(-1)
{
    init();
}

LCDCommonImpl::LCDCommonImpl(int rgbID, int videoID, int width, int height)
    :HWCImpl(rgbID, videoID, width, height),
    mFBFd(-1)
{
    init();
}


LCDCommonImpl::~LCDCommonImpl()
{
    if (mFBFd >= 0)
        close(mFBFd);
}

void LCDCommonImpl::init()
{
    mFBFd = open("/dev/graphics/fb0", O_RDWR);
    if (mFBFd < 0) {
        ALOGE("can't open framebuffer dev node: /dev/graphics/fb0");
        return;
    }

    mEnabled = true;
}

int LCDCommonImpl::enable()
{
    if (!mEnabled) {
        int ret = ioctl(mFBFd, FBIOBLANK, FB_BLANK_UNBLANK);
        if (ret < 0) {
            ALOGE("failed to enable");
            return ret;
        }
        mEnabled = true;
    }

    return 0;
}

int LCDCommonImpl::disable()
{
    if (mEnabled) {
        int ret = ioctl(mFBFd, FBIOBLANK, FB_BLANK_POWERDOWN);
        if (ret < 0) {
            ALOGE("failed to disable");
            return ret;
        }
        mEnabled = false;
    }

    return 0;
}
