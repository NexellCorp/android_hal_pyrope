#ifndef _HDMIUSEGLANDVIDEOIMPL_H
#define _HDMIUSEGLANDVIDEOIMPL_H

class HDMICommonImpl;
struct hwc_layer_1;

namespace android {

class HDMIUseGLAndVideoImpl: public HDMICommonImpl
{
public:
    HDMIUseGLAndVideoImpl(int rgbID, int videoID);
    HDMIUseGLAndVideoImpl(int rgbID, int videoID, int width, int height);
    virtual ~HDMIUseGLAndVideoImpl();

    virtual int enable();
    virtual int disable();
    virtual int prepare(hwc_display_contents_1_t *);
    virtual int set(hwc_display_contents_1_t *, void *);
    virtual private_handle_t const *getRgbHandle();
    virtual private_handle_t const *getVideoHandle();
    virtual int render();

protected:
    virtual void init();
    bool checkVideoConfigChanged();

private:
    int  mRGBLayerIndex;
    int  mVideoLayerIndex;
    HWCRenderer *mRGBRenderer;
    HWCRenderer *mVideoRenderer;
    private_handle_t const *mRGBHandle;
    private_handle_t const *mVideoHandle;
    struct hwc_layer_1 *mVideoLayer;
};

}; // namespace
#endif
