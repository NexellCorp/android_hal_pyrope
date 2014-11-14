#ifndef _NX_STREAM_H
#define _NX_STREAM_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <utils/RefBase.h>
#include <gralloc_priv.h>
#include "Constants.h"
#include "NXQueue.h"

namespace android {

class NXStream : virtual public RefBase
{
public:
    NXStream(int id, int width, int height, int format, const camera2_stream_ops_t *ops, uint32_t maxBuffers)
        : StreamId(id),
          Width(width),
          Height(height),
          Format(format),
          StreamOps(ops),
          MaxBuffers(maxBuffers),
          BufferInited(false)
    {
    }
    virtual ~NXStream() {
    }

    status_t registerBuffer(int numBuffers, buffer_handle_t *buffers);
    status_t initBuffer();
    status_t dequeueBuffer(buffer_handle_t **buffer);
    status_t enqueueBuffer(nsecs_t timestamp);
    status_t cancelBuffer();
    status_t findBufferIndex(buffer_handle_t *buffer);
    void flushBuffer();

    int getWidth() const {
        return Width;
    }
    int getHeight() const {
        return Height;
    }
    int getFormat() const {
        return Format;
    }
    uint32_t getMaxBuffers() const {
        return MaxBuffers;
    }
    uint32_t getNumBuffers() const {
        return NumBuffers;
    }
    private_handle_t const *getBuffer(int index) const {
        return Buffers[index];
    }
    nsecs_t getTimestamp() const {
        return Timestamp;
    }
    void setTimestamp(nsecs_t timestamp) {
        Timestamp = timestamp;
    }

    private_handle_t const *getLastEnqueuedBuffer() {
        return LastEnqueuedBuffer;
    }

    private_handle_t const *getNextBuffer() {
        return reinterpret_cast<private_handle_t const *>(*queue.getHead());
    }

    size_t getQueuedSize() {
        return queue.size();
    }

    const buffer_handle_t *getQueuedBuffer(int index) {
        return queue.getItem(index);
    }

private:
    int StreamId;
    int Width;
    int Height;
    int Format;
    const camera2_stream_ops_t *StreamOps;
    uint32_t MaxBuffers;
    uint32_t NumBuffers;
    private_handle_t const *Buffers[MAX_NUM_FRAMES];
    private_handle_t const *LastEnqueuedBuffer;

    bool BufferInited;
    nsecs_t Timestamp;

    NXQueue<buffer_handle_t *> queue;
};

}; // namespace

#endif
