#ifndef _NX_CAMERA_HW_INTERFACE_2_H
#define _NX_CAMERA_HW_INTERFACE_2_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>

#include "Constants.h"
#include "NXCommandThread.h"
#include "NXSensorThread.h"
#include "NXCameraSensor.h"
#include "NXStreamManager.h"

namespace android {

class NXCameraHWInterface2 : virtual public RefBase
{
public:
             NXCameraHWInterface2(int cameraId, NXCameraSensor *sensor);
    virtual ~NXCameraHWInterface2() {
    }

    bool init();
    void release();

    int getCameraId() const {
        return CameraId;
    }

    int getCrop(uint32_t &cropLeft, uint32_t &cropTop, uint32_t &cropWidth, uint32_t &cropHeight, uint32_t &baseWidth, uint32_t &baseHeight) {
        if (CommandThread == NULL)
            return NO_INIT;
        CommandThread->getCrop(cropLeft, cropTop, cropWidth, cropHeight, baseWidth, baseHeight);
        return NO_ERROR;
    }

    const camera2_frame_queue_dst_ops_t *getFrameQueueDstOps() {
        return FrameQueueDstOps;
    }

    // Camera2 interface
    virtual int setRequestQueueSrcOps(const camera2_request_queue_src_ops_t *ops);
    virtual int notifyRequestQueueNotEmpty();
    virtual int setFrameQueueDstOps(const camera2_frame_queue_dst_ops_t *ops);
    virtual int getInProgressCount();
    virtual int flushCapturesInProgress();
    virtual int constructDefaultRequest(int requestTemplate, camera_metadata_t **req);
    virtual int allocateStream(uint32_t width, uint32_t height, int format,
            const camera2_stream_ops_t *streamOps,
            uint32_t *streamId, uint32_t *formatActual, uint32_t *usage, uint32_t *maxBuffers);
    virtual int registerStreamBuffers(uint32_t streamId, int numBuffers, buffer_handle_t *buffers);
    virtual int releaseStream(uint32_t streamId);
    virtual int allocateReprocessStream(uint32_t width, uint32_t height, uint32_t format,
            const camera2_stream_in_ops_t *ops,
            uint32_t *streamId, uint32_t *consumerUsage, uint32_t *maxBuffers);
    virtual int allocateReprocessStreamFromStream(uint32_t outputStreamId,
            const camera2_stream_in_ops_t *ops,
            uint32_t *streamId);
    virtual int releaseReprocessStream(uint32_t streamId);
    virtual int triggerAction(uint32_t triggerId, uint32_t ext1, uint32_t ext2);
    virtual int setNotifyCallback(camera2_notify_callback cb, void *user);
    virtual int getMetadataVendorTagOps(vendor_tag_query_ops_t **ops);
    virtual int dump(int fd);

private:
    // member variable start with Capital
    const camera2_frame_queue_dst_ops_t *FrameQueueDstOps;
    camera2_notify_callback NotifyCb;
    void *CallbackCookie;

    // reprocess stream
    const camera2_stream_in_ops_t *StreamInOps;
    int ReprocessStreamId;
    int ReprocessStreamOutputId;

    int CameraId;
    NXCameraSensor *Sensor;

    int IonFd;
    gralloc_module_t const *GrallocHal;

    sp<NXCommandThread> CommandThread;
    sp<NXSensorThread> SensorThread;

    sp<NXStreamManager> StreamManager;
};

}; // namespace android

#endif
