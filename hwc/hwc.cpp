#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>
#include <hardware_legacy/uevent.h>
#include <utils/String8.h>

#include <EGL/egl.h>

#include <sync/sync.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <linux/videodev2_nxp_media.h>

#include <ion/ion.h>
#include <android-nxp-v4l2.h>
#include <nxp-v4l2.h>

#include <gralloc_priv.h>

#include "hdmi.h"

#define VSYNC_CTL_FILE  "/sys/devices/platform/display/active.0"
#define VSYNC_MON_FILE  "/sys/devices/platform/display/vsync.0"

#define VSYNC_ON        "1"
#define VSYNC_OFF       "0"

// #define TRACE
#ifdef TRACE
#define trace_in()      ALOGD("%s entered", __func__)
#define trace_exit()    ALOGD("%s exit", __func__)
#else
#define trace_in()
#define trace_exit()
#endif

/* video layer on/off flag */
#define USE_ONLY_GL  // lcd overlay off, if use video layer, comment this define
/* lcd video layer use flag */
#define LCD_USE_ONLY_GL

/* use v4l2 sync flag */
#define USE_V4L2_SYNC

/* use hdmi thread */
// #define USE_HDMI_THREAD

#define CHECK_COMMAND(command) do { \
    int ret = command; \
    if (ret < 0) { \
        ALOGE("%s: line %d error!!!\n", __func__, __LINE__); \
        return ret; \
    } \
} while (0)


using namespace android;
/*****************************************************************************/
/* structures */

struct nxp_hwc_composer_device_1_t {
    hwc_composer_device_1_t base;

    int                     fb_fd;
    int                     vsync_fd;
    int                     vsync_ctl_fd;
    pthread_t               vsync_thread;

    int32_t                 xres;
    int32_t                 yres;
    int32_t                 xdpi;
    int32_t                 ydpi;
    int32_t                 vsync_period;

    const hwc_procs_t      *procs;

    int                     sync_timeline_fd;
    int                     hdmi_sync_timeline_fd;

    // lcd context
    int                     overlay_layer_index; // -1 off
    bool                    overlay_state_changed;
    int                     out_index;
    int                     out_max_buffer_count;
    int                     out_q_count;

    hwc_rect                src_crop;
    int                     start_x;
    int                     start_y;
    int                     width;
    int                     height;
    int                     format;
    bool                    is_top;

    // lcd sync
    int                     lcd_fence_fd;
    int                     lcd_fence_count;

    // hdmi context
    bool                    hdmi_hpd;
    bool                    hdmi_enabled;
    bool                    hdmi_blanked;
    int                     hdmi_screen_width;
    int                     hdmi_screen_height;
    // hdmi video attribute
    bool                    hdmi_video_enabled;
    int                     hdmi_video_index;
    int                     hdmi_video_format;
    int                     hdmi_video_width;
    int                     hdmi_video_height;
    int                     hdmi_video_start_x;
    int                     hdmi_video_start_y;
    hwc_rect                hdmi_video_src_crop;
    int                     hdmi_video_out_index;
    int                     hdmi_video_out_q_count;
    int                     hdmi_video_out_max_buffer_count;
    // hdmi rgb attribute
    bool                    hdmi_rgb_enabled;
    int                     hdmi_rgb_width;
    int                     hdmi_rgb_height;
    int                     hdmi_rgb_start_x;
    int                     hdmi_rgb_start_y;
    int                     hdmi_rgb_out_index;
    int                     hdmi_rgb_out_q_count;
    int                     hdmi_rgb_out_max_buffer_count;
    hwc_rect                hdmi_rgb_src_crop;
    private_handle_t       *hdmi_rgb_only_handle[3]; // hdmi uses lcd rgb buffer
    int                     hdmi_rgb_only_index;
    private_handle_t const *hdmi_rgb_handle; // hdmi's rgb surface

    private_handle_t const *rgb_handle;

    // hdmi sync
    int                     hdmi_fence_fd;
    int                     hdmi_fence_count;

    // gralloc module
    private_module_t       *gralloc;
    int                     fb_size;

    // hdmi thread
#ifdef USE_HDMI_THREAD
    sp<NXHdmiThread>        HdmiThread;
#endif
};

/*****************************************************************************/
/* debugging functions */
#define RED_COLOR           (0x1f << 11)
#define BLUE_COLOR          (0x1f)
#define RED_BLUE_COLOR      ((0x1f << 11) | (0x1f))
#define GREEN_BLUE_COLOR    ((0x3f << 5) | (0x1f))
static void draw_to_hdmi_rgb_buffer(private_handle_t const *handle)
{
    uint16_t *vptr = (uint16_t *)mmap(NULL, handle->size, PROT_READ | PROT_WRITE, MAP_SHARED, handle->fd, 0);
    if (vptr == MAP_FAILED) {
        ALOGE("%s: can't mmap!!!", __func__);
        return;
    }

    int i, j;
    uint16_t *ptr = vptr;
    uint16_t color;
    for (i = 0; i < 480; i++) {
        ptr = vptr + i * 720;
        if (i < 120) {
            color = RED_COLOR;
        } else if (i < 240) {
            color = BLUE_COLOR;
        } else if (i < 360) {
            color = RED_BLUE_COLOR;
        } else {
            color = GREEN_BLUE_COLOR;
        }
        for (j = 0; j < 360; j++) {
            *ptr = color;
            ptr++;
        }
    }
}

#ifndef container_of
#define container_of(ptr, type, member) \
    (type *)((char *)(ptr) - offsetof(type, member))
#endif
static void dump_layer(hwc_layer_1_t &layer)
{
    char *type_str;
    if (layer.compositionType == HWC_BACKGROUND)
        type_str = (char *)"HWC_BACKGROUND";
    else if (layer.compositionType == HWC_FRAMEBUFFER_TARGET)
        type_str = (char *)"HWC_FRAMEBUFFER_TARGET";
    else if (layer.compositionType == HWC_FRAMEBUFFER)
        type_str = (char *)"HWC_FRAMEBUFFER";
    else if (layer.compositionType == HWC_OVERLAY)
        type_str = (char *)"HWC_FRAMEBUFFER";
    else
        type_str = (char *)"Unknown";

    ALOGD("Dump Layer =====>");
    ALOGD("\ttype : %s", type_str);
    ALOGD("\thints: 0x%x\tflags: 0x%x", layer.hints, layer.flags);
    if (layer.compositionType == HWC_BACKGROUND)
        ALOGD("\tbackgroundColor: rgba(0x%x,0x%x,0x%x,0x%x)",
                layer.backgroundColor.r, layer.backgroundColor.g, layer.backgroundColor.b, layer.backgroundColor.a);
    else {
        private_handle_t const *handle = reinterpret_cast<private_handle_t const *>(layer.handle);
        ALOGD("\thandle: %p", handle);
        if (handle) {
            ALOGD("\t\tflags(0x%x), size(%d), offset(%d), base(0x%x), phys(0x%x), format(0x%x)",
                    handle->flags, handle->size, handle->offset, handle->base, handle->phys, handle->format);
        }
        ALOGD("\ttransform: 0x%x\tblending: 0x%x", layer.transform, layer.blending);
        ALOGD("\tsourceCrop(%d:%d:%d:%d)", layer.sourceCrop.left, layer.sourceCrop.right, layer.sourceCrop.top, layer.sourceCrop.bottom);
        ALOGD("\tdisplayFrame(%d:%d:%d:%d)", layer.displayFrame.left, layer.displayFrame.right, layer.displayFrame.top, layer.displayFrame.bottom);
        ALOGD("\tacquireFenceFd: %d", layer.acquireFenceFd);
        ALOGD("\treleaseFenceFd: %d", layer.releaseFenceFd);
        ANativeWindowBuffer *anb = container_of(layer.handle, ANativeWindowBuffer, handle);
        ALOGD("\tw(%d), h(%d), stride(%d), format(0x%x)", anb->width, anb->height, anb->stride, anb->format);
    }
}

