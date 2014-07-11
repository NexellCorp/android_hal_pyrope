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
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/fb.h>
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
#include <NXCpu.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>
#include <hardware_legacy/uevent.h>

#include <utils/Atomic.h>

#include <gralloc_priv.h>

#include "HWCRenderer.h"
#include "HWCImpl.h"
#include "HWCreator.h"

#include "service/NXHWCService.h"

#define VSYNC_CTL_FILE      "/sys/devices/platform/display/active.0"
#define VSYNC_MON_FILE      "/sys/devices/platform/display/vsync.0"
#define VSYNC_ON            "1"
#define VSYNC_OFF           "0"

#define HDMI_STATE_FILE     "/sys/class/switch/hdmi/state"
#define FRAMEBUFFER_FILE    "/dev/graphics/fb0"

#define HWC_SCENARIO_PROPERTY_KEY    "hwc.scenario"
#define HWC_SCALE_PROPERTY_KEY       "hwc.scale"
#define HWC_RESOLUTION_PROPERTY_KEY  "hwc.resolution"
#define HWC_HDMIMODE_PROPERTY_KEY    "hwc.hdmimode"
#define HWC_SCREEN_DOWNSIZING_PROPERTY_KEY "hwc.screendownsizing"

#define MAX_SCALE_FACTOR        3

enum {
    HDMI_MODE_PRIMARY = 0,
    HDMI_MODE_SECONDARY
};

// prepare --set serializing sync
#define USE_PREPARE_SET_SERIALIZING_SYNC

using namespace android;

/**********************************************************************************************
 * Structures
 */
struct FBScreenInfo {
    int32_t     xres;
    int32_t     yres;
    int32_t     xdpi;
    int32_t     ydpi;
    int32_t     width;
    int32_t     height;
    int32_t     vsync_period;
};

struct NXHWC {
public:
    hwc_composer_device_1_t base;

    struct HWCPropertyChangeListener : public NXHWCService::PropertyChangeListener {
        public:
            HWCPropertyChangeListener(struct NXHWC *parent)
                : mParent(parent)
            {
            }
        virtual void onPropertyChanged(int code, int val);
        private:
            NXHWC *mParent;
    } *mPropertyChangeListener;

    /* fds */
    int mVsyncCtlFd;
    int mVsyncMonFd;
    int mFrameBufferFd;

    /* threads */
    pthread_t mVsyncThread;

    /* screeninfo */
    struct FBScreenInfo mScreenInfo;

    /* state */
    bool mHDMIPlugged;

    /* properties */
    uint32_t mUsageScenario;
    uint32_t mOriginalUsageScenario;
    uint32_t mHDMIPreset;
    uint32_t mScaleFactor;
    uint32_t mHDMIMode;
    bool     mScreenDownSizing;


    /* interface to SurfaceFlinger */
    const hwc_procs_t *mProcs;

    /* IMPL */
    android::HWCImpl *mLCDImpl;
    android::HWCImpl *mHDMIImpl;
    android::HWCImpl *mHDMIAlternateImpl;
    volatile int32_t mUseHDMIAlternate;
    volatile int32_t mChangeHDMIImpl;

    uint32_t mCpuVersion;

    uint32_t mHDMIWidth;
    uint32_t mHDMIHeight;
    volatile int32_t mChangingScenario;

    // for prepare, set sync
#ifdef USE_PREPARE_SET_SERIALIZING_SYNC
    Mutex mSyncLock;
    Condition mSyncSignal;
    bool mPrepared;
#endif

    Mutex mChangeImplLock;
    Condition mChangeImplSignal;
    bool mChangingImpl;

    void handleVsyncEvent();
    void handleHDMIEvent(const char *buf, int len);
    void handleUsageScenarioChanged(uint32_t usageScenario);
    void handleResolutionChanged(uint32_t reolution);
    void handleRescScaleFactorChanged(uint32_t factor);
    void handleScreenDownSizingChanged(int downsizing);

    void changeUsageScenario();
    void changeHDMIImpl();

    void getHWCProperty();
    void checkHDMIModeAndSetProperty();
    void setHDMIPreset(uint32_t preset);

    void determineUsageScenario();
};

/**
 * for property change callback
 */
static struct NXHWC *sNXHWC = NULL;

/**********************************************************************************************
 * Util Funcs
 */
