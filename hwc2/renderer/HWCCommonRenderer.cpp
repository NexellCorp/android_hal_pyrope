#undef LOG_TAG
#define LOG_TAG     "HWCCommonRenderer"

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

namespace android {
// using namespace android;

HWCCommonRenderer::HWCCommonRenderer(int id, int maxBufferCount, int planeNum)
    :HWCRenderer(id),
    mMaxBufferCount(maxBufferCount),
    mOutCount(0),
    mOutIndex(0),
    mPlaneNum(planeNum),
    mStarted(false)
{
}

HWCCommonRenderer::~HWCCommonRenderer()
{
}

int HWCCommonRenderer::render(int *fenceFd)
{
    if (mHandle) {
        int ret;
        private_handle_t const *hnd = mHandle;

        if (mOutCount > 1) {
            int dqIdx;
            ret = v4l2_dqbuf(mID, mPlaneNum, &dqIdx, NULL);
            if (ret < 0) {
                ALOGE("failed to v4l2_dqbuf()");
                return ret;
            }
            mOutCount--;
        }

        ret = v4l2_qbuf(mID, mPlaneNum, mOutIndex, hnd, -1, NULL, fenceFd, NULL);
        if (ret < 0) {
            ALOGE("failed to v4l2_qbuf()");
            return ret;
        }

        if (!mStarted) {
            ret = v4l2_streamon(mID);
            if (ret < 0) {
                ALOGE("failed to v4l2_streamon()");
                return ret;
            }
            mStarted = true;
            ALOGD("Start");
        }

        mOutCount++;
        mOutIndex++;
        mOutIndex %= mMaxBufferCount;

        mHandle = NULL;
    }

    return 0;
}

int HWCCommonRenderer::stop()
{
    if (mStarted) {
        v4l2_streamoff(mID);
        mStarted = false;
        mOutCount = 0;
        mOutIndex = 0;
        ALOGD("Stop");
    }
    return 0;
}

}; // namespace
