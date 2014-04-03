#include <linux/videodev2.h>
#include <nxp-v4l2.h>
#include "GC0308.h"

using namespace android;

enum {
    WB_INCANDESCENT = 0,
    WB_FLUORESCENT,
    WB_DAYLIGHT,
    WB_CLOUDY,
    WB_TUNGSTEN,
    WB_AUTO,
    WB_MAX
};

enum {
    COLORFX_NONE = 0,
    COLORFX_MONO,
    COLORFX_SEPIA,
    COLORFX_NEGATIVE,
    COLORFX_MAX
};

#define MIN_EXPOSURE     0
#define MAX_EXPOSURE     6

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

const int32_t ResolutionGC0308[] = {
    // 648, 480,
    608, 479,
};

// TOP/system/media/camera/include/system/camera_metadata_tags.h
static const uint8_t AvailableAfModesGC0308[] = {
    ANDROID_CONTROL_AF_MODE_OFF,
    ANDROID_CONTROL_AF_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_MACRO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO
};

static const uint8_t AvailableAeModesGC0308[] = {
    ANDROID_CONTROL_AE_MODE_OFF,
    ANDROID_CONTROL_AE_MODE_ON,
    ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH
};

static const uint8_t SceneModeOverridesGC0308[] = {
    // ANDROID_CONTROL_SCENE_MODE_PORTRAIT
    ANDROID_CONTROL_AE_MODE_ON,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
    // ANDROID_CONTROL_SCENE_MODE_LANDSCAPE
    ANDROID_CONTROL_AE_MODE_ON,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
    // ANDROID_CONTROL_SCENE_MODE_SPORTS
    ANDROID_CONTROL_AE_MODE_ON,
    ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
    // ANDROID_CONTROL_SCENE_MODE_NIGHT
#if 0
    ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE
#endif
};

static const uint8_t AvailableEffectsGC0308[] = {
    ANDROID_CONTROL_EFFECT_MODE_MONO,
    ANDROID_CONTROL_EFFECT_MODE_NEGATIVE,
    ANDROID_CONTROL_EFFECT_MODE_SEPIA,
    ANDROID_CONTROL_EFFECT_MODE_AQUA
};

static const uint8_t AvailableSceneModesGC0308[] = {
    ANDROID_CONTROL_SCENE_MODE_PORTRAIT,
    ANDROID_CONTROL_SCENE_MODE_LANDSCAPE,
    ANDROID_CONTROL_SCENE_MODE_SPORTS,
#if 0
    ANDROID_CONTROL_SCENE_MODE_NIGHT
#endif
};

static const int32_t AvailableFpsRangesGC0308[] = {
    8, 15
};

static const uint32_t ExposureCompensationRangeGC0308[] = {
    // MIN_EXPOSURE, MAX_EXPOSURE
    -3, 3
};

static const uint8_t AvailableAntibandingModesGC0308[] = {
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ
};

static const uint8_t AvailableAwbModesGC0308[] = {
    ANDROID_CONTROL_AWB_MODE_OFF,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_FLUORESCENT,
    ANDROID_CONTROL_AWB_MODE_INCANDESCENT
};

void GC0308::init()
{
    // TODO
    Width = ResolutionGC0308[0];
    Height = ResolutionGC0308[1];
    NumResolutions = ARRAY_SIZE(ResolutionGC0308)/2;
    Resolutions = ResolutionGC0308;
    NumAvailableAfModes = ARRAY_SIZE(AvailableAfModesGC0308);
    AvailableAfModes = AvailableAfModesGC0308;
    NumAvailableAeModes = ARRAY_SIZE(AvailableAeModesGC0308);
    AvailableAeModes = AvailableAeModesGC0308;
    NumSceneModeOverrides = ARRAY_SIZE(SceneModeOverridesGC0308);
    SceneModeOverrides = SceneModeOverridesGC0308;
    ExposureCompensationRange = ExposureCompensationRangeGC0308;
    AvailableAntibandingModes = AvailableAntibandingModesGC0308;
    NumAvailAntibandingModes = ARRAY_SIZE(AvailableAntibandingModesGC0308);
    AvailableAwbModes = AvailableAwbModesGC0308;
    NumAvailAwbModes = ARRAY_SIZE(AvailableAwbModesGC0308);
    AvailableEffects = AvailableEffectsGC0308;
    NumAvailEffects = ARRAY_SIZE(AvailableEffectsGC0308);
    AvailableSceneModes = AvailableSceneModesGC0308;
    NumAvailSceneModes = ARRAY_SIZE(AvailableSceneModesGC0308);
    AvailableFpsRanges = AvailableFpsRangesGC0308;
    NumAvailableFpsRanges = ARRAY_SIZE(AvailableFpsRangesGC0308);

    // TODO
    FocalLength = 3.43f;
    Aperture = 2.7f;
    MinFocusDistance = 0.1f;
    FNumber = 2.7f;

    // TODO
    //MaxFaceCount = 16;
    MaxFaceCount = 1;

    // Crop
    CropWidth = Width;
    CropHeight = Height;
    CropLeft = 0;
    CropTop = 0;
}