/*****************************************************************************/
/* hdmi functions */
static int hdmi_enable(struct nxp_hwc_composer_device_1_t *dev)
{
    if (dev->hdmi_enabled)
        return 0;

    ALOGD("%s", __func__);

#ifdef USE_HDMI_THREAD
    dev->HdmiThread = new NXHdmiThread(dev->hdmi_rgb_out_max_buffer_count, dev->hdmi_video_out_max_buffer_count);
    if (dev->HdmiThread == NULL) {
        ALOGE("Can't create HDMI Thread!!!");
        return -1;
    }
    dev->HdmiThread->run("NXHdmiThread", HAL_PRIORITY_URGENT_DISPLAY, 0);
#endif

    CHECK_COMMAND(v4l2_link(nxp_v4l2_mlc1, nxp_v4l2_hdmi));
    dev->hdmi_enabled = true;
    return 0;
}

static int hdmi_disable(struct nxp_hwc_composer_device_1_t *dev)
{
    if (!dev->hdmi_enabled)
        return 0;

    ALOGD("%s", __func__);

#ifdef USE_HDMI_THREAD
    if (dev->HdmiThread != NULL) {
        NXHdmiMessage msg(NXHdmiMessage::HDMI_EXIT, 0, 0);
        dev->HdmiThread->sendMessage(msg);
        dev->HdmiThread->requestExitAndWait();
#if 0
        NXHdmiThread *hdmiThread = dev->HdmiThread.get();
        if (hdmiThread) {
            delete hdmiThread;
        }
#endif
        dev->HdmiThread = NULL;
    }
#endif

    if (dev->hdmi_video_enabled) {
        v4l2_streamoff(nxp_v4l2_mlc1_video);
        dev->hdmi_video_enabled = false;
    }

    if (dev->hdmi_rgb_enabled) {
        v4l2_streamoff(nxp_v4l2_mlc1_rgb);
        dev->hdmi_rgb_enabled = false;
    }

    v4l2_unlink(nxp_v4l2_mlc1, nxp_v4l2_hdmi);

    dev->hdmi_enabled = false;
    return 0;
}

#define NXPFB_GET_FB_FD _IOWR('N', 101, __u32)
static int get_hdmi_rgb_only_handle(struct nxp_hwc_composer_device_1_t *pdev)
{
    if (pdev->hdmi_rgb_only_handle[pdev->hdmi_rgb_only_index]) return 0;

    int fb_fd = pdev->hdmi_rgb_only_index;
    /* TODO : fix for rgb handle fd is invalid */
    // int ret = ioctl(pdev->rgb_handle->fd, NXPFB_GET_FB_FD, &fb_fd);
    int ret = ioctl(pdev->gralloc->framebuffer->fd, NXPFB_GET_FB_FD, &fb_fd);
    if (ret < 0) {
        ALOGE("%s: failed to NXPFB_GET_FB_FD!!!", __func__);
        return -1;
    }
    ALOGD("%s: fb_fd(%d)", __func__, fb_fd);
    // this code for nxp3200 gralloc
#if 0
    private_handle_t *hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USER_MEM, pdev->fb_size);
#else
    private_handle_t *hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_ION, pdev->fb_size);
#endif
    if (!hnd) {
        ALOGE("%s: can't create fb mem private handle!!!", __func__);
        return -ENOMEM;
    }
    // hnd->fd = fb_fd;
    hnd->share_fd = fb_fd;
    hnd->size = pdev->fb_size;
    hnd->offset = 0;

    ALOGD("RGB Handle %p, size %d", hnd, hnd->size);
    pdev->hdmi_rgb_only_handle[pdev->hdmi_rgb_only_index] = hnd;

    return 0;
}

/*****************************************************************************/
/* util functions */

static int get_fb_screen_info(struct nxp_hwc_composer_device_1_t *dev)
{
    dev->fb_fd = open("/dev/graphics/fb0", O_RDWR);
    if (dev->fb_fd < 0) {
        ALOGE("failed to open framebuffer");
        return -1;
    }

    struct fb_var_screeninfo info;
    if (ioctl(dev->fb_fd, FBIOGET_VSCREENINFO, &info) == -1) {
        ALOGE("failed to ioctl FBIOGET_VSCREENINFO");
        return -1;
    }

    uint64_t base =
         (uint64_t)((info.upper_margin + info.lower_margin + info.yres)
             * (info.left_margin + info.right_margin + info.xres)
             * info.pixclock);
    uint32_t refreshRate = 60;
    if (base != 0) {
        refreshRate = 1000000000000LLU / base;
        if (refreshRate <= 0 || refreshRate > 60) {
            ALOGE("invalid refresh rate(%d), assuming 60Hz", refreshRate);
            ALOGE("upper_margin(%d), lower_margin(%d), yres(%d),left_margin(%d), right_margin(%d), xres(%d),pixclock(%d)",
                    info.upper_margin, info.lower_margin, info.yres,
                    info.left_margin, info.right_margin, info.xres,
                    info.pixclock);
            refreshRate = 60;
        }
    }

    dev->xres = info.xres;
    dev->yres = info.yres;
    dev->xdpi = 1000 * (info.xres * 25.4f) / info.width;
    dev->ydpi = 1000 * (info.yres * 25.4f) / info.height;
    dev->vsync_period = 1000000000 / refreshRate;

    ALOGV("using\n"
            "xres           = %d px\n",
            "yres           = %d px\n",
            "width          = %d mm (%f dpi)\n"
            "height         = %d mm (%f dpi)\n"
            "refresh rate   = %d Hz\n",
            dev->xres, dev->yres, info.width, dev->xdpi / 1000.0,
            info.height, dev->ydpi / 1000.0, refreshRate);

    return 0;
}

static void handle_vsync_event(struct nxp_hwc_composer_device_1_t  *pdev)
{
    if (!pdev->procs)
        return;

    int err = lseek(pdev->vsync_fd, 0, SEEK_SET);
    if (err < 0 ) {
        ALOGE("error seeking to vsync timestamp: %s", strerror(errno));
        return;
    }

    char buf[4096] = {0, };
    err = read(pdev->vsync_fd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return;
    }
    buf[sizeof(buf) - 1] = '\0';

    errno = 0;
    uint64_t timestamp = strtoull(buf, NULL, 0);
    if (!errno)
        pdev->procs->vsync(pdev->procs, 0, timestamp);
}

static void handle_hdmi_uevent(struct nxp_hwc_composer_device_1_t *pdev,
        const char *buf, int len)
{
    const char *s = buf;
    s += strlen(s) + 1;

    while (*s) {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE=")))
            pdev->hdmi_hpd = atoi(s + strlen("SWITCH_STATE=")) == 1;

        s += strlen(s) + 1;
        if (s - buf >= len)
            break;
    }

    if (pdev->hdmi_hpd) {
        ALOGD("hdmi plugged!!!");
        pdev->hdmi_blanked = false;
        pdev->hdmi_video_enabled = false;
        pdev->hdmi_rgb_enabled = false;
        pdev->rgb_handle = NULL;
    } else {
        ALOGD("hdmi unplugged!!!");
#if 1
        if (pdev->hdmi_enabled) {
            hdmi_disable(pdev);
        }
#endif
    }

    if (pdev->procs)
        pdev->procs->hotplug(pdev->procs, HWC_DISPLAY_EXTERNAL, pdev->hdmi_hpd);
}

/*****************************************************************************/
/* vsync thread */

static void *hwc_vsync_thread(void *data)
{
    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)data;
    char uevent_desc[4096];
    memset(uevent_desc, 0, sizeof(uevent_desc));

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

    uevent_init();

    char temp[4096];
    int err = read(pdev->vsync_fd, temp, sizeof(temp));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return NULL;
    }

    struct pollfd fds[2];
    fds[0].fd = pdev->vsync_fd;
    fds[0].events = POLLPRI;
    fds[1].fd = uevent_get_fd();
    fds[1].events = POLLIN;
    /* fds[1] reserved for hdmi hot plugging uevent */

    while(true) {
        err = poll(fds, 2, -1);

        if (err > 0) {
            if (fds[0].revents & POLLPRI) {
                handle_vsync_event(pdev);
            } else if (fds[1].revents & POLLIN) {
                int len = uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);
                bool hdmi = !strcmp(uevent_desc, "change@/devices/virtual/switch/hdmi");
                if (hdmi)
                    handle_hdmi_uevent(pdev, uevent_desc, len);
            }
        } else if (err == -1) {
            if (errno == EINTR) break;
            ALOGE("error in vsync thread: %s", strerror(errno));
        }
    }

    return NULL;
}

