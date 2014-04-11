#define LOG_TAG "NexellCameraHAL2"
#include <utils/Log.h>
#include <ion/ion.h>
#include <android-nxp-v4l2.h>
#include <nxp-v4l2.h>
#include <gralloc_priv.h>
#include <NXCpu.h>
#include <NXTrace.h>
#include "NXStreamThread.h"
#include "NXCameraHWInterface2.h"

// #define TRACE
#ifdef TRACE
#define trace()  NXTrace t(__func__)
#else
#define trace()
#endif

#define getPriv(dev) ((NXCameraHWInterface2 *)(device->priv))

namespace android {

static NXCameraSensor *g_sensor[2] = {NULL, NULL};
static camera_metadata_t *g_camera_info[2] = {NULL, NULL};

//////////////////////////////////////////////////////////////////////////////////
// NXCameraHWInterface2
//
NXCameraHWInterface2::NXCameraHWInterface2(int cameraId, NXCameraSensor *sensor)
    : CameraId(cameraId),
      Sensor(sensor),
      IonFd(-1)
{
}

bool NXCameraHWInterface2::init()
{
    trace();
    IonFd = ion_open();
    if (IonFd < 0) {
        ALOGE("failed to ion_open()");
        return -1;
    }

    int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
            (const hw_module_t **)&GrallocHal);
    if (ret) {
        ALOGE("failed to get Gralloc Module");
        return false;
    }

    StreamManager = new NXStreamManager(this);

    return android_nxp_v4l2_init();
}

void NXCameraHWInterface2::release()
{
    trace();
    if (IonFd >= 0) {
        ion_close(IonFd);
        IonFd = -1;
    }
}

// public interface
int NXCameraHWInterface2::setRequestQueueSrcOps(const camera2_request_queue_src_ops_t *ops)
{
    trace();
    CommandThread = new NXCommandThread(ops, Sensor);
    if (CommandThread == NULL) {
        ALOGE("failed to create NXCommandThread!!!");
        return NO_MEMORY;
    }
    NXCommandThread *cthread = CommandThread.get();
    cthread->run();
    return 0;
}

int NXCameraHWInterface2::notifyRequestQueueNotEmpty()
{
    trace();
    if (CommandThread != NULL) {
        ALOGD("notifyRequestQueueNotEmpty");
        NXCommandThread *cthread = CommandThread.get();
        cthread->wakeup();
    } else {
        ALOGE("CommandThread is NULL!!!");
    }
    return 0;
}

int NXCameraHWInterface2::setFrameQueueDstOps(const camera2_frame_queue_dst_ops_t *ops)
{
    trace();
    FrameQueueDstOps = ops;
    return 0;
}

int NXCameraHWInterface2::getInProgressCount()
{
    trace();
    return 0;
}

int NXCameraHWInterface2::flushCapturesInProgress()
{
    trace();
    return 0;
}

int NXCameraHWInterface2::constructDefaultRequest(int requestTemplate, camera_metadata_t **request)
{
    trace();

    if (request == NULL) {
        ALOGE("%s: request is NULL!!!", __func__);
        return BAD_VALUE;
    }
    if (requestTemplate < 0 || requestTemplate >= CAMERA2_TEMPLATE_COUNT) {
        ALOGE("%s: invalid requestTemplate(%d)", __func__, requestTemplate);
        return BAD_VALUE;
    }

    status_t res;
    res = Sensor->constructDefaultRequest(requestTemplate, request, true);
    if (res != OK) {
        ALOGE("%s: first call sensor constructDefaultRequest failed", __func__);
        return res;
    }
    res = Sensor->constructDefaultRequest(requestTemplate, request, false);
    if (res != OK) {
        ALOGE("%s: second call sensor constructDefaultRequest failed", __func__);
        return res;
    }

    return OK;
}

