#include <linux/videodev2.h>
#include <nxp-v4l2.h>
#include "S5K5CAGX.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define V4L2_CID_CAMERA_SCENE_MODE      (V4L2_CTRL_CLASS_CAMERA | 0x1001)
#define V4L2_CID_CAMERA_ANTI_SHAKE      (V4L2_CTRL_CLASS_CAMERA | 0x1002)
#define V4L2_CID_CAMERA_MODE_CHANGE     (V4L2_CTRL_CLASS_CAMERA | 0x1003)

namespace android {

const int32_t ResolutionS5K5CAGX[] = {
    2048, 1536,
    1600, 1200,
    1280, 1024,
    //1280, 720,
    1024, 768,
    //800, 600,
    //720, 480,
    640, 480,
    352, 288,
    320, 240,
    176, 144,
    160, 120,
};

// TOP/system/media/camera/include/system/camera_metadata_tags.h
static const uint8_t AvailableAfModesS5K5CAGX[] = {
    ANDROID_CONTROL_AF_MODE_OFF,
    ANDROID_CONTROL_AF_MODE_AUTO,
    ANDROID_CONTROL_AF_MODE_MACRO,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
    ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO
};

static const uint8_t AvailableAeModesS5K5CAGX[] = {
    ANDROID_CONTROL_AE_MODE_OFF,
    ANDROID_CONTROL_AE_MODE_ON,
    ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH
};

static const uint8_t SceneModeOverridesS5K5CAGX[] = {
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

static const uint8_t AvailableEffectsS5K5CAGX[] = {
    ANDROID_CONTROL_EFFECT_MODE_MONO,
    ANDROID_CONTROL_EFFECT_MODE_NEGATIVE,
    ANDROID_CONTROL_EFFECT_MODE_SEPIA,
    ANDROID_CONTROL_EFFECT_MODE_AQUA
};

static const uint8_t AvailableSceneModesS5K5CAGX[] = {
    //ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED,
    ANDROID_CONTROL_SCENE_MODE_PORTRAIT,
    ANDROID_CONTROL_SCENE_MODE_LANDSCAPE,
    ANDROID_CONTROL_SCENE_MODE_SPORTS,
    //ANDROID_CONTROL_SCENE_MODE_NIGHT
};

static const uint32_t ExposureCompensationRangeS5K5CAGX[] = {
    -3, 3
};

static const uint8_t AvailableAntibandingModesS5K5CAGX[] = {
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ
};

static const uint8_t AvailableAwbModesS5K5CAGX[] = {
    ANDROID_CONTROL_AWB_MODE_OFF,
    ANDROID_CONTROL_AWB_MODE_AUTO,
    ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    ANDROID_CONTROL_AWB_MODE_FLUORESCENT,
    ANDROID_CONTROL_AWB_MODE_INCANDESCENT
};

static const int32_t AvailableFpsRangesS5K5CAGX[] = {
    15, 30
};

void S5K5CAGX::init()
{
    // TODO
    Width = ResolutionS5K5CAGX[0];
    Height = ResolutionS5K5CAGX[1];
    NumResolutions = ARRAY_SIZE(ResolutionS5K5CAGX)/2;
    Resolutions = ResolutionS5K5CAGX;
    NumAvailableAfModes = ARRAY_SIZE(AvailableAfModesS5K5CAGX);
    AvailableAfModes = AvailableAfModesS5K5CAGX;
    NumAvailableAeModes = ARRAY_SIZE(AvailableAeModesS5K5CAGX);
    AvailableAeModes = AvailableAeModesS5K5CAGX;
    NumSceneModeOverrides = ARRAY_SIZE(SceneModeOverridesS5K5CAGX);
    SceneModeOverrides = SceneModeOverridesS5K5CAGX;
    ExposureCompensationRange = ExposureCompensationRangeS5K5CAGX;
    AvailableAntibandingModes = AvailableAntibandingModesS5K5CAGX;
    NumAvailAntibandingModes = ARRAY_SIZE(AvailableAntibandingModesS5K5CAGX);
    AvailableAwbModes = AvailableAwbModesS5K5CAGX;
    NumAvailAwbModes = ARRAY_SIZE(AvailableAwbModesS5K5CAGX);
    AvailableEffects = AvailableEffectsS5K5CAGX;
    NumAvailEffects = ARRAY_SIZE(AvailableEffectsS5K5CAGX);
    AvailableSceneModes = AvailableSceneModesS5K5CAGX;
    NumAvailSceneModes = ARRAY_SIZE(AvailableSceneModesS5K5CAGX);
    AvailableFpsRanges = AvailableFpsRangesS5K5CAGX;
    NumAvailableFpsRanges = ARRAY_SIZE(AvailableFpsRangesS5K5CAGX);

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

S5K5CAGX::S5K5CAGX()
{
    init();
}

S5K5CAGX::S5K5CAGX(uint32_t v4l2ID)
    : NXCameraBoardSensor(v4l2ID)
{
    init();
}

S5K5CAGX::~S5K5CAGX()
{
}

void S5K5CAGX::setAfMode(uint8_t afMode)
{
    AfMode = afMode;
}

void S5K5CAGX::afEnable(bool enable)
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
    v4l2_set_ctrl(V4l2ID, V4L2_CID_FOCUS_AUTO, val);
}

void S5K5CAGX::setEffectMode(uint8_t effectMode)
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
        case ANDROID_CONTROL_EFFECT_MODE_AQUA:
            val = COLORFX_AQUA;
            break;
        default:
            //ALOGE("%s: unsupported effectmode 0x%x", __func__, effectMode);
            return;
        }