static int get_fb_screen_info(struct FBScreenInfo *pInfo)
{
    int fd = open(FRAMEBUFFER_FILE, O_RDONLY);
    if (fd < 0) {
        ALOGE("failed to open framebuffer: %s", FRAMEBUFFER_FILE);
        return -EINVAL;
    }

    struct fb_var_screeninfo info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1) {
        ALOGE("failed to ioctl FBIOGET_VSCREENINFO");
        close(fd);
        return -EINVAL;
    }

    close(fd);

    uint64_t base =
         (uint64_t)((info.upper_margin + info.lower_margin + info.yres)
             * (info.left_margin + info.right_margin + info.xres)
             * info.pixclock);
    uint32_t refreshRate = 60;
    if (base != 0) {
        refreshRate = 1000000000000LLU / base;
        refreshRate /= 1000;
        if (refreshRate <= 0 || refreshRate > 60) {
            ALOGE("invalid refresh rate(%d), assuming 60Hz", refreshRate);
            ALOGE("upper_margin(%d), lower_margin(%d), yres(%d),left_margin(%d), right_margin(%d), xres(%d),pixclock(%d)",
                    info.upper_margin, info.lower_margin, info.yres,
                    info.left_margin, info.right_margin, info.xres,
                    info.pixclock);
            refreshRate = 60;
        }
    }

    pInfo->xres = info.xres;
    pInfo->yres = info.yres;
    pInfo->xdpi = 1000 * (info.xres * 25.4f) / info.width;
    pInfo->ydpi = 1000 * (info.yres * 25.4f) / info.height;
    // pInfo->width = info.width;
    // pInfo->height = info.height;
    pInfo->width = info.xres;
    pInfo->height = info.yres;
    pInfo->vsync_period = 1000000000 / refreshRate;
    ALOGV("lcd xres %d, yres %d, xdpi %d, ydpi %d, width %d, height %d, vsync_period %llu",
            pInfo->xres, pInfo->yres, pInfo->xdpi, pInfo->ydpi, pInfo->width, pInfo->height, pInfo->vsync_period);

    return 0;
}

static bool hdmi_connected()
{
    char val;
    int fd = open(HDMI_STATE_FILE, O_RDONLY);
    if (fd < 0) {
        ALOGE("failed to open hdmi state fd: %s", HDMI_STATE_FILE);
    } else {
        char val;
        if (read(fd, &val, 1) == 1 && val == '1') {
            return true;
        }
        close(fd);
    }

    return false;
}

/**********************************************************************************************
 * VSync & HDMI Hot Plug Monitoring Thread
 */
static void *hwc_vsync_thread(void *data)
{
    struct NXHWC *me = (struct NXHWC *)data;
    char uevent_desc[4096];
    memset(uevent_desc, 0, sizeof(uevent_desc));

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

    uevent_init();

    char temp[4096];
    int err = read(me->mVsyncMonFd, temp, sizeof(temp));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return NULL;
    }

    struct pollfd fds[2];
    fds[0].fd = me->mVsyncMonFd;
    fds[0].events = POLLPRI;
    fds[1].fd = uevent_get_fd();
    fds[1].events = POLLIN;

    while(true) {
        err = poll(fds, 2, -1);

        if (err > 0) {
            if (fds[0].revents & POLLPRI) {
                me->handleVsyncEvent();
            } else if (fds[1].revents & POLLIN) {
                if (me->mHDMIMode == HDMI_MODE_SECONDARY) {
                    int len = uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);
                    bool hdmi = !strcmp(uevent_desc, "change@/devices/virtual/switch/hdmi");
                    if (hdmi)
                        me->handleHDMIEvent(uevent_desc, len);
                }
            }
        } else if (err == -1) {
            if (errno == EINTR) break;
            ALOGE("error in vsync thread: %s", strerror(errno));
        }
    }

    return NULL;
}

/**********************************************************************************************
 * NXHWC Member Functions
 */
void NXHWC::HWCPropertyChangeListener::onPropertyChanged(int code, int val)
{
    ALOGV("onPropertyChanged: code %i, val %i", code, val);
    switch (code) {
    case INXHWCService::HWC_SCENARIO_PROPERTY_CHANGED:
        mParent->handleUsageScenarioChanged(val);
        break;
    case INXHWCService::HWC_RESOLUTION_CHANGED:
        mParent->handleResolutionChanged(val);
        break;
    case INXHWCService::HWC_RESC_SCALE_FACTOR_CHANGED:
        mParent->handleRescScaleFactorChanged(val);
        break;
    case INXHWCService::HWC_SCREEN_DOWNSIZING_CHANGED:
        mParent->handleScreenDownSizingChanged(val);
        break;
    }
}

void NXHWC::handleVsyncEvent()
{
    if (!mProcs)
        return;

    int err = lseek(mVsyncMonFd, 0, SEEK_SET);
    if (err < 0 ) {
        ALOGE("error seeking to vsync timestamp: %s", strerror(errno));
        return;
    }

    char buf[4096] = {0, };
    err = read(mVsyncMonFd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return;
    }
    buf[sizeof(buf) - 1] = '\0';

    errno = 0;
    uint64_t timestamp = strtoull(buf, NULL, 0);
    ALOGV("vsync: timestamp %llu", timestamp);
    if (!errno)
        mProcs->vsync(mProcs, 0, timestamp);
}

