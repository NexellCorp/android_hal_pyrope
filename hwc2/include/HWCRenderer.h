#ifndef _HWCRENDERER_H
#define _HWCRENDERER_H

struct private_handle_t;

namespace android {

class HWCRenderer
{
public:
    HWCRenderer(int id): mID(id), mHandle(NULL) {
    }
    virtual ~HWCRenderer() {
    }

    virtual int setHandle(private_handle_t const *handle) {
        mHandle = handle;
        return 0;
    }

    virtual private_handle_t const *getHandle() {
        return mHandle;
    }

    virtual int render() = 0;

    virtual int stop() {
        return 0;
    }

protected:
    int mID;
    private_handle_t const *mHandle;
};

}; // namespace
#endif