int NXCameraHWInterface2::allocateStream(uint32_t width, uint32_t height, int format, const camera2_stream_ops_t *streamOps,
            uint32_t *streamId, uint32_t *formatActual, uint32_t *usage, uint32_t *maxBuffers)
{
    trace();

    if (CommandThread == NULL) {
        ALOGE("CommanThread is not exist!!!");
        return NO_INIT;
    }

    if(!Sensor->isSupportedResolution(width, height)) {
        ALOGE("%dx%d is not supported!!!", width, height);
        return INVALID_OPERATION;
    }

    bool useSensorZoom = false;
    if (Sensor->getZoomFactor() > 1)
        useSensorZoom = true;

    int ret = StreamManager->allocateStream(width, height, format, streamOps, streamId, formatActual, usage, maxBuffers, useSensorZoom);
    if (ret != NO_ERROR) {
        ALOGE("failed to allocateStream(error: %d)", ret);
        return ret;
    }

    sp<NXStreamThread> spStreamThread(StreamManager->getStreamThread(*streamId));
    if (NO_ERROR != CommandThread->registerListener(*streamId, spStreamThread)) {
        ALOGE("failed to registerListener record stream to command thread");
        return NO_INIT;
    }

    return 0;
}

int NXCameraHWInterface2::registerStreamBuffers(uint32_t streamId, int numBuffers, buffer_handle_t *buffers)
{
    trace();
    NXStreamThread *streamThread = StreamManager->getStreamThread(streamId);
    if (!streamThread) {
        ALOGE("can't get streamThread for %d", streamId);
        return NO_INIT;
    }
    return streamThread->registerStreamBuffers(streamId, numBuffers, buffers);
}

int NXCameraHWInterface2::releaseStream(uint32_t streamId)
{
    trace();
    ALOGD("%s: id %d", __func__, streamId);
    if (streamId == STREAM_ID_INVALID) {
        ALOGE("can't releaseStream invalid");
        return 0;
    }
    NXStreamThread *streamThread = StreamManager->getStreamThread(streamId);
    if (!streamThread) {
        ALOGE("can't get streamThread for %d", streamId);
        return NO_INIT;
    }

    CommandThread->removeListener(streamId);
    if (streamThread->getActiveStreamId() == STREAM_ID_CALLBACK)
        streamThread->stop(true, false);
    else
        streamThread->stop(true);
    streamThread->removeStream(streamId);
    if (!streamThread->getStreamCount()) {
        streamThread->release();
        StreamManager->removeStream(streamId);
    }

    return 0;
}

int NXCameraHWInterface2::allocateReprocessStream(uint32_t width, uint32_t height, uint32_t format,
            const camera2_stream_in_ops_t *ops,
            uint32_t *streamId, uint32_t *consumerUsage, uint32_t *maxBuffers)
{
    trace();
    return 0;
}

int NXCameraHWInterface2::allocateReprocessStreamFromStream(uint32_t outputStreamId,
            const camera2_stream_in_ops_t *ops,
            uint32_t *streamId)
{
    trace();
    ALOGD("output stream id(%d)", outputStreamId);

    *streamId = STREAM_ID_REPROCESS;

    StreamInOps = ops;
    ReprocessStreamId = *streamId;
    ReprocessStreamOutputId = outputStreamId;
    return 0;
}

int NXCameraHWInterface2::releaseReprocessStream(uint32_t streamId)
{
    trace();
    return 0;
}

int NXCameraHWInterface2::triggerAction(uint32_t triggerId, uint32_t ext1, uint32_t ext2)
{
    trace();
    if (SensorThread != NULL) {
        ALOGD("triggerId %d, ext1 %d, ext2 %d", triggerId, ext1, ext2);
        SensorThread->sendMessage(new NXSensorMessage(triggerId, ext1, ext2));
    }
    return 0;
}

int NXCameraHWInterface2::setNotifyCallback(camera2_notify_callback cb, void *user)
{
    trace();
    NotifyCb = cb;
    CallbackCookie = user;
    if (SensorThread == NULL && user != NULL) {
        SensorThread = new NXSensorThread(cb, user, Sensor);
        if (SensorThread == NULL) {
            ALOGE("can't create SensorThread!!!");
            return NO_MEMORY;
        }
        SensorThread->run();
    }
    return 0;
}

int NXCameraHWInterface2::getMetadataVendorTagOps(vendor_tag_query_ops_t **ops)
{
    trace();
    *ops = NULL;
    return 0;
}

