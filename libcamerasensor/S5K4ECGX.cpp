#include <linux/videodev2.h>
#include <nxp-v4l2.h>
#include "S5K4ECGX.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

namespace android {

const int32_t ResolutionS5K4ECGX[] = {
    2560, 1920,
    2048, 1536,
    1920, 1080,
    1280, 960,
    1280, 720,
    1024, 768,
    640, 480,
};

// TOP/system/media/camera/include/system/camera_metadata_tags.h
static const uint8_t AvailableAfModesS5K4ECGX[] = {
    ANDROID_CONTROL_AF_MODE_OFF,
    ANDROID_CONTROL_AF_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_MACRO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO
};

static const uint8_t AvailableAeModesS5K4ECGX[] = {
    ANDROID_CONTROL_AE_MODE_OFF,
    ANDROID_CONTROL_AE_MODE_ON,
    ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH
};

static const uint8_t SceneModeOverridesS5K4ECGX[] = {
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
    ANDROID_CONTROL_AWB_MODE_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE
#endif
};

static const uint8_t AvailableEffectsS5K4ECGX[] = {
    ANDROID_CONTROL_EFFECT_MODE_MONO,
    ANDROID_CONTROL_EFFECT_MODE_NEGATIVE,
    ANDROID_CONTROL_EFFECT_MODE_SEPIA,
    ANDROID_CONTROL_EFFECT_MODE_AQUA
};

static const uint8_t AvailableSceneModesS5K4ECGX[] = {
    //ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED,
    ANDROID_CONTROL_SCENE_MODE_PORTRAIT,
    ANDROID_CONTROL_SCENE_MODE_LANDSCAPE,
    ANDROID_CONTROL_SCENE_MODE_SPORTS,
    //ANDROID_CONTROL_SCENE_MODE_NIGHT
};

static const uint32_t ExposureCompensationRangeS5K4ECGX[] = {
    -3, 3
};

static const uint8_t AvailableAntibandingModesS5K4ECGX[] = {
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ
};

static const uint8_t AvailableAwbModesS5K4ECGX[] = {
    ANDROID_CONTROL_AWB_MODE_OFF,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_FLUORESCENT,
    ANDROID_CONTROL_AWB_MODE_INCANDESCENT
};

static const int32_t AvailableFpsRangesS5K4ECGX[] = {
    15, 30
};

void S5K4ECGX::init()
{
    Width = ResolutionS5K4ECGX[0];
    Height = ResolutionS5K4ECGX[1];
    NumResolutions = ARRAY_SIZE(ResolutionS5K4ECGX)/2;
    Resolutions = ResolutionS5K4ECGX;
    NumAvailableAfModes = ARRAY_SIZE(AvailableAfModesS5K4ECGX);
    AvailableAfModes = AvailableAfModesS5K4ECGX;
    NumAvailableAeModes = ARRAY_SIZE(AvailableAeModesS5K4ECGX);
    AvailableAeModes = AvailableAeModesS5K4ECGX;
    NumSceneModeOverrides = ARRAY_SIZE(SceneModeOverridesS5K4ECGX);
    SceneModeOverrides = SceneModeOverridesS5K4ECGX;
    ExposureCompensationRange = ExposureCompensationRangeS5K4ECGX;
    AvailableAntibandingModes = AvailableAntibandingModesS5K4ECGX;
    NumAvailAntibandingModes = ARRAY_SIZE(AvailableAntibandingModesS5K4ECGX);
    AvailableAwbModes = AvailableAwbModesS5K4ECGX;
    NumAvailAwbModes = ARRAY_SIZE(AvailableAwbModesS5K4ECGX);
    AvailableEffects = AvailableEffectsS5K4ECGX;
    NumAvailEffects = ARRAY_SIZE(AvailableEffectsS5K4ECGX);
    AvailableSceneModes = AvailableSceneModesS5K4ECGX;
    NumAvailSceneModes = ARRAY_SIZE(AvailableSceneModesS5K4ECGX);
    AvailableFpsRanges = AvailableFpsRangesS5K4ECGX;
    NumAvailableFpsRanges = ARRAY_SIZE(AvailableFpsRangesS5K4ECGX);

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

S5K4ECGX::S5K4ECGX()
{
    init();
}

S5K4ECGX::S5K4ECGX(uint32_t v4l2ID)
    : NXCameraBoardSensor(v4l2ID)
{
    init();
}

S5K4ECGX::~S5K4ECGX()
{
}

void S5K4ECGX::setAfMode(uint8_t afMode)
{
    AfMode = afMode;
}

void S5K4ECGX::afEnable(bool enable)
{
    uint32_t val = 0;

    if (enable) {
        switch (AfMode) {
            case ANDROID_CONTROL_AF_MODE_OFF:
                val = 0; // init
                break;
            case ANDROID_CONTROL_AF_MODE_AUTO:
                val = 1; // auto
                break;
            case ANDROID_CONTROL_AF_MODE_MACRO:
                val = 1; // zoom macro
                break;
            case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
                val = 1; // zoom fixed
                break;
            case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
                val = 1; // zoom infinity
                break;
            case ANDROID_CONTROL_AF_MODE_EDOF:
                val = 1; // init
                break;
        }
    } else {
        val = 0; // init
    }
    // v4l2_set_ctrl(V4l2ID, V4L2_CID_FOCUS_AUTO, val);
}

void S5K4ECGX::setEffectMode(uint8_t effectMode)
{
    // if (effectMode != EffectMode) {
    //     uint32_t val = 0;

    //     switch (effectMode) {
    //     case ANDROID_CONTROL_EFFECT_MODE_OFF:
    //         val = COLORFX_NONE;
    //         break;
    //     case ANDROID_CONTROL_EFFECT_MODE_MONO:
    //         val = COLORFX_MONO;
    //         break;
    //     case ANDROID_CONTROL_EFFECT_MODE_NEGATIVE:
    //         val = COLORFX_NEGATIVE;
    //         break;
    //     case ANDROID_CONTROL_EFFECT_MODE_SEPIA:
    //         val = COLORFX_SEPIA;
    //         break;
    //     case ANDROID_CONTROL_EFFECT_MODE_AQUA:
    //         val = COLORFX_AQUA;
    //         break;
    //     default:
    //         //ALOGE("%s: unsupported effectmode 0x%x", __func__, effectMode);
    //         return;
    //     }

    //     // v4l2_set_ctrl(V4l2ID, V4L2_CID_COLORFX, val);
    //     EffectMode = effectMode;
    // }
}

void S5K4ECGX::setSceneMode(uint8_t sceneMode)
{
    // if (sceneMode != SceneMode) {
    //     uint32_t val = 0;

    //     switch (sceneMode) {
    //     case ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED:
    //         val = SCENE_OFF;
    //         break;
    //     case ANDROID_CONTROL_SCENE_MODE_PORTRAIT:
    //         val = SCENE_PORTRAIT;
    //         break;
    //     case ANDROID_CONTROL_SCENE_MODE_LANDSCAPE:
    //         val = SCENE_LANDSCAPE;
    //         break;
    //     case ANDROID_CONTROL_SCENE_MODE_SPORTS:
    //         val = SCENE_SPORTS;
    //         break;
    //     case ANDROID_CONTROL_SCENE_MODE_NIGHT:
    //         val = SCENE_NIGHTSHOT;
    //         break;
    //     default:
    //         //ALOGE("%s: unsupported scenemode 0x%x", __func__, sceneMode);
    //         return;
    //     }
    //     SceneMode = sceneMode;

    //     // v4l2_set_ctrl(V4l2ID, V4L2_CID_CAMERA_SCENE_MODE, val);
    // }
}

void S5K4ECGX::setAntibandingMode(uint8_t antibandingMode)
{
    // if (antibandingMode != AntibandingMode) {
    //     uint32_t val = 0;

    //     switch (antibandingMode) {
    //     case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
    //         val = ANTI_SHAKE_OFF;
    //         break;
    //     case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
    //         val = ANTI_SHAKE_50Hz;
    //         break;
    //     case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
    //         val = ANTI_SHAKE_60Hz;
    //         break;
    //     default:
    //         //ALOGE("%s: unsupported antibanding mode 0x%x", __func__, antibandingMode);
    //         return;
    //     }
    //     AntibandingMode = antibandingMode;

    //     // v4l2_set_ctrl(V4l2ID, V4L2_CID_CAMERA_ANTI_SHAKE, val);
    // }
}

void S5K4ECGX::setAwbMode(uint8_t awbMode)
{
    // if (awbMode != AwbMode) {
    //     uint32_t val = 0;

    //     switch (awbMode) {
    //     case ANDROID_CONTROL_AWB_MODE_OFF:
    //         val = WB_AUTO;
    //         break;
    //     case ANDROID_CONTROL_AWB_MODE_MODE_AUTO:
    //         val = WB_AUTO;
    //         break;
    //     case ANDROID_CONTROL_AWB_MODE_DAYLIGHT:
    //         val = WB_DAYLIGHT;
    //         break;
    //     case ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT:
    //         val = WB_DAYLIGHT;
    //         break;
    //     case ANDROID_CONTROL_AWB_MODE_FLUORESCENT:
    //         val = WB_FLUORESCENT;
    //         break;
    //     case ANDROID_CONTROL_AWB_MODE_INCANDESCENT:
    //         val = WB_INCANDESCENT;
    //         break;
    //     default:
    //         //ALOGE("%s: unsupported awb mode 0x%x", __func__, awbMode);
    //         return;
    //     }
    //     AwbMode = awbMode;

    //     // v4l2_set_ctrl(V4l2ID, V4L2_CID_DO_WHITE_BALANCE, val);
    // }
}

#define MIN_EXPOSURE    -4
#define MAX_EXPOSURE     4

void S5K4ECGX::setExposure(int32_t exposure)
{
    if (exposure < MIN_EXPOSURE || exposure > MAX_EXPOSURE) {
        ALOGE("%s: invalid exposure %d", __func__, exposure);
        return;
    }

    if (exposure != Exposure) {
        Exposure = exposure;
        // v4l2_set_ctrl(V4l2ID, V4L2_CID_EXPOSURE, exposure);
    }
}

uint32_t S5K4ECGX::getZoomFactor(void)
{
    // disable zoom
    return 1;
}

status_t S5K4ECGX::setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height)
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

}; // namespace
