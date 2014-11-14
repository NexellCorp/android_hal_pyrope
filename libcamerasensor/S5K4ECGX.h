#ifndef _S5K4ECGX_H
#define _S5K4ECGX_H

#include <nx_camera_board.h>

namespace android {

class S5K4ECGX : public NXCameraBoardSensor
{
public:
    S5K4ECGX();
    S5K4ECGX(uint32_t v4l2ID);
    virtual ~S5K4ECGX();

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
