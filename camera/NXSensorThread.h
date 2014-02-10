#ifndef _NX_SENSOR_THREAD_H
#define _NX_SENSOR_THREAD_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <NXMessageThread.h>
#include <NXCameraSensor.h>

namespace android {

struct NXSensorMessage
{
    NXSensorMessage() {
    }
    NXSensorMessage(uint32_t t, uint32_t e1, uint32_t e2)
        : type(t), ext1(e1), ext2(e2)
    {
    }
    uint32_t type; // CAMERA2_MSG_xxx : hardware/camera2.h
    uint32_t ext1;
    uint32_t ext2;
};

class NXSensorThread: public NXMessageThread<NXSensorMessage *>
{
public:
    NXSensorThread(camera2_notify_callback cb, void *cookie, NXCameraSensor *sensor)
        : NotifyCb(cb),
          CallbackCookie(cookie),
          Sensor(sensor),
          mAeInPrecapture(false)
    {
    }
    virtual ~NXSensorThread() {
    }

private:
    virtual bool processMessage(NXSensorMessage* msg);

private:
    camera2_notify_callback NotifyCb;
    void *CallbackCookie;
    NXCameraSensor *Sensor;

    bool mAeInPrecapture;
};

}; // namespace
#endif
