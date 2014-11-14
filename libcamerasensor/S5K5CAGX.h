#ifndef _S5K5CAGX_H
#define _S5K5CAGX_H

#include <nx_camera_board.h>

#define V4L2_CID_CAMERA_SCENE_MODE		(V4L2_CTRL_CLASS_CAMERA | 0x1001)
#define V4L2_CID_CAMERA_ANTI_SHAKE		(V4L2_CTRL_CLASS_CAMERA | 0x1002)
#define V4L2_CID_CAMERA_MODE_CHANGE		(V4L2_CTRL_CLASS_CAMERA | 0x1003)

enum {
    WB_AUTO = 0,
    WB_DAYLIGHT,
    WB_CLOUDY,
    WB_FLUORESCENT,
    WB_INCANDESCENT,
    WB_MAX
};

enum {
    COLORFX_NONE = 0,
    COLORFX_SEPIA,
    COLORFX_AQUA,
    COLORFX_MONO,
    COLORFX_NEGATIVE,
    COLORFX_SKETCH,
    COLORFX_MAX
};

#define MIN_EXPOSURE    -4
#define MAX_EXPOSURE     4

enum {
    SCENE_OFF = 0,
    SCENE_PORTRAIT,
    SCENE_LANDSCAPE,
    SCENE_SPORTS,
    SCENE_NIGHTSHOT,
    SCENE_MAX
};

enum {
    ANTI_SHAKE_OFF = 0,
    ANTI_SHAKE_50Hz,
    ANTI_SHAKE_60Hz,
    ANTI_SHAKE_MAX
};

namespace android {

class S5K5CAGX : public NXCameraBoardSensor
{
public:
    S5K5CAGX();
    S5K5CAGX(uint32_t v4l2ID);
    virtual ~S5K5CAGX();

    virtual void setAfMode(uint8_t afMode);
    virtual void afEnable(bool enable);
    virtual void setEffectMode(uint8_t effectMode);
    virtual void setSceneMode(uint8_t sceneMode);
    virtual void setAntibandingMode(uint8_t antibandingMode);
    virtual void setAwbMode(uint8_t awbMode);
    virtual void setExposure(int32_t exposure);
    virtual uint32_t getZoomFactor(void);
    virtual status_t setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height);

private:
    void init();

private:
    uint8_t AfMode;
    uint8_t EffectMode;
    uint8_t SceneMode;
    uint8_t AntibandingMode;
    uint8_t AwbMode;
    int32_t Exposure;
    uint32_t Left;
    uint32_t Top;
    uint32_t CropWidth;
    uint32_t CropHeight;
    uint32_t CropLeft;
    uint32_t CropTop;
};

}; // namespace
#endif
