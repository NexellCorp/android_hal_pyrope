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
#if 0
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
