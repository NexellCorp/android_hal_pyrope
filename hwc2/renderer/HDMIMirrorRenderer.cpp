#undef LOG_TAG
#define LOG_TAG     "HDMIMirrorRenderer"

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

namespace android {
// using namespace android;

HDMIMirrorRenderer::HDMIMirrorRenderer(int id, int maxBufferCount)
    :HWCRenderer(id),
    mFBFd(-1),
    mFBSize(0),
    mMaxBufferCount(maxBufferCount),
    mOutCount(0),
    mOutIndex(0),
    mStarted(false),
    mHandle(NULL)
{
    for (int i = 0; i < mMaxBufferCount; i++)
        mMirrorHandleArray[i] = NULL;
}

HDMIMirrorRenderer::~HDMIMirrorRenderer()
{
    struct private_handle_t *hnd;
    for (int i = 0; i < mMaxBufferCount; i++) {
        hnd = mMirrorHandleArray[i];
        if (hnd) delete hnd;
    }

    if (mFBFd >= 0)
        close(mFBFd);
}

#define NXPFB_GET_FB_FD _IOWR('N', 101, __u32)
int HDMIMirrorRenderer::setHandle(private_handle_t const *handle)
{
#if 0
    if (!handle)
        return 0;

    if (mFBFd < 0) {
        mFBFd = open("/dev/graphics/fb0", O_RDWR);
        if (mFBFd < 0) {
            ALOGE("failed to open framebuffer");
            return -EINVAL;
        }

        struct fb_var_screeninfo info;
        if (ioctl(mFBFd, FBIOGET_VSCREENINFO, &info) == -1) {
            ALOGE("can't get fb_var_screeninfo");
            return -EINVAL;
        }

        struct fb_fix_screeninfo finfo;
        if (ioctl(mFBFd, FBIOGET_FSCREENINFO, &finfo) == -1) {
            ALOGE("can't get fb_fix_screeninfo");
            return -EINVAL;
        }

        mFBSize = finfo.line_length * info.yres;
    }

    mMirrorIndex = handle->offset / mFBSize;
    if (!mMirrorHandleArray[mMirrorIndex]) {
        private_handle_t *hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_ION, mFBSize);
        if (!hnd) {
            ALOGE("can't create handle: index %d", mMirrorIndex);
            return -ENOMEM;
        }
        int share_fd = mMirrorIndex;
        int ret = ioctl(mFBFd, NXPFB_GET_FB_FD, &share_fd);
        if (ret < 0) {
            ALOGE("setHandle: failed to ioctl NXPFB_GET_FB_FD for index %d", mMirrorIndex);
            return ret;
        }
        hnd->share_fd = share_fd;
        hnd->size = mFBSize;
        hnd->offset = 0;
        mMirrorHandleArray[mMirrorIndex] = hnd;
    }
#else
    mHandle = const_cast<private_handle_t *>(handle);
#endif

    return 0;
}

private_handle_t const *HDMIMirrorRenderer::getHandle()
{
#if 0
    return mMirrorHandleArray[mMirrorIndex];
#else
    return mHandle;
#endif
}

int HDMIMirrorRenderer::render()
{
    int ret;
#if 0
    private_handle_t *hnd = mMirrorHandleArray[mMirrorIndex];
#else
    private_handle_t *hnd = mHandle;
#endif

    if (!hnd)
        return 0;

    ret = v4l2_qbuf(mID, 1, mOutIndex, hnd, -1, NULL);
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
        ALOGD("Start");
        mStarted = true;
    }

    mOutCount++;
    mOutIndex++;
    mOutIndex %= mMaxBufferCount;

    if (mOutCount >= mMaxBufferCount) {
        int dqIdx;
        ret = v4l2_dqbuf(mID, 1, &dqIdx, NULL);
        if (ret < 0) {
            ALOGE("failed to v4l2_dqbuf()");
            return ret;
        }
        mOutCount--;
    }

    return 0;
}

int HDMIMirrorRenderer::stop()
{
    if (mStarted) {
        v4l2_streamoff(mID);
        mStarted = false;
        ALOGD("Stop");
    }
    return 0;
}

}; // namespace
