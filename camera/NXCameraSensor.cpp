#define LOG_TAG "NXCameraSensor"
#include <utils/Log.h>

#include "Constants.h"
#include "NXCameraSensor.h"

namespace android {

// constants
const int32_t NXCameraSensor::kSensitivityRange[2] = {
    100, 1600
};
const nsecs_t NXCameraSensor::kExposureTimeRange[2] = {
    1000L, 30000000000L // 1us - 30sec
};
const nsecs_t NXCameraSensor::kFrameDurationRange[2] = {
    33331760L, 30000000000L // 1/30sec - 30sec
};

const uint8_t NXCameraSensor::kColorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;

const uint32_t kAvailableFormats[2] = {
    // HAL_PIXEL_FORMAT_YCbCr_422_I,
    HAL_PIXEL_FORMAT_YV12,
    HAL_PIXEL_FORMAT_YCrCb_420_SP,
};

const uint32_t NXCameraSensor::kMaxRawValue = 4000;
const uint32_t NXCameraSensor::kBlackLevel = 1000;

const uint64_t kAvailableRawMinDurations[1] = {
    NXCameraSensor::kFrameDurationRange[0],
};

const uint64_t kAvailableProcessedMinDurations[1] = {
    NXCameraSensor::kFrameDurationRange[0],
};

NXCameraSensor::NXCameraSensor(int cameraId)
    : CameraId(cameraId)
{
    RawSensor = get_board_camera_sensor(cameraId);
}

NXCameraSensor::~NXCameraSensor()
{
    delete RawSensor;
}

int32_t NXCameraSensor::getSensorW()
{
    return RawSensor->Width;
}

int32_t NXCameraSensor::getSensorH()
{
    return RawSensor->Height;
}

bool NXCameraSensor::isSupportedResolution(int width, int height)
{
    for (int i = 0; i < RawSensor->NumResolutions; i++) {
        if (RawSensor->Resolutions[2*i] == width &&
            RawSensor->Resolutions[2*i+1] == height)
            return true;
    }
    return false;
}

static status_t addOrSize(camera_metadata_t *request,
        bool sizeRequest,
        size_t *entryCount,
        size_t *dataCount,
        uint32_t tag,
        const void *entryData,
        size_t entryDataCount)
{
    status_t res;
    if (!sizeRequest) {
        return add_camera_metadata_entry(request, tag, entryData,
                entryDataCount);
    } else {
        int type = get_camera_metadata_tag_type(tag);
        if (type < 0) return BAD_VALUE;
        (*entryCount)++;
        ALOGV("tag %s: size %d", get_camera_metadata_tag_name(tag), entryDataCount);
        (*dataCount) += calculate_camera_metadata_entry_data_size(type, entryDataCount);
        return OK;
    }
}

status_t NXCameraSensor::constructStaticInfo(camera_metadata_t **info,
        int cameraId, bool sizeRequest)
{
    size_t entryCount = 0;
    size_t dataCount = 0;
    status_t ret;

#define ADD_OR_SIZE( tag, data, count ) \
    if ( ( ret = addOrSize(*info, sizeRequest, &entryCount, &dataCount, \
                    tag, data, count) ) != OK) return ret;

    // android.lens
    ADD_OR_SIZE(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &RawSensor->MinFocusDistance, 1);
    ADD_OR_SIZE(ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE, &RawSensor->MinFocusDistance, 1);

    ADD_OR_SIZE(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &RawSensor->FocalLength, 1);
    ADD_OR_SIZE(ANDROID_LENS_INFO_AVAILABLE_APERTURES, &RawSensor->Aperture, 1);

    static const float filterDensity = 0;
    ADD_OR_SIZE(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES, &filterDensity, 1);
    static const uint8_t availableOpticalStabilization = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
    ADD_OR_SIZE(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION, &availableOpticalStabilization, 1);

    static const int32_t lensShadingMapSize[] = {1, 1};
    ADD_OR_SIZE(ANDROID_LENS_INFO_SHADING_MAP_SIZE, lensShadingMapSize, sizeof(lensShadingMapSize)/sizeof(int32_t));

    //static const float lensShadingMap[3 * 1 * 1] = {1.f, 1.f, 1.f};
    //ADD_OR_SIZE(ANDROID_LENS_SHADING_MAP, lensShadingMap, sizeof(lensShadingMap)/sizeof(float));

    int32_t lensFacing = CameraId ? ANDROID_LENS_FACING_FRONT : ANDROID_LENS_FACING_BACK;
    ADD_OR_SIZE(ANDROID_LENS_FACING, &lensFacing, 1);

    // android.sensor
    ADD_OR_SIZE(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, NXCameraSensor::kExposureTimeRange, 2);
    ADD_OR_SIZE(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &NXCameraSensor::kFrameDurationRange[1], 1);
    ADD_OR_SIZE(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE,
            NXCameraSensor::kSensitivityRange,
            sizeof(NXCameraSensor::kSensitivityRange)/sizeof(int32_t));
    ADD_OR_SIZE(ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT, &NXCameraSensor::kColorFilterArrangement, 1);

    static const float sensorPhysicalSize[2] = {3.20f, 2.40f}; // mm
    ADD_OR_SIZE(ANDROID_SENSOR_INFO_PHYSICAL_SIZE, sensorPhysicalSize, 2);

    int32_t pixelArraySize[2] = {
        RawSensor->Width, RawSensor->Height};
    ADD_OR_SIZE(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixelArraySize, 2);
    ADD_OR_SIZE(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, pixelArraySize, 2);

    ADD_OR_SIZE(ANDROID_SENSOR_INFO_WHITE_LEVEL, &NXCameraSensor::kMaxRawValue, 1);

    static const int32_t blackLevelPattern[4] = {
        NXCameraSensor::kBlackLevel, NXCameraSensor::kBlackLevel,
        NXCameraSensor::kBlackLevel, NXCameraSensor::kBlackLevel
    };
    ADD_OR_SIZE(ANDROID_SENSOR_BLACK_LEVEL_PATTERN, blackLevelPattern, sizeof(blackLevelPattern)/sizeof(int32_t));

    // android.flash
    uint8_t flashAvailable = (CameraId == 0) ? 1 : 0;
    ADD_OR_SIZE(ANDROID_FLASH_INFO_AVAILABLE, &flashAvailable, 1);

    static const int64_t flashChargeDuration = 0;
    ADD_OR_SIZE(ANDROID_FLASH_INFO_CHARGE_DURATION, &flashChargeDuration, 1);

    // android.tonemap

    static const int32_t tonemapCurvePoints = 128;
    ADD_OR_SIZE(ANDROID_TONEMAP_MAX_CURVE_POINTS, &tonemapCurvePoints, 1);

    // android.scaler
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_FORMATS, kAvailableFormats, sizeof(kAvailableFormats)/sizeof(uint32_t));

