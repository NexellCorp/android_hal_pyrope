#define LOG_TAG "ScalerZoomController"

#include <sys/types.h>
#include <sys/stat.h>

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>

#include <ion/ion.h>
#include <ion-private.h>

#include "ScalerZoomController.h"

namespace android {

ScalerZoomController::ScalerZoomController()
    :NXZoomController(),
     Width(0),
     Height(0),
     BaseWidth(0),
     BaseHeight(0),
     SrcLeft(0),
     SrcTop(0),
     SrcWidth(0),
     SrcHeight(0)
{
    memset(&ZoomContext, 0, sizeof(struct scale_ctx));
}

ScalerZoomController::ScalerZoomController(int cropLeft,
        int cropTop,
        int cropWidth,
        int cropHeight,
        int baseWidth,
        int baseHeight,
        int dstWidth,
        int dstHeight)
    :NXZoomController(),
     Width(dstWidth),
     Height(dstHeight),
     BaseWidth(baseWidth),
     BaseHeight(baseHeight),
     SrcLeft(0),
     SrcTop(0),
     SrcWidth(0),
     SrcHeight(0)
{
    memset(&ZoomContext, 0, sizeof(struct scale_ctx));
    setCrop(cropLeft, cropTop, cropWidth, cropHeight);
}

ScalerZoomController::~ScalerZoomController()
{
}

void ScalerZoomController::useDefault()
{
    Mutex::Autolock l(ZoomLock);
    ZoomContext.left = SrcLeft;
    ZoomContext.top  = SrcTop;
    ZoomContext.src_width  = BaseWidth;
    ZoomContext.src_height = BaseHeight;
    ZoomContext.dst_width  = Width;
    ZoomContext.dst_height = Height;
}

bool ScalerZoomController::handleZoom(struct nxp_vid_buffer *srcBuffer, private_handle_t const *dstHandle)
{
    Mutex::Autolock l(ZoomLock);
    if (nxScalerRun(srcBuffer, dstHandle, &ZoomContext)) {
        ALOGE("failed to nxScalerRun()");
        return false;
    }
    return true;
}

bool ScalerZoomController::handleZoom(struct nxp_vid_buffer *srcBuffer, struct nxp_vid_buffer *dstBuffer)
{
    Mutex::Autolock l(ZoomLock);
    if (nxScalerRun(srcBuffer, dstBuffer, &ZoomContext)) {
        ALOGE("failed to nxScalerRun()");
        return false;
    }
    return true;
}

bool ScalerZoomController::handleZoom(private_handle_t const *srcHandle, struct nxp_vid_buffer *dstBuffer)
{
    Mutex::Autolock l(ZoomLock);
    if (nxScalerRun(srcHandle, dstBuffer, &ZoomContext)) {
        ALOGE("failed to nxScalerRun()");
        return false;
    }
    return true;
}

bool ScalerZoomController::handleZoom(private_handle_t const *srcHandle, private_handle_t const *dstHandle)
{
    Mutex::Autolock l(ZoomLock);
    if (nxScalerRun(srcHandle, dstHandle, &ZoomContext)) {
        ALOGE("failed to nxScalerRun()");
        return false;
    }
    return true;
}

void ScalerZoomController::setCrop(int left, int top, int width, int height)
{
    if (SrcLeft != left || SrcTop != top || SrcWidth != width || SrcHeight != height) {
        SrcLeft = left;
        SrcTop = top;
        SrcWidth = width;
        SrcHeight = height;

        int calcLeft, calcTop, calcWidth, calcHeight;

        if (!Width || !Height || !BaseWidth || !BaseHeight) {
            ALOGE("invalid wxh(%dx%d), base wxh(%dx%d)", Width, Height, BaseWidth, BaseHeight);
            return;
        }

        if (BaseWidth == width) {
            calcLeft  = 0;
            calcWidth = Width;
        } else {
            calcLeft  = left * Width / BaseWidth;
            calcWidth = width * Width / BaseWidth;
        }

        if (BaseHeight == height) {
            calcTop = 0;
            calcHeight = Height;
        } else {
            calcTop = top * Height / BaseHeight;
            calcHeight = height * Height / BaseHeight;
        }

        // align 32pixel for cb,cr 16pixel align
        calcLeft = (calcLeft + 31) & (~31);

        // check!
        if ((calcLeft + calcWidth) > Width)
            calcWidth = Width - calcLeft;
        if ((calcTop + calcHeight) > Height)
            calcHeight = Height - calcTop;
        Mutex::Autolock l(ZoomLock);
        ZoomContext.left = calcLeft;
        ZoomContext.top  = calcTop;
        ZoomContext.src_width  = calcWidth;
        ZoomContext.src_height = calcHeight;
        ZoomContext.dst_width  = Width;
        ZoomContext.dst_height = Height;

        ALOGD("Width %d, Height %d, BaseWidth %d, BaseHeight %d, left %d, top %d, src_width %d, src_height %d, dst_width %d, dst_height %d, src_code 0x%x, dst_code 0x%x",
                Width, Height, BaseWidth, BaseHeight, calcLeft, calcTop, calcWidth, calcHeight, Width, Height, ZoomContext.src_code, ZoomContext.dst_code);
    }
}

}; // namespace
