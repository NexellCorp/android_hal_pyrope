#ifndef _LCDUSEONLYGLIMPL_H
#define _LCDUSEONLYGLIMPL_H

class LCDCommonImpl;

namespace android {

class LCDUseOnlyGLImpl: public LCDCommonImpl
{
public:
    LCDUseOnlyGLImpl(int rgbID);
    LCDUseOnlyGLImpl(int rgbID, int width, int height);
    virtual ~LCDUseOnlyGLImpl();

    virtual int prepare(hwc_display_contents_1_t *);
    virtual int set(hwc_display_contents_1_t *, void *);
    virtual private_handle_t const *getRgbHandle();
    virtual private_handle_t const *getVideoHandle();
    virtual int render();

protected:
    virtual void init();

private:
    HWCRenderer *mRGBRenderer;
    int mRGBLayerIndex;
    private_handle_t const *mRGBHandle;
};

}; // namespace
#endif
