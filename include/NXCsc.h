#ifndef _NX_CSC_H
#define _NX_CSC_H

#include <nxp-v4l2.h>
#include <gralloc_priv.h>

bool nxCsc(const struct nxp_vid_buffer *srcBuf, uint32_t srcFormat,
           const struct nxp_vid_buffer *dstBuf, uint32_t dstFormat,
           uint32_t width, uint32_t height);
bool nxCsc(const struct nxp_vid_buffer *srcBuf, uint32_t srcFormat,
           private_handle_t const *dstHandle,
           uint32_t width, uint32_t height);
bool nxCsc(private_handle_t const *srcHandle,
           const struct nxp_vid_buffer *dstBuf,
           uint32_t width, uint32_t height);
bool nxCsc(private_handle_t const *srcHandle,
           private_handle_t const *dstHandle,
           int width, int height);

#endif
