#define LOG_TAG "NXZoomController"

#include <sys/types.h>
#include <sys/stat.h>

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>

#include <ion/ion.h>
#include <ion-private.h>

#include <NXAllocator.h>

#include "NXZoomController.h"

namespace android {

NXZoomController::NXZoomController()
    :BufferCount(0),
     BufferAllocated(false),
     Buffer(NULL)
{
}

NXZoomController::~NXZoomController()
{
    freeBuffer();
}

bool NXZoomController::allocBuffer(int bufferCount, int width, int height, int format)
{
    if (BufferAllocated)
        return true;
    Buffer = new nxp_vid_buffer[bufferCount];
    if (!Buffer) {
        ALOGE("failed to alloc nxp_vid_buffer object");
        return false;
    }
    memset(Buffer, 0, bufferCount * sizeof(struct nxp_vid_buffer));

    bool ret = allocateBuffer(Buffer, bufferCount, width, height, format);
    if (!ret) {
        ALOGE("failed to alloc buffer");
        return ret;
    }

    BufferCount = bufferCount;
    BufferAllocated = true;
    return true;
}

void NXZoomController::freeBuffer()
{
    if (!BufferAllocated)
        return;

    ::freeBuffer(Buffer, BufferCount);
    delete [] Buffer;
    Buffer = NULL;
    BufferCount = 0;
    BufferAllocated = false;
}

struct nxp_vid_buffer *NXZoomController::getBuffer(int index)
{
    if (BufferAllocated)
        return &Buffer[index];
    return NULL;
}

}; // namespace
