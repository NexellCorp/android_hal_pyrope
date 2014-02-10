#ifndef _SCALER_ZOOM_CONTROLLER_H
#define _SCALER_ZOOM_CONTROLLER_H

#include <utils/Thread.h>
#include <NXScaler.h>
#include "NXZoomController.h"

namespace android {

class ScalerZoomController: public NXZoomController
{
public:
    ScalerZoomController();
    ScalerZoomController(int cropLeft, int cropTop, int cropWidth, int cropHeight, int baseWidth, int baseHeight, int dstWidth, int dstHeight);
    virtual ~ScalerZoomController();

    // overriding
    virtual void setBase(int baseWidth, int baseHeight) {
        BaseWidth  = baseWidth;
        BaseHeight = baseHeight;
    }
    virtual void setSource(int width, int height) {
        SrcWidth = width;
        SrcHeight = height;
    }
    virtual void SetDest(int width, int height) {
        Width = width;
        Height = height;
    }
    virtual void setFormat(int srcFormat, int dstFormat) {
        ZoomContext.src_code = srcFormat;
        ZoomContext.dst_code = dstFormat;
    }
    virtual void setCrop(int left, int top, int width, int height);
    virtual void useDefault();
    virtual bool handleZoom(struct nxp_vid_buffer *srcBuffer, private_handle_t const *dstHandle);
    virtual bool handleZoom(struct nxp_vid_buffer *srcBuffer, struct nxp_vid_buffer *dstBuffer);
    virtual bool handleZoom(private_handle_t const *srcHandle, struct nxp_vid_buffer *dstBuffer);
    virtual bool handleZoom(private_handle_t const *srcHandle, private_handle_t const *dstHandle);
    virtual bool isZoomAvaliable() {
        return ZoomContext.left != 0 ||
               ZoomContext.top  != 0;
    }

private:
    int Width;
    int Height;
    int BaseWidth;
    int BaseHeight;

    int SrcLeft;
    int SrcTop;
    int SrcWidth;
    int SrcHeight;

    struct scale_ctx ZoomContext;
    Mutex ZoomLock;
};

}; // namespace

#endif
