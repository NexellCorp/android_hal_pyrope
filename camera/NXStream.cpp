#ifndef LOG_TAG
#define LOG_TAG "NXStream"
#endif
#include "NXStream.h"

namespace android {

//#define TRACE_BUFFER
#ifdef TRACE_BUFFER
#define trace_buffer(args...)  ALOGD(args)
#else
#define trace_buffer(args...)
#endif

status_t NXStream::registerBuffer(int numBuffers, buffer_handle_t *buffers)
{
    NumBuffers = numBuffers;
    trace_buffer("registerBuffer: buffer num %d", numBuffers);
    for (int i = 0; i < numBuffers; i++) {
        Buffers[i] = reinterpret_cast<private_handle_t const *>(buffers[i]);
    }
    LastEnqueuedBuffer = NULL;
    return OK;
}

status_t NXStream::initBuffer()
{
    if (!BufferInited) {
        status_t res;
        buffer_handle_t *buffer;
        for (uint32_t i = 0; i < MaxBuffers; i++) {
            res = dequeueBuffer(&buffer);
            if (res != NO_ERROR || buffer == NULL) {
                ALOGE("initBuffer streamId %d: failed to dequeue_buffer index %d", StreamId, i);
                // return res;
                break;
            }
        }
        BufferInited = true;
    }
    return OK;
}

status_t NXStream::dequeueBuffer(buffer_handle_t **buffer)
{
    status_t res =  StreamOps->dequeue_buffer(StreamOps, buffer);
    if (res == NO_ERROR) {
        if (findBufferIndex(*buffer) >= 0) {
            queue.queue(*buffer);
            trace_buffer("user buffer dq -> internal buffer eq: %p", *buffer);
            return NO_ERROR;
        }
    }
    return BAD_VALUE;
}

status_t NXStream::enqueueBuffer(nsecs_t timestamp)
{
    if (!queue.isEmpty()) {
        buffer_handle_t *buffer = queue.dequeue();
        if (findBufferIndex(buffer) >= 0) {
            LastEnqueuedBuffer = reinterpret_cast<private_handle_t const *>(*buffer);

            // private_handle_t const *hPrivate = LastEnqueuedBuffer;
            // ALOGD("fds(%d), fds(0x%08x,0x%08x,0x%08x), phys( 0x%08x, 0x%08x, 0x%08x ), base(0x%08x, 0x%08x, 0x%08x)\n",
            // hPrivate->share_fd, hPrivate->share_fds[0], hPrivate->share_fds[1], hPrivate->share_fds[2],
            // hPrivate->phys, hPrivate->phys1, hPrivate->phys2,
            // hPrivate->base, hPrivate->base1, hPrivate->base2 );

            trace_buffer("internal buffer dq -> user buffer eq: %p", buffer);
            status_t ret = StreamOps->enqueue_buffer(StreamOps, timestamp, buffer);
            if (ret != NO_ERROR) {
                StreamOps->cancel_buffer(StreamOps, buffer);
            }
            return ret;
        } else {
            return BAD_VALUE;
        }
    } else {
        ALOGE("enqueueBuffer -> queue is empty!!! : StreamId %d", StreamId);
        return NO_ERROR;
    }
}

status_t NXStream::cancelBuffer()
{
    if (!queue.isEmpty()) {
        buffer_handle_t *buffer = queue.dequeue();
        if (findBufferIndex(buffer) >= 0) {
            trace_buffer("internal buffer dq -> user buffer cancel: %p", buffer);
            return StreamOps->cancel_buffer(StreamOps, buffer);
        } else {
            return NO_ERROR;
        }
    } else {
        ALOGE("cancelBuffer -> queue is empty!!! : StreamId %d", StreamId);
        return NO_ERROR;
    }
}

void NXStream::flushBuffer() {
    ALOGD("flushBuffer entered: queue size %d", queue.size());
    while (!queue.isEmpty()) {
        buffer_handle_t *buffer = queue.dequeue();
#if 0
        if (findBufferIndex(buffer) >= 0)
            StreamOps->cancel_buffer(StreamOps, buffer);
#else
        StreamOps->cancel_buffer(StreamOps, buffer);
#endif
        trace_buffer("user buffer cancel %p", buffer);
    }
    BufferInited = false;
    ALOGD("flushBuffer exit");
}

status_t NXStream::findBufferIndex(buffer_handle_t *buffer)
{
    if (!Buffers) {
        ALOGE("stream %d, No Buffers!!!", StreamId);
        return NO_INIT;
    }

    for (uint32_t i = 0; i < NumBuffers; i++) {
        if (Buffers[i] == *buffer) {
            return i;
        }
    }
    ALOGE("Can't find buffer(%p) : StreamId %d", buffer, StreamId);
    return BAD_VALUE;
}

};