void NXHWC::handleHDMIEvent(const char *buf, int len)
{
    const char *s = buf;
    s += strlen(s) + 1;
    int hpd = 0;

    while (*s) {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE=")))
            hpd = atoi(s + strlen("SWITCH_STATE=")) == 1;

        s += strlen(s) + 1;
        if (s - buf >= len)
            break;
    }

    if (hpd) {
        if (!mHDMIPlugged) {
            ALOGD("hdmi plugged!!!");

            mHDMIPlugged = true;
            mHDMIImpl->enable();
            mProcs->hotplug(mProcs, HWC_DISPLAY_EXTERNAL, mHDMIPlugged);
        }
    } else {
        if (mHDMIPlugged) {
            ALOGD("hdmi unplugged!!!");

            mHDMIPlugged = false;
            mHDMIImpl->disable();
            if (mHDMIAlternateImpl)
                mHDMIAlternateImpl->disable();
            mProcs->hotplug(mProcs, HWC_DISPLAY_EXTERNAL, mHDMIPlugged);
        }
    }
}

void NXHWC::handleUsageScenarioChanged(uint32_t usageScenario)
{
    if (usageScenario != mUsageScenario) {
        ALOGD("handleUsageScenarioChange: hwc usage scenario %d ---> %d", mUsageScenario, usageScenario);
        android_atomic_release_cas(mChangingScenario, 1, &mChangingScenario);
        mUsageScenario = usageScenario;
    }
}

void NXHWC::changeUsageScenario()
{
    ALOGD("Change Usage Scenario real!!!");
    android::HWCImpl *oldLCDImpl, *oldHDMIImpl, *oldHDMIAlternativeImpl;
    android::HWCImpl *newLCDImpl, *newHDMIImpl, *newHDMIAlternativeImpl;

    oldLCDImpl = mLCDImpl;
    oldHDMIImpl = mHDMIImpl;
    oldHDMIAlternativeImpl = mHDMIAlternateImpl;

    mHDMIImpl->disable();
    if (mHDMIAlternateImpl != NULL && mHDMIAlternateImpl->getEnabled())
        mHDMIAlternateImpl->disable();

    newLCDImpl = HWCreator::create(HWCreator::DISPLAY_LCD, mUsageScenario, mScreenInfo.width, mScreenInfo.height);
    if (!newLCDImpl) {
        ALOGE("failed to create lcd implementor: scenario %d", mUsageScenario);
    }

    ALOGD("hdmi wxh(%dx%d), srcwxh(%dx%d)", mHDMIWidth, mHDMIHeight, mScreenInfo.width, mScreenInfo.height);
    newHDMIImpl = HWCreator::create(HWCreator::DISPLAY_HDMI, mUsageScenario,
            mHDMIWidth, mHDMIHeight,
            mScreenInfo.width, mScreenInfo.height, mScaleFactor);
    if (!newHDMIImpl) {
        ALOGE("failed to create hdmi implementor: scenario %d", mUsageScenario);
    }

    newHDMIAlternativeImpl = HWCreator::create(HWCreator::DISPLAY_HDMI_ALTERNATE,
            mUsageScenario,
            mHDMIWidth,
            mHDMIHeight,
            mScreenInfo.width,
            mScreenInfo.height,
            mScaleFactor);
    mUseHDMIAlternate = 0;
    mChangeHDMIImpl = 0;

    mLCDImpl = newLCDImpl;
    mHDMIImpl = newHDMIImpl;
    mHDMIAlternateImpl = newHDMIAlternativeImpl;

    mHDMIImpl->enable();

    delete oldLCDImpl;
    delete oldHDMIImpl;
    if (oldHDMIAlternativeImpl)
        delete oldHDMIAlternativeImpl;

    ALOGD("complete!!!");

    android_atomic_release_cas(mChangingScenario, 0, &mChangingScenario);
}

