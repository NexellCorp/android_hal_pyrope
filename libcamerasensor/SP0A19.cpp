#include <linux/videodev2.h>
#include <nxp-v4l2.h>
#include "SP0A19.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define V4L2_CID_EFFECT         (V4L2_CTRL_CLASS_CAMERA | 0x1001)
#define V4L2_CID_SCENE          (V4L2_CTRL_CLASS_CAMERA | 0x1002)
#define V4L2_CID_FLASH          (V4L2_CTRL_CLASS_CAMERA | 0x1003)

#define MIN_EXPOSURE            0
#define MAX_EXPOSURE            6

enum {
    COLORFX_NONE = 0,
    COLORFX_WANDB,
    COLORFX_NEGATIVE,
    COLORFX_SEPIA,
    COLORFX_BLUISH,
    COLORFX_GREEN
};

enum {
    WB_AUTO = 0,
    WB_TUNGSTEN_LAMP1,
    WB_TUNGSTEN_LAMP2,
    WB_DAYLIGHT,
    WB_CLOUDY
};

namespace android {

const int32_t ResolutionSP0A19[] = {
    608, 479,
};

// TOP/system/media/camera/include/system/camera_metadata_tags.h
static const uint8_t AvailableAfModesSP0A19[] = {
    ANDROID_CONTROL_AF_MODE_OFF,
};

static const uint8_t AvailableAeModesSP0A19[] = {
    ANDROID_CONTROL_AE_MODE_OFF,
};

#if 1
static const uint8_t SceneModeOverridesSP0A19[] = {
    // ANDROID_CONTROL_SCENE_MODE_NIGHT
    ANDROID_CONTROL_AE_MODE_OFF,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_OFF
};
#else
static const uint8_t SceneModeOverridesSP0A19[] = {
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
};
#endif

static const uint8_t AvailableEffectsSP0A19[] = {
    ANDROID_CONTROL_EFFECT_MODE_OFF,
    ANDROID_CONTROL_EFFECT_MODE_NEGATIVE,
    ANDROID_CONTROL_EFFECT_MODE_SEPIA,
    ANDROID_CONTROL_EFFECT_MODE_AQUA
};

#if 1
static const uint8_t AvailableSceneModesSP0A19[] = {
    //ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED,
    ANDROID_CONTROL_SCENE_MODE_NIGHT
};
#else
static const uint8_t AvailableSceneModesSP0A19[] = {
    ANDROID_CONTROL_SCENE_MODE_PORTRAIT,
    ANDROID_CONTROL_SCENE_MODE_LANDSCAPE,
    ANDROID_CONTROL_SCENE_MODE_SPORTS,
};
#endif

static const uint32_t ExposureCompensationRangeSP0A19[] = {
    MIN_EXPOSURE, MAX_EXPOSURE
};

static const uint8_t AvailableAntibandingModesSP0A19[] = {
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF,
};

static const uint8_t AvailableAwbModesSP0A19[] = {
    ANDROID_CONTROL_AWB_MODE_OFF,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_FLUORESCENT,
    ANDROID_CONTROL_AWB_MODE_INCANDESCENT
};

static const int32_t AvailableFpsRangesSP0A19[] = {
    8, 15
};

void SP0A19::init()
{
    Width = ResolutionSP0A19[0];
    Height = ResolutionSP0A19[1];
    NumResolutions = ARRAY_SIZE(ResolutionSP0A19)/2;
    Resolutions = ResolutionSP0A19;
    NumAvailableAfModes = ARRAY_SIZE(AvailableAfModesSP0A19);
    AvailableAfModes = AvailableAfModesSP0A19;
    NumAvailableAeModes = ARRAY_SIZE(AvailableAeModesSP0A19);
    AvailableAeModes = AvailableAeModesSP0A19;
    NumSceneModeOverrides = ARRAY_SIZE(SceneModeOverridesSP0A19);
    SceneModeOverrides = SceneModeOverridesSP0A19;
    ExposureCompensationRange = ExposureCompensationRangeSP0A19;
    AvailableAntibandingModes = AvailableAntibandingModesSP0A19;
    NumAvailAntibandingModes = ARRAY_SIZE(AvailableAntibandingModesSP0A19);
    AvailableAwbModes = AvailableAwbModesSP0A19;
    NumAvailAwbModes = ARRAY_SIZE(AvailableAwbModesSP0A19);
    AvailableEffects = AvailableEffectsSP0A19;
    NumAvailEffects = ARRAY_SIZE(AvailableEffectsSP0A19);
    AvailableSceneModes = AvailableSceneModesSP0A19;
    NumAvailSceneModes = ARRAY_SIZE(AvailableSceneModesSP0A19);
    AvailableFpsRanges = AvailableFpsRangesSP0A19;
    NumAvailableFpsRanges = ARRAY_SIZE(AvailableFpsRangesSP0A19);

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

SP0A19::SP0A19()
{
    init();
}

SP0A19::SP0A19(uint32_t v4l2ID)
    : NXCameraBoardSensor(v4l2ID)
{
    init();
}

SP0A19::~SP0A19()
{
}

void SP0A19::setAfMode(uint8_t afMode)
{
    AfMode = afMode;
}

void SP0A19::afEnable(bool enable)
{
}

void SP0A19::setEffectMode(uint8_t effectMode)
{
    if (effectMode != EffectMode) {
        uint32_t val = 0;

        switch (effectMode) {
        case ANDROID_CONTROL_EFFECT_MODE_OFF:
            val = COLORFX_NONE;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_NEGATIVE:
            val = COLORFX_NEGATIVE;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_SEPIA:
            val = COLORFX_SEPIA;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_AQUA:
            val = COLORFX_BLUISH;
            break;
        default:
            //ALOGE("%s: unsupported effectmode 0x%x", __func__, effectMode);
            return;
        }

        v4l2_set_ctrl(V4l2ID, V4L2_CID_EFFECT, val);
        EffectMode = effectMode;
    }
}

void SP0A19::setSceneMode(uint8_t sceneMode)
{
    if (sceneMode != SceneMode) {
        uint32_t val = 0;

        if (sceneMode == ANDROID_CONTROL_SCENE_MODE_NIGHT) {
            val = 1;
        } else {
            val = 0;
        }

        v4l2_set_ctrl(V4l2ID, V4L2_CID_SCENE, val);
        SceneMode = sceneMode;
    }
}

void SP0A19::setAntibandingMode(uint8_t antibandingMode)
{
}

void SP0A19::setAwbMode(uint8_t awbMode)
{
    if (awbMode != AwbMode) {
        uint32_t val = 0;

        switch (awbMode) {
        case ANDROID_CONTROL_AWB_MODE_OFF:
            val = WB_AUTO;
            break;
        case ANDROID_CONTROL_AWB_MODE_AUTO:
            val = WB_AUTO;
            break;
        case ANDROID_CONTROL_AWB_MODE_DAYLIGHT:
            val = WB_DAYLIGHT;
            break;
        case ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT:
            val = WB_CLOUDY;
            break;
        case ANDROID_CONTROL_AWB_MODE_FLUORESCENT:
            val = WB_TUNGSTEN_LAMP1;
            break;
        case ANDROID_CONTROL_AWB_MODE_INCANDESCENT:
            val = WB_TUNGSTEN_LAMP2;
            break;
        default:
            //ALOGE("%s: unsupported awb mode 0x%x", __func__, awbMode);
            return;
        }
        AwbMode = awbMode;

        v4l2_set_ctrl(V4l2ID, V4L2_CID_DO_WHITE_BALANCE, val);
    }
}

void SP0A19::setExposure(int32_t exposure)
{
    if (exposure < MIN_EXPOSURE || exposure > MAX_EXPOSURE) {
        ALOGE("%s: invalid exposure %d", __func__, exposure);
        return;
    }

    if (exposure != Exposure) {
        Exposure = exposure;
        v4l2_set_ctrl(V4l2ID, V4L2_CID_EXPOSURE, exposure);
    }
}

uint32_t SP0A19::getZoomFactor(void)
{
    // disable zoom
    return 1;
}

status_t SP0A19::setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height)
{
    /* check crop */
    if (width != CropWidth) {
        ALOGD("Zoom Crop %d ===> %d", CropWidth, width);
        CropWidth = width;
        CropHeight = height;
        CropLeft = left;
        CropTop = top;

        // return v4l2_set_crop(V4l2ID, CropLeft, CropTop, CropWidth, CropHeight);
    }

    return NO_ERROR;
}

int SP0A19::setFormat(int width, int height, int format)
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

}; // namespace
