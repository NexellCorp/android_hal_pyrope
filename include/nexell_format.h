#ifndef _NEXELL_FORMAT_H
#define _NEXELL_FORMAT_H

enum {
    HAL_PIXEL_FORMAT_NEXELL_YV12            = 0x100, // Y,Cb,Cr 420 format, 3 plane separated
    HAL_PIXEL_FORMAT_NEXELL_YCbCr_422_SP    = 0x101, // Y,CbCr 422 format, 2 plane separated, NV16
    HAL_PIXEL_FORMAT_NEXELL_YCrCb_420_SP    = 0x102, // Y,CrCb 420 format, 2 plane separated, NV21
    HAL_PIXEL_FORMAT_CUSTOM_MAX
};

#define ALIGN(x, a)       (((x) + (a) - 1) & ~((a) - 1))

#endif
