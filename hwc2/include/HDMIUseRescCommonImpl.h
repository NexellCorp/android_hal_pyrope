#ifndef _HDMIUSERESCCOMMONIMPL_H
#define _HDMIUSERESCCOMMONIMPL_H

class HDMICommonImpl;
class RescConfigure;

namespace android {

class HDMIUseRescCommonImpl: public HDMICommonImpl
{
public:
    HDMIUseRescCommonImpl(
            int rgbID,
            int videoID,
            int width,
            int height,
            int srcWidth,
            int srcHeight,
            int scaleFactor,
            HDMICommonImpl *impl,
            RescConfigure *rescConfigure);
    virtual ~HDMIUseRescCommonImpl();

    virtual int enable();
    virtual int disable();
    virtual int prepare(hwc_display_contents_1_t *);
    virtual int set(hwc_display_contents_1_t *, void *);
    virtual private_handle_t const *getRgbHandle();
    virtual private_handle_t const *getVideoHandle();
    virtual int render();

private:
    int mScaleFactor;
    HWCImpl *mImpl;
    RescConfigure *mRescConfigure;
    bool mRescConfigured;
};

}; // namespace

#endif
