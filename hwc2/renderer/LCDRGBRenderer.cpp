#undef LOG_TAG
#define LOG_TAG     "LCDRGBRenderer"

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

namespace android {

LCDRGBRenderer::LCDRGBRenderer(int id)
    :HWCRenderer(id),
    mFBFd(-1)
{
}

LCDRGBRenderer::~LCDRGBRenderer()
{
    if (mFBFd >= 0)
        close(mFBFd);
}

int LCDRGBRenderer::render()
{
    if (mHandle) {
        if (mFBFd < 0) {
            mFBFd = open("/dev/graphics/fb0", O_RDWR);
            if (mFBFd < 0) {
                ALOGE("failed to open framebuffer");
                return -EINVAL;
            }

            if (ioctl(mFBFd, FBIOGET_VSCREENINFO, &mFBVarInfo) == -1) {
                ALOGE("can't get fb_var_screeninfo");
                return -EINVAL;
            }

            struct fb_fix_screeninfo finfo;
            if (ioctl(mFBFd, FBIOGET_FSCREENINFO, &finfo) == -1) {
                ALOGE("can't get fb_fix_screeninfo");
                return -EINVAL;
            }
            mFBLineLength = finfo.line_length;
        }

        if (mHandle->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
            mFBVarInfo.activate = FB_ACTIVATE_VBL;
            mFBVarInfo.yoffset  = mHandle->offset / mFBLineLength;
            if (ioctl(mFBFd, FBIOPUT_VSCREENINFO, &mFBVarInfo) == -1) {
                ALOGE("failed to FBIOPUT_VSCREENINFO");
                return -EINVAL;
            }
        } else {
            ALOGE("can't render not framebuffer type handle");
            return -EINVAL;
        }

        mHandle = NULL;
    }

    return 0;
}

}; // namespace
