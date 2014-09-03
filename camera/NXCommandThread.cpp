#ifndef LOG_TAG
#define LOG_TAG "NXCommandThread"
#endif

#include "NXCommandThread.h"

#define TRACE
#ifdef TRACE
#define trace_in() ALOGD("%s entered", __func__)
#define trace_exit() ALOGD("%s exit", __func__)
#endif

#ifndef LOG_TAG
#define LOG_TAG "NXCommandThread"
#endif

#define COMMAND_WAIT_TIME_NS   10000000

namespace android {

NXCommandThread::NXCommandThread(const camera2_request_queue_src_ops_t *ops, NXCameraSensor *sensor)
    : Thread(false),
      IsIdle(true),
      MsgReceived(false),
      RequestQueueSrcOps(ops),
      Sensor(sensor)
{
    CropLeft = 0;
    CropTop = 0;
    CropWidth = sensor->getSensorW();
    CropHeight = sensor->getSensorH();

#if 0
    Exif = (exif_attribute_t *)malloc(sizeof(exif_attribute_t));
    memset(Exif, 0, sizeof(exif_attribute_t));
#else
    Exif = new exif_attribute_t();
#endif
}

NXCommandThread::~NXCommandThread()
{
#if 0
    free(Exif);
#else
    delete Exif;
#endif
}

void NXCommandThread::wakeup()
{
    trace_in();
    Mutex::Autolock lock(SignalMutex);
    MsgReceived = true;
    SignalCondition.signal();
    trace_exit();
}

bool NXCommandThread::threadLoop()
{
#if 0
    trace_in();
    SignalMutex.lock();
    ALOGD("threadLoop MsgReceived %d", MsgReceived);
    if (MsgReceived) {
        MsgReceived = false;
    } else {
        IsIdle = true;
        SignalCondition.wait(SignalMutex);
    }
    SignalMutex.unlock();

    IsIdle = false;
    return processCommand();
#else
    if (exitPending()) {
        ALOGD("exit Pending!!!");
        return false;
    }
    SignalMutex.lock();
    while (!MsgReceived)
        SignalCondition.wait(SignalMutex);
    MsgReceived = false;
    SignalMutex.unlock();
    return processCommand();
#endif
}

status_t NXCommandThread::checkEntry(const camera_metadata_entry_t &entry, uint8_t type, size_t count)
{
    if (entry.type != type) {
        ALOGE("Missmatching(%s) type(%d)", get_camera_metadata_tag_name(entry.tag), entry.type);
        return BAD_VALUE;
    }

    if (count != 0 && entry.count != count) {
        ALOGE("Missmatching(%s) count(%d)", get_camera_metadata_tag_name(entry.tag), entry.count);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t NXCommandThread::handleRequest(camera_metadata_t *request)
{
    uint32_t numEntry = 0;
    uint32_t index = 0;
    uint32_t i = 0;
    uint32_t cnt = 0;
    camera_metadata_entry curEntry;
    status_t ret = NO_ERROR;;

    numEntry = (uint32_t)get_camera_metadata_entry_count(request);
    if (!numEntry) {
        ALOGE("handleRequest: No Entry");
        return -ENOENT;
    }

    ALOGV("====================> numEntry %d", numEntry);
    for (index = 0; index < numEntry; index++) {
        if (get_camera_metadata_entry(request, index, &curEntry) == 0) {
            switch (curEntry.tag) {
            case ANDROID_LENS_FOCUS_DISTANCE:
                ret = checkEntry(curEntry, TYPE_FLOAT, 1);
                if (ret == NO_ERROR)
                    ALOGV("focus distance %.8f", curEntry.data.f[0]);
                break;

            case ANDROID_LENS_APERTURE:
                ret = checkEntry(curEntry, TYPE_FLOAT, 1);
                if (ret == NO_ERROR)
                    ALOGV("lens aperture %.8f", curEntry.data.f[0]);
                break;

            case ANDROID_LENS_FOCAL_LENGTH:
                ret = checkEntry(curEntry, TYPE_FLOAT, 1);
                if (ret == NO_ERROR)
                    ALOGV("focal length %.8f", curEntry.data.f[0]);
                break;

            case ANDROID_LENS_FILTER_DENSITY:
                ret = checkEntry(curEntry, TYPE_FLOAT, 1);
                if (ret == NO_ERROR)
                    ALOGV("filter density %.8f", curEntry.data.f[0]);
                break;

            case ANDROID_LENS_OPTICAL_STABILIZATION_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("optical stabilization mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_SENSOR_TIMESTAMP:
                ret = checkEntry(curEntry, TYPE_INT64, 1);
                if (ret == NO_ERROR)
                    ALOGV("sensor timestamp: %llu", curEntry.data.i64[0]);
                break;

            case ANDROID_SENSOR_SENSITIVITY:
                ret = checkEntry(curEntry, TYPE_INT32, 1);
                if (ret == NO_ERROR)
                    ALOGV("sensor sensitivity: %d", curEntry.data.i32[0]);
                break;

            case ANDROID_SENSOR_EXPOSURE_TIME:
                ret = checkEntry(curEntry, TYPE_INT64, 1);
                if (ret == NO_ERROR)
                    ALOGV("sensor exposure time: %llu", curEntry.data.i64[0]);
                break;

            case ANDROID_FLASH_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("flash mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_FLASH_FIRING_POWER:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("flash firing power: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_FLASH_FIRING_TIME:
                ret = checkEntry(curEntry, TYPE_INT64, 1);
                if (ret == NO_ERROR)
                    ALOGV("flash firing time: %llu", curEntry.data.i64[0]);
                break;

            case ANDROID_SCALER_CROP_REGION:
                ret = checkEntry(curEntry, TYPE_INT32, 3);
                if (ret == NO_ERROR)
#if 0
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("crop %d: %d", i, curEntry.data.i32[i]);
#else
                {
                    if (CropWidth != (unsigned int)curEntry.data.i32[2]) {
                        CropWidth = curEntry.data.i32[2];
                        int cropHeight = Sensor->getSensorH();
                        cropHeight = (cropHeight*CropWidth)/Sensor->getSensorW();
                        int left = curEntry.data.i32[0];
                        int top = curEntry.data.i32[1];
                        ALOGD("CROP===> left %d, top %d, w %d, h %d",
                                left, top, CropWidth, cropHeight);
                        Sensor->setZoomCrop(left, top, CropWidth, cropHeight);
                    }
                }
#endif
                break;

            case ANDROID_JPEG_QUALITY:
                ret = checkEntry(curEntry, TYPE_INT32, 1);
                if (ret == NO_ERROR)
                    ALOGV("jpeg quality: %d", curEntry.data.i32[0]);
                break;

            case ANDROID_JPEG_THUMBNAIL_SIZE:
                ret = checkEntry(curEntry, TYPE_INT32, 2);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("thumbnail size(%d): %d", i, curEntry.data.i32[i]);
                break;

            case ANDROID_JPEG_THUMBNAIL_QUALITY:
                ret = checkEntry(curEntry, TYPE_INT32, 1);
                if (ret == NO_ERROR)
                    ALOGV("thumbnail quality: %d", curEntry.data.i32[0]);
                break;

            case ANDROID_JPEG_GPS_COORDINATES:
                ret = checkEntry(curEntry, TYPE_DOUBLE, 3);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("gps coordinates %d: %.8f", i, curEntry.data.d[i]);
                break;

            case ANDROID_JPEG_GPS_PROCESSING_METHOD:
                ret = checkEntry(curEntry, TYPE_BYTE);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("gps processing method %d: %d", i, curEntry.data.u8[i]);
                break;

            case ANDROID_JPEG_GPS_TIMESTAMP:
                ret = checkEntry(curEntry, TYPE_INT64, 1);
                if (ret == NO_ERROR)
                    ALOGV("jpeg gps timestamp: %llu", curEntry.data.i64[0]);
                break;

            case ANDROID_JPEG_ORIENTATION:
                ret = checkEntry(curEntry, TYPE_INT32, 1);
                if (ret == NO_ERROR)
                    ALOGD("jpeg orientation: %d", curEntry.data.i32[0]);
                break;

            case ANDROID_STATISTICS_FACE_DETECT_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("face detect mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_CONTROL_CAPTURE_INTENT:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("capture intent: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_CONTROL_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR) {
                    ControlMode = curEntry.data.u8[0];
                    ALOGV("control mode: %d", ControlMode);
                }
                break;

            case ANDROID_CONTROL_VIDEO_STABILIZATION_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("video stabilization mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_CONTROL_AE_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("ae mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_CONTROL_AE_LOCK:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("ae lock: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION:
                ret = checkEntry(curEntry, TYPE_INT32, 1);
                if (ret == NO_ERROR) {
                    ALOGV("ae exp compensation: %d", curEntry.data.i32[0]);
                    Sensor->setExposure(curEntry.data.i32[0]);
                }
                break;

            case ANDROID_CONTROL_AWB_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR) {
                    ALOGV("awb mode: %d", curEntry.data.u8[0]);
                    Sensor->setAwbMode(curEntry.data.u8[0]);
                }
                break;

            case ANDROID_CONTROL_AWB_LOCK:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("awb lock: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_CONTROL_AF_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR) {
                    ALOGV("af mode: %d", curEntry.data.u8[0]);
                    Sensor->setAfMode(curEntry.data.u8[0]);
                }
                break;

            case ANDROID_CONTROL_AF_REGIONS:
                ret = checkEntry(curEntry, TYPE_INT32, 5);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("af regions %d: %d", i, curEntry.data.i32[i]);
                break;

            case ANDROID_CONTROL_AE_REGIONS:
                ret = checkEntry(curEntry, TYPE_INT32, 5);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("ae regions %d: %d", i, curEntry.data.i32[i]);
                break;

            case ANDROID_REQUEST_ID:
                ret = checkEntry(curEntry, TYPE_INT32, 1);
                if (ret == NO_ERROR)
                    ALOGV("request id: %d", curEntry.data.i32[i]);
                break;

            case ANDROID_REQUEST_METADATA_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("metadata mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_REQUEST_OUTPUT_STREAMS:
                ret = checkEntry(curEntry, TYPE_BYTE);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("output streams %d: %d", i, curEntry.data.u8[i]);
                break;

            case ANDROID_REQUEST_INPUT_STREAMS:
                ret = checkEntry(curEntry, TYPE_BYTE);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("input streams %d: %d", i, curEntry.data.u8[i]);
                break;

            case ANDROID_REQUEST_TYPE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("request type: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_REQUEST_FRAME_COUNT:
                ret = checkEntry(curEntry, TYPE_INT32, 1);
                if (ret == NO_ERROR)
                    ALOGV("request frame count: %d", curEntry.data.i32[0]);
                break;

            case ANDROID_SENSOR_FRAME_DURATION:
                ret = checkEntry(curEntry, TYPE_INT64, 1);
                if (ret == NO_ERROR)
                    ALOGV("frame duration: %llu", curEntry.data.i64[0]);
                break;

            case ANDROID_CONTROL_SCENE_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR) {
                    SceneMode = curEntry.data.u8[0];
                    ALOGV("scene mode: %d", SceneMode);
                    if (ControlMode == ANDROID_CONTROL_MODE_USE_SCENE_MODE)
                        Sensor->setSceneMode(SceneMode);
                }
                break;

            case ANDROID_CONTROL_AE_TARGET_FPS_RANGE:
                ret = checkEntry(curEntry, TYPE_INT32, 2);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("ae target fps range %d: %d", i, curEntry.data.i32[i]);
                break;

            case ANDROID_HOT_PIXEL_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("hot pixel mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_DEMOSAIC_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("demosaic mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_NOISE_REDUCTION_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("noise mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_NOISE_REDUCTION_STRENGTH:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("noise strength: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_SHADING_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("shading mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_GEOMETRIC_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("geometric mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_COLOR_CORRECTION_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("color mode: %d", curEntry.data.u8[0]);
                break;

            //case ANDROID_COLOR_TRANSFORM:
                //ret = checkEntry(curEntry, TYPE_FLOAT);
                //if (ret == NO_ERROR)
                    //for (i = 0; i < curEntry.count; i++)
                    //ALOGV("color transform[%d] %.8f", i, curEntry.data.f[i]);
                //break;

            case ANDROID_TONEMAP_CURVE_RED:
                ret = checkEntry(curEntry, TYPE_FLOAT);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("tonemap curve red[%d]  %.8f", i, curEntry.data.f[i]);
                break;

            case ANDROID_TONEMAP_CURVE_GREEN:
                ret = checkEntry(curEntry, TYPE_FLOAT);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("tonemap curve green[%d]  %.8f", i, curEntry.data.f[i]);
                break;

            case ANDROID_TONEMAP_CURVE_BLUE:
                ret = checkEntry(curEntry, TYPE_FLOAT);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("tonemap curve blue[%d]  %.8f", i, curEntry.data.f[i]);
                break;

            case ANDROID_TONEMAP_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("tonemap mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_EDGE_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("edge mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_EDGE_STRENGTH:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("edge strength: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_STATISTICS_HISTOGRAM_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("histogram mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_STATISTICS_SHARPNESS_MAP_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR)
                    ALOGV("sharpness mode: %d", curEntry.data.u8[0]);
                break;

            case ANDROID_CONTROL_EFFECT_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR) {
                    ALOGV("effect mode: %d", curEntry.data.u8[0]);
                    Sensor->setEffectMode(curEntry.data.u8[0]);
                }
                break;

            case ANDROID_CONTROL_AE_ANTIBANDING_MODE:
                ret = checkEntry(curEntry, TYPE_BYTE, 1);
                if (ret == NO_ERROR) {
                    ALOGV("ae antibanding mode: %d", curEntry.data.u8[0]);
                    Sensor->setAntibandingMode(curEntry.data.u8[0]);
                }
                break;

            case ANDROID_CONTROL_AWB_REGIONS:
                ret = checkEntry(curEntry, TYPE_INT32);
                if (ret == NO_ERROR)
                    for (i = 0; i < curEntry.count; i++)
                        ALOGV("awb regions[%d] : %d", i, curEntry.data.i32[i]);
                break;

            default:
                ALOGE("Bad metadata tag(%s, 0x%x, type %d)", get_camera_metadata_tag_name(curEntry.tag),
                        curEntry.tag, get_camera_metadata_tag_type(curEntry.tag));
                break;
            }
        }
    }
    ALOGV("<====================");

    return NO_ERROR;
}

void NXCommandThread::doListenersCallback(camera_metadata_entry_t &streams, camera_metadata_t *request)
{
    Mutex::Autolock l(ListenerMutex);
    CommandListener *listener = NULL;
    for (uint32_t i = 0; i < streams.count; i++) {
        int streamId = streams.data.i32[i];
        ALOGV("%s: streamId(%d)", __func__, streamId);
        if (CommandListeners.indexOfKey(streamId) >= 0) {
            listener = CommandListeners.valueFor(streamId).get();
            if (!listener) {
                ALOGE("can't find command listener for stream id %d", streamId);
                continue;
            }
            listener->onCommand(streamId, request);
            if (CropLeft || CropTop)
                listener->onZoomChanged(CropLeft, CropTop, CropWidth, CropHeight, Sensor->getSensorW(), Sensor->getSensorH());
        }
    }
}

void NXCommandThread::doListenersCallback()
{
    Mutex::Autolock l(ListenerMutex);
    CommandListener *listener = NULL;
    int streamId;
    for (size_t i = 0; i < CommandListeners.size(); i++) {
        streamId = CommandListeners.keyAt(i);
        listener = CommandListeners.valueFor(streamId).get();
        if (listener)
            listener->onCommand(streamId, NULL);
    }
}

void NXCommandThread::doZoomChanged(int left, int top, int width, int height, int baseWidth, int baseHeight)
{
    Mutex::Autolock l(ListenerMutex);
    CommandListener *listener = NULL;
    int streamId;
    for (size_t i = 0; i < CommandListeners.size(); i++) {
        streamId = CommandListeners.keyAt(i);
        listener = CommandListeners.valueFor(streamId).get();
        if (listener)
            listener->onZoomChanged(left, top, width, height, baseWidth, baseHeight);
    }
}

void NXCommandThread::doExifChanged()
{
    Mutex::Autolock l(ListenerMutex);
    CommandListener *listener = NULL;
    int streamId;
    ALOGD("doExifChanged()");
    for (size_t i = 0; i < CommandListeners.size(); i++) {
        streamId = CommandListeners.keyAt(i);
        listener = CommandListeners.valueFor(streamId).get();
        if (listener)
            listener->onExifChanged(Exif);
    }
}

void NXCommandThread::waitIdle()
{
    int count = 2;
    while(IsIdle == false && count >= 0) {
        ALOGD("waitIdle");
        usleep(100000);
        count--;
    }
}

bool NXCommandThread::handleExif(camera_metadata_t *request)
{
    bool changed = false;
    camera_metadata_entry_t entry;

    // thumbnail size
    int ret = find_camera_metadata_entry(request, ANDROID_JPEG_THUMBNAIL_SIZE, &entry);
    if (ret == NO_ERROR) {
        changed |= Exif->setThumbResolution(entry.data.i32[0], entry.data.i32[1]);
        // debugging
        if (changed)
            ALOGD("thumbnail resolution: %dx%d", Exif->widthThumb, Exif->heightThumb);
    }

    // thumbnail quality
    ret = find_camera_metadata_entry(request, ANDROID_JPEG_THUMBNAIL_QUALITY, &entry);
    if (ret == NO_ERROR)
        changed |= Exif->setThumbnailQuality(entry.data.i32[0]);

    // orientation
    ret = find_camera_metadata_entry(request, ANDROID_JPEG_ORIENTATION, &entry);
    if (ret == NO_ERROR) {
        int32_t orientation = entry.data.i32[0];
        switch (entry.data.i32[0]) {
        case 90:
            orientation = EXIF_ORIENTATION_90;
            break;
        case 180:
            orientation = EXIF_ORIENTATION_180;
            break;
        case 270:
            orientation = EXIF_ORIENTATION_270;
            break;
        case 0:
        default:
            orientation = EXIF_ORIENTATION_UP;
            break;
        }
        changed |= Exif->setOrientation(orientation);
    }

    // iso_speed_rating
    ret = find_camera_metadata_entry(request, ANDROID_SENSOR_SENSITIVITY, &entry);
    if (ret == NO_ERROR)
        changed |= Exif->setIsoSpeedRating(entry.data.i32[0]);

    // white_balance
    ret = find_camera_metadata_entry(request, ANDROID_CONTROL_AWB_MODE, &entry);
    if (ret == NO_ERROR) {
        uint32_t white_balance;
        if (entry.data.u8[0] == ANDROID_CONTROL_AWB_MODE_AUTO)
            white_balance = EXIF_WB_AUTO;
        else
            white_balance = EXIF_WB_MANUAL;
        changed |= Exif->setWhiteBalance(white_balance);
    }

    // scene mode
    ret = find_camera_metadata_entry(request, ANDROID_CONTROL_SCENE_MODE, &entry);
    if (ret == NO_ERROR) {
        uint32_t scene_capture_type;
        switch (entry.data.u8[0]) {
        case ANDROID_CONTROL_SCENE_MODE_PORTRAIT:
            scene_capture_type = EXIF_SCENE_PORTRAIT;
            break;
        case ANDROID_CONTROL_SCENE_MODE_LANDSCAPE:
            scene_capture_type = EXIF_SCENE_LANDSCAPE;
            break;
        case ANDROID_CONTROL_SCENE_MODE_NIGHT:
            scene_capture_type = EXIF_SCENE_NIGHT;
            break;
        default:
            scene_capture_type = EXIF_SCENE_STANDARD;
            break;
        }
        changed |= Exif->setSceneCaptureType(scene_capture_type);
    }

    // exposure_time
    ret = find_camera_metadata_entry(request, ANDROID_SENSOR_EXPOSURE_TIME, &entry);
    if (ret == NO_ERROR) {
        int64_t exposureTime = entry.data.i64[0];
        int shutterSpeed = exposureTime/1000;
        if (shutterSpeed > 500000)
            shutterSpeed -= 100000;
        if (shutterSpeed < 0)
            shutterSpeed = 100;

        rational_t exposure_time;
        exposure_time.num = 1;
        exposure_time.den = (uint32_t)((double)100000 / shutterSpeed);
        changed |= Exif->setExposureTime(exposure_time);
    }

    // focal_length
    ret = find_camera_metadata_entry(request, ANDROID_LENS_FOCAL_LENGTH, &entry);
    if (ret == NO_ERROR) {
        rational_t focal_length;
        focal_length.num = (uint32_t)(entry.data.f[0] * EXIF_DEF_FOCAL_LEN_DEN);
        focal_length.den = EXIF_DEF_FOCAL_LEN_DEN;
        // ALOGD("focalLength: %f", entry.data.f[0]);
        changed |= Exif->setFocalLength(focal_length);
    }

    // gps coordinates
    ret = find_camera_metadata_entry(request, ANDROID_JPEG_GPS_COORDINATES, &entry);
    if (ret == NO_ERROR) {
        bool gpsChanged = Exif->setGpsCoordinates(entry.data.d);
        if (gpsChanged) {
            ALOGD("gps_coordinates: %lf, %lf, %lf", Exif->gps_coordinates[0], Exif->gps_coordinates[1], Exif->gps_coordinates[2]);
        }
        changed |= gpsChanged;
    }

    // gps processing method
    ret = find_camera_metadata_entry(request, ANDROID_JPEG_GPS_PROCESSING_METHOD, &entry);
    if (ret == NO_ERROR) {
        changed |= Exif->setGpsProcessingMethod(entry.data.u8, entry.count);
        // for debugging
#if 0
        if (strcmp(reinterpret_cast<char const *>(Exif->gps_processing_method), "None"))
            ALOGD("gps_processing_method: %s", Exif->gps_processing_method);
#endif
    }

    // gps timestamp i64
    ret = find_camera_metadata_entry(request, ANDROID_JPEG_GPS_TIMESTAMP, &entry);
    if (ret == NO_ERROR)
        changed |= Exif->setGpsTimestamp(entry.data.i64[0]);

    return changed;
}

bool NXCommandThread::processCommand()
{
    trace_in();

    status_t ret = NO_ERROR;

    while (!exitPending()) {
        camera_metadata_t *request = NULL;

        RequestQueueSrcOps->dequeue_request(RequestQueueSrcOps, &request);
        if (!request) {
            ALOGE("request is NULL!!!");
            doListenersCallback();
            IsIdle = true;
            ALOGD("Exit processCommand");
            return true;
        }

        camera_metadata_entry_t entry;

        /* for ae exposure */
        ret = find_camera_metadata_entry(request, ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &entry);
        if (ret == NO_ERROR) {
            Sensor->setExposure(entry.data.i32[0]);
        }

        /* for awb mode */
        ret = find_camera_metadata_entry(request, ANDROID_CONTROL_AWB_MODE, &entry);
        if (ret == NO_ERROR) {
            Sensor->setAwbMode(entry.data.u8[0]);
        }

        /* for af mode */
        ret = find_camera_metadata_entry(request, ANDROID_CONTROL_AF_MODE, &entry);
        if (ret == NO_ERROR) {
            Sensor->setAfMode(entry.data.u8[0]);
        }

        /* for scene mode */
        ret = find_camera_metadata_entry(request, ANDROID_CONTROL_SCENE_MODE, &entry);
        if (ret == NO_ERROR) {
            Sensor->setSceneMode(entry.data.u8[0]);
        }

        /* for effect mode */
        ret = find_camera_metadata_entry(request, ANDROID_CONTROL_EFFECT_MODE, &entry);
        if (ret == NO_ERROR) {
            Sensor->setEffectMode(entry.data.u8[0]);
        }

        /* for antibanding mode */
        ret = find_camera_metadata_entry(request, ANDROID_CONTROL_AE_ANTIBANDING_MODE, &entry);
        if (ret == NO_ERROR) {
            Sensor->setAntibandingMode(entry.data.u8[0]);
        }

        /* for zoom */
        ret = find_camera_metadata_entry(request, ANDROID_SCALER_CROP_REGION, &entry);
        if (ret == NO_ERROR && CropWidth != (unsigned int)entry.data.i32[2]) {
            CropWidth = entry.data.i32[2];
            CropHeight = (Sensor->getSensorH()*CropWidth)/Sensor->getSensorW();
            CropLeft = entry.data.i32[0];
            CropTop = entry.data.i32[1];
            ALOGV("CROP===> left %d, top %d, w %d, h %d", CropLeft, CropTop, CropWidth, CropHeight);
            doZoomChanged(CropLeft, CropTop, CropWidth, CropHeight, Sensor->getSensorW(), Sensor->getSensorH());
        }

        /* for exif */
        if (handleExif(request))
            doExifChanged();

        /* for streams */
        ret = find_camera_metadata_entry(request, ANDROID_REQUEST_OUTPUT_STREAMS, &entry);
        if (ret != NO_ERROR)
            ALOGE("can't find streams request");
        else
            doListenersCallback(entry, request);

        RequestQueueSrcOps->free_request(RequestQueueSrcOps, request);
    }

    trace_exit();
    return true;
}

status_t NXCommandThread::registerListener(int32_t id, sp<CommandListener> listener)
{
    Mutex::Autolock l(ListenerMutex);
    ALOGD("%s: add %d %p", __func__, id, listener.get());
    if (ListenerMap[id]) {
        CommandListeners.replaceValueFor(id, listener);
    } else {
        CommandListeners.add(id, listener);
    }
    ListenerMap[id] = 1;
    return NO_ERROR;
}

status_t NXCommandThread::removeListener(int32_t id)
{
    Mutex::Autolock l(ListenerMutex);
    ALOGD("%s: remove %d", __func__, id);
    if (ListenerMap[id]) {
        CommandListeners.removeItem(id);
        ListenerMap[id] = 0;
    }
#if 0
    if (CommandListeners.isEmpty()) {
        ALOGE("No valid listener for %d", id);
        return NO_ERROR;
    }
    ALOGD("not empty");
    if (CommandListeners.indexOfKey(id) >= 0) {
        ALOGD("valid id");
#if 0
        CommandListener *listener = CommandListeners.valueFor(id).get();
        if (listener)
            CommandListeners.removeItem(id);
#else
        CommandListeners.removeItem(id);
#endif
    }
#endif
    return NO_ERROR;
}

}; // namespace android
