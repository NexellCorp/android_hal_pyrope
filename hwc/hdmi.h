#ifndef _NX_HDMI_H
#define _NX_HDMI_H

#include <nxp-v4l2.h>
#include <NXMessageThread.h>

namespace android {

struct NXHdmiMessage
{
    enum {
        HDMI_PLAY,   // e1 rgb handle, e2 video handle
        HDMI_STOP_RGB,
        HDMI_EXIT = 1000,
    } MsgType;

    NXHdmiMessage() {
    }
    NXHdmiMessage(uint32_t t, uint32_t e1, uint32_t e2)
        : type(t), ext1(e1), ext2(e2)
    {
    }
#if 0
    NXHdmiMessage(const NXHdmiMessage &obj) {
        type = obj.type;
        ext = obj.ext;
    }
#endif
    uint32_t type;
    uint32_t ext1;
    uint32_t ext2;
};

class NXHdmiThread: public NXMessageThread<NXHdmiMessage>
{
public:
    NXHdmiThread() {
    }
    NXHdmiThread(int rgbMaxBufferCount, int videoMaxBufferCount);
    virtual ~NXHdmiThread();

private:
    virtual bool processMessage(const NXHdmiMessage &msg);
    void vsyncMonEnable(bool enable);
    void waitVsync(void);

private:
    // handlers
    bool handlePlay(uint32_t msgVal1, uint32_t msgVal2);
    bool handleStartVideo(uint32_t msgVal);
    bool handlePlayVideo(uint32_t msgVal);
    bool handleStopVideo();
    bool handleStartRgb(uint32_t msgVal);
    bool handlePlayRgb(uint32_t msgVal);
    bool handleStopRgb();

private:
    bool RgbOn;
    bool VideoOn;

    int RgbMaxBufferCount;
    int RgbOutIndex;
    int RgbOutQCount;
    int VideoMaxBufferCount;
    int VideoOutIndex;
    int VideoOutQCount;

    // vsync
    int VsyncCtlFd;
    int VsyncMonFd;
};

}; // namespace

#endif
