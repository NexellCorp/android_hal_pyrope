#include <stdlib.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <gui/Surface.h>
#include <gui/ISurface.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <cutils/log.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include <nxp-v4l2.h>
#include <android-nxp-v4l2.h>
#include <gralloc_priv.h>

#define LOG_TAG "hwc_test"

using namespace android;

#if 0
#define WIDTH           1024
#define HEIGHT          768
#else
#define WIDTH           320
#define HEIGHT          768
#endif

#define RED_COLOR       (0x1f << 11)
#define GREEN_COLOR     (0x3f << 5)
#define BLUE_COLOR      (0x1f << 0)
#define BLACK_COLOR     (0x0)
#define NUMBER_OF_PIXEL (WIDTH * HEIGHT)
#define BPP             2
#define NUMBER_OF_BUFFER    32

#define YUV_WIDTH       800
#define YUV_HEIGHT      600
#define YUV_BUFFER_COUNT    6

#define CHECK_COMMAND(command) do { \
        int ret = command; \
        if (ret < 0) { \
            ALOGE("func %s line %d error!!!\n", __func__, __LINE__); \
            return ret; \
        } \
    } while (0)

static int camera_init(void)
{
    if (false == android_nxp_v4l2_init()) {
        ALOGD("failed to android_nxp_v4l2_init");
    }
    int width = YUV_WIDTH;
    int height = YUV_HEIGHT;
    // CHECK_COMMAND(v4l2_set_format(nxp_v4l2_clipper0, width, height, V4L2_PIX_FMT_YUYV));
    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_clipper0, width, height, V4L2_PIX_FMT_YUV420M));
    CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_clipper0, 0, 0, width, height));
    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_sensor0, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    CHECK_COMMAND(v4l2_reqbuf(nxp_v4l2_clipper0, YUV_BUFFER_COUNT));
    return 0;
}

int main(int argc, char *argv[])
{
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    int err = camera_init();
    if (err < 0) {
        ALOGE("failed to camera_init");
        return err;
    }

    // RGB Surface
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();
    sp<SurfaceControl> surfaceControl = client->createSurface(String8("Test Surface"),
            WIDTH, HEIGHT, HAL_PIXEL_FORMAT_RGB_565, ISurfaceComposerClient::eFXSurfaceNormal);

    sp<Surface> surface = surfaceControl->getSurface();
    SurfaceComposerClient::openGlobalTransaction();
    surfaceControl->show();
    surfaceControl->setLayer(100000);
    SurfaceComposerClient::closeGlobalTransaction();

    sp<ANativeWindow> window(surface);
    err = native_window_set_buffer_count(window.get(), NUMBER_OF_BUFFER);
    if (err) {
        ALOGE("failed to native_window_set_buffer_count!!!");
        return -1;
    }
    err = native_window_set_scaling_mode(window.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (err) {
        ALOGE("failed to native_window_set_scaling_mode!!!");
        return -1;
    }

    unsigned short *pBufferAddr;
    ANativeWindowBuffer *ANBuffer;

    // YUV Surface
    // sp<SurfaceControl> yuvSurfaceControl = client->createSurface(String8("YUV Surface"),
    //         YUV_WIDTH, YUV_HEIGHT, HAL_PIXEL_FORMAT_YCbCr_422_I, ISurfaceComposerClient::eFXSurfaceNormal);
    sp<SurfaceControl> yuvSurfaceControl = client->createSurface(String8("YUV Surface"),
            YUV_WIDTH, YUV_HEIGHT, HAL_PIXEL_FORMAT_YV12, ISurfaceComposerClient::eFXSurfaceNormal);
    if (yuvSurfaceControl == NULL) {
        ALOGE("failed to create yuv surface!!!");
        return -1;
    }

    sp<Surface> yuvSurface = yuvSurfaceControl->getSurface();
    SurfaceComposerClient::openGlobalTransaction();
    yuvSurfaceControl->show();
    // LAYER Test
    //yuvSurfaceControl->setLayer(100001);
    yuvSurfaceControl->setLayer(99999);
    SurfaceComposerClient::closeGlobalTransaction();

    sp<ANativeWindow> yuvWindow(yuvSurface);
    err = native_window_set_buffer_count(yuvWindow.get(), YUV_BUFFER_COUNT + 2);
    if (err) {
        ALOGE("failed to yuv native_window_set_buffer_count!!!");
        return -1;
    }
    err = native_window_set_scaling_mode(yuvWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (err) {
        ALOGE("failed to yuv native_window_set_scaling_mode!!!");
        return -1;
    }
    err = native_window_set_usage(yuvWindow.get(), GRALLOC_USAGE_HW_CAMERA_WRITE);
    if (err) {
        ALOGE("failed to yuv native_window_set_usage!!!");
        return -1;
    }
    // err = native_window_set_buffers_geometry(yuvWindow.get(), YUV_WIDTH, YUV_HEIGHT, HAL_PIXEL_FORMAT_YCbCr_422_I);
    err = native_window_set_buffers_geometry(yuvWindow.get(), YUV_WIDTH, YUV_HEIGHT, HAL_PIXEL_FORMAT_YV12);
    if (err) {
        ALOGE("failed to yuv native_window_set_buffers_geometry!!!");
        return -1;
    }

    ANativeWindowBuffer *yuvANBuffer[YUV_BUFFER_COUNT];
    private_handle_t const *yuvHandle[YUV_BUFFER_COUNT];

    // for video layer priority test
#if 0
    CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_VID_PRIORITY, 1));
    CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_VID_COLORKEY, 0x0));
    //CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_RGB_TRANSCOLOR, 0x0));