    int32_t availableRawSizes[2] = {
        RawSensor->Width, RawSensor->Height
    };
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_RAW_SIZES, availableRawSizes, 2);

    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_RAW_MIN_DURATIONS, kAvailableRawMinDurations, sizeof(kAvailableRawMinDurations)/sizeof(uint64_t));

    // test camcoder
#if 0
    static const int32_t processed_sizes[] = {
        720, 480,
        352, 288,
        320, 240,
        176, 144,
        160, 120,
    };
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES, processed_sizes, 5 * 2);
#else
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES, RawSensor->Resolutions, RawSensor->NumResolutions * 2);
#endif

    // TODO
#if 1
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_JPEG_SIZES, RawSensor->Resolutions, RawSensor->NumResolutions * 2);
#else
    static const uint32_t availableJpegSizes[2] = {
        800, 600
    };
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_JPEG_SIZES, availableJpegSizes, sizeof(availableJpegSizes)/sizeof(uint32_t));
#endif

     static float maxZoom = (float)RawSensor->getZoomFactor();
     if (maxZoom <= 1.0)
         maxZoom = DEFAULT_ZOOM_FACTOR;
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, &maxZoom, 1);

    // android.jpeg
    static const int32_t jpegThumnailSizes[] = {
        160, 120,
        160, 160,
        160, 90,
        144, 96,
        0, 0,
    };
    ADD_OR_SIZE(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES, jpegThumnailSizes, sizeof(jpegThumnailSizes)/sizeof(int32_t));

    static const int32_t jpegMaxSize = 10 * 1024 * 1024; // 10M
    ADD_OR_SIZE(ANDROID_JPEG_MAX_SIZE, &jpegMaxSize, 1);

    // android.stats
    static const uint8_t availableFaceDetectModes[] = {
        ANDROID_STATISTICS_FACE_DETECT_MODE_OFF,
        ANDROID_STATISTICS_FACE_DETECT_MODE_FULL
    };
    ADD_OR_SIZE(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES, availableFaceDetectModes, sizeof(availableFaceDetectModes));

    ADD_OR_SIZE(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, &RawSensor->MaxFaceCount, 1);

    static const int32_t histogramSize = 64;
    ADD_OR_SIZE(ANDROID_STATISTICS_INFO_HISTOGRAM_BUCKET_COUNT, &histogramSize, 1);

    static const int32_t maxHistogramCount = 1000;
    ADD_OR_SIZE(ANDROID_STATISTICS_INFO_MAX_HISTOGRAM_COUNT, &maxHistogramCount, 1);

    static const int32_t sharpnessMapSize[2] = {64, 64};
    ADD_OR_SIZE(ANDROID_STATISTICS_INFO_SHARPNESS_MAP_SIZE, sharpnessMapSize, sizeof(sharpnessMapSize)/sizeof(int32_t));

    static const int32_t maxSharpnessMapValue = 1000;
    ADD_OR_SIZE(ANDROID_STATISTICS_INFO_MAX_SHARPNESS_MAP_VALUE, &maxSharpnessMapValue, 1);

    ADD_OR_SIZE(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, RawSensor->AvailableSceneModes, RawSensor->NumAvailSceneModes);

    ADD_OR_SIZE(ANDROID_CONTROL_AVAILABLE_EFFECTS, RawSensor->AvailableEffects, RawSensor->NumAvailEffects);

    int32_t max3aRegions = 1;
    ADD_OR_SIZE(ANDROID_CONTROL_MAX_REGIONS, &max3aRegions, 1);

    ADD_OR_SIZE(ANDROID_CONTROL_AE_AVAILABLE_MODES, RawSensor->AvailableAeModes, RawSensor->NumAvailableAeModes);

    static const camera_metadata_rational exposureCompensationStep = {
            1, 1
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_COMPENSATION_STEP,
            &exposureCompensationStep, 1);

    int32_t exposureCompensationRange[] = {-3, 3};
    ADD_OR_SIZE(ANDROID_CONTROL_AE_COMPENSATION_RANGE,
            exposureCompensationRange,
            sizeof(exposureCompensationRange)/sizeof(int32_t));

    if (RawSensor->NumAvailableFpsRanges > 0) {
        ADD_OR_SIZE(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, RawSensor->AvailableFpsRanges, RawSensor->NumAvailableFpsRanges);
    } else {
        static const int32_t availableTargetFpsRanges[] = {
            // 15, 15, 24, 24, 25, 25, 25, 15, 30, 30, 30
            15, 15, 25, 25, 30, 30
        };
        ADD_OR_SIZE(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, availableTargetFpsRanges, sizeof(availableTargetFpsRanges)/sizeof(int32_t));
    }

    ADD_OR_SIZE(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES, RawSensor->AvailableAntibandingModes, RawSensor->NumAvailAntibandingModes);

    ADD_OR_SIZE(ANDROID_CONTROL_AWB_AVAILABLE_MODES, RawSensor->AvailableAwbModes, RawSensor->NumAvailAwbModes);

    ADD_OR_SIZE(ANDROID_CONTROL_AF_AVAILABLE_MODES, RawSensor->AvailableAfModes, RawSensor->NumAvailableAfModes);

    static const uint8_t availableVstabModes[] = {
        ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF,
        ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES, availableVstabModes, sizeof(availableVstabModes));

    // TODO
    ADD_OR_SIZE(ANDROID_CONTROL_SCENE_MODE_OVERRIDES, RawSensor->SceneModeOverrides, RawSensor->NumSceneModeOverrides);

    static const uint8_t quirkTriggerAuto = 1;
    ADD_OR_SIZE(ANDROID_QUIRKS_TRIGGER_AF_WITH_AUTO, &quirkTriggerAuto, 1);

    static const uint8_t quirkUseZslFormat = 1;
    ADD_OR_SIZE(ANDROID_QUIRKS_USE_ZSL_FORMAT, &quirkUseZslFormat, 1);

    static const uint8_t quirkMeteringCropRegion = 1;
    ADD_OR_SIZE(ANDROID_QUIRKS_METERING_CROP_REGION, &quirkMeteringCropRegion, 1);