/*****************************************************************************/
/** callback functions */
static bool nxp_supports_overlay(hwc_layer_1_t &layer)
{
#ifndef USE_ONLY_GL
    // if (layer.transform != 0) return false;
    // check no blending
    if (layer.blending != HWC_BLENDING_NONE) return false;

    private_handle_t const *buffer = reinterpret_cast<private_handle_t const *>(layer.handle);
    if (buffer != NULL) {
        int format = buffer->format;
        // test code
#if 0
        bool supported =  (format == HAL_PIXEL_FORMAT_YCbCr_422_SP) ||
            (format == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
            (format == HAL_PIXEL_FORMAT_YCbCr_422_I)  ||
            (format == HAL_PIXEL_FORMAT_YV12);
        if (supported) {
            ALOGD("overlay transform: 0x%x", layer.transform);
            if (layer.transform != 0) return false;
            else return true;
        } else {
            ALOGD("rgb transform: 0x%x", layer.transform);
        }
        return false;
#else
        return (format == HAL_PIXEL_FORMAT_YCbCr_422_SP) ||
            (format == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
            (format == HAL_PIXEL_FORMAT_YCbCr_422_I)  ||
            (format == HAL_PIXEL_FORMAT_YV12);
#endif
    } else {
        return false;
    }
#else
    return false;
#endif
}

static int nxp_prepare_lcd(struct nxp_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t *contents)
{
    trace_in();

    bool overlaySet = false;
    int overlay_layer_index = -1;
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            ALOGV("\tlayer %u: framebuffer target(%d-%d,%d-%d)", i,
                    layer.displayFrame.left, layer.displayFrame.right,
                    layer.displayFrame.top, layer.displayFrame.bottom);
            pdev->rgb_handle = reinterpret_cast<private_handle_t const *>(layer.handle);
#ifndef USE_ONLY_GL
            if (pdev->hdmi_hpd) {
                pdev->hdmi_rgb_width = layer.displayFrame.right - layer.displayFrame.left;
                pdev->hdmi_rgb_height = layer.displayFrame.bottom - layer.displayFrame.top;
                if (pdev->rgb_handle) {
                    pdev->hdmi_rgb_only_index = pdev->rgb_handle->offset / pdev->fb_size;
                    get_hdmi_rgb_only_handle(pdev);
                }
            }
#endif
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND) {
            ALOGV("\tlayer %u: background", i);
            continue;
        }

#if !defined(USE_ONLY_GL) && !defined(LCD_USE_ONLY_GL)
        if (!overlaySet && nxp_supports_overlay(layer)) {
            ALOGV("\tlayer %u: overlay(%d-%d,%d-%d)", i,
                    layer.displayFrame.left, layer.displayFrame.right,
                    layer.displayFrame.top, layer.displayFrame.bottom);
            overlaySet = true;
            overlay_layer_index = i;
            layer.compositionType = HWC_OVERLAY;
            if (overlay_layer_index < (contents->numHwLayers - 2)) {
                pdev->is_top = false;
                layer.hints |= HWC_HINT_CLEAR_FB;
            } else {
                pdev->is_top = true;
            }
            continue;
        }
#endif

        ALOGV("\tlayer %u: framebuffer(%d-%d,%d-%d)", i,
                layer.displayFrame.left, layer.displayFrame.right,
                layer.displayFrame.top, layer.displayFrame.bottom);
        layer.compositionType = HWC_FRAMEBUFFER;
    }

    if (overlaySet) {
        if (pdev->overlay_layer_index < 0)
            pdev->overlay_state_changed = true;
        pdev->overlay_layer_index = overlay_layer_index;
    } else {
        if (pdev->overlay_layer_index >= 0) {
            pdev->overlay_layer_index = -1;
            pdev->overlay_state_changed = true;
        }
    }

    trace_exit();
    return 0;
}

static int nxp_prepare_hdmi(struct nxp_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t *contents)
{
    trace_in();

    bool overlaySet = false;
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND) {
            continue;
        }

        if (nxp_supports_overlay(layer)) {
            overlaySet = true;
            break;
        }
    }

    pdev->hdmi_rgb_handle = NULL; // refresh hdmi rgb handle
    if (overlaySet) {
        int overlay_index = -1;
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            //dump_layer(layer);

            if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
                pdev->hdmi_rgb_handle = reinterpret_cast<private_handle_t const *>(layer.handle);
                pdev->hdmi_rgb_width = layer.displayFrame.right - layer.displayFrame.left;
                pdev->hdmi_rgb_height = layer.displayFrame.bottom - layer.displayFrame.top;
                pdev->hdmi_rgb_start_x = layer.displayFrame.left;
                pdev->hdmi_rgb_start_y = layer.displayFrame.top;
                ALOGV("hdmi rgb: start_x %d, start_y %d, width %d, height %d", pdev->hdmi_rgb_start_x, pdev->hdmi_rgb_start_y, pdev->hdmi_rgb_width, pdev->hdmi_rgb_height);
                memcpy(&pdev->hdmi_rgb_src_crop, &layer.sourceCrop, sizeof(hwc_rect));
                // if (pdev->hdmi_rgb_handle)
                //     ALOGV("hdmi_rgb_handle: flags(0x%x), size(%d), offset(%d), base(0x%x), phys(0x%x), format(0x%x)",
                //             pdev->hdmi_rgb_handle->flags, pdev->hdmi_rgb_handle->size, pdev->hdmi_rgb_handle->offset,
                //             pdev->hdmi_rgb_handle->base, pdev->hdmi_rgb_handle->phys, pdev->hdmi_rgb_handle->format);
                continue;
            }

            if (layer.compositionType == HWC_BACKGROUND) {
                continue;
            }

            if (overlay_index < 0 && nxp_supports_overlay(layer)) {
                overlay_index = i;
                layer.compositionType = HWC_OVERLAY;
                layer.hints |= HWC_HINT_CLEAR_FB;
                continue;
            }

            layer.compositionType = HWC_FRAMEBUFFER;
        }
        pdev->hdmi_video_index = overlay_index;
    } else {
        pdev->hdmi_video_index = -1;
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];

            if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
#ifdef USE_ONLY_GL
                pdev->hdmi_rgb_handle = reinterpret_cast<private_handle_t const *>(layer.handle);
                pdev->hdmi_rgb_width = layer.displayFrame.right - layer.displayFrame.left;
                pdev->hdmi_rgb_height = layer.displayFrame.bottom - layer.displayFrame.top;
                pdev->hdmi_rgb_start_x = layer.displayFrame.left;
                pdev->hdmi_rgb_start_y = layer.displayFrame.top;
                ALOGD("hdmi rgb: start_x %d, start_y %d, width %d, height %d", pdev->hdmi_rgb_start_x, pdev->hdmi_rgb_start_y, pdev->hdmi_rgb_width, pdev->hdmi_rgb_height);
                memcpy(&pdev->hdmi_rgb_src_crop, &layer.sourceCrop, sizeof(hwc_rect));
#endif
                continue;
            }

            if (layer.compositionType == HWC_BACKGROUND) {
                continue;
            }
#ifndef USE_ONLY_GL
            layer.compositionType = HWC_OVERLAY;
#else
            layer.compositionType = HWC_FRAMEBUFFER;
#endif
        }
    }

    trace_exit();
    return 0;
}

static int nxp_prepare(struct hwc_composer_device_1 *dev,
        size_t numDisplays, hwc_display_contents_1_t **displays)
{
    trace_in();
    if (!numDisplays || !displays)
        return 0;

    ALOGV("%s: numDisplays(%d)", __func__, numDisplays);
    struct nxp_hwc_composer_device_1_t *pdev = (struct nxp_hwc_composer_device_1_t *)dev;
    hwc_display_contents_1_t *lcd_contents = displays[HWC_DISPLAY_PRIMARY];
    hwc_display_contents_1_t *hdmi_contents = displays[HWC_DISPLAY_EXTERNAL];

    if (lcd_contents)
        nxp_prepare_lcd(pdev, lcd_contents);
    if (hdmi_contents)
        nxp_prepare_hdmi(pdev, hdmi_contents);

#if 0
    if (!pdev->hdmi_hpd && pdev->hdmi_enabled) {
        hdmi_disable(pdev);
    }
#endif

    trace_exit();
    return 0;
}

