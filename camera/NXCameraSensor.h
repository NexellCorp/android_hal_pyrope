#ifndef _NXCAMERA_SENSOR_H
#define _NXCAMERA_SENSOR_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <nx_camera_board.h>

namespace android {

class NXCameraSensor {
public:
    NXCameraSensor(int cameraId);
    ~NXCameraSensor();

    int32_t getSensorW();
    int32_t getSensorH();

    bool isSupportedResolution(int width, int height);

    status_t constructStaticInfo(camera_metadata_t **info,
            int cameraId, bool sizeRequest);

    status_t constructDefaultRequest(int requestTemplate,
            camera_metadata_t **request, bool sizeRequest);

    int CameraId;
    NXCameraBoardSensor *RawSensor;

public:
    // sensor operations
    void setAfMode(uint8_t afMode) {
        RawSensor->setAfMode(afMode);
    }

    void afEnable(bool enable) {
        RawSensor->afEnable(enable);
    }

    void setEffectMode(uint8_t effectMode) {
        RawSensor->setEffectMode(effectMode);
    }

    void setSceneMode(uint8_t sceneMode) {
        RawSensor->setSceneMode(sceneMode);
    }

    void setAntibandingMode(uint8_t antibandingMode) {
        RawSensor->setAntibandingMode(antibandingMode);
    }

    void setAwbMode(uint8_t awbMode) {
        RawSensor->setAwbMode(awbMode);
    }

    void setExposure(int32_t exposure) {
        RawSensor->setExposure(exposure);
    }

    uint32_t getZoomFactor(void) {
        return RawSensor->getZoomFactor();
    }

    status_t setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height) {
        return RawSensor->setZoomCrop(left, top, width, height);
    }

public:
    // static sensor characteristics
    static const unsigned int kResolution[2][2];

    static const nsecs_t kExposureTimeRange[2];
    static const nsecs_t kFrameDurationRange[2];
    static const nsecs_t kMinVerticalBlank;

    static const uint8_t kColorFilterArrangement;

    // output image data characteristics
    static const uint32_t kMaxRawValue;
    static const uint32_t kBlackLevel;

    // sensor sensitivity, approximate
    static const float kSaturationVoltage;
    static const uint32_t kSaturationElectrons;
    static const float kVolsPerLuxSecond;
    static const float kElectronsPerLuxSecond;

    static const float kBaseGainFactor;

    static const float kReadNoiseStddevBeforeGain;
    static const float kReadNoiseStddevAfterGain;
    static const float kReadNoiseVarBeforeGain;
    static const float kReadNoiseVarAfterGain;

    static const nsecs_t kRowReadoutTime;

    static const int32_t kSensitivityRange[2];
    static const uint32_t kDefaultSensitivity;
};

}
#endif
