#ifndef _HDMIUSEONLYMIRRORRESCIMPL_H
#define _HDMIUSEONLYMIRRORRESCIMPL_H

class HDMIUseOnlyMirrorImpl;

namespace android {

class HDMIUseOnlyMirrorRescImpl: public HDMIUseOnlyMirrorImpl
{
public:
    HDMIUseOnlyMirrorRescImpl(int rgbID);
    HDMIUseOnlyMirrorRescImpl(int rgbID, int width, int height, int srcWidth, int srcHeight, int scaleFactor);
    virtual ~HDMIUseOnlyMirrorRescImpl() {
    }

    virtual int enable();
    virtual int disable();
    // virtual int prepare(hwc_display_contents_1_t *);
    // virtual int set(hwc_display_contents_1_t *, void *);
    // virtual private_handle_t const *getRgbHandle();
    // virtual private_handle_t const *getVideoHandle();
    // virtual int render();

protected:
    int configResc(int srcWidth, int srcHeight, int dstWidth, int dstHeight, uint32_t format);
    virtual int config();
};

}; // namespace

#endif
