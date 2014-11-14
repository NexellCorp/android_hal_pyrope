#ifndef _HDMIUSEONLYGLIMPL_H
#define _HDMIUSEONLYGLIMPL_H

class HDMICommonImpl;
struct hwc_layer_1;

namespace android {

class HDMIUseOnlyGLImpl: public HDMICommonImpl
{
public:
    HDMIUseOnlyGLImpl(int rgbID);
    HDMIUseOnlyGLImpl(int rgbID, int width, int height);
    virtual ~HDMIUseOnlyGLImpl();

    virtual int enable();
    virtual int disable();
    virtual int prepare(hwc_display_contents_1_t *);
    virtual int set(hwc_display_contents_1_t *, void *);
    virtual private_handle_t const *getRgbHandle();
    virtual private_handle_t const *getVideoHandle();
    virtual int render();

protected:
    virtual void init();

private:
    int mRGBLayerIndex;
    HWCRenderer *mRGBRenderer;
    private_handle_t const *mRGBHandle;
};

}; // namespace
#endif
