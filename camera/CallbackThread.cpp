#define LOG_TAG "CallbackThread"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>

#include <ion/ion.h>
#include <ion-private.h>
#include <gralloc_priv.h>
#include <nx_camera_board.h>

#include <NXCpu.h>
#include <NXCsc.h>

#include "Constants.h"
#include "CallbackThread.h"

namespace android {

CallbackThread::CallbackThread(int width, int height, sp<NXStreamManager> &streamManager)
    :NXStreamThread(width, height, streamManager)
{
}

CallbackThread::~CallbackThread()
{
}

void CallbackThread::onCommand(int32_t streamId, camera_metadata_t *metadata)
{
    if (streamId == STREAM_ID_CALLBACK) {
        if (metadata) {
            const char *threadName = "CallbackThread";
            start(streamId, const_cast<char *>(threadName), streamId);
        }
    }
}

status_t CallbackThread::readyToRun()
{
    ALOGD("callback readyToRun entered: wxd(%dx%d)", Width, Height);

    uint32_t cpuVersion = getNXCpuVersion();
    ALOGD("CPU Version: %u", cpuVersion);

    NXStream *stream = getActiveStream();
    if (!stream) {
        ALOGE("No ActiveStream!!!");
        return NO_INIT;
    }

    status_t res = stream->initBuffer();
    if (res != NO_ERROR) {
        ALOGE("failed to initBuffer");
        return NO_INIT;
    }

    // check format
    if ((stream->getFormat() != HAL_PIXEL_FORMAT_YCrCb_420_SP) &&
        (stream->getFormat() != HAL_PIXEL_FORMAT_YV12)) {
        ALOGE("Not NV21 format or YV12 format!!!: 0x%x", stream->getFormat());
        return BAD_TYPE;
    }

    ALOGD("readyToRun exit");
    return NO_ERROR;
}

bool CallbackThread::threadLoop()
{
    CHECK_AND_EXIT();

    int ret;
    buffer_handle_t *buf = NULL;

    NXStream *stream = getActiveStream();
    if (!stream) {
        ALOGE("can't getActiveStream()");
        ERROR_EXIT();
    }

    sp<NXStreamThread> previewThread = StreamManager->getStreamThread(STREAM_ID_PREVIEW);
    if (previewThread == NULL || !previewThread->isRunning()) {
        ALOGD("Preview thread is not running... wait");
        usleep(100*1000);
        return true;
    }
    CHECK_AND_EXIT();

    int width, height;
    private_handle_t const *srcHandle = previewThread->getLastBuffer(width, height);
    if (!srcHandle) {
        ALOGE("can't get preview thread last buffer... wait");
        usleep(500*1000);
        return true;
    }
    CHECK_AND_EXIT();

    if (Width != width || Height != height) {
        ALOGE("preview wxh(%dx%d) is diffent from me(%dx%d)", width, height, Width, Height);
        ERROR_EXIT();
    }

    private_handle_t const *dstHandle = stream->getNextBuffer();
    if (!dstHandle) {
        ALOGE("can't get dstHandle");
        ERROR_EXIT();
    }

    nxCsc(srcHandle, dstHandle, Width, Height);
    CHECK_AND_EXIT();

    ret = stream->enqueueBuffer(systemTime(SYSTEM_TIME_MONOTONIC));
    ALOGD("enqueueBuffer end");

    if (ret != NO_ERROR) {
        ALOGE("failed to enqueue_buffer");
        ERROR_EXIT();
    }
    CHECK_AND_EXIT();

    ret = stream->dequeueBuffer(&buf);
    if (ret != NO_ERROR || buf == NULL) {
        ALOGE("failed to dequeue_buffer");
        ERROR_EXIT();
    }
    ALOGV("End dequeueBuffer()");
    CHECK_AND_EXIT();

    usleep(50*1000); // for preview thread

    return true;
}

}; // namespace android
