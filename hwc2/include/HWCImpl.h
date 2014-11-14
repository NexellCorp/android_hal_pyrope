#ifndef _HWCIMPL_H
#define _HWCIMPL_H

struct private_handle_t;
struct hwc_display_contents_1;

namespace android {

class HWCImpl
{
public:
    HWCImpl() {
    }
    HWCImpl(int rgbID, int videoID)
        :mRgbID(rgbID),
        mVideoID(videoID),
        mWidth(0),
        mHeight(0),
        mSrcWidth(0),
        mSrcHeight(0),
        mEnabled(false)
    {
    }
    HWCImpl(int rgbID, int videoID, int width, int height, int srcWidth = 0, int srcHeight = 0)
        :mRgbID(rgbID),
        mVideoID(videoID),
        mWidth(width),
        mHeight(height),
        mSrcWidth(srcWidth),
        mSrcHeight(srcHeight),
        mEnabled(false)
    {
    }

    virtual ~HWCImpl() {
    }

    virtual int prepare(hwc_display_contents_1_t *) = 0;
    virtual int set(hwc_display_contents_1_t *, void *) = 0;
    virtual private_handle_t const *getRgbHandle() = 0;
    virtual private_handle_t const *getVideoHandle() = 0;
    virtual int render() = 0;
    virtual int enable() = 0;
    virtual int disable() = 0;

    int getWidth() const {
        return mWidth;
    }

    int getHeight() const {
        return mHeight;
    }

    void setWidth(int width) {
        mWidth = width;
    }

    void setHeight(int height) {
        mHeight = height;
    }

    bool getEnabled() const {
        return mEnabled;
    }

    void setSrcWidth(int width) {
        mSrcWidth = width;
    }

    int getSrcWidth() const {
        return mSrcWidth;
    }

    void setSrcHeight(int height) {
        mSrcHeight = height;
    }

    int getSrcHeight() const {
        return mSrcHeight;
    }

protected:
    virtual bool canOverlay(struct hwc_layer_1 &layer) {
        if (layer.transform != 0) {
            ALOGV("transform: 0x%x", layer.transform);
            return false;
        }

        if (layer.blending != HWC_BLENDING_NONE) {
            ALOGV("blending: 0x%x", layer.blending);
            return false;
        }

        private_handle_t const *hnd = reinterpret_cast<private_handle_t const *>(layer.handle);
        if (hnd) {
            int format = hnd->format;
            return  (format == HAL_PIXEL_FORMAT_YCbCr_422_SP) ||
                    (format == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
                    (format == HAL_PIXEL_FORMAT_YCbCr_422_I)  ||
                    (format == HAL_PIXEL_FORMAT_YV12);
        }

        // if (hnd)
        //     ALOGD("format: 0x%x", hnd->format);

        return false;
    }

protected:
    int mRgbID;
    int mVideoID;
    int mWidth;
    int mHeight;
    int mSrcWidth;
    int mSrcHeight;
    bool mEnabled;
};

}; // namespace

#endif