#endif

    for (int i = 0; i < YUV_BUFFER_COUNT; i++) {
        err = native_window_dequeue_buffer_and_wait(yuvWindow.get(), &yuvANBuffer[i]);
        if (err) {
            ALOGE("failed to yuv dequeue buffer");
            return -1;
        }
        yuvHandle[i] = reinterpret_cast<private_handle_t const *>(yuvANBuffer[i]->handle);
        // CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_clipper0, 1, i, yuvHandle[i], -1, NULL));
        CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_clipper0, 3, i, yuvHandle[i], -1, NULL));
    }

    CHECK_COMMAND(v4l2_streamon(nxp_v4l2_clipper0));

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds(WIDTH, HEIGHT);

    int count = 0;
    int capture_index = 0;
    ANativeWindowBuffer *yuvTempBuffer;

    while (1) {
        //ALOGD("display %d", count);
        err = native_window_dequeue_buffer_and_wait(window.get(), &ANBuffer);
        if (err) {
            ALOGE("failed to dequeue buffer!!!");
            return -1;
        }
        err = mapper.lock(ANBuffer->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, (void **)&pBufferAddr);
        if (err) {
            ALOGE("failed to lock!!!");
            return -1;
        }
#if 1
        unsigned short color = (0 == (count%3)) ? RED_COLOR:
                               (1 == (count%3)) ? GREEN_COLOR:
                                                  BLACK_COLOR;
#else
        unsigned short color = (0 == (count%3)) ? RED_COLOR:
                               (1 == (count%3)) ? GREEN_COLOR:
                                                  BLACK_COLOR;
        color = BLACK_COLOR;
#endif
        for (int j = 0; j < NUMBER_OF_PIXEL; j++)
            pBufferAddr[j] = color;
        mapper.unlock(ANBuffer->handle);
        window->queueBuffer(window.get(), ANBuffer, -1);

        //usleep(100000);
        count++;

        CHECK_COMMAND(v4l2_dqbuf(nxp_v4l2_clipper0, 3, &capture_index, NULL));
        private_handle_t const *handle = yuvHandle[capture_index];
        yuvWindow->queueBuffer(yuvWindow.get(), yuvANBuffer[capture_index], -1);
        err = native_window_dequeue_buffer_and_wait(yuvWindow.get(), &yuvTempBuffer);
        if (err) {
            ALOGE("failed to yuv temp dequeue!!!");
            return -1;
        }
        //yuvANBuffer[capture_index] = yuvTempBuffer;
        //yuvHandle[capture_index] = reinterpret_cast<private_handle_t const *>(yuvTempBuffer->handle);
        // CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_clipper0, 1, capture_index, yuvHandle[capture_index], -1, NULL));
        CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_clipper0, 3, capture_index, yuvHandle[capture_index], -1, NULL));
    }
    CHECK_COMMAND(v4l2_streamoff(nxp_v4l2_clipper0));

    IPCThreadState::self()->joinThreadPool();
    return 0;
}
