#define LOG_TAG "NXCsc"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

// for format
#include <system/graphics.h>

#include <utils/Log.h>

#include <nexell_format.h>

#include "csc.h"
#include <NXCsc.h>

bool nxCsc(const struct nxp_vid_buffer *srcBuf, uint32_t srcFormat,
           const struct nxp_vid_buffer *dstBuf, uint32_t dstFormat,
           uint32_t width, uint32_t height)
{
    return false;
}

// test signature
// uint8_t sig1[10] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x10};
// uint8_t sig2[10] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0};
// uint8_t sig3[10] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44};
// static int curSigNum = 0;
bool nxCsc(const struct nxp_vid_buffer *srcBuf, uint32_t srcFormat,
           private_handle_t const *dstHandle,
           uint32_t width, uint32_t height)
{
    switch (srcFormat) {
    case HAL_PIXEL_FORMAT_YV12:
        switch (dstHandle->format) {
        case HAL_PIXEL_FORMAT_YCrCb_420_SP: // NV21
            {
                size_t size = width * (height + height/2);
                char *dstY = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, dstHandle->share_fd, 0);
                if (MAP_FAILED == dstY) {
                    ALOGE("mmap failed for dst");
                    return false;
                }
                char *dstCrCb = dstY + (width * height);
                cscYV12ToNV21(srcBuf->virt[0], srcBuf->virt[1], srcBuf->virt[2],
                        dstY, dstCrCb,
                        srcBuf->stride[0], width,
                        width, height);

                // if (curSigNum == 0) {
                //     memcpy(dstY, sig1, 10);
                // } else if (curSigNum == 1) {
                //     memcpy(dstY, sig2, 10);
                // } else {
                //     memcpy(dstY, sig3, 10);
                // }
                // ALOGD("%s: signature %d, phys 0x%x", __func__, curSigNum, dstHandle->phys);
                // curSigNum++;
                // curSigNum %= 3;

                munmap(dstY, size);
            }
            return true;
        }
        break;
    }
    return false;
}

bool nxCsc(private_handle_t const *srcHandle,
           const struct nxp_vid_buffer *dstBuf,
           uint32_t width, uint32_t height)
{
    return true;
}

bool nxCsc(private_handle_t const *srcHandle,
           private_handle_t const *dstHandle,
           int width, int height)
{
    switch (srcHandle->format) {
    case HAL_PIXEL_FORMAT_YV12:
        switch (dstHandle->format) {
        case HAL_PIXEL_FORMAT_YCrCb_420_SP: // NV21
            {
                char *srcY = (char *)mmap(NULL, srcHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, srcHandle->share_fd, 0);
                if (MAP_FAILED == srcY) {
                    ALOGE("mmap failed for srcY");
                    return false;
                }
                char *srcCb = srcY + (srcHandle->stride * ALIGN(srcHandle->height, 16));
                char *srcCr = srcCb + (ALIGN(srcHandle->stride >> 1, 16) * ALIGN(srcHandle->height >> 1, 16));
                char *dstY = (char *)mmap(NULL, dstHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, dstHandle->share_fd, 0);
                if (MAP_FAILED == dstY) {
                    ALOGE("mmap failed for dstY");
                    munmap(srcY, srcHandle->size);
                    return false;
                }
                char *dstCrCb = dstY + (dstHandle->stride * ALIGN(dstHandle->height, 16));
                cscYV12ToNV21(srcY, srcCb, srcCr,
                        dstY, dstCrCb,
                        width, width,
                        width, height);

                munmap(dstY, dstHandle->size);
                munmap(srcY, srcHandle->size);
            }
            return true;
        }
        break;
    }
    return false;
}
