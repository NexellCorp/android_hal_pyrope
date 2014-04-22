#define LOG_TAG "NXStreamThread"

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <gralloc_priv.h>
#include "NXStreamThread.h"
#include <stdio.h>
#include "libnxjpeg.h"

#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <ion/ion.h>
#include <ion-private.h>

// #define TRACE
#ifdef TRACE
#define trace_in() ALOGD("%s entered", __func__)
#define trace_exit() ALOGD("%s exit", __func__)
#else
#define trace_in()
#define trace_exit()
#endif

namespace android {

NXStreamThread::NXStreamThread(nxp_v4l2_id id,
        int width,
        int height,
        sp<NXZoomController> &zoomController,
        sp<NXStreamManager> &streamManager)
    : Thread(false),
      Id(id),
      Width(width),
      Height(height),
      ZoomController(zoomController),
      PixelIndex(YUV420_PLANAR),
      ActiveStreamId(0),
      StreamManager(streamManager),
      Pausing(false),
      State(STATE_EXIT)
{
    ZoomController->setFormat(PIXINDEX2PIXCODE(PixelIndex), PIXINDEX2PIXCODE(PixelIndex));
    ThreadName[0] = '\0';
}

NXStreamThread::NXStreamThread(int width, int height, sp<NXStreamManager> &streamManager)
    : Thread(false),
      Width(width),
      Height(height),
      PixelIndex(YUV420_PLANAR),
      ActiveStreamId(0),
      StreamManager(streamManager),
      Pausing(false),
      State(STATE_EXIT)
{
    ThreadName[0] = '\0';
}

NXStreamThread::~NXStreamThread()
{
}

void NXStreamThread::init(nxp_v4l2_id id)
{
    if (id == nxp_v4l2_decimator0 || id == nxp_v4l2_clipper0)
        SensorId = nxp_v4l2_sensor0;
    else
        SensorId = nxp_v4l2_sensor1;

    Sensor = get_board_camera_sensor_by_v4l2_id(SensorId);
    InitialSkipCount = 0;
}

/* id is stream id */
int NXStreamThread::addStream(int streamId, sp<NXStream> stream)
{
    Mutex::Autolock l(StreamLock);
    if (Streams.indexOfKey(streamId) >= 0) {
        ALOGE("addStream: id %d already added!!!", streamId);
        return ALREADY_EXISTS;
    }
    Streams.add(streamId, stream);
    ALOGD("addStream(): %d", streamId);
    return NO_ERROR;
}

/* id is stream id */
NXStream *NXStreamThread::getStream(int streamId)
{
    Mutex::Autolock l(StreamLock);
    if (Streams.indexOfKey(streamId) >= 0)
        return Streams.valueFor(streamId).get();
    else
        return NULL;
}

/* id is stream id */
int NXStreamThread::removeStream(int streamId)
{
    ALOGD("%s: %d", __func__, streamId);
    Mutex::Autolock l(StreamLock);
    if (Streams.indexOfKey(streamId) < 0) {
        ALOGE("removeStream: id %d is not added", streamId);
        return NO_ERROR;
    }
    Streams.removeItem(streamId);
    ALOGD("removeStream(): %d", streamId);

    return NO_ERROR;
}

/* id is stream id */
int NXStreamThread::registerStreamBuffers(int streamId, int numBuffers, buffer_handle_t *registerBuffers)
{
    trace_in();
    Mutex::Autolock l(StreamLock);

    if (Streams.indexOfKey(streamId) < 0) {
        ALOGE("not added streamId %d", streamId);
        return NO_INIT;
    }

    NXStream *stream = Streams.valueFor(streamId).get();
    status_t res = stream->registerBuffer(numBuffers, registerBuffers);
    if (res != NO_ERROR)
        ALOGE("failed to registerBuffer");

    return res;
}

/* must be called when stopped */
void NXStreamThread::release()
{
    ALOGD("%s\n", __func__);
    Mutex::Autolock l(StreamLock);
    Streams.clear();
    ALOGD("%s exit\n", __func__);
}

void NXStreamThread::onZoomChanged(int left, int top, int width, int height, int baseWidth, int baseHeight)
{
    ZoomController->setBase(baseWidth, baseHeight);
    ZoomController->setCrop(left, top, width, height);
}

status_t NXStreamThread::readyToRun()
{
    return NO_ERROR;
}

status_t NXStreamThread::start(int streamId, char *threadName, unsigned int priority)
{
    if (getState() == STATE_RUNNING)
        return NO_ERROR;

    uint32_t waitCount = 10; // wait exit 10seconds
    while (getState() == STATE_WAIT_EXIT && waitCount > 0) {
        ALOGD("start... wait Exit");
        usleep(100*1000);
        waitCount--;
    }

    if (getState() == STATE_WAIT_EXIT) {
        ALOGE("Can't start because can't exit!!!");
        return NO_INIT;
    }

    if (false == setActiveStreamId(streamId)) {
        ALOGE("invalid streamId %d", streamId);
        return INVALID_OPERATION;
    }

    ALOGD("===> start %s, streamId %d, priority %d", threadName, streamId, priority);
    strcpy(ThreadName, threadName);
    status_t ret = run(ThreadName, priority, 0);

    if (ret == NO_ERROR)
        setState(STATE_RUNNING);

    return ret;
}

status_t NXStreamThread::stop(bool waitExit, bool streamOff)
{
    if (getState() == STATE_EXIT)
        return NO_ERROR;

    ALOGD("<=== stop %s, streamId %d, state %d, waitExit %d", ThreadName, ActiveStreamId, getState(), waitExit);

    int timeOut = 5;
    if (waitExit) {
        setState(STATE_WAIT_EXIT);
        while (getState() != STATE_EXIT && timeOut > 0) {
            ALOGD("wait STATE_EXIT...%dms", timeOut*100);
            usleep(100*1000);
            timeOut--;
        }
    }

    if (getState() != STATE_EXIT) {
        ALOGE("wait exit timeout, force to exit!!!");
        setState(STATE_EXIT);
    }

    if (timeOut > 0) {
        NXStream *stream = getActiveStream();
        if (stream) {
            stream->flushBuffer();
            ALOGD("flush end");
        }
    }

    if (streamOff)
        v4l2_streamoff(Id);

    ALOGD("stop end");
    return NO_ERROR;
}

status_t NXStreamThread::getFormat(uint32_t &width, uint32_t &height, uint32_t &format) const
{
    width = Width;
    height = Height;
    format = PIXINDEX2PIXFORMAT(PixelIndex);
    return NO_ERROR;
}

}; // namespace android
