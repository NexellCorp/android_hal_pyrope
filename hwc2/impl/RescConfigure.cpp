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

//#define SCALE_STEP      25 // 4%
//#define SCALE_STEP      20 // 5%
//#define SCALE_STEP      10 // 10%
//static void _get_dst_size_by_scalefactor(int *dstWidth, int *dstHeight, int scaleFactor)
//{
    //int width = *dstWidth;
    //int height = *dstHeight;

    //width = width - (width/SCALE_STEP)*scaleFactor;
    //if (width % 2)
        //width++;

    //height = height - (height/SCALE_STEP)*scaleFactor;
    //if (height % 2)
        //height++;

    //*dstWidth = width;
    //*dstHeight = height;
    //ALOGD("Scaled dst resolution: %dx%d", *dstWidth, *dstHeight);
//}

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
    //_get_dst_size_by_scalefactor(&dstWidth, &dstHeight, scaleFactor);

    if (srcWidth == 1920 && srcHeight == 1080) {
        dstWidth = 1728;
        dstHeight = 972;
    } else if (srcWidth == 1280 && srcHeight == 720) {
        dstWidth = 1066;
        dstHeight = 600;
    } else if (srcWidth == 720 && srcHeight == 576) {
        dstWidth = 624;
        dstHeight = 500;
    } else {
        dstWidth = srcWidth;
        dstHeight = srcHeight;
    }

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
