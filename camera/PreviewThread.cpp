#define LOG_TAG "PreviewThread"

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

#include "Constants.h"
#include "PreviewThread.h"

namespace android {

PreviewThread::PreviewThread(nxp_v4l2_id id,
        int width,
        int height,
        sp<NXZoomController> &zoomController,
        sp<NXStreamManager> &streamManager)
    :NXStreamThread(id, width, height, zoomController, streamManager)
{
    init(id);
}

PreviewThread::~PreviewThread()
{
}

void PreviewThread::init(nxp_v4l2_id id)
{
    NXStreamThread::init(id);
}

void PreviewThread::onCommand(int32_t streamId, camera_metadata_t *metadata)
{
    if (streamId == STREAM_ID_PREVIEW) {
        if (metadata) {
            const char *threadName = "PreviewThread";
            start(streamId, const_cast<char *>(threadName), streamId);
        }
    } else if (streamId == STREAM_ID_ZSL) {
        ALOGV("get ZSL Command");
    } else {
        ALOGE("Invalid Stream ID: %d", streamId);
    }
}

status_t PreviewThread::readyToRun()
{
    ALOGD("preview readyToRun entered: wxd(%dx%d)", Width, Height);

    uint32_t cpuVersion = getNXCpuVersion();
    ALOGD("CPU Version: %u", cpuVersion);

    NXStream *stream = getActiveStream();
    if (!stream) {
        ALOGE("No ActiveStream!!!");
        return NO_INIT;
    }

    if (stream->getWidth() != Width || stream->getHeight() != Height) {
        ALOGD("Context Changed!!!(%dx%d --> %dx%d)", Width, Height, stream->getWidth(), stream->getHeight());
        Width = stream->getWidth();
        Height = stream->getHeight();
        ZoomController->freeBuffer();
        ZoomController->setSource(Width, Height);
        ZoomController->setDest(Width, Height);
    }

    status_t res = stream->initBuffer();
    if (res != NO_ERROR) {
        ALOGE("failed to initBuffer");
        return NO_INIT;
    }

    int ret = 0;
    if (cpuVersion)
        ret = v4l2_set_format(Id, Width, Height, PIXINDEX2PIXFORMAT(PixelIndex));
    else
        ret = v4l2_set_format(Id, Width, Height, V4L2_PIX_FMT_YUV444); // this is for urbetter 601 camera
    if (ret < 0) {
        ALOGE("failed to v4l2_set_format for %d", Id);
        return NO_INIT;
    }

    if (cpuVersion)
        ZoomController->setFormat(PIXINDEX2PIXCODE(PixelIndex), PIXINDEX2PIXCODE(PixelIndex));
    else
        ZoomController->setFormat(PIXINDEX2PIXCODE(YUV444_PLANAR), PIXINDEX2PIXCODE(PixelIndex));

    ret = v4l2_set_crop(Id, 0, 0, Width, Height);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_crop for %d", Id);
        return NO_INIT;
    }