void NXHWC::changeHDMIImpl()
{
    ALOGD("changeHDMIImpl entered");

    // 1. check hdmi connected, disable hdmi
    //mHDMIImpl->disable();
    //if (mHDMIAlternateImpl)
        //mHDMIAlternateImpl->disable();
    if (mHDMIPlugged) {
        mHDMIPlugged = false;
        mHDMIImpl->disable();
        if (mHDMIAlternateImpl)
            mHDMIAlternateImpl->disable();
        ALOGD("changeHDMIImpl: force disconnect!");
        mProcs->hotplug(mProcs, HWC_DISPLAY_EXTERNAL, mHDMIPlugged);
    }

    // 2. create new impl
    android::HWCImpl *newHDMIImpl, *newHDMIAlternativeImpl;

    newHDMIImpl = HWCreator::create(HWCreator::DISPLAY_HDMI,
            mUsageScenario,
            mHDMIWidth,
            mHDMIHeight,
            mScreenInfo.width,
            mScreenInfo.height,
            mScaleFactor);
    if (!newHDMIImpl) {
        ALOGE("handleResolutionChanged: failed to create hdmi implementor: scenario %d", mUsageScenario);
        return;
    }

    newHDMIAlternativeImpl = HWCreator::create(HWCreator::DISPLAY_HDMI_ALTERNATE,
            mUsageScenario,
            mHDMIWidth,
            mHDMIHeight,
            mScreenInfo.width,
            mScreenInfo.height,
            mScaleFactor);

    mUseHDMIAlternate = 0;

    // 3. swap impl
    android::HWCImpl *oldHDMIImpl = mHDMIImpl;
    android::HWCImpl *oldHDMIAlternativeImpl = mHDMIAlternateImpl;

    mHDMIImpl = newHDMIImpl;
    mHDMIAlternateImpl = newHDMIAlternativeImpl;

    // 4. delete old impl
    delete oldHDMIImpl;
    if (oldHDMIAlternativeImpl)
        delete oldHDMIAlternativeImpl;

    // 5. check hdmi connected and if so, enable
    if (hdmi_connected()) {
        ALOGD("changeHDMIImpl: force connect!");
        mHDMIPlugged = true;
        mHDMIImpl->enable();
        mProcs->hotplug(mProcs, HWC_DISPLAY_EXTERNAL, mHDMIPlugged);
    }

    mChangingImpl = false;
    mChangeImplSignal.signal();
    ALOGD("changeHDMIImpl exit");
}

void NXHWC::handleRescScaleFactorChanged(uint32_t factor)
{
    ALOGD("handleRescScaleFactorChanged: %d", factor);

    // this is not used
#if 0
    uint32_t scaleFactor = MAX_SCALE_FACTOR - factor;

    if (scaleFactor == mScaleFactor)
        return;

    mScaleFactor = scaleFactor;
    determineUsageScenario();

    {
        Mutex::Autolock l(mChangeImplLock);
        mChangingImpl = true;
    }
#endif
}

void NXHWC::handleScreenDownSizingChanged(int downsizing)
{
    ALOGD("handleScreenDownSizingChanged: %d", downsizing);

    bool isDownSizing = (bool)downsizing;

    if (isDownSizing == mScreenDownSizing)
        return;

    mScreenDownSizing = isDownSizing;
    determineUsageScenario();

    {
        Mutex::Autolock l(mChangeImplLock);
        mChangingImpl = true;
    }
}

void NXHWC::handleResolutionChanged(uint32_t preset)
{
    ALOGD("handleResolutionChanged: %d", preset);

    if (preset == mHDMIPreset)
        return;

    setHDMIPreset(preset);

    {
        Mutex::Autolock l(mChangeImplLock);
        mChangingImpl = true;
    }
}

void NXHWC::getHWCProperty()
{
    int len;
    char buf[PROPERTY_VALUE_MAX];
    len = property_get((const char *)HWC_SCENARIO_PROPERTY_KEY, buf, "2"); // default - LCDUseOnlyGL, HDMIUseOnlyGL
    if (len <= 0)
        mUsageScenario = 2;
    else
        mUsageScenario = atoi(buf);

    if (mUsageScenario >= HWCreator::USAGE_SCENARIO_MAX) {
        ALOGW("invalid hwc scenario %d", mUsageScenario);
        mUsageScenario = HWCreator::LCD_USE_ONLY_GL_HDMI_USE_ONLY_MIRROR;
    }
    mOriginalUsageScenario = mUsageScenario;

    len = property_get((const char *)HWC_RESOLUTION_PROPERTY_KEY, buf, "18"); // default - 1920x1080
    if (len <= 0)
        setHDMIPreset(18);
    else
        setHDMIPreset(atoi(buf));

#if 0
    len = property_get((const char *)HWC_SCALE_PROPERTY_KEY, buf, "3"); // default - no down scale
    if (len <= 0)
        mScaleFactor = 0;
    else
        mScaleFactor = MAX_SCALE_FACTOR - atoi(buf);
#else
    mScaleFactor = 0;
#endif

    len = property_get((const char *)HWC_SCREEN_DOWNSIZING_PROPERTY_KEY, buf, "0"); // default - no downsizing
    if (len <= 0)
        mScreenDownSizing = false;
    else
        mScreenDownSizing = buf[0] == '1' ? true : false;
}

