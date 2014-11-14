#ifndef _NX_COMMAND_THREAD_H
#define _NX_COMMAND_THREAD_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <system/camera_metadata.h>
#include <utils/Thread.h>
#include <utils/KeyedVector.h>
#include "Exif.h"
#include "Constants.h"
#include "NXCameraSensor.h"

namespace android {

class NXCommandThread : public Thread
{
public:
    NXCommandThread() {
        memset(ListenerMap, 0, STREAM_ID_MAX);
    }
    NXCommandThread(const camera2_request_queue_src_ops_t *ops, NXCameraSensor *sensor);
    virtual    ~NXCommandThread();

    // listener interface
    struct CommandListener: virtual public RefBase {
        virtual void onCommand(int32_t id, camera_metadata_t *metadata) = 0;
        virtual void onZoomChanged(int left, int top, int width, int height, int baseWidth, int baseHeight) {
            return;
        }
        virtual void onExifChanged(exif_attribute_t *exif) {
            return;
        }
    };

    status_t registerListener(int32_t id, sp<CommandListener> listener);
    status_t removeListener(int32_t id);

    void  wakeup();
    void  waitIdle();

    void  getCrop(uint32_t &cropLeft, uint32_t &cropTop, uint32_t &cropWidth, uint32_t &cropHeight, uint32_t &baseWidth, uint32_t &baseHeight) {
        cropLeft   = CropLeft;
        cropTop    = CropTop;
        cropWidth  = CropWidth;
        cropHeight = CropHeight;
        baseWidth  = Sensor->getSensorW();
        baseHeight = Sensor->getSensorH();
    }

private:
    virtual bool threadLoop();

    status_t checkEntry(const camera_metadata_entry_t &entry, uint8_t type, size_t count = 0);
    status_t handleRequest(camera_metadata_t *request);
    // if exif attribute changed, return true, else return false
    bool handleExif(camera_metadata_t *request);
    bool processCommand();
    void doListenersCallback(camera_metadata_entry_t &streams, camera_metadata_t *request);
    void doListenersCallback();
    void doZoomChanged(int left, int top, int width, int height, int baseWidth, int baseHeight);
    void doExifChanged();

private:
    Mutex  SignalMutex;
    Condition SignalCondition;

    bool IsIdle;
    bool MsgReceived;

    // variables
    const camera2_request_queue_src_ops_t *RequestQueueSrcOps;
    NXCameraSensor *Sensor;

    Mutex ListenerMutex;
    KeyedVector<int32_t, sp<CommandListener> > CommandListeners;
    unsigned char ListenerMap[STREAM_ID_MAX];

private:
    // camera parameters
    uint32_t ControlMode;
    uint32_t SceneMode;

    uint32_t CropLeft;
    uint32_t CropTop;
    uint32_t CropWidth;
    uint32_t CropHeight;

    exif_attribute_t *Exif;
};

}; // namespace android

#endif
