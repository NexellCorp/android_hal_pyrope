#include <linux/videodev2.h>
#include <nxp-v4l2.h>
#include "SP0838.h"

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

const int32_t ResolutionSP0838[] = {
    // 648, 480,
    608, 479,
};

// TOP/system/media/camera/include/system/camera_metadata_tags.h
static const uint8_t AvailableAfModesSP0838[] = {
    ANDROID_CONTROL_AF_MODE_OFF,
    ANDROID_CONTROL_AF_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_MACRO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO
};

static const uint8_t AvailableAeModesSP0838[] = {
    ANDROID_CONTROL_AE_MODE_OFF,
    ANDROID_CONTROL_AE_MODE_ON,
    ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH
};

static const uint8_t SceneModeOverridesSP0838[] = {
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

static const uint8_t AvailableEffectsSP0838[] = {
    ANDROID_CONTROL_EFFECT_MODE_MONO,
    ANDROID_CONTROL_EFFECT_MODE_NEGATIVE,
    ANDROID_CONTROL_EFFECT_MODE_SEPIA,
    ANDROID_CONTROL_EFFECT_MODE_AQUA
};

static const uint8_t AvailableSceneModesSP0838[] = {
    ANDROID_CONTROL_SCENE_MODE_PORTRAIT,
    ANDROID_CONTROL_SCENE_MODE_LANDSCAPE,
    ANDROID_CONTROL_SCENE_MODE_SPORTS,
#if 0
    ANDROID_CONTROL_SCENE_MODE_NIGHT
#endif
};

static const int32_t AvailableFpsRangesSP0838[] = {
    8, 15
};

static const uint32_t ExposureCompensationRangeSP0838[] = {
    // MIN_EXPOSURE, MAX_EXPOSURE
    -3, 3
};

static const uint8_t AvailableAntibandingModesSP0838[] = {
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ
};

static const uint8_t AvailableAwbModesSP0838[] = {
    ANDROID_CONTROL_AWB_MODE_OFF,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_FLUORESCENT,
    ANDROID_CONTROL_AWB_MODE_INCANDESCENT
};

void SP0838::init()
{
    // TODO
    Width = ResolutionSP0838[0];
    Height = ResolutionSP0838[1];
    NumResolutions = ARRAY_SIZE(ResolutionSP0838)/2;
    Resolutions = ResolutionSP0838;
    NumAvailableAfModes = ARRAY_SIZE(AvailableAfModesSP0838);
    AvailableAfModes = AvailableAfModesSP0838;
    NumAvailableAeModes = ARRAY_SIZE(AvailableAeModesSP0838);
    AvailableAeModes = AvailableAeModesSP0838;
    NumSceneModeOverrides = ARRAY_SIZE(SceneModeOverridesSP0838);
    SceneModeOverrides = SceneModeOverridesSP0838;
    ExposureCompensationRange = ExposureCompensationRangeSP0838;
    AvailableAntibandingModes = AvailableAntibandingModesSP0838;
    NumAvailAntibandingModes = ARRAY_SIZE(AvailableAntibandingModesSP0838);
    AvailableAwbModes = AvailableAwbModesSP0838;
    NumAvailAwbModes = ARRAY_SIZE(AvailableAwbModesSP0838);
    AvailableEffects = AvailableEffectsSP0838;
    NumAvailEffects = ARRAY_SIZE(AvailableEffectsSP0838);
    AvailableSceneModes = AvailableSceneModesSP0838;
    NumAvailSceneModes = ARRAY_SIZE(AvailableSceneModesSP0838);
    AvailableFpsRanges = AvailableFpsRangesSP0838;
    NumAvailableFpsRanges = ARRAY_SIZE(AvailableFpsRangesSP0838);

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

SP0838::SP0838()
{
    init();
}

SP0838::SP0838(uint32_t v4l2ID)
    : NXCameraBoardSensor(v4l2ID)
{
    init();
}

SP0838::~SP0838()
{
}

void SP0838::setAfMode(uint8_t afMode)
{
    AfMode = afMode;
}

void SP0838::afEnable(bool enable)
{
}

void SP0838::setEffectMode(uint8_t effectMode)
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

void SP0838::setSceneMode(uint8_t sceneMode)
{
}

void SP0838::setAntibandingMode(uint8_t antibandingMode)
{
}

void SP0838::setAwbMode(uint8_t awbMode)
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

void SP0838::setExposure(int32_t exposure)
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

uint32_t SP0838::getZoomFactor(void)
{
    // disable zoom
    return 1;
}

status_t SP0838::setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height)
{
    return NO_ERROR;
}

int SP0838::setFormat(int width, int height, int format)
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
