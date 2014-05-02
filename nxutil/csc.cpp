#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <utils/Log.h>

#include "csc.h"

int cscYV12ToNV21(char *srcY, char *srcCb, char *srcCr,
                  char *dstY, char *dstCrCb,
                  uint32_t srcStride, uint32_t dstStride,
                  uint32_t width, uint32_t height)
{
    uint32_t i, j;
    char *psrc = srcY;
    char *pdst = dstY;
    char *psrcCb = srcCb;
    char *psrcCr = srcCr;

    ALOGD("srcY %p, srcCb %p, srcCr %p, dstY %p, dstCrCb %p, srcStride %d, dstStride %d, width %d, height %d",
            srcY, srcCb, srcCr, dstY, dstCrCb, srcStride, dstStride, width, height);
    // Y
#if 1
    for (i = 0; i < height; i++) {
        memcpy(pdst, psrc, width);
        psrc += srcStride;
        pdst += dstStride;
    }
#else
    memcpy(pdst, psrc, width * height);
#endif

    // CrCb
    pdst = dstCrCb;
    for (i = 0; i < (height >> 1); i++) {
        for (j = 0; j < (width >> 1); j++) {
            pdst[j << 1] = psrcCr[j];
            pdst[(j << 1)+ 1] = psrcCb[j];
        }
        psrcCb += srcStride >> 1;
        psrcCr += srcStride >> 1;
        pdst   += dstStride;
    }

    return 0;
}

extern "C" {
extern void csc_ARGB8888_to_NV12_NEON(unsigned char *dstY, unsigned char *dstCbCr, unsigned char *src, unsigned int width, unsigned int height);
extern void csc_ARGB8888_to_NV21_NEON(unsigned char *dstY, unsigned char *dstCbCr, unsigned char *src, unsigned int width, unsigned int height);
}

int cscARGBToNV21(char *src, char *dstY, char *dstCbCr, uint32_t srcWidth, uint32_t srcHeight, uint32_t cbFirst)
{
    if( cbFirst )
    {
        csc_ARGB8888_to_NV12_NEON((unsigned char *)dstY, (unsigned char *)dstCbCr, (unsigned char *)src, srcWidth, srcHeight);
    }
    else
    {
        csc_ARGB8888_to_NV21_NEON((unsigned char *)dstY, (unsigned char *)dstCbCr, (unsigned char *)src, srcWidth, srcHeight);
    }
    return 0;
}

//  Copy Virtual Address Space to H/W Addreadd Space
int cscYV12ToYV12(  char *srcY, char *srcU, char *srcV,
                    char *dstY, char *dstU, char *dstV,
                    uint32_t srcStride, uint32_t dstStrideY, uint32_t dstStrideUV,
                    uint32_t width, uint32_t height )
{
    uint32_t i, j;
    char *pSrc = srcY;
    char *pDst = dstY;
    char *pSrc2 = srcV;
    char *pDst2 = dstV;
    //  Copy Y
    if( srcStride == dstStrideY )
    {
        memcpy( dstY, srcY, srcStride*height );
    }
    else
    {
        for( i=0 ; i<height; i++ )
        {
            memcpy(pDst, pSrc, width);
            pSrc += srcStride;
            pDst += dstStrideY;
        }
    }
    //  Copy UV
    pSrc = srcU;
    pDst = dstU;
    height /= 2;
    width /= 2;
    for( i=0 ; i<height ; i++ )
    {
        memcpy( pDst , pSrc , width );
        memcpy( pDst2, pSrc2, width );
        pSrc += srcStride/2;
        pDst += dstStrideUV;
        pSrc2 += srcStride/2;
        pDst2 += dstStrideUV;
    }
    return 0;
}