    ret = Sensor->setFormat(Width, Height, V4L2_MBUS_FMT_YUYV8_2X8);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_format for %d", SensorId);
        return NO_INIT;
    }

    if (get_board_camera_is_mipi(SensorId)) {
        ret = v4l2_set_format(nxp_v4l2_mipicsi, Width, Height, V4L2_MBUS_FMT_YUYV8_2X8);
        if (ret < 0) {
            ALOGE("failed to v4l2_set_format for %d", nxp_v4l2_mipicsi);
            return NO_INIT;
        }
    }

    if (ZoomController->useZoom()) {
        uint32_t zoomFormat;
        if (cpuVersion)
            zoomFormat = PIXINDEX2PIXFORMAT(PixelIndex);
        else
            zoomFormat = PIXINDEX2PIXFORMAT(YUV444_PLANAR);

        if (false == ZoomController->allocBuffer(MAX_PREVIEW_ZOOM_BUFFER, Width, Height, zoomFormat)) {
            ALOGE("failed to allocate preview zoom buffer");
            return NO_MEMORY;
        }
        ret = v4l2_reqbuf(Id, ZoomController->getBufferCount());
        if (ret < 0) {
            ALOGE("failed to v4l2_reqbuf for preview");
            return NO_INIT;
        }
        int planeNum = ZoomController->getBuffer(0)->plane_num;
        for (int i = 0; i < ZoomController->getBufferCount(); i++) {
            ret = v4l2_qbuf(Id, planeNum, i, ZoomController->getBuffer(i), -1, NULL);
            if (ret < 0) {
                ALOGE("failed to v4l2_qbuf for preview %d", i);
                return NO_INIT;
            }
        }
    } else {
        size_t queuedSize = stream->getQueuedSize();
        ret = v4l2_reqbuf(Id, queuedSize);
        if (ret < 0) {
            ALOGE("failed to v4l2_reqbuf for %d", Id);
            v4l2_streamoff(Id);
            return NO_INIT;
        }

        for (size_t i = 0; i < queuedSize; i++) {
            const buffer_handle_t *b = stream->getQueuedBuffer(queuedSize - i - 1);
            ALOGV("HW Q %p", b);
            ret = v4l2_qbuf(Id, 3, i, reinterpret_cast<private_handle_t const *>(*b), -1, NULL);
            if (ret < 0) {
                ALOGE("failed to v4l2_qbuf for ID %d, index %d", Id, i);
                return NO_INIT;
            }
        }
    }

    set_board_preview_mode(SensorId, Width, Height);

    ALOGV("call streamon");
    ret = v4l2_streamon(Id);
    if (ret < 0) {
        ALOGE("failed to v4l2_streamon for %d", Id);
        return NO_INIT;
    }

    InitialSkipCount = get_board_preview_skip_frame(SensorId);

    ALOGD("readyToRun exit");
    return NO_ERROR;
}

bool PreviewThread::threadLoop()
{
    int dqIdx;
    int ret;
    nsecs_t timestamp;
    buffer_handle_t *buf = NULL;

    NXStream *stream = getActiveStream();
    if (!stream) {
        ALOGE("%d: can't getActiveStream()", __LINE__);
        ERROR_EXIT();
    }

    ret = v4l2_dqbuf(Id, 3, &dqIdx, NULL);
    if (ret < 0) {
        ALOGE("failed to v4l2_dqbuf for preview");
        ERROR_EXIT();
    }
    ALOGV("dqIdx: %d", dqIdx);

    if (InitialSkipCount) {
        InitialSkipCount--;
        ALOGV("Preview Skip Frame: %d", InitialSkipCount);
        stream->cancelBuffer();
    } else {
        if (ZoomController->useZoom()) {
            struct nxp_vid_buffer *srcBuf = ZoomController->getBuffer(dqIdx);
            private_handle_t const *dstHandle = stream->getNextBuffer();
            if (!dstHandle) {
                ALOGE("can't get dstHandle!!!");
                ERROR_EXIT();
            }
            ALOGV("phys: 0x%x, 0x%x, 0x%x", srcBuf->phys[0], srcBuf->phys[1], srcBuf->phys[2]);
            ZoomController->handleZoom(srcBuf, dstHandle);
            ALOGV("end handleZoom()");
        }
        CHECK_AND_EXIT();

        timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
        ret = stream->enqueueBuffer(timestamp);
        ALOGV("enqueueBuffer end(timestamp: %llu)", timestamp);

        if (ret != NO_ERROR) {
            ALOGE("failed to enqueue_buffer (idx:%d)", dqIdx);
            ERROR_EXIT();
        }
    }

    CHECK_AND_EXIT();
    ret = stream->dequeueBuffer(&buf);
    if (ret != NO_ERROR || buf == NULL) {
        ALOGE("failed to dequeue_buffer");
        ERROR_EXIT();
    }
    ALOGV("End dequeueBuffer()");

    if (ZoomController->useZoom()) {
        ret = v4l2_qbuf(Id, 3, dqIdx, ZoomController->getBuffer(dqIdx), -1, NULL);
    } else {
        ret = v4l2_qbuf(Id, 3, dqIdx, reinterpret_cast<private_handle_t const *>(*buf), -1, NULL);
    }
    if (ret) {
        ALOGE("failed to v4l2_qbuf()");
        ERROR_EXIT();
    }
    ALOGV("End v4l2_qbuf()");

    CHECK_AND_EXIT();

    return true;
}

}; // namespace android