        v4l2_set_ctrl(V4l2ID, V4L2_CID_COLORFX, val);
        EffectMode = effectMode;
    }
}

void S5K5CAGX::setSceneMode(uint8_t sceneMode)
{
    if (sceneMode != SceneMode) {
        uint32_t val = 0;

        switch (sceneMode) {
        case ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED:
            val = SCENE_OFF;
            break;
        case ANDROID_CONTROL_SCENE_MODE_PORTRAIT:
            val = SCENE_PORTRAIT;
            break;
        case ANDROID_CONTROL_SCENE_MODE_LANDSCAPE:
            val = SCENE_LANDSCAPE;
            break;
        case ANDROID_CONTROL_SCENE_MODE_SPORTS:
            val = SCENE_SPORTS;
            break;
        case ANDROID_CONTROL_SCENE_MODE_NIGHT:
            val = SCENE_NIGHTSHOT;
            break;
        default:
            //ALOGE("%s: unsupported scenemode 0x%x", __func__, sceneMode);
            return;
        }
        SceneMode = sceneMode;

        v4l2_set_ctrl(V4l2ID, V4L2_CID_CAMERA_SCENE_MODE, val);
    }
}

void S5K5CAGX::setAntibandingMode(uint8_t antibandingMode)
{
    if (antibandingMode != AntibandingMode) {
        uint32_t val = 0;

        switch (antibandingMode) {
        case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
            val = ANTI_SHAKE_OFF;
            break;
        case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
            val = ANTI_SHAKE_50Hz;
            break;
        case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
            val = ANTI_SHAKE_60Hz;
            break;
        default:
            //ALOGE("%s: unsupported antibanding mode 0x%x", __func__, antibandingMode);
            return;
        }
        AntibandingMode = antibandingMode;

        v4l2_set_ctrl(V4l2ID, V4L2_CID_CAMERA_ANTI_SHAKE, val);
    }
}

void S5K5CAGX::setAwbMode(uint8_t awbMode)
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

        v4l2_set_ctrl(V4l2ID, V4L2_CID_DO_WHITE_BALANCE, val);
    }
}

#define MIN_EXPOSURE    -4
#define MAX_EXPOSURE     4

void S5K5CAGX::setExposure(int32_t exposure)
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

uint32_t S5K5CAGX::getZoomFactor(void)
{
    // disable zoom
    return 1;
}

status_t S5K5CAGX::setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height)
{
    /* check crop */
    if (width != CropWidth) {
        ALOGD("Zoom Crop %d ===> %d", CropWidth, width);
        CropWidth = width;
        CropHeight = height;
        CropLeft = left;
        CropTop = top;

        return v4l2_set_crop(V4l2ID, CropLeft, CropTop, CropWidth, CropHeight);
    }

    return NO_ERROR;
}

}; // namespace