static int nxp_config_lcd_overlay(struct nxp_hwc_composer_device_1_t *pdev)
{
    trace_in();
    ALOGV("%s: w(%d), h(%d), crop(%d,%d,%d,%d)", __func__, pdev->width, pdev->height,
            pdev->src_crop.left, pdev->src_crop.right, pdev->src_crop.top, pdev->src_crop.bottom);

    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_mlc0_video,
                pdev->src_crop.right - pdev->src_crop.left,
                pdev->src_crop.bottom - pdev->src_crop.top,
                // V4L2_PIX_FMT_YUYV));
                V4L2_PIX_FMT_YUV420M));
    CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_mlc0_video,
                pdev->start_x,
                pdev->start_y,
                pdev->width,
                pdev->height));
    CHECK_COMMAND(v4l2_reqbuf(nxp_v4l2_mlc0_video, pdev->out_max_buffer_count));
    if (pdev->is_top) {
        CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_VID_PRIORITY, 0));
    } else {
        CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_VID_PRIORITY, 1));
        CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_VID_COLORKEY, 0x0));
    }
    trace_exit();
    return 0;
}

static int nxp_set_lcd_buffer(struct nxp_hwc_composer_device_1_t *pdev,
        private_handle_t const *handle, bool is_start)
{
    trace_in();
    int out_index = pdev->out_index;
    int out_q_count = pdev->out_q_count;

#ifndef USE_V4L2_SYNC
    CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_mlc0_video, 3, out_index, handle, -1, NULL));
#else
#if 0
    int syncfd;
    CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_mlc0_video, 3, out_index, handle, -1, NULL, &syncfd));
    pdev->lcd_fence_fd = syncfd;
    ALOGD("LCD SYNC FD: %d", syncfd);
#else
    CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_mlc0_video, 3, out_index, handle, -1, NULL));
#endif
#endif
    out_q_count++;
    out_index++;
    out_index %= pdev->out_max_buffer_count;

    if (is_start)
        CHECK_COMMAND(v4l2_streamon(nxp_v4l2_mlc0_video));

    if (out_q_count >= pdev->out_max_buffer_count) {
        int dq_index;
        CHECK_COMMAND(v4l2_dqbuf(nxp_v4l2_mlc0_video, 1, &dq_index, NULL));
        out_q_count--;
    }

    pdev->out_index = out_index;
    pdev->out_q_count = out_q_count;
    ALOGV("set %p", handle);
    trace_exit();
    return 0;
}

static int nxp_post_framebuffer(struct nxp_hwc_composer_device_1_t *pdev)
{
    ALOGV("%s: handle %p", __func__, pdev->rgb_handle);

    if (pdev->rgb_handle) {
        private_handle_t const *hnd = pdev->rgb_handle;
        private_module_t *m = pdev->gralloc;

        if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
            m->info.activate = FB_ACTIVATE_VBL;
            m->info.yoffset = hnd->offset / m->finfo.line_length;
            /* psw0523 workaround: mali eglSwapbuffer() async problem */
#if 0
            m->info.yoffset += 0x320;
            if (m->info.yoffset > 0x640)
                m->info.yoffset = 0;
#endif
            // ALOGD("rgb Offset: 0x%x", m->info.yoffset);
            if (ioctl(m->framebuffer->fd, FBIOPUT_VSCREENINFO, &m->info) == -1) {
                ALOGE("post_framebuffer failed!!!");
                return -EINVAL;
            }
        }
    }
    return 0;
}

static int nxp_set_lcd(struct nxp_hwc_composer_device_1_t *pdev, hwc_display_contents_1_t *contents)
{
    trace_in();

    if (pdev->overlay_state_changed) {
        pdev->overlay_state_changed = false;
        if (pdev->overlay_layer_index >= 0) {
            ALOGD("first lcd overlay on!!!");
            // on
            hwc_layer_1_t &layer = contents->hwLayers[pdev->overlay_layer_index];
            private_handle_t const *buffer = reinterpret_cast<private_handle_t const *>(layer.handle);

            pdev->out_q_count = 0;
            pdev->out_index = 0;
            pdev->out_max_buffer_count = 4;

            pdev->format = buffer->format;
            pdev->width = layer.displayFrame.right - layer.displayFrame.left;
            pdev->height = layer.displayFrame.bottom - layer.displayFrame.top;
            pdev->start_x = layer.displayFrame.left;
            pdev->start_y = layer.displayFrame.top;
            memcpy(&pdev->src_crop, &layer.sourceCrop, sizeof(hwc_rect));

#ifndef USE_V4L2_SYNC
            pdev->sync_timeline_fd = sw_sync_timeline_create();
            if (pdev->sync_timeline_fd < 0) {
                ALOGE("failed to sw_sync_timeline_create()");
                return -EINVAL;
            }

            pdev->lcd_fence_count = 1;
            pdev->lcd_fence_fd = sw_sync_fence_create(pdev->sync_timeline_fd, "hwc lcd fence", 1);
            if (pdev->lcd_fence_fd < 0) {
                ALOGE("%s: can't create lcd fence fd!!!", __func__);
                return -EINVAL;
            }
            ALOGD("%s: lcd_fence_fd: %d", __func__, pdev->lcd_fence_fd);
#endif

            int ret = nxp_config_lcd_overlay(pdev);
            if (ret < 0) {
                ALOGE("%s: failed to config overlay!!!", __func__);
                return ret;
            }
            ret = nxp_set_lcd_buffer(pdev, buffer, true);
            if (ret < 0) {
                ALOGE("%s: failed to set lcd buffer!!!", __func__);
                return ret;
            }
            layer.releaseFenceFd = pdev->lcd_fence_fd;
        } else {
            // off
            ALOGD("off lcd overlay!!!");
            // close(pdev->lcd_fence_fd);
            // close(pdev->sync_timeline_fd);
            v4l2_streamoff(nxp_v4l2_mlc0_video);
        }
    } else if (pdev->overlay_layer_index >= 0) {
        hwc_layer_1_t &layer = contents->hwLayers[pdev->overlay_layer_index];
        private_handle_t const *buffer = reinterpret_cast<private_handle_t const *>(layer.handle);

        bool restart = false;
        if (pdev->format != buffer->format) {
            pdev->format = buffer->format;
            restart = true;
        }
        ALOGV("displayFrame: %d:%d-%d:%d", layer.displayFrame.left, layer.displayFrame.top,
                layer.displayFrame.right, layer.displayFrame.bottom);
        int width = layer.displayFrame.right - layer.displayFrame.left;
        if (width != pdev->width) {
            pdev->width = width;
            restart = true;
        }
        int height = layer.displayFrame.bottom - layer.displayFrame.top;
        if (height != pdev->height) {
            pdev->height = height;
            restart = true;
        }
        if (layer.displayFrame.left != pdev->start_x) {
            pdev->start_x = layer.displayFrame.left;
            restart = true;
        }
        if (layer.displayFrame.top != pdev->start_y) {
            pdev->start_y= layer.displayFrame.top;
            restart = true;
        }
        if (layer.sourceCrop.left != pdev->src_crop.left ||
                layer.sourceCrop.right != pdev->src_crop.right ||
                layer.sourceCrop.top != pdev->src_crop.top ||
                layer.sourceCrop.bottom != pdev->src_crop.bottom) {
            memcpy(&pdev->src_crop, &layer.sourceCrop, sizeof(hwc_rect));
            restart = true;
        }

        if (restart) {
            ALOGV("restart");
            v4l2_streamoff(nxp_v4l2_mlc0_video);

            pdev->out_q_count = 0;
            pdev->out_index = 0;
            pdev->out_max_buffer_count = 4;

            int ret = nxp_config_lcd_overlay(pdev);
            if (ret < 0) {
                ALOGE("%s: failed to config overlay!!!", __func__);
                return ret;
            }
        }
        int ret = nxp_set_lcd_buffer(pdev, buffer, restart);
        if (ret < 0) {
            ALOGE("%s: failed to set lcd buffer!!!", __func__);
            return ret;
        }
#ifndef USE_V4L2_SYNC
        ALOGV("%s: sw_sync_timeline_inc %d", __func__, pdev->lcd_fence_fd);
        sw_sync_timeline_inc(pdev->sync_timeline_fd, 1);
        pdev->lcd_fence_count++;
        pdev->lcd_fence_fd = sw_sync_fence_create(pdev->sync_timeline_fd, "hwc lcd fence", pdev->lcd_fence_count);
#endif
        layer.releaseFenceFd = pdev->lcd_fence_fd;
        //layer.acquireFenceFd = pdev->lcd_fence_fd;
    }
#if 1
    else {
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];

            if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
                ALOGV("\tlayer %u: framebuffer target(%d-%d,%d-%d)", i,
                        layer.displayFrame.left, layer.displayFrame.right,
                        layer.displayFrame.top, layer.displayFrame.bottom);
                pdev->rgb_handle = reinterpret_cast<private_handle_t const *>(layer.handle);
#ifndef USE_ONLY_GL
                if (pdev->hdmi_hpd) {
                    pdev->hdmi_rgb_width = layer.displayFrame.right - layer.displayFrame.left;
                    pdev->hdmi_rgb_height = layer.displayFrame.bottom - layer.displayFrame.top;
                    if (pdev->rgb_handle) {
                        pdev->hdmi_rgb_only_index = pdev->rgb_handle->offset / pdev->fb_size;
                        get_hdmi_rgb_only_handle(pdev);
                    }
                }
                break;
#else
                break;
#endif
            }
        }
    }