GC0308::GC0308()
{
    init();
}

GC0308::GC0308(uint32_t v4l2ID)
    : NXCameraBoardSensor(v4l2ID)
{
    init();
}

GC0308::~GC0308()
{
}

void GC0308::setAfMode(uint8_t afMode)
{
    AfMode = afMode;
}

void GC0308::afEnable(bool enable)
{
}

void GC0308::setEffectMode(uint8_t effectMode)
{
    if (effectMode != EffectMode) {
        uint32_t val = 0;

        switch (effectMode) {
        case ANDROID_CONTROL_EFFECT_MODE_OFF:
            val = COLORFX_NONE;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_MONO:
            val = COLORFX_MONO;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_NEGATIVE:
            val = COLORFX_NEGATIVE;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_SEPIA:
            val = COLORFX_SEPIA;
            break;
        default:
            //ALOGE("%s: unsupported effectmode 0x%x", __func__, effectMode);
            return;
        }

        v4l2_set_ctrl(V4l2ID, V4L2_CID_COLORFX, val);
        EffectMode = effectMode;
    }
}

void GC0308::setSceneMode(uint8_t sceneMode)
{
}

void GC0308::setAntibandingMode(uint8_t antibandingMode)
{
}

void GC0308::setAwbMode(uint8_t awbMode)
{
    if (awbMode != AwbMode) {
        uint32_t val = 0;

        switch (awbMode) {
        case ANDROID_CONTROL_AWB_MODE_OFF:
            v4l2_set_ctrl(V4l2ID, V4L2_CID_AUTO_WHITE_BALANCE, 0);
            return;
        case ANDROID_CONTROL_AWB_MODE_AUTO:
            v4l2_set_ctrl(V4l2ID, V4L2_CID_AUTO_WHITE_BALANCE, 1);
            return;
        case ANDROID_CONTROL_AWB_MODE_DAYLIGHT:
            val = WB_DAYLIGHT;
            break;
        case ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT:
            val = WB_DAYLIGHT;
            break;
        case ANDROID_CONTROL_AWB_MODE_FLUORESCENT:
            val = WB_FLUORESCENT;
            break;
        case ANDROID_CONTROL_AWB_MODE_INCANDESCENT:
            val = WB_INCANDESCENT;
            break;
        default:
            //ALOGE("%s: unsupported awb mode 0x%x", __func__, awbMode);
            return;
        }
        AwbMode = awbMode;

        v4l2_set_ctrl(V4l2ID, V4L2_CID_WHITE_BALANCE_TEMPERATURE, val);
    }
}

void GC0308::setExposure(int32_t exposure)
{
    if (exposure < MIN_EXPOSURE || exposure > MAX_EXPOSURE) {
        ALOGE("%s: invalid exposure %d", __func__, exposure);
        return;
    }

    if (exposure != Exposure) {
        Exposure = exposure;
        v4l2_set_ctrl(V4l2ID, V4L2_CID_BRIGHTNESS, exposure);
    }
}

uint32_t GC0308::getZoomFactor(void)
{
    // disable zoom
    return 1;
}

status_t GC0308::setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height)
{
    return NO_ERROR;
}

int GC0308::setFormat(int width, int height, int format)
{
    int sensorWidth, sensorHeight;
    if (width == 608)
        sensorWidth = 640;
    else {
        ALOGE("%s: invalid width %d", __func__, width);
        return -EINVAL;
    }
    if (height == 479)
        sensorHeight = 480;
    else {
        ALOGE("%s: invalid height %d", __func__, height);
        return -EINVAL;
    }
    return v4l2_set_format(V4l2ID, sensorWidth, sensorHeight, format);
}
