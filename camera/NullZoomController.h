#ifndef _NULL_ZOOM_CONTROLLER_H
#define _NULL_ZOOM_CONTROLLER_H

#include "NXZoomController.h"

namespace android {

class NullZoomController: public NXZoomController
{
public:
    NullZoomController() {
    }
    virtual ~NullZoomController() {
    }

    // overriding
    virtual void setBase(int baseWidth, int baseHeight) {
    }
    virtual void setCrop(int left, int top, int width, int height) {
    }
    virtual void setFormat(int srcFormat, int dstFormat) {
    }
    virtual bool handleZoom(struct nxp_vid_buffer *srcBuffer, private_handle_t const *dstHandle) {
        return true;
    }
    virtual bool handleZoom(struct nxp_vid_buffer *srcBuffer, struct nxp_vid_buffer *dstBuffer) {
        return true;
    }
    virtual bool handleZoom(private_handle_t const *srcHandle, struct nxp_vid_buffer *dstBuffer) {
        return true;
    }
    virtual bool handleZoom(private_handle_t const *srcHandle, private_handle_t const *dstHandle) {
        return true;
    }
    virtual bool isZoomAvaliable() {
        return false;
    }
    virtual bool allocBuffer(int bufferCount, int width, int height, int format) {
        return true;
    }
    virtual void freeBuffer() {
    }
    virtual struct nxp_vid_buffer *getBuffer(int index) {
        return NULL;
    }
    virtual bool useZoom() {
        return false;
    }
};

}; // namespace

#endif