#endif

    // psw0523 test
    // nxp_post_framebuffer(pdev);

    trace_exit();
    return 0;
}

static int nxp_hdmi_config_rgb(struct nxp_hwc_composer_device_1_t *pdev)
{
    trace_in();
    ALOGD("%s: w(%d), h(%d), crop(%d,%d,%d,%d), sw(%d), sh(%d)", __func__, pdev->hdmi_rgb_width, pdev->hdmi_rgb_height,
            pdev->hdmi_rgb_src_crop.left, pdev->hdmi_rgb_src_crop.right, pdev->hdmi_rgb_src_crop.top, pdev->hdmi_rgb_src_crop.bottom,
            pdev->hdmi_screen_width, pdev->hdmi_screen_height);
    if (pdev->hdmi_rgb_width == 1280)
        CHECK_COMMAND(v4l2_set_preset(nxp_v4l2_hdmi, V4L2_DV_720P60));
    else
        CHECK_COMMAND(v4l2_set_preset(nxp_v4l2_hdmi, V4L2_DV_1080P60));

    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_mlc1_rgb, pdev->hdmi_rgb_width, pdev->hdmi_rgb_height, V4L2_PIX_FMT_RGB32));

#if 1
    if (pdev->hdmi_video_enabled) {
        CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_mlc1_rgb, 0, pdev->hdmi_video_start_y, pdev->hdmi_rgb_width, pdev->hdmi_rgb_height - pdev->hdmi_video_start_y));
        // CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_mlc1_rgb, 0, 60, pdev->hdmi_rgb_width, pdev->hdmi_rgb_height - 60));
    } else {
        CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_mlc1_rgb, 0, 0, pdev->hdmi_rgb_width, pdev->hdmi_rgb_height));
    }
#else
    CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_mlc1_rgb, 0, 0, pdev->hdmi_rgb_width, pdev->hdmi_rgb_height));
#endif
    CHECK_COMMAND(v4l2_reqbuf(nxp_v4l2_mlc1_rgb, pdev->hdmi_rgb_out_max_buffer_count));
    trace_exit();
    return 0;
}

static int nxp_hdmi_set_rgb_buffer(struct nxp_hwc_composer_device_1_t *pdev, private_handle_t const *buffer, bool is_start)
{
    trace_in();
    int out_index = pdev->hdmi_rgb_out_index;
    int out_q_count = pdev->hdmi_rgb_out_q_count;

#if 1
    CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_mlc1_rgb, 1, out_index, buffer, -1, NULL));
#else
    // abnormal quit
    int ret = v4l2_qbuf(nxp_v4l2_mlc1_rgb, 1, out_index, buffer, -1, NULL);
    if (ret) {
        ALOGE("abnormal rgb layer quit!!!");
        v4l2_streamoff(nxp_v4l2_mlc1_rgb);
        pdev->hdmi_rgb_enabled = false;
        for (int i = 0; i < 3; i++) {
            pdev->hdmi_rgb_only_handle[i] = NULL;
        }
        return 0;
    }
#endif
    out_q_count++;
    out_index++;
    out_index %= pdev->hdmi_rgb_out_max_buffer_count;

    if (is_start)
        CHECK_COMMAND(v4l2_streamon(nxp_v4l2_mlc1_rgb));

    if (out_q_count >= pdev->hdmi_rgb_out_max_buffer_count) {
        int dq_index;
        CHECK_COMMAND(v4l2_dqbuf(nxp_v4l2_mlc1_rgb, 1, &dq_index, NULL));
        out_q_count--;
    }

    pdev->hdmi_rgb_out_index = out_index;
    pdev->hdmi_rgb_out_q_count = out_q_count;
    ALOGV("%s: set %p", __func__, pdev->rgb_handle);
    trace_exit();
    return 0;
}

static int nxp_hdmi_config_video(struct nxp_hwc_composer_device_1_t *pdev)
{
    trace_in();
    ALOGD("%s: w(%d), h(%d), crop(%d,%d,%d,%d)", __func__, pdev->hdmi_video_width, pdev->hdmi_video_height,
           pdev->hdmi_video_src_crop.left, pdev->hdmi_video_src_crop.right, pdev->hdmi_video_src_crop.top, pdev->hdmi_video_src_crop.bottom);

    if (pdev->hdmi_video_width == 1280)
        CHECK_COMMAND(v4l2_set_preset(nxp_v4l2_hdmi, V4L2_DV_720P60));
    else {
        CHECK_COMMAND(v4l2_set_preset(nxp_v4l2_hdmi, V4L2_DV_1080P60));
        // HACK
        // pdev->hdmi_video_start_y = 0;
        // ALOGD("================> hdmi start y: %d", pdev->hdmi_video_start_y);
    }

    int start_y = 0;
    if (pdev->hdmi_video_start_y > 0) {
        start_y = pdev->hdmi_video_width == 1280 ? (720 - pdev->hdmi_video_height) >> 1 :
            (1080 - pdev->hdmi_video_height) >> 1;
        ALOGD("hdmi_video_width %d, start_y %d", pdev->hdmi_video_width, start_y);
    }

    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_mlc1_video,
                pdev->hdmi_video_src_crop.right - pdev->hdmi_video_src_crop.left,
                pdev->hdmi_video_src_crop.bottom - pdev->hdmi_video_src_crop.top,
                // V4L2_PIX_FMT_YUYV));
                V4L2_PIX_FMT_YUV420M));
    CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_mlc1_video,
                pdev->hdmi_video_start_x,
                start_y,
                pdev->hdmi_video_width,
                pdev->hdmi_video_height));

    CHECK_COMMAND(v4l2_reqbuf(nxp_v4l2_mlc1_video, pdev->hdmi_video_out_max_buffer_count));
    CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc1_video, V4L2_CID_MLC_VID_PRIORITY, 1));
    CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc1_video, V4L2_CID_MLC_VID_COLORKEY, 0x0));
    trace_exit();
    return 0;
}

static int nxp_hdmi_set_video_buffer(struct nxp_hwc_composer_device_1_t *pdev, private_handle_t const *buffer, bool is_start)
{
    trace_in();
    int out_index = pdev->hdmi_video_out_index;
    int out_q_count = pdev->hdmi_video_out_q_count;

#ifndef USE_V4L2_SYNC
    CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_mlc1_video, 3, out_index, buffer, -1, NULL));