int NXCameraHWInterface2::dump(int fd)
{
    trace();
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////
// camera2_device_ops
//
static int set_request_queue_src_ops(const struct camera2_device *device,
                    const camera2_request_queue_src_ops_t *request_src_ops)
{
    trace();
    return getPriv(device)->setRequestQueueSrcOps(request_src_ops);
}

static int notify_request_queue_not_empty(const struct camera2_device *device)
{
    trace();
    return getPriv(device)->notifyRequestQueueNotEmpty();
}

static int set_frame_queue_dst_ops(const struct camera2_device *device,
                    const camera2_frame_queue_dst_ops_t *frame_dst_ops)
{
    trace();
    return getPriv(device)->setFrameQueueDstOps(frame_dst_ops);
}

static int get_in_progress_count(const struct camera2_device *device)
{
    trace();
    return getPriv(device)->getInProgressCount();
}

static int flush_captures_in_progress(const struct camera2_device *device)
{
    trace();
    return getPriv(device)->flushCapturesInProgress();
}

static int construct_default_request(const struct camera2_device *device,
                    int request_template, camera_metadata_t **request)
{
    trace();
    return getPriv(device)->constructDefaultRequest(request_template, request);
}

static int allocate_stream(
        const struct camera2_device *device,
        // inputs
        uint32_t width,
        uint32_t height,
        int      format,
        const camera2_stream_ops_t *stream_ops,
        // outputs
        uint32_t *stream_id,
        uint32_t *format_actual,
        uint32_t *usage,
        uint32_t *max_buffers)
{
    trace();
    return getPriv(device)->allocateStream(width, height, format, stream_ops,
            stream_id, format_actual, usage, max_buffers);
}

static int register_stream_buffers(
        const struct camera2_device *device,
        uint32_t stream_id,
        int num_buffers,
        buffer_handle_t *buffers)
{
    trace();
    return getPriv(device)->registerStreamBuffers(stream_id, num_buffers, buffers);
}

static int release_stream(
        const struct camera2_device *device,
        uint32_t stream_id)
{
    trace();
    return getPriv(device)->releaseStream(stream_id);
}

static int allocate_reprocess_stream(const struct camera2_device *device,
        uint32_t width,
        uint32_t height,
        uint32_t format,
        const camera2_stream_in_ops_t *reprocess_stream_ops,
        // outputs
        uint32_t *stream_id,
        uint32_t *consumer_usage,
        uint32_t *max_buffers)
{
    trace();
    return getPriv(device)->allocateReprocessStream(width, height, format, reprocess_stream_ops,
            stream_id, consumer_usage, max_buffers);
}

static int allocate_reprocess_stream_from_stream(const struct camera2_device *device,
        uint32_t output_stream_id,
        const camera2_stream_in_ops_t *reprocess_stream_ops,
        // outputs
        uint32_t *stream_id)
{
    trace();
    return getPriv(device)->allocateReprocessStreamFromStream(output_stream_id,
            reprocess_stream_ops, stream_id);
}

static int release_reprocess_stream(
        const struct camera2_device *device,
        uint32_t stream_id)
{
    trace();
    return getPriv(device)->releaseReprocessStream(stream_id);
}

static int trigger_action(const struct camera2_device *device,
        uint32_t trigger_id,
        int32_t ext1,
        int32_t ext2)
{
    trace();
    return getPriv(device)->triggerAction(trigger_id, ext1, ext2);
}

static int set_notify_callback(const struct camera2_device *device,
        camera2_notify_callback notify_cb,
        void *user)
{
    trace();
    return getPriv(device)->setNotifyCallback(notify_cb, user);
}

static int get_metadata_vendor_tag_ops(const struct camera2_device *device,
                    vendor_tag_query_ops_t **ops)
{
    trace();
    return getPriv(device)->getMetadataVendorTagOps(ops);
}

static int dump(const struct camera2_device *device, int fd)
{
    trace();
    return getPriv(device)->dump(fd);
}

static camera2_device_ops_t camera2_device_ops = {
    set_request_queue_src_ops:           set_request_queue_src_ops,
    notify_request_queue_not_empty:      notify_request_queue_not_empty,
    set_frame_queue_dst_ops:             set_frame_queue_dst_ops,
    get_in_progress_count:               get_in_progress_count,
    flush_captures_in_progress:          flush_captures_in_progress,
    construct_default_request:           construct_default_request,

    allocate_stream:                     allocate_stream,
    register_stream_buffers:             register_stream_buffers,
    release_stream:                      release_stream,

    allocate_reprocess_stream:           allocate_reprocess_stream,
    allocate_reprocess_stream_from_stream: allocate_reprocess_stream_from_stream,
    release_reprocess_stream:            release_reprocess_stream,

    trigger_action:                      trigger_action,
    set_notify_callback:                 set_notify_callback,
    get_metadata_vendor_tag_ops:         get_metadata_vendor_tag_ops,
    dump:                                dump,
};

//////////////////////////////////////////////////////////////////////////////////
// HAL2 interface
//
static int get_number_of_cameras()
{
    return get_board_number_of_cameras();
}

static int get_camera_info(int cameraId, struct camera_info *info)
{
    int cameraCount = get_number_of_cameras();

    if (cameraId >= cameraCount) {
        ALOGE("%s: invalid cameraId(%d)", __func__, cameraId);
        return BAD_VALUE;
    }

    if (cameraCount == 2 && cameraId == 1) {
        // TODO
        info->facing = CAMERA_FACING_FRONT;
        if (!g_sensor[1]) {
            g_sensor[1] = new NXCameraSensor(1);
        }
    } else {
        info->facing = CAMERA_FACING_BACK;
        if (!g_sensor[0]) {
            g_sensor[0] = new NXCameraSensor(0);
        }
    }

    info->orientation = get_board_camera_orientation(cameraId);

    info->device_version = HARDWARE_DEVICE_API_VERSION(2, 0);

    status_t res;
    if (g_camera_info[cameraId] == NULL) {
        res = g_sensor[cameraId]->constructStaticInfo(&g_camera_info[cameraId], cameraId, true);
        if (res != OK) {
            ALOGE("%s: Unable to allocate static info: %s (%d)", __func__, strerror(-res), res);
            return res;
        }
        res = g_sensor[cameraId]->constructStaticInfo(&g_camera_info[cameraId], cameraId, false);
        if (res != OK) {
            ALOGE("%s: Unable to fill in static info: %s (%d)", __func__, strerror(-res), res);
            return res;
        }
    }
    info->static_camera_characteristics = g_camera_info[cameraId];
    return NO_ERROR;
}

static int camera_device_close(struct hw_device_t *device)
{
    trace();
    camera2_device_t *dev = (camera2_device_t *)device;
    NXCameraHWInterface2 *priv = static_cast<NXCameraHWInterface2 *>(dev->priv);
    priv->release();
    delete priv;
    delete dev;

    return 0;
}

static int camera_device_open(const struct hw_module_t *module,
        const char *id,
        struct hw_device_t **device)
{
    trace();

    int cameraId = atoi(id);

    camera2_device_t *dev = new camera2_device_t;

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = CAMERA_DEVICE_API_VERSION_2_0;
    dev->common.module = const_cast<hw_module_t *>(module);
    dev->common.close = camera_device_close;

    dev->ops = &camera2_device_ops;

    NXCameraHWInterface2 *hal = new NXCameraHWInterface2(cameraId, g_sensor[cameraId]);
    if (!hal) {
        ALOGE("can't create NXCameraHWInterface2");
        return -ENOMEM;
    }
    ALOGD("%s: hal %p", __func__, hal);

    if (false == hal->init()) {
        ALOGE("failed to hal init");
        return -EINVAL;
    }

    dev->priv = hal;
    *device = (hw_device_t *)dev;
    return 0;
}

static hw_module_methods_t camera_module_methods = {
    open : camera_device_open
};

extern "C" {
    struct camera_module HAL_MODULE_INFO_SYM = {
        common : {
            tag                 : HARDWARE_MODULE_TAG,
            module_api_version  : CAMERA_MODULE_API_VERSION_2_0,
            hal_api_version     : HARDWARE_HAL_API_VERSION,
            id                  : CAMERA_HARDWARE_MODULE_ID,
            name                : "Pyrope Camera HAL2",
            author              : "Nexell",
            methods             : &camera_module_methods,
            dso                 : NULL,
            reserved            : {0},
         },
        get_number_of_cameras   : get_number_of_cameras,
        get_camera_info         : get_camera_info,
    };
}

}; // namespace android