void NXHWC::checkHDMIModeAndSetProperty()
{
    int fd = open("/sys/devices/platform/nxp-hdmi/modalias", O_RDONLY);
    char *mode;
    if (fd < 0) {
        ALOGD("%s: hdmi is secondary", __func__);
        mHDMIMode = HDMI_MODE_SECONDARY;
        mode = (char *)"secondary";
    } else {
        ALOGD("%s: hdmi is primary", __func__);
        mHDMIMode = HDMI_MODE_PRIMARY;
        mode = (char *)"primary";
        close(fd);
    }
    property_set((const char *)HWC_HDMIMODE_PROPERTY_KEY, mode);
}

void NXHWC::setHDMIPreset(uint32_t preset)
{
    if (preset != mHDMIPreset) {
        switch (preset) {
            case V4L2_DV_1080P60:
                mHDMIWidth = 1920;
                mHDMIHeight = 1080;
                mHDMIPreset = preset;
                break;
            case V4L2_DV_720P60:
                mHDMIWidth = 1280;
                mHDMIHeight = 720;
                mHDMIPreset = preset;
                break;
            case V4L2_DV_576P50:
                mHDMIWidth = 720;
                mHDMIHeight = 576;
                mHDMIPreset = preset;
                break;
            case V4L2_DV_480P60:
                mHDMIWidth = 720;
                mHDMIHeight = 480;
                mHDMIPreset = preset;
                break;
            default:
                mHDMIWidth = 1920;
                mHDMIHeight = 1080;
                mHDMIPreset = V4L2_DV_1080P60;
                break;
        }

        ALOGD("HDMI Resolution: %dx%d", mHDMIWidth, mHDMIHeight);
    }
}

void NXHWC::determineUsageScenario()
{
    //if (mScaleFactor > 0) {
    if (mScreenDownSizing) {
        switch (mUsageScenario) {
        case HWCreator::LCD_USE_ONLY_GL_HDMI_USE_ONLY_MIRROR:
            mUsageScenario = HWCreator::LCD_USE_ONLY_GL_HDMI_USE_MIRROR_RESC;
            break;

        case HWCreator::LCD_USE_ONLY_GL_HDMI_USE_ONLY_GL:
            mUsageScenario = HWCreator::LCD_USE_ONLY_GL_HDMI_USE_GL_RESC;
            break;

        case HWCreator::LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO:
            mUsageScenario = HWCreator::LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO_RESC;
            break;

        case HWCreator::LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO:
            mUsageScenario = HWCreator::LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO_RESC;
            break;

        case HWCreator::LCD_USE_GL_AND_VIDEO_HDMI_USE_ONLY_GL:
            mUsageScenario = HWCreator::LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_RESC;
            break;

        case HWCreator::LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO:
            mUsageScenario = HWCreator::LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO_RESC;
            break;

        case HWCreator::LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO:
            mUsageScenario = HWCreator::LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO_RESC;
            break;
        }
    } else {
        mUsageScenario = mOriginalUsageScenario;
    }
}

/**********************************************************************************************
 * Android HWComposer Callback Funcs
 */
static int hwc_wait_commit(struct hwc_composer_device_1 *dev)
{
#ifdef USE_PREPARE_SET_SERIALIZING_SYNC
    struct NXHWC *me = (struct NXHWC *)dev;
    Mutex::Autolock l(me->mSyncLock);
    while (me->mPrepared)
        me->mSyncSignal.wait(me->mSyncLock);
#endif
    return 0;
}

static int hwc_prepare(struct hwc_composer_device_1 *dev,
        size_t numDisplays, hwc_display_contents_1_t **displays)
{
    if (!numDisplays || !displays)
        return 0;

    struct NXHWC *me = (struct NXHWC *)dev;

#ifdef USE_PREPARE_SET_SERIALIZING_SYNC
    {
        Mutex::Autolock l(me->mSyncLock);
        me->mPrepared = true;
    }
#endif

    hwc_display_contents_1_t *lcdContents = displays[HWC_DISPLAY_PRIMARY];
    hwc_display_contents_1_t *hdmiContents = displays[HWC_DISPLAY_EXTERNAL];

    ALOGV("prepare: lcd %p, hdmi %p", lcdContents, hdmiContents);

    if (lcdContents)
        me->mLCDImpl->prepare(lcdContents);

    if (hdmiContents) {
        int ret = me->mHDMIImpl->prepare(hdmiContents);
        if (me->mHDMIAlternateImpl) {
            if (ret < 0) {
                if (!me->mUseHDMIAlternate) {
                    ALOGD("Change to Alternate");
                    me->mChangeHDMIImpl = 1;
                    me->mUseHDMIAlternate = 1;
                } else {
                    me->mHDMIAlternateImpl->prepare(hdmiContents);
                }
            } else {
                if (me->mUseHDMIAlternate) {
                    ALOGD("Change to Original");
                    me->mChangeHDMIImpl = 1;
                    me->mUseHDMIAlternate = 0;
                }
            }
        }
    }

    return 0;
}