#else
#if 0
    int syncfd;
    CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_mlc1_video, 3, out_index, buffer, -1, NULL, &syncfd, NULL));
    ALOGD("HDMI SYNC FD: %d", syncfd);
    pdev->hdmi_fence_fd = syncfd;
#else
    CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_mlc1_video, 3, out_index, buffer, -1, NULL));
    pdev->hdmi_fence_fd = -1;
#endif
#endif
    out_q_count++;
    out_index++;
    out_index %= pdev->hdmi_video_out_max_buffer_count;

    if (is_start)
        CHECK_COMMAND(v4l2_streamon(nxp_v4l2_mlc1_video));

    if (out_q_count >= pdev->hdmi_video_out_max_buffer_count) {
        int dq_index;
        CHECK_COMMAND(v4l2_dqbuf(nxp_v4l2_mlc1_video, 1, &dq_index, NULL));
        out_q_count--;
    }

    pdev->hdmi_video_out_index = out_index;
    pdev->hdmi_video_out_q_count = out_q_count;
    ALOGV("%s: set %p", __func__, buffer);
    trace_exit();
    return 0;
}

static int nxp_set_hdmi(struct nxp_hwc_composer_device_1_t *pdev, hwc_display_contents_1_t *contents)
{
    trace_in();

    if (pdev->hdmi_hpd) {
        if (!pdev->hdmi_enabled)
            hdmi_enable(pdev);
#if 1
    } else {
        if (pdev->hdmi_enabled)
            hdmi_disable(pdev);
        return 0;
#endif
    }

#ifdef USE_HDMI_THREAD
    private_handle_t const *rgbBuffer = NULL, *videoBuffer = NULL;
#endif

    if (pdev->hdmi_video_index >= 0) {
        // video
        hwc_layer_1_t &layer = contents->hwLayers[pdev->hdmi_video_index];
        private_handle_t const *buffer = reinterpret_cast<private_handle_t const *>(layer.handle);

        if (!pdev->hdmi_video_enabled) {
            // first on
            ALOGD("HDMI VIDEO FIRST ON!!!");
            pdev->hdmi_video_format = buffer->format;
            pdev->hdmi_video_width = layer.displayFrame.right - layer.displayFrame.left;
            pdev->hdmi_video_height = layer.displayFrame.bottom - layer.displayFrame.top;
            pdev->hdmi_video_start_x = layer.displayFrame.left;
            pdev->hdmi_video_start_y = layer.displayFrame.top;
            ALOGD("hdmi start y: %d", pdev->hdmi_video_start_y);
            // TODO, HACK: 56/2
            // pdev->hdmi_video_start_y /= 2;
            // end HACK
            memcpy(&pdev->hdmi_video_src_crop, &layer.sourceCrop, sizeof(hwc_rect));
            pdev->hdmi_video_enabled = true;
            pdev->hdmi_video_out_index = 0;
            pdev->hdmi_video_out_q_count = 0;
            pdev->hdmi_video_out_max_buffer_count = 4;

#ifndef USE_V4L2_SYNC
            /* create hdmi fence */
            pdev->hdmi_sync_timeline_fd = sw_sync_timeline_create();
            if (pdev->hdmi_sync_timeline_fd < 0) {
                ALOGE("failed to sw_sync_timeline_create() for hdmi");
                return -EINVAL;
            }

            pdev->hdmi_fence_count = 1;
            pdev->hdmi_fence_fd = sw_sync_fence_create(pdev->hdmi_sync_timeline_fd, "hwc hdmi fence", 1);
            if (pdev->hdmi_fence_fd < 0) {
                ALOGE("%s: can't create hdmi fence fd!!!", __func__);
                return -EINVAL;
            }
            ALOGD("%s: hdmi_fence_fd: %d", __func__, pdev->hdmi_fence_fd);
#endif

            nxp_hdmi_config_video(pdev);
#ifdef USE_HDMI_THREAD
            videoBuffer = buffer;
#else
            nxp_hdmi_set_video_buffer(pdev, buffer, true);
#endif
            layer.releaseFenceFd = pdev->hdmi_fence_fd;

            if (pdev->hdmi_rgb_enabled) {
                // off already enabled RGB
                ALOGD("off hdmi rgb only!!!");
#ifdef USE_HDMI_THREAD
                v4l2_streamoff(nxp_v4l2_mlc1_rgb);
                NXHdmiMessage rmsg(NXHdmiMessage::HDMI_STOP_RGB, 0, 0);
                pdev->HdmiThread->sendMessage(rmsg);
#else
                v4l2_streamoff(nxp_v4l2_mlc1_rgb);
#endif
                pdev->hdmi_rgb_enabled = false;
            }
        } else {
#ifdef USE_HDMI_THREAD
            videoBuffer = buffer;
#else
            nxp_hdmi_set_video_buffer(pdev, buffer, false);
#endif

#ifndef USE_V4L2_SYNC
            ALOGV("%s: sw_sync_timeline_inc %d for hdmi", __func__, pdev->hdmi_fence_fd);
            sw_sync_timeline_inc(pdev->hdmi_sync_timeline_fd, 1);
            pdev->hdmi_fence_count++;
            pdev->hdmi_fence_fd = sw_sync_fence_create(pdev->hdmi_sync_timeline_fd, "hwc hdmi fence", pdev->hdmi_fence_count);
#endif
            ALOGV("layer: acquireFenceFd %d\n", layer.acquireFenceFd);
            layer.releaseFenceFd = pdev->hdmi_fence_fd;
        }

        if (pdev->hdmi_rgb_handle) {
            if (!pdev->hdmi_rgb_enabled) {
                ALOGD("Enable HDMI RGB!!!");
                pdev->hdmi_rgb_out_max_buffer_count = 3;
                pdev->hdmi_rgb_out_index = 0;
                pdev->hdmi_rgb_out_q_count = 0;
                nxp_hdmi_config_rgb(pdev);
                pdev->hdmi_rgb_enabled = true;
                // test code
                //draw_to_hdmi_rgb_buffer(pdev->hdmi_rgb_handle);
#ifdef USE_HDMI_THREAD
                rgbBuffer = pdev->hdmi_rgb_handle;
#else
                int ret = nxp_hdmi_set_rgb_buffer(pdev, pdev->hdmi_rgb_handle, true);
#endif
            } else {
                //draw_to_hdmi_rgb_buffer(pdev->hdmi_rgb_handle);
#ifdef USE_HDMI_THREAD
                rgbBuffer = pdev->hdmi_rgb_handle;
#else
                int ret = nxp_hdmi_set_rgb_buffer(pdev, pdev->hdmi_rgb_handle, false);
#endif
            }
        }
    } else {
        if (pdev->hdmi_video_enabled) {
            // off hdmi yuv layer
            ALOGD("STOP hdmi yuv!!!");
#ifndef USE_HDMI_THREAD
            v4l2_streamoff(nxp_v4l2_mlc1_video);
#endif
            pdev->hdmi_video_enabled = false;
            // off rgb
            if (pdev->hdmi_rgb_enabled) {
#ifdef USE_HDMI_THREAD
                ALOGD("STOP hdmi rgb!!!");
                v4l2_streamoff(nxp_v4l2_mlc1_rgb);
                NXHdmiMessage rmsg(NXHdmiMessage::HDMI_STOP_RGB, 0, 0);
                pdev->HdmiThread->sendMessage(rmsg);
#else
                v4l2_streamoff(nxp_v4l2_mlc1_rgb);
#endif
                pdev->hdmi_rgb_enabled = false;
            }

            // close(pdev->hdmi_fence_fd);
            close(pdev->hdmi_sync_timeline_fd);
        }

#ifdef USE_ONLY_GL
        if (pdev->hdmi_rgb_handle) {
            if (!pdev->hdmi_rgb_enabled) {
                ALOGD("Enable HDMI RGB!!!");
                pdev->hdmi_rgb_out_max_buffer_count = 3;
                pdev->hdmi_rgb_out_index = 0;
                pdev->hdmi_rgb_out_q_count = 0;
                nxp_hdmi_config_rgb(pdev);
                pdev->hdmi_rgb_enabled = true;
                // test code
                //draw_to_hdmi_rgb_buffer(pdev->hdmi_rgb_handle);
    #ifdef USE_HDMI_THREAD
                rgbBuffer = pdev->hdmi_rgb_handle;
    #else
                int ret = nxp_hdmi_set_rgb_buffer(pdev, pdev->hdmi_rgb_handle, true);
    #endif
            } else {
                //draw_to_hdmi_rgb_buffer(pdev->hdmi_rgb_handle);
    #ifdef USE_HDMI_THREAD
                rgbBuffer = pdev->hdmi_rgb_handle;
    #else
                int ret = nxp_hdmi_set_rgb_buffer(pdev, pdev->hdmi_rgb_handle, false);
    #endif
            }
        }
#else
        // display mirror
        if (pdev->rgb_handle == NULL) {
            ALOGE("%s: rgb_handle is NULL!!!", __func__);
            return -1;
        }

        if (!pdev->hdmi_rgb_enabled) {
            pdev->hdmi_rgb_out_max_buffer_count = 3;
            pdev->hdmi_rgb_out_index = 0;
            pdev->hdmi_rgb_out_q_count = 0;
            nxp_hdmi_config_rgb(pdev);
            pdev->hdmi_rgb_enabled = true;
            ALOGD("START hdmi rgb mirror!!!");
    #ifdef USE_HDMI_THREAD
            // pdev->hdmi_rgb_only_index = pdev->rgb_handle->offset / pdev->fb_size;
            rgbBuffer = pdev->hdmi_rgb_only_handle[pdev->hdmi_rgb_only_index];
    #else
            int ret = nxp_hdmi_set_rgb_buffer(pdev, pdev->hdmi_rgb_only_handle[pdev->hdmi_rgb_only_index], true);
#if 0
            if (ret) {
                ALOGE("ABNORMAL Error!!! restarting");
                v4l2_streamoff(nxp_v4l2_mlc1_rgb);
                pdev->hdmi_rgb_enabled = false;
                for (int i = 0; i < 3; i++) {
                    if (pdev->hdmi_rgb_only_handle[i]) {
                        close(pdev->hdmi_rgb_only_handle[i]->share_fd);
                        delete pdev->hdmi_rgb_only_handle[i];
                        pdev->hdmi_rgb_only_handle[i] = NULL;
                    }
                }
                pdev->hdmi_rgb_out_max_buffer_count = 3;
                pdev->hdmi_rgb_out_index = 0;
                pdev->hdmi_rgb_out_q_count = 0;
                nxp_hdmi_config_rgb(pdev);
                pdev->hdmi_rgb_enabled = true;
                get_hdmi_rgb_only_handle(pdev);
                nxp_hdmi_set_rgb_buffer(pdev, pdev->hdmi_rgb_only_handle[pdev->hdmi_rgb_only_index], true);
            }
#endif
    #endif
        } else {
    #ifdef USE_HDMI_THREAD
            rgbBuffer = pdev->hdmi_rgb_only_handle[pdev->hdmi_rgb_only_index];
    #else
            int ret = nxp_hdmi_set_rgb_buffer(pdev, pdev->hdmi_rgb_only_handle[pdev->hdmi_rgb_only_index], false);
#if 0
            if (ret) {
                ALOGE("ABNORMAL Error!!!");
                v4l2_streamoff(nxp_v4l2_mlc1_rgb);
                pdev->hdmi_rgb_enabled = false;
                for (int i = 0; i < 3; i++) {
                    if (pdev->hdmi_rgb_only_handle[i]) {
                        close(pdev->hdmi_rgb_only_handle[i]->share_fd);
                        delete pdev->hdmi_rgb_only_handle[i];
                        pdev->hdmi_rgb_only_handle[i] = NULL;
                    }
                }
            }
#endif
    #endif
        }
#endif
    }

#ifdef USE_HDMI_THREAD
    NXHdmiMessage msg(NXHdmiMessage::HDMI_PLAY, (uint32_t)rgbBuffer, (uint32_t)videoBuffer);
    ALOGV("rgbBuffer(%p), videoBuffer(%p)", rgbBuffer, videoBuffer);
    pdev->HdmiThread->sendMessage(msg);
#endif

    trace_exit();
    return 0;
}

