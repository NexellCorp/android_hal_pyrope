#ifndef _HWCCOMMONRENDERER_H
#define _HWCCOMMONRENDERER_H

struct private_handle_t;
class HWCRenderer;

namespace android {

class HWCCommonRenderer: public HWCRenderer
{
public:
    HWCCommonRenderer(int id, int maxBufferCount, int planeNum = 3);
    virtual ~HWCCommonRenderer();

    virtual int render(int *fenceFd = NULL);
    virtual int stop();

private:
    int mMaxBufferCount;
    int mOutCount;
    int mOutIndex;
    int mPlaneNum;

    bool mStarted;
};

}; // namespace

#endif