static int hwc_set(struct hwc_composer_device_1 *dev,
        size_t numDisplays, hwc_display_contents_1_t **displays)
{
    if (!numDisplays || !displays)
        return 0;

    struct NXHWC *me = (struct NXHWC *)dev;

    hwc_display_contents_1_t *lcdContents = displays[HWC_DISPLAY_PRIMARY];
    hwc_display_contents_1_t *hdmiContents = displays[HWC_DISPLAY_EXTERNAL];

    ALOGV("hwc_set lcd %p, hdmi %p", lcdContents, hdmiContents);

    private_handle_t const *rgbHandle = NULL;

    if (lcdContents) {
        me->mLCDImpl->set(lcdContents, NULL);
        rgbHandle = me->mLCDImpl->getRgbHandle();
    }

    if (hdmiContents && me->mHDMIPlugged) {
        if (me->mChangeHDMIImpl) {
            if (me->mUseHDMIAlternate) {
                ALOGD("Use Alternate");
                me->mHDMIImpl->disable();
                me->mHDMIAlternateImpl->enable();
            } else {
                ALOGD("Use Original");
                me->mHDMIAlternateImpl->disable();
                me->mHDMIImpl->enable();
            }
            me->mChangeHDMIImpl = 0;
        } else {
            android::HWCImpl *impl;
            if (me->mUseHDMIAlternate) {
                impl = me->mHDMIAlternateImpl;
            } else {
                impl = me->mHDMIImpl;
            }
            impl->set(hdmiContents, (void *)rgbHandle);
            impl->render();
        }

        // handle unplug
#if 0
        if (!me->mHDMIPlugged) {
            if (me->mHDMIImpl->getEnabled()) {
                ALOGD("hwc_set() call mHDMIImpl->disable()");
                me->mHDMIImpl->disable();
            }
            if (me->mHDMIAlternateImpl && me->mHDMIAlternateImpl->getEnabled()) {
                ALOGD("hwc_set() call mHDMIAlternateImpl->disable()");
                me->mHDMIAlternateImpl->disable();
            }
        }
#endif
    }

    if (lcdContents)
        me->mLCDImpl->render();


    // handle scenario change
    if (android_atomic_acquire_load(&me->mChangingScenario) > 0)
        me->changeUsageScenario();

#ifdef USE_PREPARE_SET_SERIALIZING_SYNC
    {
        Mutex::Autolock l(me->mSyncLock);
        me->mPrepared = false;
        me->mSyncSignal.signal();
    }
#endif

    {
        Mutex::Autolock l(me->mChangeImplLock);
        if (me->mChangingImpl)
            me->changeHDMIImpl();
    }

    return 0;
}

static int hwc_eventControl(struct hwc_composer_device_1 *dev, int dpy,
        int event, int enabled)
{
    struct NXHWC *me = (struct NXHWC *)dev;

    switch (event) {
    case HWC_EVENT_VSYNC:
        __u32 val = !!enabled;
        ALOGD("HWC_EVENT_VSYNC: val %d", val);
        int err;
        if (val)
            err = write(me->mVsyncCtlFd, VSYNC_ON, sizeof(VSYNC_ON));
        else
            err = write(me->mVsyncCtlFd, VSYNC_OFF, sizeof(VSYNC_OFF));
        if (err < 0) {
            ALOGE("failed to write to vsync ctl fd");
            return -errno;
        }
    }

    return 0;
}

