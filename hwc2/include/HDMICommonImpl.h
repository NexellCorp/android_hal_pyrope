#ifndef _HDMICOMMONIMPL_H
#define _HDMICOMMONIMPL_H

class HWCImpl;
struct hwc_layer_1;

namespace android {

class HDMICommonImpl: public HWCImpl
{
public:
    HDMICommonImpl() {
    }
    HDMICommonImpl(int rgbID, int videoID);
    HDMICommonImpl(int rgbID, int videID, int width, int height, int srcWidth, int srcHeight);

    virtual ~HDMICommonImpl() {
    }

    virtual int disable() {
        mRgbConfigured = false;
        mVideoConfigured = false;
        mHDMIConfigured = false;
        return 0;
    }

protected:
    virtual void init();

    int configRgb(struct hwc_layer_1 &layer);
    int configVideo(struct hwc_layer_1 &layer, const private_handle_t *h = NULL);
    int configHDMI(int width, int height);
    int configHDMI(uint32_t preset);
    int configVideoCrop(struct hwc_layer_1 &layer);

    void unConfigRgb() {
        mRgbConfigured = false;
    }
    void unConfigVideo() {
        mVideoConfigured = false;
    }
    void unConfigHDMI() {
        mHDMIConfigured = false;
    }

protected:
    int  mSubID;
    bool mRgbConfigured;
    bool mVideoConfigured;
    bool mHDMIConfigured;

    // video crop
    int mVideoLeft;
    int mVideoTop;
    int mVideoRight;
    int mVideoBottom;
};
}; // namespace

#endif
