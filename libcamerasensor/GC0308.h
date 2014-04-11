#ifndef _GC0308_H
#define _GC0308_H

#include <nx_camera_board.h>

#define V4L2_CID_CAMERA_SCENE_EXPOSURE      (V4L2_CTRL_CLASS_CAMERA | 0x1001)
#define V4L2_CID_PRIVATE_PREV_CAPT          (V4L2_CTRL_CLASS_CAMERA | 0x1002)

namespace android {

class GC0308 : public NXCameraBoardSensor
{
public:
    GC0308();
    GC0308(uint32_t v4l2ID);
    virtual ~GC0308();

    virtual void setAfMode(uint8_t afMode);
    virtual void afEnable(bool enable);
    virtual void setEffectMode(uint8_t effectMode);
    virtual void setSceneMode(uint8_t sceneMode);
    virtual void setAntibandingMode(uint8_t antibandingMode);
    virtual void setAwbMode(uint8_t awbMode);
    virtual void setExposure(int32_t exposure);
    virtual uint32_t getZoomFactor(void);
    virtual status_t setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height);
    virtual int setFormat(int width, int height, int format);

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
