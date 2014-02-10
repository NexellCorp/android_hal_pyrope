#ifndef _HDMIUSEONLYMIRRORIMPL_H
#define _HDMIUSEONLYMIRRORIMPL_H

class HDMICommonImpl;

namespace android {

class HDMIUseOnlyMirrorImpl: public HDMICommonImpl
{
public:
    HDMIUseOnlyMirrorImpl(int rgbID);
    HDMIUseOnlyMirrorImpl(int rgbID, int width, int height, int srcWidth = 0, int srcHeight = 0);
    virtual ~HDMIUseOnlyMirrorImpl();

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
    int config();

private:
    bool mConfigured;
    HWCRenderer *mMirrorRenderer;
};

}; // namespace
#endif
