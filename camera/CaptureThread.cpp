#define LOG_TAG "CaptureThread"

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

#include <fourcc.h>

// if defined, use hw jpeg library
#define USE_HW_JPEG

#ifdef USE_HW_JPEG
#include <libnxjpeghw.h>
#else
#include <libnxjpeg.h>
#endif

#include <NXAllocator.h>

#include "Constants.h"
#include "CaptureThread.h"

namespace android {

CaptureThread::CaptureThread(nxp_v4l2_id id,
        const camera2_frame_queue_dst_ops_t *ops,
        int width,
        int height,
        sp<NXZoomController> &zoomController,
        sp<NXStreamManager> &streamManager)
    : NXStreamThread(id, width, height, zoomController, streamManager),
      FrameQueueDstOps(ops),
      CaptureBufferAllocated(false),
      Exif(NULL),
      CurrentExif(NULL)
{
    init(id);
}

void CaptureThread::init(nxp_v4l2_id id)
{
    NXStreamThread::init(id);
    ExifProcessor = new NXExifProcessor();
}

CaptureThread::~CaptureThread()
{
    freeCaptureBuffer();
    if (CurrentExif)
        delete CurrentExif;
    delete ExifProcessor;
}


void CaptureThread::onCommand(int32_t streamId, camera_metadata_t *metadata)
{
    if (streamId == STREAM_ID_CAPTURE) {
        if (metadata) {
            sp<NXStreamThread> previewThread = StreamManager->getStreamThread(STREAM_ID_PREVIEW);
            sp<NXStreamThread> recordThread = StreamManager->getStreamThread(STREAM_ID_RECORD);
            if ((recordThread == NULL  || !recordThread->isRunning()) &&
                previewThread != NULL) {
                ALOGD("stop preview---->");
                previewThread->stop(true);
                ALOGD("previewThread stopped");
            }
            makeFrame(metadata);

            if (CurrentExif)
                delete CurrentExif;
            if (Exif) {
                ALOGD("Set CurrentExif");
                CurrentExif = new exif_attribute_t(*Exif);
                ALOGD("End Set CurrentExif");
            }

            start(streamId, (char *)"CaptureThread", PRIORITY_FOREGROUND);
        }
        // capture thread auto stop
    }
}

void CaptureThread::onExifChanged(exif_attribute_t *exif)
{
    ALOGD("onExifChanged: %p", exif);
    if (!Exif && !CurrentExif) {
        ALOGD("Set CurrentExif");
        CurrentExif = new exif_attribute_t(*exif);
        ALOGD("End Set CurrentExif");
    }
    Exif = exif;
}

status_t CaptureThread::readyToRun()
{
    ALOGD("capture readyToRun entered");

    NXStream *stream = getActiveStream();
    if (!stream) {
        ALOGE("No ActiveStream!!!");
        return NO_INIT;
    }
    status_t res = stream->initBuffer();
    if (res != NO_ERROR) {
        ALOGE("failed to initBuffer");
        return res;
    }

    sp<NXStreamThread> recordThread = StreamManager->getStreamThread(STREAM_ID_RECORD);
    if (!(recordThread != NULL && recordThread->isRunning())) {
        if (allocCaptureBuffer(Width, Height) == false) {
            return NO_MEMORY;
        }

        int ret = v4l2_set_format(Id, Width, Height, PIXINDEX2PIXFORMAT(PixelIndex));
        if (ret < 0) {
            ALOGE("failed to v4l2_set_format for capture");
            return NO_INIT;
        }

        ret = v4l2_set_crop(Id, 0, 0, Width, Height);
        if (ret < 0) {
            ALOGE("failed to v4l2_set_crop for capture");
            return NO_INIT;
        }

        ret = Sensor->setFormat(Width, Height, V4L2_MBUS_FMT_YUYV8_2X8);
        if (ret < 0) {
            ALOGE("failed to v4l2_set_format for capture sensor0");
            return NO_INIT;
        }

        if (get_board_camera_is_mipi(SensorId)) {
            ret = v4l2_set_format(nxp_v4l2_mipicsi, Width, Height, V4L2_MBUS_FMT_YUYV8_2X8);
            if (ret < 0) {
                ALOGE("failed to v4l2_set_format for %d", nxp_v4l2_mipicsi);
                return NO_INIT;
            }
        }

        ret = v4l2_reqbuf(Id, MAX_CAPTURE_BUFFER);
        if (ret < 0) {
            ALOGE("failed to v4l2_reqbuf for capture");
            return NO_INIT;
        }

        int plane_num = CaptureBuffer[0].plane_num;
        for (int i = 0; i < MAX_CAPTURE_BUFFER; i++) {
            ret = v4l2_qbuf(Id, plane_num, i, &CaptureBuffer[i], -1, NULL);
            if (ret < 0) {
                ALOGE("failed to v4l2_qbuf for capture %d", i);
                return NO_INIT;
            }
        }

        set_board_capture_mode(SensorId, Width, Height);

        ret = v4l2_streamon(Id);
        if (ret < 0) {
            ALOGE("failed to v4l2_streamon for capture");
            return NO_INIT;
        }
    }

    InitialSkipCount = get_board_capture_skip_frame(SensorId, Width, Height);

    ALOGD("capture readyToRun exit");
    return NO_ERROR;
}

bool CaptureThread::makeFrame(camera_metadata_t *srcMetadata)
{
    camera_metadata_t *dstMetadata = allocate_camera_metadata(35, 500);
    ALOGD("%s: %p", __func__, dstMetadata);

    camera_metadata_entry_t entry;
    int32_t int32Data;
    uint8_t byteData;

    status_t res = find_camera_metadata_entry(srcMetadata, ANDROID_REQUEST_ID, &entry);
    if (res == OK) {
        int32Data = entry.data.i32[0];
        ALOGD("request id: %d", int32Data);
        res = add_camera_metadata_entry(dstMetadata, ANDROID_REQUEST_ID, &int32Data, 1);
    }

    res = find_camera_metadata_entry(srcMetadata, ANDROID_REQUEST_METADATA_MODE, &entry);
    if (res == OK) {
        byteData = entry.data.u8[0];
        ALOGD("request metadata mode: %d", byteData);
        res = add_camera_metadata_entry(dstMetadata, ANDROID_REQUEST_METADATA_MODE, &byteData, 1);
    }

    // frameCount
    int32Data = 1;
    add_camera_metadata_entry(dstMetadata, ANDROID_REQUEST_FRAME_COUNT, &int32Data, 1);

    // timestamp
    nsecs_t timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
    add_camera_metadata_entry(dstMetadata, ANDROID_SENSOR_TIMESTAMP, &timestamp, 1);
    CaptureTimeStamp = timestamp;

    size_t numEntries = get_camera_metadata_entry_count(dstMetadata);
    size_t frameSize = get_camera_metadata_size(dstMetadata);
    ALOGD("numEntries(%d), frameSize(%d)", numEntries, frameSize);

    camera_metadata_t *currentFrame = NULL;
    res = FrameQueueDstOps->dequeue_frame(FrameQueueDstOps, numEntries, frameSize, &currentFrame);
    if (res != NO_ERROR || currentFrame == NULL) {
        ALOGE("frame dequeue error");
        free_camera_metadata(dstMetadata);
        return false;
    }
    res = append_camera_metadata(currentFrame, dstMetadata);
    if (res != NO_ERROR) {
        ALOGE("append_camera_metadata error");
        free_camera_metadata(dstMetadata);
        return false;
    }
    FrameQueueDstOps->enqueue_frame(FrameQueueDstOps, currentFrame);
    free_camera_metadata(dstMetadata);
    ALOGD("makeFrame success!!!");
    return true;
}

// add for planar jpeg encoding
struct ycbcr_planar {
    unsigned char *y;
    unsigned char *cb;
    unsigned char *cr;
};

void CaptureThread::dumpCaptureBuffer()
{
    ALOGD("DUMP CAPTURE BUFFER");
    ALOGD("=============================================");
    if (PixelIndex == YUV422_PACKED) {
        ALOGD("Format: YUV422_PACKED");
        for (int i = 0; i < MAX_CAPTURE_BUFFER; i++) {
            ALOGD("BUFFER %d", i);
            ALOGD("\tfd: %d,\tsize %d,\tphys 0x%x,\tvirt 0x%x",
                    CaptureBuffer[i].fds[0],
                    CaptureBuffer[i].sizes[0],
                    (unsigned int)CaptureBuffer[i].phys[0],
                    (unsigned int)CaptureBuffer[i].virt[0]);
        }
    } else {
        ALOGD("Format: YUV420_PLANAR");
        for (int i = 0; i < MAX_CAPTURE_BUFFER; i++) {
            ALOGD("BUFFER %d", i);
            ALOGD("Y");
            ALOGD("\tfd: %d,\tsize %d,\tphys 0x%x,\tvirt 0x%x",
                    CaptureBuffer[i].fds[0],
                    CaptureBuffer[i].sizes[0],
                    (unsigned int)CaptureBuffer[i].phys[0],
                    (unsigned int)CaptureBuffer[i].virt[0]);
            ALOGD("CB");
            ALOGD("\tfd: %d,\tsize %d,\tphys 0x%x,\tvirt 0x%x",
                    CaptureBuffer[i].fds[1],
                    CaptureBuffer[i].sizes[1],
                    (unsigned int)CaptureBuffer[i].phys[1],
                    (unsigned int)CaptureBuffer[i].virt[1]);
            ALOGD("CR");
            ALOGD("\tfd: %d,\tsize %d,\tphys 0x%x,\tvirt 0x%x",
                    CaptureBuffer[i].fds[2],
                    CaptureBuffer[i].sizes[2],
                    (unsigned int)CaptureBuffer[i].phys[2],
                    (unsigned int)CaptureBuffer[i].virt[2]);
        }
    }
    ALOGD("=============================================");
}

bool CaptureThread::allocCaptureBuffer(int width, int height)
{
    if (CaptureBufferAllocated)
        return true;

    bool ret = allocateBuffer(CaptureBuffer, MAX_CAPTURE_BUFFER, width, height, PIXINDEX2PIXFORMAT(PixelIndex));
    if (!ret)
        return ret;

    CaptureBufferAllocated = true;
    return true;
}

void CaptureThread::freeCaptureBuffer()
{
    if (!CaptureBufferAllocated)
        return;

    freeBuffer(CaptureBuffer, MAX_CAPTURE_BUFFER);
    CaptureBufferAllocated = false;
}

bool CaptureThread::capture(unsigned int srcYPhys, unsigned int srcCBPhys, unsigned int srcCRPhys,
        unsigned int srcYVirt, unsigned int srcCBVirt, unsigned int srcCRVirt,
        void *dstBase, int dstSize, int width, int height, uint32_t dstOffset)
{
    int jpegSize;
    int jpegBufSize;
    char *jpegBuf;
    camera2_jpeg_blob *jpegBlob;

    if (PixelIndex == YUV422_PACKED) {
#ifdef USE_HW_JPEG
        ALOGE("HWJPEG not support YUV422_PACKED format!!!");
        return false;
#else
        jpegSize = NX_JpegEncoding((unsigned char *)dstBase, dstSize,
                (unsigned char const *)srcYVirt, width, height, 100, NX_PIXELFORMAT_YUYV);
#endif
    } else {
#ifdef USE_HW_JPEG
        ALOGV("jpeg src buf: 0x%x, 0x%x, 0x%x, dst virt 0x%x", srcYPhys, srcCBPhys, srcCRPhys, dstBase);
        jpegSize = NX_JpegHWEncoding((void *)(((uint32_t)dstBase) + dstOffset), dstSize,
                width, height, FOURCC_MVS0,
                srcYPhys, srcYVirt, width,
                srcCBPhys, srcCBVirt, width >> 1,
                srcCRPhys, srcCRVirt, width >> 1,
                dstOffset == 0);
#else
        struct ycbcr_planar planar;
        planar.y = (unsigned char *)srcYVirt;
        planar.cb = (unsigned char *)srcCBVirt;
        planar.cr = (unsigned char *)srcCRVirt;
        jpegSize = NX_JpegEncoding((unsigned char *)dstBase, dstSize,
                (unsigned char const *)&planar, width, height, 100, NX_PIXFORMAT_YUV420);
#endif
    }
    if (jpegSize <= 0) {
        ALOGE("failed to NX_JpegEncoding!!!");
        return false;
    }

    jpegBufSize = dstSize;
    jpegBuf = (char *)dstBase;
    jpegBlob = (camera2_jpeg_blob *)(&jpegBuf[jpegBufSize - sizeof(camera2_jpeg_blob)]);
    jpegBlob->jpeg_size = jpegSize + dstOffset;
    jpegBlob->jpeg_blob_id = CAMERA2_JPEG_BLOB_ID;

    ALOGD("capture: jpegSize(%d), totalSize(%d)", jpegSize, jpegBlob->jpeg_size);
    ALOGD("capture success!!!");
    return true;
}

bool CaptureThread::capture(struct nxp_vid_buffer *srcBuf, private_handle_t const *dstHandle, int width, int height, uint32_t dstOffset)
{
    return capture((unsigned int)srcBuf->phys[0], (unsigned int)srcBuf->phys[1], (unsigned int)srcBuf->phys[2],
            (unsigned int)srcBuf->virt[0], (unsigned int)srcBuf->virt[1], (unsigned int)srcBuf->virt[2],
            (void *)dstHandle->base, dstHandle->size, width, height, dstOffset);
}

bool CaptureThread::capture(private_handle_t const *srcHandle, private_handle_t const *dstHandle, int width, int height, uint32_t dstOffset)
{
    return capture(srcHandle->phys, srcHandle->phys1, srcHandle->phys2,
            srcHandle->base, srcHandle->base1, srcHandle->base2,
            (void *)dstHandle->base, dstHandle->size, width, height, dstOffset);
}

bool CaptureThread::threadLoop()
{
    ALOGD("Capture threadLoop entered");

    struct NXStream *stream = getActiveStream();
    if (!stream) {
        ALOGE("can't getActiveStream()");
        ERROR_EXIT();
    }

    sp<NXStreamThread> recordThread = StreamManager->getStreamThread(STREAM_ID_RECORD);
    if (!(recordThread != NULL && recordThread->isRunning())) {
        int dqIdx;
        int ret;
        nsecs_t timestamp;
        int plane_num = CaptureBuffer[0].plane_num;

        ALOGV("dq command to device!!!");
        ret = v4l2_dqbuf(Id, plane_num, &dqIdx, NULL);
        if (ret < 0) {
            ALOGE("failed to v4l2_dqbuf for Capture id %d", Id);
            ERROR_EXIT();
        }
        ALOGV("dq end");

        if (InitialSkipCount) {
            InitialSkipCount--;
            ALOGD("Capture Skip Frame: %d, %d", InitialSkipCount, ret);
            ret = v4l2_qbuf(Id, plane_num, dqIdx, &CaptureBuffer[dqIdx], -1, NULL);
        } else {
            struct nxp_vid_buffer *srcBuf = &CaptureBuffer[dqIdx];
            ALOGD("src phys: 0x%lx, 0x%lx, 0x%lx", srcBuf->phys[0], srcBuf->phys[1], srcBuf->phys[2]);

            if (ZoomController->useZoom() && ZoomController->isZoomAvaliable()) {
                if (false == ZoomController->allocBuffer(MAX_CAPTURE_ZOOM_BUFFER, Width, Height, PIXINDEX2PIXFORMAT(PixelIndex))) {
                    ALOGE("failed to allocate capture zoom buffer");
                    ERROR_EXIT();
                }
                struct nxp_vid_buffer *dstBuf = ZoomController->getBuffer(0);
                ZoomController->handleZoom(srcBuf, dstBuf);
                ALOGD("dst phys: 0x%lx, 0x%lx, 0x%lx", dstBuf->phys[0], dstBuf->phys[1], dstBuf->phys[2]);
                srcBuf = dstBuf;
            }

            private_handle_t const *dstHandle = stream->getNextBuffer();
            NXExifProcessor::ExifResult result = ExifProcessor->makeExif(Width, Height, srcBuf, CurrentExif, dstHandle);
            if (result.getSize() == 0) {
                ALOGE("Failed to makeExif()!!!");
            } else {
                if (false == capture(srcBuf, dstHandle, Width, Height, result.getSize()))
                    ALOGE("Capture Failed!!!");

                ExifProcessor->clear();
            }

            status_t res = stream->enqueueBuffer(CaptureTimeStamp);
            if (res != NO_ERROR) {
                ALOGE("failed to enqueue!!!");
                ERROR_EXIT();
            }

            buffer_handle_t *handle;
            res = stream->dequeueBuffer(&handle);
            if (res != NO_ERROR || handle == NULL) {
                ALOGE("failed to dequeue");
            }

            v4l2_qbuf(Id, plane_num, dqIdx, &CaptureBuffer[dqIdx], -1, NULL);

            stop(false);
            return false;
        }
    } else {
        int width = 0, height = 0;
        private_handle_t const *srcHandle = recordThread->getLastBuffer(width, height);
        private_handle_t const *dstHandle = stream->getNextBuffer();

        ALOGD("srcHandle %p, dstHandle %p", srcHandle, dstHandle);

        if (!srcHandle) {
            int waitRecordingBufferCount100ms = 5;
            while(waitRecordingBufferCount100ms--) {
                usleep(100000);
                srcHandle = recordThread->getLastBuffer(width, height);
                if (srcHandle)
                    break;
                ALOGD("waitRecordingBufferCount: %d", waitRecordingBufferCount100ms);
            }
        }

        if (srcHandle == NULL || dstHandle == NULL) {
            ALOGE("can't capture!!!");
            stream->cancelBuffer();
        } else {
            NXExifProcessor::ExifResult result = ExifProcessor->makeExif(width, height, srcHandle, CurrentExif, dstHandle);
            if (result.getSize() == 0) {
                ALOGE("Failed to makeExif()!!!");
                stream->cancelBuffer();
            } else {
                if (false == capture(srcHandle, dstHandle, width, height, result.getSize())) {
                    ALOGE("Capture Failed!!!");
                    stream->cancelBuffer();
                } else {
                    ExifProcessor->clear();
                    status_t res = stream->enqueueBuffer(CaptureTimeStamp);
                    if (res != NO_ERROR) {
                        ALOGE("failed to enqueue!!!");
                        ERROR_EXIT();
                    }
                }
            }
        }

        buffer_handle_t *handle;
        status_t res = stream->dequeueBuffer(&handle);
        if (res != NO_ERROR || handle == NULL) {
            ALOGE("failed to dequeue");
        }

        stop(false, false);
        return false;
    }

    ALOGD("Capture threadLoop exit");
    return true;
}

}; // namespace android
