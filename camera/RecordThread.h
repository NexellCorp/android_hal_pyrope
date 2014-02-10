#ifndef _RECORD_THREAD_H
#define _RECORD_THREAD_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <utils/Thread.h>
#include <gralloc_priv.h>
#include <nxp-v4l2.h>
#include <nx_camera_board.h>
#include "NXCommandThread.h"
#include "NXStream.h"
#include "NXStreamThread.h"
#include "NXStreamManager.h"

namespace android {

class RecordThread: public NXStreamThread
{
public:
    RecordThread(nxp_v4l2_id id,
            int width,
            int height,
            sp<NXZoomController> &zoomController,
            sp<NXStreamManager> &streamManager);
    virtual ~RecordThread();

    virtual status_t readyToRun();
    virtual void onCommand(int32_t streamId, camera_metadata_t *metadata);

protected:
    virtual void init(nxp_v4l2_id id);

private:
    virtual bool threadLoop();

// private:
//     uint32_t PreviewWidth;
//     uint32_t PreviewHeight;

private:
    nxp_v4l2_id SensorId;
    NXCameraBoardSensor *Sensor;
};

}; // namespace

#endif
