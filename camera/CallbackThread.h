#ifndef _CALLBACK_THREAD_H
#define _CALLBACK_THREAD_H

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

class CallbackThread: public NXStreamThread
{
public:
    CallbackThread(int width, int height, sp<NXStreamManager> &streamManager);
    virtual ~CallbackThread();

    virtual status_t readyToRun();
    virtual void onCommand(int32_t streamId, camera_metadata_t *metadata);

protected:
    virtual void init(nxp_v4l2_id id) {
        // do nothing
    }

private:
    virtual bool threadLoop();
};

}; // namespace

#endif
