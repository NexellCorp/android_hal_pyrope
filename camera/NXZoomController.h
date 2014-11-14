#ifndef _NX_ZOOM_CONTROLLER_H
#define _NX_ZOOM_CONTROLLER_H

#include <nxp-v4l2.h>
#include <gralloc_priv.h>

#include <utils/RefBase.h>

namespace android {

// pure class
class NXZoomController : public virtual RefBase
{
public:
    NXZoomController();
    virtual ~NXZoomController();

    virtual void setBase(int baseWidth, int baseHeight) = 0;
    virtual void setSource(int width, int height) {
    }
    virtual void setDest(int width, int height) {
    }
    virtual void setCrop(int left, int top, int srcWidth, int srcHeight) = 0;
    // TODO this is ugly code!!!
    // reset setting to default(ignore calculate width, height, crop)
    virtual void useDefault() {
    }
    virtual void setFormat(int srcFormat, int dstFormat) = 0;
    virtual bool handleZoom(struct nxp_vid_buffer *srcBuffer, private_handle_t const *dstHandle) = 0;
    virtual bool handleZoom(struct nxp_vid_buffer *srcBuffer, struct nxp_vid_buffer *dstBuffer) = 0;
    virtual bool handleZoom(private_handle_t const *srcHandle, struct nxp_vid_buffer *dstBuffer) = 0;
    virtual bool handleZoom(private_handle_t const *srcHandle, private_handle_t const *dstHandle) = 0;
    virtual bool isZoomAvaliable() = 0;

    virtual bool allocBuffer(int bufferCount, int width, int height, int format);
    virtual void freeBuffer();
    virtual struct nxp_vid_buffer *getBuffer(int index);
    virtual bool useZoom() {
        return true;
    }

    int getBufferCount() const {
        return BufferCount;
    }

private:
    int BufferCount;
    bool BufferAllocated;
    struct nxp_vid_buffer *Buffer;
};

}; // namespace

#endif
