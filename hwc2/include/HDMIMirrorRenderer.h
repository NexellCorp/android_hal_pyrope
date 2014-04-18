#ifndef _HDMIMIRRORRENDERER_H
#define _HDMIMIRRORRENDERER_H

struct private_handle_t;
class HWCRenderer;

namespace android {

#define MAX_MIRROR_HANDLE_COUNT     4

class HDMIMirrorRenderer: public HWCRenderer
{
public:
    HDMIMirrorRenderer(int id, int maxBufferCount = 3);
    virtual ~HDMIMirrorRenderer();

    virtual int setHandle(private_handle_t const *handle);
    virtual private_handle_t const *getHandle();
    virtual int stop();
    virtual int render();

private:
    int mFBFd;
    int mFBSize;

    int mMaxBufferCount;
    int mOutCount;
    int mOutIndex;
    int mMirrorIndex;

    bool mStarted;

    private_handle_t *mMirrorHandleArray[MAX_MIRROR_HANDLE_COUNT];
    private_handle_t *mHandle;
};

}; // namespace

#endif
