#ifndef _HDMIUSEMIRRORANDVIDEOIMPL_H
#define _HDMIUSEMIRRORANDVIDEOIMPL_H

class HDMICommonImpl;
struct hwc_layer_1;

namespace android {

class HDMIUseMirrorAndVideoImpl: public HDMICommonImpl
{
public:
    HDMIUseMirrorAndVideoImpl(HDMICommonImpl *mirrorImpl, HDMICommonImpl *glAndVideoImpl);
    virtual ~HDMIUseMirrorAndVideoImpl();

    virtual int enable();
    virtual int disable();
    virtual int prepare(hwc_display_contents_1_t *);
    virtual int set(hwc_display_contents_1_t *, void *);
    virtual private_handle_t const *getRgbHandle();
    virtual private_handle_t const *getVideoHandle();
    virtual int render();

private:
    bool hasVideoLayer(hwc_display_contents_1_t *);

private:
    HDMICommonImpl *mMirrorImpl;
    HDMICommonImpl *mGLAndVideoImpl;
    bool mUseMirror;
};

}; // namespace
#endif
