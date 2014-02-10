#undef LOG_TAG
#define LOG_TAG     "RescConfigure"

#include <linux/media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <ion/ion.h>
#include <nxp-v4l2.h>

#include <cutils/log.h>

#include "RescConfigure.h"

using namespace android;

int RescConfigure::configure(int srcWidth, int srcHeight, int dstWidth, int dstHeight, uint32_t format, int scaleFactor)
{
    ALOGD("configure(): src %dx%d, dst %dx%d, format 0x%x, scaleFactor %d",
            srcWidth, srcHeight, dstWidth, dstHeight, format, scaleFactor);

    // set resc source format
    int ret = v4l2_set_format_with_pad(nxp_v4l2_resol, 0, srcWidth, srcHeight, format);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_format_with_pad() for resc source");
        return ret;
    }
    // set resc dest format
    // TODO scale factor
    ret = v4l2_set_format_with_pad(nxp_v4l2_resol, 1, dstWidth, dstHeight, format);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_format_with_pad() for resc dest");
        return ret;
    }
    // set resc source crop
    ret = v4l2_set_crop_with_pad(nxp_v4l2_resol, 0, 0, 0, srcWidth, srcHeight);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_crop_with_pad() for resc source");
        return ret;
    }

    return 0;
}
