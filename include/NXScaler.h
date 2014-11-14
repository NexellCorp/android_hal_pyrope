#ifndef _NX_SCALER_H
#define _NX_SCALER_H

#include <stdio.h>
#include <sys/types.h>

#include <nxp-v4l2.h>
#include <mach/nxp-scaler.h>
#include <gralloc_priv.h>

struct scale_ctx {
    uint32_t left;
    uint32_t top;
    uint32_t src_width;
    uint32_t src_height;
    uint32_t src_code;
    uint32_t dst_width;
    uint32_t dst_height;
    uint32_t dst_code;
};

int nxScalerRun(const struct nxp_vid_buffer *srcBuf, const struct nxp_vid_buffer *dstBuf, const struct scale_ctx *ctx);
int nxScalerRun(const struct nxp_vid_buffer *srcBuf, private_handle_t const *dstHandle, const struct scale_ctx *ctx);
int nxScalerRun(private_handle_t const *srcHandle, private_handle_t const *dstHandle, const struct scale_ctx *ctx);
int nxScalerRun(private_handle_t const *srcHandle, const struct nxp_vid_buffer *dstBuf, const struct scale_ctx *ctx);

int getPhysForHandle(private_handle_t const *handle, unsigned long *phys);
int getVirtForHandle(private_handle_t const *handle, unsigned long *virt);
void releaseVirtForHandle(private_handle_t const *handle, unsigned long virt);

#endif
