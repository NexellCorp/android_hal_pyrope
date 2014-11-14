#ifndef _NX_STREAM_MANAGER_H
#define _NX_STREAM_MANAGER_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>

#include "Constants.h"

namespace android {

class NXCameraHWInterface2;
class NXStreamThread;
class NXStream;

class NXStreamManager : virtual public RefBase
{
public:
    NXStreamManager(NXCameraHWInterface2 *parent)
        :Parent(parent) {
    }
    virtual ~NXStreamManager() {
    }

    int allocateStream(uint32_t width, uint32_t height, int format, const camera2_stream_ops_t *streamOps,
            uint32_t *streamId, uint32_t *formatActual, uint32_t *usage, uint32_t *maxBuffers, bool useSensorZoom);
    int removeStream(uint32_t streamId);

    NXStreamThread *getStreamThread(uint32_t streamId) {
        if(StreamThreads.indexOfKey(streamId) >= 0)
            return StreamThreads.valueFor(streamId).get();
        return NULL;
    }

private:
    NXCameraHWInterface2 *Parent;
    KeyedVector<int32_t, sp<NXStreamThread> > StreamThreads;

    uint32_t whatStreamAllocate(int format);

    int setStreamThread(uint32_t streamId, sp<NXStreamThread> thread) {
        if(StreamThreads.indexOfKey(streamId) >= 0)
            return ALREADY_EXISTS;
        StreamThreads.add(streamId, thread);
        return OK;
    }
};

}; // namespace

#endif