static int nxp_set(struct hwc_composer_device_1 *dev,
        size_t numDisplays, hwc_display_contents_1_t **displays)
{
    trace_in();

    if (!displays)
        return 0;

    struct nxp_hwc_composer_device_1_t *pdev = (struct nxp_hwc_composer_device_1_t *)dev;

    hwc_display_contents_1_t *lcd_contents = displays[HWC_DISPLAY_PRIMARY];
    hwc_display_contents_1_t *hdmi_contents = displays[HWC_DISPLAY_EXTERNAL];

    // TODO
#if 1
    if (lcd_contents)
        nxp_set_lcd(pdev, lcd_contents);
#endif

    if (hdmi_contents)
        nxp_set_hdmi(pdev, hdmi_contents);

#if 1
    nxp_post_framebuffer(pdev);
#else
    if (lcd_contents)
        nxp_set_lcd(pdev, lcd_contents);
#endif

    trace_exit();
    return 0;
}

static int nxp_eventControl(struct hwc_composer_device_1 *dev, int dpy,
        int event, int enabled)
{
    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)dev;

    switch (event) {
        case HWC_EVENT_VSYNC:
            __u32 val = !!enabled;
            int err;
            if (val) {
                err = write(pdev->vsync_ctl_fd, VSYNC_ON, sizeof(VSYNC_ON));
            } else {
                err = write(pdev->vsync_ctl_fd, VSYNC_OFF, sizeof(VSYNC_OFF));
            }
            if (err < 0) {
                ALOGE("failed to vsync ctl");
                return -errno;
            }
    }

    return 0;
}

static int nxp_blank(struct hwc_composer_device_1 *dev, int disp, int blank)
{
    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)dev;

    ALOGD("%s entered: disp(%d), blank(%d)", __func__, disp, blank);
    switch (disp) {
        case HWC_DISPLAY_PRIMARY:
            {
                int fb_blank = blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK;
                int err = ioctl(pdev->fb_fd, FBIOBLANK, fb_blank);
                if (err < 0) {
                    if (errno == EBUSY)
                        ALOGI("%sblank ioctl failed (display already %sblanked)",
                                blank ? "" : "un", blank ? "" : "un");
                    else
                        ALOGE("%sblank ioctl failed: %s", blank ? "" : "un",
                                strerror(errno));
                    return -errno;
                }
            }
            break;

        case HWC_DISPLAY_EXTERNAL:
            if (pdev->hdmi_hpd) {
                ALOGD("%s: blank hdmi!!!", __func__);
                if (blank && !pdev->hdmi_blanked)
                    hdmi_disable(pdev);
                pdev->hdmi_blanked = !!blank;
            }
            break;

        default:
            return -EINVAL;
    }

    return 0;
}

static int nxp_query(struct hwc_composer_device_1 *dev, int what, int *value)
{
    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)dev;

    switch (what) {
        case HWC_BACKGROUND_LAYER_SUPPORTED:
            /* current, we do not support background layer */
            value[0] = 0;
            break;
        case HWC_VSYNC_PERIOD:
            value[0] = pdev->vsync_period;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static void nxp_registerProcs(struct hwc_composer_device_1 *dev,
        hwc_procs_t const *procs)
{
    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)dev;
    pdev->procs = procs;
}

static void nxp_dump(struct hwc_composer_device_1 *dev, char *buff, int buff_len)
{
    /* TODO : report current state*/
    if (buff_len <= 0) return;

    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)dev;

    android::String8 result;

    result.append("nxp hwcomposer status\n");
    result.appendFormat("xres         : %d", pdev->xres);
    result.appendFormat("yres         : %d", pdev->yres);
    result.appendFormat("xdpi         : %f", pdev->xdpi/1000.0);
    result.appendFormat("ydpi         : %f", pdev->ydpi/1000.0);
    result.appendFormat("vsync period : %d", pdev->vsync_period);
    result.append("\n");

    strlcpy(buff, result.string(), buff_len);
}

