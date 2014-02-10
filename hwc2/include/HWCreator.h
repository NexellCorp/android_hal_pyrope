#ifndef _HWCREATOR_H
#define _HWCREATOR_H

class HWCImpl;

namespace android {

class HWCreator {
public:
    enum DISPLAY_TYPE {
        DISPLAY_LCD = 0,
        DISPLAY_HDMI,
        DISPLAY_HDMI_ALTERNATE,
        DISPLAY_MAX
    };

    enum USAGE_SCENARIO {
        LCD_USE_ONLY_GL_HDMI_USE_ONLY_MIRROR                    = 0,
        LCD_USE_ONLY_GL_HDMI_USE_MIRROR_RESC                    = 1,
        LCD_USE_ONLY_GL_HDMI_USE_ONLY_GL                        = 2,
        LCD_USE_ONLY_GL_HDMI_USE_GL_RESC                        = 3,
        LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO                   = 4,
        LCD_USE_ONLY_GL_HDMI_USE_GL_AND_VIDEO_RESC              = 5,
        LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO               = 6,
        LCD_USE_ONLY_GL_HDMI_USE_MIRROR_AND_VIDEO_RESC          = 7,
        LCD_USE_GL_AND_VIDEO_HDMI_USE_ONLY_GL                   = 8,
        LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_RESC                   = 9,
        LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO              = 10,
        LCD_USE_GL_AND_VIDEO_HDMI_USE_GL_AND_VIDEO_RESC         = 11,
        LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO          = 12,
        LCD_USE_GL_AND_VIDEO_HDMI_USE_MIRROR_AND_VIDEO_RESC     = 13,
        USAGE_SCENARIO_MAX
    };

public:
    static android::HWCImpl *create(
            uint32_t dispType,
            uint32_t usageScenario,
            int32_t  width = 0,
            int32_t  height = 0,
            int32_t  srcWidth = 0,
            int32_t  srcHeight = 0,
            int32_t  scaleFactor = 1
            );
    static android::HWCImpl *createByPreset(
            uint32_t dispType,
            uint32_t usageScenario,
            int32_t  preset,
            int32_t  srcWidth = 0,
            int32_t  srcHeight = 0,
            int32_t  scaleFactor = 1
            );
};

}; // namespace
#endif
