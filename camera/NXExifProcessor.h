#ifndef _NX_EXIF_PROCESSOR_H
#define _NX_EXIF_PROCESSOR_H

#include <nxp-v4l2.h>
#include <gralloc_priv.h>
#include "Exif.h"
#include "NXZoomController.h"

namespace android {

class NXExifProcessor
{
public:
    // inner class for result
    class ExifResult {
        public:
            ExifResult(unsigned char *result, uint32_t size)
                : Result(result),
                  Size(size) {
            }
            ExifResult(const ExifResult& rhs)
                : Result(rhs.Result),
                  Size(rhs.Size) {
            }
            const unsigned char *getResult() const {
                return Result;
            }
            uint32_t getSize() const {
                return Size;
            }
        private:
            const unsigned char *Result;
            uint32_t Size;
    };

    NXExifProcessor()
        : ZoomController(NULL),
          Exif(NULL),
          Width(0),
          Height(0),
          SrcBuffer(NULL),
          SrcHandle(NULL),
          DstHandle(NULL),
          ScaleBuffer(NULL),
          ThumbnailBuffer(NULL),
          OutBuffer(NULL),
          ThumbnailJpegSize(0),
          OutSize(0)
    {
    }
    virtual ~NXExifProcessor() {
    }

    virtual ExifResult makeExif(
            uint32_t width,
            uint32_t height,
            const struct nxp_vid_buffer *srcBuffer,
            exif_attribute_t *exif,
            private_handle_t const *dstHandle = NULL);
    virtual ExifResult makeExif(
            uint32_t width,
            uint32_t height,
            private_handle_t const *srcHandle,
            exif_attribute_t *exif,
            private_handle_t const *dstHandle = NULL);

    virtual void clear() {
        freeOutBuffer();
        Width = Height = 0;
        SrcBuffer = NULL;
        SrcHandle = NULL;
        DstHandle = NULL;
        OutSize = 0;
        ThumbnailJpegSize = 0;
    }

private:
    NXZoomController *ZoomController;

    exif_attribute_t *Exif;
    uint32_t Width;
    uint32_t Height;
    const struct nxp_vid_buffer *SrcBuffer;
    private_handle_t const *SrcHandle;
    private_handle_t const *DstHandle;

    struct nxp_vid_buffer *ScaleBuffer;
    unsigned char *ThumbnailBuffer;
    unsigned char *OutBuffer;

    uint32_t ThumbnailJpegSize;
    uint32_t OutSize;

private:
    virtual ExifResult makeExif();
    virtual bool preprocessExif();
    virtual bool allocScaleBuffer();
    virtual void freeScaleBuffer();
    virtual bool allocThumbnailBuffer();
    virtual void freeThumbnailBuffer();
    virtual bool scaleDown();
    virtual bool encodeThumb();
    virtual bool allocOutBuffer();
    virtual void freeOutBuffer();
    virtual bool processExif();
    virtual bool postprocessExif();

    // common inline functions
    ExifResult errorOut() {
        freeThumbnailBuffer();
        freeScaleBuffer();
        freeOutBuffer();
        return ExifResult(NULL, 0);
    }

    void writeExifIfd(unsigned char *&pCur, unsigned short tag, unsigned short type, unsigned int count, unsigned int value) {
        memcpy(pCur, &tag, 2);
        pCur += 2;
        memcpy(pCur, &type, 2);
        pCur += 2;
        memcpy(pCur, &count, 4);
        pCur += 4;
        memcpy(pCur, &value, 4);
        pCur += 4;
    }

    void writeExifIfd(unsigned char *&pCur, unsigned short tag, unsigned short type, unsigned int count, unsigned char *pValue) {
        char buf[4] = {0, };

        if (count > 4)
            return;

        memcpy(buf, pValue, count);
        memcpy(pCur, &tag, 2);
        pCur += 2;
        memcpy(pCur, &type, 2);
        pCur += 2;
        memcpy(pCur, &count, 4);
        pCur += 4;
        memcpy(pCur, buf, 4);
        pCur += 4;
    }

    void writeExifIfd(unsigned char *&pCur, unsigned short tag, unsigned short type, unsigned int count, unsigned char *pValue, unsigned int &offset, unsigned char *start) {
        memcpy(pCur, &tag, 2);
        pCur += 2;
        memcpy(pCur, &type, 2);
        pCur += 2;
        memcpy(pCur, &count, 4);
        pCur += 4;
        memcpy(pCur, &offset, 4);
        pCur += 4;
        memcpy(start + offset, pValue, count);
        offset += count;
    }

    void writeExifIfd(unsigned char *&pCur, unsigned short tag, unsigned short type, unsigned int count, rational_t *pValue, unsigned int &offset, unsigned char *start) {
        memcpy(pCur, &tag, 2);
        pCur += 2;
        memcpy(pCur, &type, 2);
        pCur += 2;
        memcpy(pCur, &count, 4);
        pCur += 4;
        memcpy(pCur, &offset, 4);
        pCur += 4;
        memcpy(start + offset, pValue, 8 * count);
        offset += 8 * count;
    }
};

}; // namespace

#endif
