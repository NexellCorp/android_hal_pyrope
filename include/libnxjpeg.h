#ifndef __LIBNXJPEG_H__
#define __LIBNXJPEG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <jpeglib.h>
#include <jerror.h>

typedef enum {
	NX_PIXFORMAT_YUV420 = 0,
	NX_PIXFORMAT_YUYV,
	NX_PIXFORMAT_RGB24,
	NX_PIXFORMAT_RGB32,
	NX_PIXFORMAT_RGB555,
	NX_PIXFORMAT_RGB565,
	NX_PIXFORMAT_GREY,
	NX_PIXFORMAT_MJPEG,
	NX_PIXFORMAT_JPEG,
	NX_PIXFORMAT_MPEG,
	NX_PIXFORMAT_HI240,
	NX_PIXFORMAT_YUVY,
	NX_PIXFORMAT_YUV422P,
	NX_PIXFORMAT_YUV411P,
	NX_PIXFORMAT_YUV410P,
	NX_PIXFORMAT_RGB332,
	NX_PIXFORMAT_RGB444,
	NX_PIXFORMAT_RGB555X,
	NX_PIXFORMAT_RGB565X,
	NX_PIXFORMAT_BGR24,
	NX_PIXFORMAT_BGR32,
	NX_PIXFORMAT_Y16,
	NX_PIXFORMAT_PAL8,
	NX_PIXFORMAT_YVU410,
	NX_PIXFORMAT_YVU420,
	NX_PIXFORMAT_Y41P,
	NX_PIXFORMAT_YUV444,
	NX_PIXFORMAT_YUV555,
	NX_PIXFORMAT_YUV565,
	NX_PIXFORMAT_YUV32,
	NX_PIXFORMAT_NV12,
	NX_PIXFORMAT_NV21,
	NX_PIXFORMAT_YYUV,
	NX_PIXFORMAT_HM12,
	NX_PIXFORMAT_SBGGR8,
	NX_PIXFORMAT_SGBRG8,
	NX_PIXFORMAT_SGRBG8,
	NX_PIXFORMAT_SRGGB8,
	NX_PIXFORMAT_SBGGR16,
	NX_PIXFORMAT_SN9C10X,
	NX_PIXFORMAT_SN9C20X_I420,
	NX_PIXFORMAT_PWC1,
	NX_PIXFORMAT_PWC2,
	NX_PIXFORMAT_ET61X251,
	NX_PIXFORMAT_SPCA501,
	NX_PIXFORMAT_SPCA505,
	NX_PIXFORMAT_SPCA508,
	NX_PIXFORMAT_SPCA561,
	NX_PIXFORMAT_PAC207,
	NX_PIXFORMAT_PJPG,
	NX_PIXFORMAT_YVYU,
	NX_PIXFORMAT_UYVY,
	NX_PIXFORMAT_MR97310A,
	NX_PIXFORMAT_SQ905C,
	NX_PIXFORMAT_YUV420P, // deprecated, use YUV420
	NX_PIXFORMAT_YUV422,	// deprecated, use YUYV
	NX_PIXFORMAT_YUV411,  // deprecated, use YUV411P
	NX_PIXFORMAT_MAX
} NXPixFormat;

/* functions */
int NX_EncodingYUV420P(unsigned char *destImage,
        int imageSize,
        const unsigned char *inputYImage,
        const unsigned char *inputCbImage,
        const unsigned char *inputCrImage,
        int width,
        int height,
        int quality);

int NX_JpegEncoding(unsigned char *destImage,
        int imageSize,
        const unsigned char *inputImage,
        int width,
        int height,
        int quality,
        NXPixFormat pixFormat);


#ifdef __cplusplus
}
#endif

#endif