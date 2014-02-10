#ifndef _NX_CAMERA_BOARD_H
#define _NX_CAMERA_BOARD_H

#include <nxp-v4l2.h>
#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>

namespace android {

class NXCameraBoardSensor {
public:
    NXCameraBoardSensor() {
    }
    NXCameraBoardSensor(uint32_t v4l2ID)
        : V4l2ID(v4l2ID) {

    }
    virtual ~NXCameraBoardSensor() {
    }

public:
    virtual void setAfMode(uint8_t afMode) = 0;
    virtual void afEnable(bool enable) = 0;
    virtual void setEffectMode(uint8_t effectMode) = 0;
    virtual void setSceneMode(uint8_t sceneMode) = 0;
    virtual void setAntibandingMode(uint8_t antibandingMode) = 0;
    virtual void setAwbMode(uint8_t awbMode) = 0;
    virtual void setExposure(int32_t exposure) = 0;
    virtual uint32_t getZoomFactor(void) = 0;
    virtual status_t setZoomCrop(uint32_t left, uint32_t top, uint32_t width, uint32_t height) = 0;
    virtual int setFormat(int width, int height, int format) {
        return v4l2_set_format(V4l2ID, width, height, format);
    }

protected:
    uint32_t V4l2ID;

public:
    int32_t Width;
    int32_t Height;
    int32_t NumResolutions;
    const int32_t *Resolutions;
    int32_t NumAvailableAfModes;
    const uint8_t *AvailableAfModes;
    int32_t NumAvailableAeModes;
    const uint8_t *AvailableAeModes;
    int32_t NumSceneModeOverrides;
    const uint8_t *SceneModeOverrides;
    float FocalLength;
    float Aperture;
    float MinFocusDistance;
    float FNumber;
    int32_t MaxFaceCount;

    const uint8_t *AvailableEffects;
    int32_t NumAvailEffects;
    const uint8_t *AvailableSceneModes;
    int32_t NumAvailSceneModes;
    const uint32_t *ExposureCompensationRange;
    const uint8_t *AvailableAntibandingModes;
    int32_t NumAvailAntibandingModes;
    const uint8_t *AvailableAwbModes;
    int32_t NumAvailAwbModes;
    const int32_t *AvailableFpsRanges;
    int32_t NumAvailableFpsRanges;
};

extern "C" {
int get_board_number_of_cameras();
}

NXCameraBoardSensor *get_board_camera_sensor(int id);
NXCameraBoardSensor *get_board_camera_sensor_by_v4l2_id(int v4l2_id);
uint32_t get_board_preview_v4l2_id(int cameraId);
uint32_t get_board_capture_v4l2_id(int cameraId);
uint32_t get_board_record_v4l2_id(int cameraId);
bool     get_board_camera_is_mipi(uint32_t v4l2_sensorId);
uint32_t get_board_preview_skip_frame(int v4l2_sensorId, int width = 0, int height = 0);
uint32_t get_board_capture_skip_frame(int v4l2_sensorId, int width = 0, int height = 0);
void set_board_preview_mode(int v4l2_sensorId, int width, int height);
void set_board_capture_mode(int v4l2_sensorId, int width, int height);
uint32_t get_board_camera_orientation(int cameraId);

} // namespace android

#endif
