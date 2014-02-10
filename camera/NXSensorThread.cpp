#ifndef LOG_TAG
#define LOG_TAG "NXSensorThread"
#endif

#include "NXCommandThread.h"
#include "NXSensorThread.h"

namespace android {

bool NXSensorThread::processMessage(NXSensorMessage *msg)
{
    if (msg != NULL) {
        ALOGD("msg type %d, ext1 %d, ext2 %d", msg->type, msg->ext1, msg->ext2);
        switch (msg->type) {
            case CAMERA2_TRIGGER_AUTOFOCUS:
                ALOGD("trigger autofocus id(%d), user(%p)", msg->ext1, CallbackCookie);
                // psw0523 fix for shorten capture time
                // Sensor->afEnable(true);
                NotifyCb(CAMERA2_MSG_AUTOFOCUS, ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED, msg->ext1, 0, CallbackCookie);
                break;
            case CAMERA2_TRIGGER_CANCEL_AUTOFOCUS:
                ALOGD("trigger cancel autofocus id(%d)", msg->ext1);
                // psw0523 fix for shorten capture time
                // Sensor->afEnable(false);
                NotifyCb(CAMERA2_MSG_AUTOFOCUS, ANDROID_CONTROL_AF_STATE_INACTIVE, msg->ext1, 0, CallbackCookie);
                break;
            case CAMERA2_TRIGGER_PRECAPTURE_METERING:
                if (!mAeInPrecapture) {
                    mAeInPrecapture = true;
                    ALOGD("trigger precapture metering: start, cookie(%p)", CallbackCookie);
                    NotifyCb(CAMERA2_MSG_AUTOEXPOSURE, ANDROID_CONTROL_AE_STATE_PRECAPTURE, msg->ext1, 0, CallbackCookie);
                    NotifyCb(CAMERA2_MSG_AUTOWB, ANDROID_CONTROL_AE_STATE_PRECAPTURE, msg->ext1, 0, CallbackCookie);
                    NXSensorMessage *selfmsg = new NXSensorMessage(CAMERA2_TRIGGER_PRECAPTURE_METERING, msg->ext1, 0);
                    // sendMessageDelayed(selfmsg, 100000); // 100ms delay
                    sendMessageDelayed(selfmsg, 5000); // 5ms delay
                } else {
                    mAeInPrecapture = false;
                    ALOGD("trigger precapture metering: end(%p)", CallbackCookie);
                    NotifyCb(CAMERA2_MSG_AUTOEXPOSURE, ANDROID_CONTROL_AE_STATE_CONVERGED, msg->ext1, 0, CallbackCookie);
                    NotifyCb(CAMERA2_MSG_AUTOWB, ANDROID_CONTROL_AWB_STATE_CONVERGED, msg->ext1, 0, CallbackCookie);
                }
                break;
        }
        delete msg;
    }

    return true;
}

};

