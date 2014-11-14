#undef LOG_TAG
#define LOG_TAG     "HWCreator"

#include <cutils/log.h>

#include <nxp-v4l2.h>

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>

#include <gralloc_priv.h>

#include "HWCRenderer.h"
#include "RescConfigure.h"
#include "HWCImpl.h"
#include "LCDCommonImpl.h"
#include "LCDUseOnlyGLImpl.h"
#include "LCDUseGLAndVideoImpl.h"
#include "HDMICommonImpl.h"
#include "HDMIUseOnlyMirrorImpl.h"
#include "HDMIUseMirrorAndVideoImpl.h"
#include "HDMIUseOnlyGLImpl.h"
#include "HDMIUseGLAndVideoImpl.h"
#include "HDMIUseRescCommonImpl.h"
#include "HWCreator.h"

using namespace android;

android::HWCImpl *HWCreator::create(
        uint32_t dispType,
        uint32_t usageScenario,
        int32_t  width,
        int32_t  height,
        int32_t  srcWidth,
        int32_t  srcHeight,
        int32_t  scaleFactor)
{
    if (dispType == DISPLAY_LCD) {
        switch (usageScenario) {
            case LCD_USE_ONLY_GL_HDMI_USE_ONLY_MIRROR:
            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_RESC:
            case LCD_USE_ONLY_GL_HDMI_USE_ONLY_GL:
            case LCD_USE_ONLY_GL_HDMI_USE_GL_RESC:
            case LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO:
            case LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO_RESC:
            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO:
            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO_RESC:
                return new LCDUseOnlyGLImpl(nxp_v4l2_mlc0_rgb, width, height);

            case LCD_USE_GL_AND_VIDEO_HDMI_USE_ONLY_GL:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_RESC:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO_RESC:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO_RESC:
                return new LCDUseGLAndVideoImpl(nxp_v4l2_mlc0_rgb, nxp_v4l2_mlc0_video, width, height);

            default:
                return NULL;
        }
    } else if (dispType == DISPLAY_HDMI) {
        switch (usageScenario) {
            case LCD_USE_ONLY_GL_HDMI_USE_ONLY_MIRROR:
                return new HDMIUseOnlyMirrorImpl(nxp_v4l2_mlc1_rgb, srcWidth, srcHeight, srcWidth, srcHeight);

            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_RESC:
            {
                HDMICommonImpl *impl = new HDMIUseOnlyMirrorImpl(nxp_v4l2_mlc1_rgb, srcWidth, srcHeight, srcWidth, srcHeight);
                RescConfigure *rescConfigure = new RescConfigure();
                return new HDMIUseRescCommonImpl(nxp_v4l2_mlc1_rgb,
                        nxp_v4l2_mlc1_video,
                        /* TODO */
                        1280,
                        720,
                        srcWidth,
                        srcHeight,
                        scaleFactor,
                        impl,
                        rescConfigure);
            }

            case LCD_USE_ONLY_GL_HDMI_USE_ONLY_GL:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_ONLY_GL:
                return new HDMIUseOnlyGLImpl(nxp_v4l2_mlc1_rgb, width, height);

            case LCD_USE_ONLY_GL_HDMI_USE_GL_RESC:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_RESC:
            {
                HDMICommonImpl *impl = new HDMIUseOnlyGLImpl(nxp_v4l2_mlc1_rgb, width, height);
                RescConfigure *rescConfigure = new RescConfigure();
                return new HDMIUseRescCommonImpl(nxp_v4l2_mlc1_rgb,
                        nxp_v4l2_mlc1_video,
                        width,
                        height,
                        width,
                        height,
                        scaleFactor,
                        impl,
                        rescConfigure);
            }

            case LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO:
                return new HDMIUseGLAndVideoImpl(nxp_v4l2_mlc1_rgb, nxp_v4l2_mlc1_video, width, height);

            case LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO_RESC:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO_RESC:
            {
                HDMICommonImpl *impl = new HDMIUseGLAndVideoImpl(nxp_v4l2_mlc1_rgb, nxp_v4l2_mlc1_video, width, height);
                RescConfigure *rescConfigure = new RescConfigure();
                return new HDMIUseRescCommonImpl(nxp_v4l2_mlc1_rgb,
                        nxp_v4l2_mlc1_video,
                        width,
                        height,
                        width,
                        height,
                        scaleFactor,
                        impl,
                        rescConfigure);
            }

            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO:
            {
                HDMICommonImpl *mirrorImpl = new HDMIUseOnlyMirrorImpl(nxp_v4l2_mlc1_rgb, srcWidth, srcHeight, srcWidth, srcHeight);
                HDMICommonImpl *glAndVideoImpl = new HDMIUseGLAndVideoImpl(nxp_v4l2_mlc1_rgb, nxp_v4l2_mlc1_video, width, height);
                return new HDMIUseMirrorAndVideoImpl(mirrorImpl, glAndVideoImpl);
            }

            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO_RESC:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO_RESC:
            {
                HDMICommonImpl *mirrorImpl = new HDMIUseOnlyMirrorImpl(nxp_v4l2_mlc1_rgb, srcWidth, srcHeight, srcWidth, srcHeight);
                HDMICommonImpl *glAndVideoImpl = new HDMIUseGLAndVideoImpl(nxp_v4l2_mlc1_rgb, nxp_v4l2_mlc1_video, width, height);
                HDMICommonImpl *impl = new HDMIUseMirrorAndVideoImpl(mirrorImpl, glAndVideoImpl);
                RescConfigure *rescConfigure = new RescConfigure();
                return new HDMIUseRescCommonImpl(nxp_v4l2_mlc1_rgb,
                        nxp_v4l2_mlc1_video,
                        width,
                        height,
                        srcWidth,
                        srcHeight,
                        scaleFactor,
                        impl,
                        rescConfigure);
            }

            default:
                return NULL;
        }
    } else if (dispType == DISPLAY_HDMI_ALTERNATE) {
        switch (usageScenario) {
            case LCD_USE_ONLY_GL_HDMI_USE_ONLY_MIRROR:
                return new HDMIUseOnlyGLImpl(nxp_v4l2_mlc1_rgb);

            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_RESC:
            {
                HDMICommonImpl *impl = new HDMIUseOnlyGLImpl(nxp_v4l2_mlc1_rgb, width, height);
                RescConfigure *rescConfigure = new RescConfigure();
                return new HDMIUseRescCommonImpl(nxp_v4l2_mlc1_rgb,
                        nxp_v4l2_mlc1_video,
                        width,
                        height,
                        width,
                        height,
                        scaleFactor,
                        impl,
                        rescConfigure);
            }

            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO:
                return new HDMIUseGLAndVideoImpl(nxp_v4l2_mlc1_rgb, nxp_v4l2_mlc1_video, width, height);

            case LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO_RESC:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO_RESC:
            {
                HDMICommonImpl *impl = new HDMIUseGLAndVideoImpl(nxp_v4l2_mlc1_rgb, nxp_v4l2_mlc1_video, width, height);
                RescConfigure *rescConfigure = new RescConfigure();
                return new HDMIUseRescCommonImpl(nxp_v4l2_mlc1_rgb,
                        nxp_v4l2_mlc1_video,
                        width,
                        height,
                        width,
                        height,
                        scaleFactor,
                        impl,
                        rescConfigure);
            }

            case LCD_USE_ONLY_GL_HDMI_USE_ONLY_GL:
            case LCD_USE_ONLY_GL_HDMI_USE_GL_RESC:
            case LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO:
            case LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO_RESC:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_ONLY_GL:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_RESC:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO:
            case LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO_RESC:
            default:
                return NULL;
        }
    } else {
        ALOGE("invalid dispType: %d", dispType);
        return NULL;
    }
}

android::HWCImpl *HWCreator::createByPreset(
        uint32_t dispType,
        uint32_t usageScenario,
        int32_t  preset,
        int32_t  srcWidth,
        int32_t  srcHeight,
        int32_t  scaleFactor)
{
    return NULL;
}
