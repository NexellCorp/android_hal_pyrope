#ifndef _CSC_H
#define _CSC_H

int cscYV12ToNV21(char *srcY, char *srcCb, char *srcCr,
                  char *dstY, char *dstCbCr,
                  uint32_t srcStride, uint32_t dstStride,
                  uint32_t width, uint32_t height);

int cscARGBToNV21(char *src, char *dstY, char *dstCbCr, uint32_t srcWidth, uint32_t srcHeight);

#endif
