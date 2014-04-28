#ifndef _CSC_H
#define _CSC_H

#ifdef __cplusplus
extern "C"{
#endif


int cscYV12ToNV21(char *srcY, char *srcCb, char *srcCr,
                  char *dstY, char *dstCbCr,
                  uint32_t srcStride, uint32_t dstStride,
                  uint32_t width, uint32_t height);

int cscARGBToNV21(char *src, char *dstY, char *dstCbCr, uint32_t srcWidth, uint32_t srcHeight, uint32_t cbFirst);

#ifdef __cplusplus
}
#endif

#endif
