#include <linux/videodev2.h>
#include <nxp-v4l2.h>
#include "GC2035.h"

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

const int32_t ResolutionGC2035[] = {
    1568, 1176,
    768, 576,       
};

// TOP/system/media/camera/include/system/camera_metadata_tags.h
static const uint8_t AvailableAfModesGC2035[] = {
    ANDROID_CONTROL_AF_MODE_OFF,
    ANDROID_CONTROL_AF_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_MACRO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO
};

static const uint8_t AvailableAeModesGC2035[] = {
    ANDROID_CONTROL_AE_MODE_OFF,
    ANDROID_CONTROL_AE_MODE_ON,
    ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH
};

static const uint8_t SceneModeOverridesGC2035[] = {
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

static const uint8_t AvailableEffectsGC2035[] = {
    ANDROID_CONTROL_EFFECT_MODE_MONO,
    ANDROID_CONTROL_EFFECT_MODE_NEGATIVE,
    ANDROID_CONTROL_EFFECT_MODE_SEPIA,
    ANDROID_CONTROL_EFFECT_MODE_AQUA
};

static const uint8_t AvailableSceneModesGC2035[] = {
    ANDROID_CONTROL_SCENE_MODE_PORTRAIT,
    ANDROID_CONTROL_SCENE_MODE_LANDSCAPE,
    ANDROID_CONTROL_SCENE_MODE_SPORTS,
#if 0
    ANDROID_CONTROL_SCENE_MODE_NIGHT
#endif
};

static const int32_t AvailableFpsRangesGC2035[] = {
    8, 15
};

static const uint32_t ExposureCompensationRangeGC2035[] = {
    // MIN_EXPOSURE, MAX_EXPOSURE
    -3, 3
};

static const uint8_t AvailableAntibandingModesGC2035[] = {
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ
};

static const uint8_t AvailableAwbModesGC2035[] = {
    ANDROID_CONTROL_AWB_MODE_OFF,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_FLUORESCENT,
    ANDROID_CONTROL_AWB_MODE_INCANDESCENT
};

void GC2035::init()
{
    // TODO
    Width = ResolutionGC2035[0];
    Height = ResolutionGC2035[1];
    NumResolutions = ARRAY_SIZE(ResolutionGC2035)/2;
    Resolutions = ResolutionGC2035;
    NumAvailableAfModes = ARRAY_SIZE(AvailableAfModesGC2035);
    AvailableAfModes = AvailableAfModesGC2035;
    NumAvailableAeModes = ARRAY_SIZE(AvailableAeModesGC2035);
    AvailableAeModes = AvailableAeModesGC2035;
    NumSceneModeOverrides = ARRAY_SIZE(SceneModeOverridesGC2035);
    SceneModeOverrides = SceneModeOverridesGC2035;
    ExposureCompensationRange = ExposureCompensationRangeGC2035;
    AvailableAntibandingModes = AvailableAntibandingModesGC2035;
    NumAvailAntibandingModes = ARRAY_SIZE(AvailableAntibandingModesGC2035);
    AvailableAwbModes = AvailableAwbModesGC2035;
    NumAvailAwbModes = ARRAY_SIZE(AvailableAwbModesGC2035);
    AvailableEffects = AvailableEffectsGC2035;
    NumAvailEffects = ARRAY_SIZE(AvailableEffectsGC2035);
    AvailableSceneModes = AvailableSceneModesGC2035;
    NumAvailSceneModes = ARRAY_SIZE(AvailableSceneModesGC2035);
    AvailableFpsRanges = AvailableFpsRangesGC2035;
    NumAvailableFpsRanges = ARRAY_SIZE(AvailableFpsRangesGC2035);

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

GC2035::GC2035()
{
    init();
}

GC2035::GC2035(uint32_t v4l2ID)
    : NXCameraBoardSensor(v4l2ID)
{
    init();
}

GC2035::~GC2035()
{
}

void GC2035::setAfMode(uint8_t afMode)
{
    AfMode = afMode;
}

void GC2035::afEnable(bool enable)
{
}

void GC2035::setEffectMode(uint8_t effectMode)
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

void GC2035::setSceneMode(uint8_t sceneMode)
{
}

void GC2035::setAntibandingMode(uint8_t antibandingMode)
{
}

void GC2035::setAwbMode(uint8_t awbMode)
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

void GC2035::setExposure(int32_t exposure)
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

uint32_t GC2035::getZoomFactor(void)
{
    // disable zoom
    return 1;
}

status_t GC2035::setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height)
{
    return NO_ERROR;
}

int GC2035::setFormat(int width, int height, int format)
{
    int sensorWidth, sensorHeight;
    if (width == 1568)
        sensorWidth = 1600;
    else if(width == 768)
	sensorWidth = 800;
    else {
        ALOGE("%s: invalid width %d", __func__, width);
        return -EINVAL;
    }
    if (height == 1176)
        sensorHeight = 1200;
    else if(height == 576)
	sensorHeight = 600;
    else {
        ALOGE("%s: invalid height %d", __func__, height);
        return -EINVAL;
    }
    return v4l2_set_format(V4l2ID, sensorWidth, sensorHeight, format);
}
