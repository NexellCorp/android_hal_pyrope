#ifndef _NX_STREAM_THREAD_H
#define _NX_STREAM_THREAD_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <utils/Thread.h>
#include <utils/Atomic.h>
#include <utils/KeyedVector.h>
#include <gralloc_priv.h>
#include <nxp-v4l2.h>
#include "NXCameraHWInterface2.h"
#include "NXCommandThread.h"
#include "NXStream.h"
#include "NXZoomController.h"
#include "NXStreamManager.h"
#include "NXStreamThread.h"

#define CHECK_AND_EXIT() do { \
    if (getState() == STATE_WAIT_EXIT) { \
        ALOGD("%d exit", __LINE__); \
        setState(STATE_EXIT); \
        return false; \
    } \
} while (0)

#if 0
#define ERROR_EXIT() do { \
    v4l2_streamoff(Id); \
    setState(STATE_EXIT); \
    return false; \
} while (0)
#else
#define ERROR_EXIT() do { \
    setState(STATE_EXIT); \
    return false; \
} while (0)
#endif

namespace android {

class NXStreamThread:
    virtual public Thread,
    virtual public NXCommandThread::CommandListener
{
public:
    enum {
        STATE_EXIT = 0,
        STATE_RUNNING,
        STATE_WAIT_EXIT
    } STATE;

    NXStreamThread(int width, int height, sp<NXStreamManager> &streamManager);
    NXStreamThread(nxp_v4l2_id id,
            int width,
            int height,
            sp<NXZoomController> &zoomController,
            sp<NXStreamManager> &streamManager);
    virtual ~NXStreamThread();

    int addStream(int streamId, sp<NXStream> stream);
    NXStream *getStream(int streamId);
    int removeStream(int streamId);

    int registerStreamBuffers(int streamId, int numBuffers, buffer_handle_t *registerBuffers);
    void release();

    virtual status_t readyToRun();

    virtual void onCommand(int32_t streamId, camera_metadata_t *metadata) = 0;
    virtual void onZoomChanged(int left, int top, int width, int height, int baseWidth, int baseHeight);

    virtual status_t start(int streamId, char *threadName, unsigned int priority = PRIORITY_DEFAULT);
    virtual status_t stop(bool waitExit, bool streamOff = true);

    size_t getActiveBufferCount(void) {
        Mutex::Autolock l(StreamLock);
        NXStream *stream;
        size_t size = 0;
        for (size_t i = 0; i < Streams.size(); i++) {
            stream = Streams[i].get();
            size += stream->getQueuedSize();
        }
        return size;
    }

    size_t getStreamCount(void) const {
        return Streams.size();
    }

    size_t getActiveStreamId(void) const {
        return ActiveStreamId;
    }

    NXStream* getActiveStream() {
        Mutex::Autolock l(StreamLock);
        return Streams.valueFor(ActiveStreamId).get();
    }

    bool setActiveStreamId(int32_t streamId) {
        Mutex::Autolock l(StreamLock);
        if (Streams.indexOfKey(streamId) >= 0) {
            ActiveStreamId = streamId;
            return true;
        }
        return false;
    }

    status_t getFormat(uint32_t &width, uint32_t &height, uint32_t &format) const;

    private_handle_t const *getLastBuffer(int &width, int &height) {
        NXStream *stream = getActiveStream();
        if (!stream)
            return NULL;
        private_handle_t const *hnd = stream->getLastEnqueuedBuffer();
        width = Width;
        height = Height;
        return hnd;
    }

    void setState(int32_t state) {
        android_atomic_release_cas(State, state, &State);
    }

    int32_t getState() const {
        return android_atomic_acquire_load(&State);
    }

    bool isRunning() const {
        return getState() == STATE_RUNNING;
    }

    virtual void pause() {
        Mutex::Autolock l(PauseLock);
        Pausing = true;
    }
    virtual void resume() {
        Mutex::Autolock l(PauseLock);
        Pausing = false;
        SignalResume.signal();
    }
    virtual void waitResume() {
        Mutex::Autolock l(PauseLock);
        while (Pausing) {
            SignalResume.wait(PauseLock);
        }
    }

protected:
    virtual void init(nxp_v4l2_id id) = 0;

protected:
    char ThreadName[MAX_THREAD_NAME];

    KeyedVector<int32_t, sp<NXStream> > Streams;

    nxp_v4l2_id Id;
    int Width;
    int Height;
    sp<NXZoomController> ZoomController;
    uint32_t PixelIndex;

    nxp_v4l2_id SensorId;
    uint32_t InitialSkipCount;
    NXCameraBoardSensor *Sensor;

    int ActiveStreamId; // current using stream id

    sp<NXStreamManager> StreamManager;

private:
    virtual bool threadLoop() = 0;

private:
    Mutex StreamLock; // for Streams

    Mutex PauseLock;
    Condition SignalResume;
    bool Pausing;

    volatile int32_t State;
};

}; // namespace

#endif