static int hwc_blank(struct hwc_composer_device_1 *dev, int disp, int blank)
{
    struct NXHWC *me = (struct NXHWC *)dev;

    ALOGD("hwc_blank: disp %d, blank %d", disp, blank);
    switch (disp) {
    case HWC_DISPLAY_PRIMARY:
        if (blank)
            return me->mLCDImpl->disable();
        else
            return me->mLCDImpl->enable();
        break;

    case HWC_DISPLAY_EXTERNAL:
        if (blank)
            v4l2_set_ctrl(nxp_v4l2_hdmi, V4L2_CID_HDMI_ON_OFF, 0);
        else
            v4l2_set_ctrl(nxp_v4l2_hdmi, V4L2_CID_HDMI_ON_OFF, 1);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static int hwc_query(struct hwc_composer_device_1 *dev, int what, int *value)
{
    struct NXHWC *me = (struct NXHWC *)dev;

    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        value[0] = 0;
        break;
    case HWC_VSYNC_PERIOD:
        value[0] = me->mScreenInfo.vsync_period;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static int hwc_getDisplayConfigs(struct hwc_composer_device_1 *dev,
        int disp, uint32_t *configs, size_t *numConfigs)
{
    if (*numConfigs == 0)
        return 0;

    struct NXHWC *me = (struct NXHWC *)dev;

    if (disp == HWC_DISPLAY_PRIMARY) {
        configs[0] = 0;
        *numConfigs = 1;
        return 0;
    } else if (disp == HWC_DISPLAY_EXTERNAL) {
        if (!me->mHDMIPlugged)
            return -EINVAL;

        configs[0] = 0;
        *numConfigs = 1;
        return 0;
    }

    return -EINVAL;
}

static int hwc_getDisplayAttributes(struct hwc_composer_device_1 *dev,
        int disp, uint32_t config, const uint32_t *attributes, int32_t *values)
{
    struct NXHWC *me = (struct NXHWC *)dev;
    struct FBScreenInfo *pInfo = &me->mScreenInfo;

    for (int i =0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++) {
        if (disp == HWC_DISPLAY_PRIMARY) {
            switch (attributes[i]) {
            case HWC_DISPLAY_VSYNC_PERIOD:
                values[i] = pInfo->vsync_period;
                break;
            case HWC_DISPLAY_WIDTH:
                ALOGD("width: %d", pInfo->xres);
                values[i] = pInfo->xres;
                break;
            case HWC_DISPLAY_HEIGHT:
                ALOGD("height: %d", pInfo->yres);
                values[i] = pInfo->yres;
                break;
            case HWC_DISPLAY_DPI_X:
                values[i] = pInfo->xdpi;
                break;
            case HWC_DISPLAY_DPI_Y:
                values[i] = pInfo->ydpi;
                break;
            default:
                ALOGE("unknown display attribute %u", attributes[i]);
                return -EINVAL;
            }
        } else if (disp == HWC_DISPLAY_EXTERNAL) {
            switch (attributes[i]) {
            case HWC_DISPLAY_VSYNC_PERIOD:
                values[i] = pInfo->vsync_period;
                break;
            case HWC_DISPLAY_WIDTH:
                values[i] = me->mHDMIWidth;
                break;
            case HWC_DISPLAY_HEIGHT:
                values[i] = me->mHDMIHeight;
                break;
            case HWC_DISPLAY_DPI_X:
                break;
            case HWC_DISPLAY_DPI_Y:
                break;
            default:
                ALOGE("unknown display attribute %u", attributes[i]);
                return -EINVAL;
            }
        }
    }

    return 0;
}

static void hwc_registerProcs(struct hwc_composer_device_1 *dev,
        hwc_procs_t const *procs)
{
    struct NXHWC *me = (struct NXHWC *)dev;
    ALOGD("%s", __func__);
    me->mProcs = procs;
}

static int hwc_close(hw_device_t *device)
{
    struct NXHWC *me = (struct NXHWC *)device;

    pthread_kill(me->mVsyncThread, SIGTERM);
    pthread_join(me->mVsyncThread, NULL);

    if (me->mVsyncCtlFd > 0)
        close(me->mVsyncCtlFd);
    if (me->mVsyncMonFd > 0)
        close(me->mVsyncMonFd);

    if (me->mLCDImpl)
        delete me->mLCDImpl;
    if (me->mHDMIImpl)
        delete me->mHDMIImpl;
    if (me->mHDMIAlternateImpl)
        delete me->mHDMIAlternateImpl;
    delete me;

    sNXHWC = NULL;

    return 0;
}

static int hwc_open(const struct hw_module_t *module, const char *name, struct hw_device_t **device)
{
    int ret;

    ALOGD("hwc_open");

    NXHWC::HWCPropertyChangeListener *listener = NULL;
    NXHWCService *service = NULL;

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        ALOGE("invalid name: %s", name);
        return -EINVAL;
    }

    NXHWC *me = new NXHWC();
    if (!me) {
        ALOGE("can't create NXHWC");
        return -ENOMEM;
    }

    int fd = open(VSYNC_CTL_FILE, O_RDWR);
    if (fd < 0) {
        ALOGE("failed to open vsync ctl fd: %s", VSYNC_CTL_FILE);
        goto error_out;
    }
    me->mVsyncCtlFd = fd;

    fd = open(VSYNC_MON_FILE, O_RDONLY);
    if (fd < 0) {
        ALOGE("failed to open vsync mon fd: %s", VSYNC_MON_FILE);
        goto error_out;
    }
    me->mVsyncMonFd = fd;

    ret = get_fb_screen_info(&me->mScreenInfo);
    if (ret < 0) {
        ALOGE("failed to get_fb_screen_info()");
        goto error_out;
    }

    me->getHWCProperty();
    me->checkHDMIModeAndSetProperty();

    if (me->mHDMIMode == HDMI_MODE_SECONDARY && hdmi_connected()) {
        me->mHDMIPlugged = true;
        ALOGD("HDMI Plugged boot!!!");
    }

    me->mLCDImpl = HWCreator::create(HWCreator::DISPLAY_LCD,
            me->mUsageScenario,
            me->mScreenInfo.width,
            me->mScreenInfo.height);
    if (!me->mLCDImpl) {
        ALOGE("failed to create lcd implementor: scenario %d", me->mUsageScenario);
        goto error_out;
    }

    me->mHDMIImpl = HWCreator::create(HWCreator::DISPLAY_HDMI,
            me->mUsageScenario,
            me->mHDMIWidth,
            me->mHDMIHeight,
            me->mScreenInfo.width,
            me->mScreenInfo.height,
            me->mScaleFactor);
    if (!me->mHDMIImpl) {
        ALOGE("failed to create hdmi implementor: scenario %d", me->mUsageScenario);
        goto error_out;
    }

    me->mHDMIAlternateImpl = HWCreator::create(HWCreator::DISPLAY_HDMI_ALTERNATE,
            me->mUsageScenario,
            me->mHDMIWidth,
            me->mHDMIHeight,
            me->mScreenInfo.width,
            me->mScreenInfo.height,
            me->mScaleFactor);
    me->mUseHDMIAlternate = 0;
    ALOGD("mHDMIAlternateImpl %p", me->mHDMIAlternateImpl);

    ret = pthread_create(&me->mVsyncThread, NULL, hwc_vsync_thread, me);
    if (ret) {
        ALOGE("failed to start vsync thread: %s", strerror(ret));
        goto error_out;
    }

    me->mChangingScenario = 0;
    me->mChangingImpl = false;

    android_nxp_v4l2_init();

    // TODO
#if 0
    if (me->mHDMIPlugged)
#endif
    me->mHDMIImpl->enable(); // set(): real enable

    me->base.common.tag     = HARDWARE_DEVICE_TAG;
    me->base.common.version = HWC_DEVICE_API_VERSION_1_1;
    me->base.common.module  = const_cast<hw_module_t *>(module);
    me->base.common.close   = hwc_close;

    me->base.prepare        = hwc_prepare;
    me->base.set            = hwc_set;
    me->base.eventControl   = hwc_eventControl;
    me->base.blank          = hwc_blank;
    me->base.query          = hwc_query;
    me->base.registerProcs  = hwc_registerProcs;
    me->base.getDisplayConfigs      = hwc_getDisplayConfigs;
    me->base.getDisplayAttributes   = hwc_getDisplayAttributes;

    me->base.reserved_proc[0] = (void *)hwc_wait_commit;

    *device = &me->base.common;

    listener = new NXHWC::HWCPropertyChangeListener(me);
    if (!listener) {
        ALOGE("can't create HWCPropertyChangeListener!!!");
        goto error_out;
    }
    me->mPropertyChangeListener = listener;

    ALOGD("start NXHWCService");
    service = startNXHWCService();
    if (!service) {
        ALOGE("can't start NXHWCService!!!");
        goto error_out;
    }

    service->registerListener(listener);

    // prepare - set sync
#ifdef USE_PREPARE_SET_SERIALIZING_SYNC
    me->mPrepared = false;
#endif

    sNXHWC = me;
    return 0;

error_out:
#if 0
    if (me->mVsyncCtlFd > 0)
        close(me->mVsyncCtlFd);
    if (me->mVsyncMonFd > 0)
        close(me->mVsyncMonFd);
    if (me->mLCDImpl)
        delete me->mLCDImpl;
    if (me->mHDMIImpl)
        delete me->mHDMIImpl;
    delete me;
#else
    hwc_close(&me->base.common);
#endif
    return -EINVAL;
}

static struct hw_module_methods_t hwc_module_methods = {
open: hwc_open,
};

hwc_module_t HAL_MODULE_INFO_SYM = {
common: {
tag:                HARDWARE_MODULE_TAG,
module_api_version: HWC_MODULE_API_VERSION_0_1,
hal_api_version:    HARDWARE_HAL_API_VERSION,
id:                 HWC_HARDWARE_MODULE_ID,
name:               "NEXELL pyrope hwcomposer module",
author:             "swpark@nexell.co.kr",
methods:            &hwc_module_methods,
dso:                0,
reserved:           {0},
        }
};

