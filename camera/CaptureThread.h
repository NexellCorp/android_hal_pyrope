#ifndef _CAPTURE_THREAD_H
#define _CAPTURE_THREAD_H

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
#include "Exif.h"
#include "NXExifProcessor.h"
#include "NXStreamManager.h"

#define MAX_CAPTURE_BUFFER      4

namespace android {

class CaptureThread: public NXStreamThread
{
public:
    CaptureThread(nxp_v4l2_id id,
            const camera2_frame_queue_dst_ops_t *ops,
            int width,
            int height,
            sp<NXZoomController> &zoomController,
            sp<NXStreamManager> &streamMangaer);
    virtual ~CaptureThread();

    virtual status_t readyToRun();
    virtual void onCommand(int32_t streamId, camera_metadata_t *metadata);
    virtual void onExifChanged(exif_attribute_t *exif);

protected:
    virtual void init(nxp_v4l2_id id);

private:
    virtual bool threadLoop();
    bool capture(unsigned int srcYPhys, unsigned int srcCBPhys, unsigned int srcCRPhys,
                 unsigned int srcYVirt, unsigned int srcCBVirt, unsigned int srcCRVirt,
                 void *dstBase, int dstSize, int width, int height, uint32_t dstOffset = 0);
    bool capture(private_handle_t const *srcHandle, private_handle_t const *dstHandle, int width, int height, uint32_t dstOffset = 0);
    bool capture(struct nxp_vid_buffer *srcBuf, private_handle_t const *dstHandle, int width, int height, uint32_t dstOffset = 0);
    bool makeFrame(camera_metadata_t *srcMetadata);
    bool allocCaptureBuffer(int width, int height);
    void freeCaptureBuffer();
    void dumpCaptureBuffer();

private:
    const camera2_frame_queue_dst_ops_t *FrameQueueDstOps;

    struct nxp_vid_buffer CaptureBuffer[MAX_CAPTURE_BUFFER];
    bool CaptureBufferAllocated;

    nsecs_t CaptureTimeStamp;

    exif_attribute_t *Exif;
    exif_attribute_t *CurrentExif;
    NXExifProcessor *ExifProcessor;
};

}; // namespace

#endif