static int nxp_getDisplayConfigs(struct hwc_composer_device_1 *dev,
        int disp, uint32_t *configs, size_t *numConfigs)
{
    ALOGD("%s entered", __func__);
    if (*numConfigs == 0)
        return 0;

    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)dev;

    if (disp == HWC_DISPLAY_PRIMARY) {
        configs[0] = 0;
        *numConfigs = 1;
        ALOGD("Primary");
        return 0;
    } else if (disp == HWC_DISPLAY_EXTERNAL) {
        if (!pdev->hdmi_hpd)
            return -EINVAL;

        pdev->hdmi_screen_width = 1920;
        /* TODO: HACK */
        // pdev->hdmi_screen_height = 1080;
        pdev->hdmi_screen_height = 1200;

        configs[0] = 0;
        *numConfigs = 1;
        ALOGD("HDMI");
        return 0;
    }

    return -EINVAL;
}

static int nxp_getDisplayAttributes(struct hwc_composer_device_1 *dev,
        int disp, uint32_t config, const uint32_t *attributes, int32_t *values)
{
    ALOGD("%s entered: disp %d", __func__, disp);
    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)dev;

    for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++) {
        if (disp == HWC_DISPLAY_PRIMARY) {
            switch (attributes[i]) {
                case HWC_DISPLAY_VSYNC_PERIOD:
                    values[i] = pdev->vsync_period;
                    break;
                case HWC_DISPLAY_WIDTH:
                    values[i] = pdev->xres;
                    break;
                case HWC_DISPLAY_HEIGHT:
                    values[i] = pdev->yres;
                    break;
                case HWC_DISPLAY_DPI_X:
                    values[i] = pdev->xdpi;
                    break;
                case HWC_DISPLAY_DPI_Y:
                    values[i] = pdev->ydpi;
                    break;
                default:
                    ALOGE("unknown display attribute %u", attributes[i]);
                    return -EINVAL;
            }
        } else if (disp == HWC_DISPLAY_EXTERNAL) {
            switch (attributes[i]) {
                case HWC_DISPLAY_VSYNC_PERIOD:
                    values[i] = pdev->vsync_period;
                    //values[i] = 60;
                    break;
                case HWC_DISPLAY_WIDTH:
                    values[i] = pdev->hdmi_screen_width;
                    break;
                case HWC_DISPLAY_HEIGHT:
                    values[i] = pdev->hdmi_screen_height;
                    break;
                case HWC_DISPLAY_DPI_X:
                case HWC_DISPLAY_DPI_Y:
                    return 0;
                default:
                    ALOGE("unknown display attribute %u", attributes[i]);
                    return -EINVAL;
            }
        } else {
            ALOGE("unknown display type %u", disp);
            return -EINVAL;
        }
    }
    return 0;
}

static int nxp_close(hw_device_t *device)
{
    ALOGD("%s", __func__);
    struct nxp_hwc_composer_device_1_t *pdev =
        (struct nxp_hwc_composer_device_1_t *)device;

    for (int i = 0; i < 3; i++) {
        if (pdev->hdmi_rgb_only_handle[i] != NULL) {
            private_handle_t *handle = pdev->hdmi_rgb_only_handle[i];
            close(handle->fd);
            delete handle;
            pdev->hdmi_rgb_only_handle[i] = NULL;
        }
    }

    pthread_kill(pdev->vsync_thread, SIGTERM);
    pthread_join(pdev->vsync_thread, NULL);
    close(pdev->vsync_fd);
    close(pdev->vsync_ctl_fd);
    close(pdev->fb_fd);
    return 0;
}

static int nxp_open(const struct hw_module_t *module, const char *name,
        struct hw_device_t **device)
{
    int ret;
    int refreshRate;
    int hdmi_sw_fd;

    ALOGD("%s", __func__);
    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        return -EINVAL;
    }

    struct nxp_hwc_composer_device_1_t *dev;
    dev = (struct nxp_hwc_composer_device_1_t *)malloc(sizeof(*dev));
    if (!dev) {
        ALOGE("failed to malloc nxp_hwc_composer_device_1_t");
        return -ENOMEM;
    }
    memset(dev, 0, sizeof(*dev));

    dev->vsync_ctl_fd = open(VSYNC_CTL_FILE, O_RDWR);
    if (dev->vsync_ctl_fd < 0) {
        ALOGE("failed to open vsync ctl fd(%s)", VSYNC_CTL_FILE);
        goto err_vsync_ctl_fd;
    }

    // hdmi status
    dev->hdmi_hpd = false;
    hdmi_sw_fd = open("/sys/class/switch/hdmi/state", O_RDONLY);
    if (hdmi_sw_fd) {
        char val;
        if (read(hdmi_sw_fd, &val, 1) == 1 && val == '1') {
            dev->hdmi_hpd = true;
            ALOGD("hdmi plugged boot!!!");
        }
    }

    dev->vsync_fd = open(VSYNC_MON_FILE, O_RDONLY);
    if (dev->vsync_fd < 0) {
        ALOGE("failed to open vsync monitor fd(%s)", VSYNC_MON_FILE);
        goto err_vsync_mon_fd;
    }

    ret = get_fb_screen_info(dev);
    if (ret < 0) {
        ALOGE("failed to get_fb_screen_info()");
    }

    dev->hdmi_rgb_out_max_buffer_count = 3;
    dev->hdmi_video_out_max_buffer_count = 4;

    dev->base.common.tag = HARDWARE_DEVICE_TAG;
    dev->base.common.version = HWC_DEVICE_API_VERSION_1_1;
    dev->base.common.module = const_cast<hw_module_t *>(module);
    dev->base.common.close = nxp_close;

    dev->base.prepare = nxp_prepare;
    dev->base.set = nxp_set;
    dev->base.eventControl = nxp_eventControl;
    dev->base.blank = nxp_blank;
    dev->base.query = nxp_query;
    dev->base.registerProcs = nxp_registerProcs;
    dev->base.getDisplayConfigs = nxp_getDisplayConfigs;
    dev->base.getDisplayAttributes = nxp_getDisplayAttributes;

    *device = &dev->base.common;

    ret = pthread_create(&dev->vsync_thread, NULL, hwc_vsync_thread, dev);
    if (ret) {
        ALOGE("failed to start vsync thread: %s", strerror(ret));
        goto err_vsync_thread;
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.hwc.force_cpu", value, "0");
    dev->overlay_layer_index = -1;

    android_nxp_v4l2_init();

    hw_module_t *pmodule;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **)&pmodule) == 0) {
        dev->gralloc = reinterpret_cast<private_module_t *>(pmodule);
        dev->fb_size = dev->gralloc->finfo.line_length * dev->gralloc->info.yres;

        for (int i = 0; i < 3; i++) {
            dev->hdmi_rgb_only_handle[i] = NULL;
        }
    } else {
        ALOGE("Could not get gralloc module!!!");
        return -ENODEV;
    }

    // for sync
#if 0
    dev->sync_timeline_fd = sw_sync_timeline_create();
    if (dev->sync_timeline_fd < 0) {
        ALOGE("failed to sw_sync_timeline_create()");
        return -EINVAL;
    }
#endif
    ALOGD("%s: sync_timeline_fd %d", __func__, dev->sync_timeline_fd);
    dev->lcd_fence_fd = -1;
    dev->lcd_fence_count = 0;
    dev->hdmi_fence_fd = -1;
    dev->hdmi_fence_count = 0;

    return 0;

err_vsync_thread:
    close(dev->vsync_fd);
err_vsync_mon_fd:
    close(dev->vsync_ctl_fd);
err_vsync_ctl_fd:
    free(dev);
    return -1;
}

/*****************************************************************************/

static struct hw_module_methods_t nxp_hwc_module_methods = {
open: nxp_open,
};

hwc_module_t HAL_MODULE_INFO_SYM = {
common: {
tag: HARDWARE_MODULE_TAG,
     module_api_version: HWC_MODULE_API_VERSION_0_1,
     hal_api_version: HARDWARE_HAL_API_VERSION,
     id: HWC_HARDWARE_MODULE_ID,
     name: "Nexell nxp hwcomposer module",
     author: "swpark@nexell.co.kr",
     methods: &nxp_hwc_module_methods,
     dso : 0,
     reserved: {0},
        }
};