#undef ADD_OR_SIZE
    if (sizeRequest) {
        *info = allocate_camera_metadata(entryCount, dataCount);
        if (*info == NULL) {
            ALOGE("Unable to allocate camera static info(%d entries, %d bytes extra data)", entryCount, dataCount);
            return NO_MEMORY;
        }
    }
    return OK;
}

status_t NXCameraSensor::constructDefaultRequest(int requestTemplate,
        camera_metadata_t **request, bool sizeRequest)
{
    size_t entryCount = 0;
    size_t dataCount = 0;
    status_t ret;

#define ADD_OR_SIZE( tag, data, count ) \
    if ( ( ret = addOrSize(*request, sizeRequest, &entryCount, &dataCount, \
                    tag, data, count) ) != OK) return ret

    static const int64_t USEC = 1000LL;
    static const int64_t MSEC = USEC * 1000LL;
    static const int64_t SEC = MSEC * 1000LL;

    // android.request
    static const uint8_t metadataMode = ANDROID_REQUEST_METADATA_MODE_NONE;
    ADD_OR_SIZE(ANDROID_REQUEST_METADATA_MODE, &metadataMode, 1);

    static const int32_t id = 0;
    ADD_OR_SIZE(ANDROID_REQUEST_ID, &id, 1);

    static const int32_t frameCount = 0;
    ADD_OR_SIZE(ANDROID_REQUEST_FRAME_COUNT, &frameCount , 1);

    entryCount += 1;
    dataCount += 5;

    // android.lens
    static const float focusDistance = 0;
    ADD_OR_SIZE(ANDROID_LENS_FOCUS_DISTANCE, &focusDistance, 1);

    ADD_OR_SIZE(ANDROID_LENS_APERTURE, &RawSensor->Aperture, 1);

    ADD_OR_SIZE(ANDROID_LENS_FOCAL_LENGTH, &RawSensor->FocalLength, 1);

    static const float filterDensity = 0;
    ADD_OR_SIZE(ANDROID_LENS_FILTER_DENSITY, &filterDensity, 1);

    static const uint8_t opticalStabilizationMode = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
    ADD_OR_SIZE(ANDROID_LENS_OPTICAL_STABILIZATION_MODE, &opticalStabilizationMode, 1);

    // android.Sensor
    static const int64_t frameDuration = 33333333L; // 1/30 s
    ADD_OR_SIZE(ANDROID_SENSOR_FRAME_DURATION, &frameDuration, 1);

    // android.flash
    static const uint8_t flashMode = ANDROID_FLASH_MODE_OFF;
    ADD_OR_SIZE(ANDROID_FLASH_MODE, &flashMode, 1);

    static const uint8_t flashPower = 10;
    ADD_OR_SIZE(ANDROID_FLASH_FIRING_POWER, &flashPower, 1);

    static const int64_t firingTime = 0;
    ADD_OR_SIZE(ANDROID_FLASH_FIRING_TIME, &firingTime, 1);

    /** Processing block modes */
    uint8_t hotPixelMode = 0;
    uint8_t demosaicMode = 0;
    uint8_t noiseMode = 0;
    uint8_t shadingMode = 0;
    uint8_t geometricMode = 0;
    uint8_t colorMode = 0;
    uint8_t tonemapMode = 0;
    uint8_t edgeMode = 0;
    uint8_t vstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;

    switch (requestTemplate) {
      case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        vstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      case CAMERA2_TEMPLATE_STILL_CAPTURE:
      case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        hotPixelMode = ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY;
        demosaicMode = ANDROID_DEMOSAIC_MODE_HIGH_QUALITY;
        noiseMode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        shadingMode = ANDROID_SHADING_MODE_HIGH_QUALITY;
        geometricMode = ANDROID_GEOMETRIC_MODE_HIGH_QUALITY;
        colorMode = ANDROID_COLOR_CORRECTION_MODE_HIGH_QUALITY;
        tonemapMode = ANDROID_TONEMAP_MODE_HIGH_QUALITY;
        edgeMode = ANDROID_EDGE_MODE_HIGH_QUALITY;
        break;
      case CAMERA2_TEMPLATE_VIDEO_RECORD:
        vstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      case CAMERA2_TEMPLATE_PREVIEW:
      default:
        hotPixelMode = ANDROID_HOT_PIXEL_MODE_FAST;
        demosaicMode = ANDROID_DEMOSAIC_MODE_FAST;
        noiseMode = ANDROID_NOISE_REDUCTION_MODE_FAST;
        shadingMode = ANDROID_SHADING_MODE_FAST;
        geometricMode = ANDROID_GEOMETRIC_MODE_FAST;
        colorMode = ANDROID_COLOR_CORRECTION_MODE_FAST;
        tonemapMode = ANDROID_TONEMAP_MODE_FAST;
        edgeMode = ANDROID_EDGE_MODE_FAST;
        break;
    }
    ADD_OR_SIZE(ANDROID_HOT_PIXEL_MODE, &hotPixelMode, 1);
    ADD_OR_SIZE(ANDROID_DEMOSAIC_MODE, &demosaicMode, 1);
    ADD_OR_SIZE(ANDROID_NOISE_REDUCTION_MODE, &noiseMode, 1);
    ADD_OR_SIZE(ANDROID_SHADING_MODE, &shadingMode, 1);
    ADD_OR_SIZE(ANDROID_GEOMETRIC_MODE, &geometricMode, 1);
    ADD_OR_SIZE(ANDROID_COLOR_CORRECTION_MODE, &colorMode, 1);
    ADD_OR_SIZE(ANDROID_TONEMAP_MODE, &tonemapMode, 1);
    ADD_OR_SIZE(ANDROID_EDGE_MODE, &edgeMode, 1);
    ADD_OR_SIZE(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstabMode, 1);

    // android.noise
    static const uint8_t noiseStrength = 5;
    ADD_OR_SIZE(ANDROID_NOISE_REDUCTION_STRENGTH, &noiseStrength, 1);

    // android.color
    static const float colorTransform[9] = {
        1.f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f
    };
    ADD_OR_SIZE(ANDROID_COLOR_CORRECTION_TRANSFORM, colorTransform, 9);

    // android.tonemap
    static const float tonemapCurve[4] = {
        0.f, 0.f,
        1.f, 1.f
    };
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_RED, tonemapCurve, 32);
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_GREEN, tonemapCurve, 32);
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_BLUE, tonemapCurve, 32);

    // android.edge
    static const uint8_t edgeStrength = 5;
    ADD_OR_SIZE(ANDROID_EDGE_STRENGTH, &edgeStrength, 1);

    // android.scaler
    int32_t cropRegion[3] = {
        0, 0, RawSensor->Width
    };
    ADD_OR_SIZE(ANDROID_SCALER_CROP_REGION, cropRegion, 3);

    // android.jpeg
    static const int32_t jpegQuality = 100;
    ADD_OR_SIZE(ANDROID_JPEG_QUALITY, &jpegQuality, 1);

    static const int32_t thumbnailSize[2] = {
        160, 120
    };
    ADD_OR_SIZE(ANDROID_JPEG_THUMBNAIL_SIZE, thumbnailSize, 2);

    static const int32_t thumbnailQuality = 100;
    ADD_OR_SIZE(ANDROID_JPEG_THUMBNAIL_QUALITY, &thumbnailQuality, 1);

    static const double gpsCoordinates[3] = {
        0, 0, 0
    };
    ADD_OR_SIZE(ANDROID_JPEG_GPS_COORDINATES, gpsCoordinates, 3);

    static const uint8_t gpsProcessingMethod[32] = "None";
    ADD_OR_SIZE(ANDROID_JPEG_GPS_PROCESSING_METHOD, gpsProcessingMethod, 32);

    static const int64_t gpsTimestamp = 0;
    ADD_OR_SIZE(ANDROID_JPEG_GPS_TIMESTAMP, &gpsTimestamp, 1);

    static const int32_t jpegOrientation = get_board_camera_orientation(CameraId);
    ADD_OR_SIZE(ANDROID_JPEG_ORIENTATION, &jpegOrientation, 1);

    // android.stats
    static const uint8_t faceDetectMode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    ADD_OR_SIZE(ANDROID_STATISTICS_FACE_DETECT_MODE, &faceDetectMode, 1);

    static const uint8_t histogramMode = ANDROID_STATISTICS_HISTOGRAM_MODE_OFF;
    ADD_OR_SIZE(ANDROID_STATISTICS_HISTOGRAM_MODE, &histogramMode, 1);

    static const uint8_t sharpnessMapMode = ANDROID_STATISTICS_SHARPNESS_MAP_MODE_OFF;
    ADD_OR_SIZE(ANDROID_STATISTICS_SHARPNESS_MAP_MODE, &sharpnessMapMode, 1);

    // android.control
    uint8_t controlIntent = 0;
    switch (requestTemplate) {
    case CAMERA2_TEMPLATE_PREVIEW:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
        break;
    case CAMERA2_TEMPLATE_STILL_CAPTURE:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
        break;
    case CAMERA2_TEMPLATE_VIDEO_RECORD:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
        break;
    case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
        break;
    case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG;
        break;
    default:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;
        break;
    }
    ADD_OR_SIZE(ANDROID_CONTROL_CAPTURE_INTENT, &controlIntent, 1);

    static const uint8_t controlMode = ANDROID_CONTROL_MODE_AUTO;
    ADD_OR_SIZE(ANDROID_CONTROL_MODE, &controlMode, 1);

    static const uint8_t effectMode = ANDROID_CONTROL_EFFECT_MODE_OFF;
    ADD_OR_SIZE(ANDROID_CONTROL_EFFECT_MODE, &effectMode, 1);

    static const uint8_t sceneMode = ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED;
    ADD_OR_SIZE(ANDROID_CONTROL_SCENE_MODE, &sceneMode, 1);

    static const uint8_t aeMode = ANDROID_CONTROL_AE_MODE_ON;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_MODE, &aeMode, 1);

    int32_t controlRegions[5] = {
        0, 0, RawSensor->Width, RawSensor->Height, 1000
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_REGIONS, controlRegions, 5);

    static const int32_t aeExpCompensation = 0;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &aeExpCompensation, 1);

    static const int32_t aeTargetFpsRange[2] = {
        15, 30
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, aeTargetFpsRange, 2);

    static const uint8_t aeAntibandingMode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &aeAntibandingMode, 1);

    static const uint8_t awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
    ADD_OR_SIZE(ANDROID_CONTROL_AWB_MODE, &awbMode, 1);

    ADD_OR_SIZE(ANDROID_CONTROL_AWB_REGIONS, controlRegions, 5);

    uint8_t afMode = 0;
    switch (requestTemplate) {
    case CAMERA2_TEMPLATE_PREVIEW:
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        break;
    case CAMERA2_TEMPLATE_STILL_CAPTURE:
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        break;
    case CAMERA2_TEMPLATE_VIDEO_RECORD:
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        break;
    case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        break;
    case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        break;
    default:
        afMode = ANDROID_CONTROL_AF_MODE_AUTO;
        break;
    }
    ADD_OR_SIZE(ANDROID_CONTROL_AF_MODE, &afMode, 1);

    ADD_OR_SIZE(ANDROID_CONTROL_AF_REGIONS, controlRegions, 5);

    if (sizeRequest) {
        *request = allocate_camera_metadata(entryCount, dataCount);
        if (*request == NULL) {
            ALOGE("Unable to allocate new requst template type %d (%d entries, %d bytes extra data)",
                    requestTemplate, entryCount, dataCount);
            return NO_MEMORY;
        }
    }
    return OK;
#undef ADD_OR_SIZE
}

} // namespace android
