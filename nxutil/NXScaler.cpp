#define LOG_TAG "NXScaler"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/ioctl.h>

#include "NXScaler.h"

#define _ALIGN(value, base) (((value) + ((base) - 1))) & ~((base) - 1)
// #define _ALIGN(value, base) (value)

static int scaler_fd = -1;

static void recalcSourceBuffer(const unsigned long srcPhys, unsigned long &calcPhys,
        bool isY, const uint32_t left, const uint32_t top, const uint32_t stride, const uint32_t code)
{
    if (isY) {
        if (code == PIXCODE_YUV422_PACKED) {
            calcPhys = _ALIGN(srcPhys + (top * stride) + (left << 1), 16);
        } else {
            calcPhys = _ALIGN(srcPhys + (top * stride) + left, 16);
        }
    } else {
        switch (code) {
        case PIXCODE_YUV420_PLANAR:
            // calcPhys = _ALIGN(srcPhys + (top * stride) + (left >> 1), 16);
            calcPhys = _ALIGN(srcPhys + ((top >> 1) * stride) + (left >> 1), 16);
            break;
        case PIXCODE_YUV422_PLANAR:
            calcPhys = _ALIGN(srcPhys + (top * stride) + (left >> 1), 16);
            break;
        case PIXCODE_YUV444_PLANAR:
            calcPhys = _ALIGN(srcPhys + (top * stride) + left, 16);
            break;
        }
    }
}

int nxScalerRun(const struct nxp_vid_buffer *srcBuf, const struct nxp_vid_buffer *dstBuf, const struct scale_ctx *ctx)
{
    if (scaler_fd < 0) {
        scaler_fd = open("/dev/nxp-scaler", O_RDWR);
        if (scaler_fd < 0) {
            ALOGE("failed to open scaler device node");
            return -EINVAL;
        }
    }

    struct nxp_scaler_ioctl_data data;
    bzero(&data, sizeof(struct nxp_scaler_ioctl_data));

    unsigned long srcPhys[3];

    if (ctx->left > 0 || ctx->top > 0) {
        if (ctx->src_code == PIXCODE_YUV422_PACKED) {
            recalcSourceBuffer(srcBuf->phys[0], data.src_phys[0], true, ctx->left, ctx->top, srcBuf->stride[0], ctx->src_code);
        } else {
            recalcSourceBuffer(srcBuf->phys[0], data.src_phys[0], true, ctx->left, ctx->top, srcBuf->stride[0], ctx->src_code);
            recalcSourceBuffer(srcBuf->phys[1], data.src_phys[1], false, ctx->left, ctx->top, srcBuf->stride[1], ctx->src_code);
            recalcSourceBuffer(srcBuf->phys[2], data.src_phys[2], false, ctx->left, ctx->top, srcBuf->stride[2], ctx->src_code);
        }
    } else {
        memcpy(data.src_phys, srcBuf->phys, sizeof(unsigned long) * 3);
    }

    memcpy(data.src_stride, srcBuf->stride, sizeof(unsigned long) * 3);
    data.src_width = ctx->src_width;
    data.src_height = ctx->src_height;
    data.src_code = ctx->src_code;
    memcpy(data.dst_phys, dstBuf->phys, sizeof(unsigned long) * 3);
    memcpy(data.dst_stride, dstBuf->stride, sizeof(unsigned long) * 3);
    data.dst_width = ctx->dst_width;
    data.dst_height = ctx->dst_height;
    data.dst_code = ctx->dst_code;

    return ioctl(scaler_fd, IOCTL_SCALER_SET_AND_RUN, &data);
}

int nxScalerRun(const struct nxp_vid_buffer *srcBuf, private_handle_t const *dstHandle, const struct scale_ctx *ctx)
{
    if (scaler_fd < 0) {
        scaler_fd = open("/dev/nxp-scaler", O_RDWR);
        if (scaler_fd < 0) {
            ALOGE("failed to open scaler device node");
            return -EINVAL;
        }
    }

    struct nxp_scaler_ioctl_data data;
    bzero(&data, sizeof(struct nxp_scaler_ioctl_data));

    unsigned long srcPhys[3];

    if (ctx->left > 0 || ctx->top > 0) {
        if (ctx->src_code == PIXCODE_YUV422_PACKED) {
            recalcSourceBuffer(srcBuf->phys[0], data.src_phys[0], true, ctx->left, ctx->top, srcBuf->stride[0], ctx->src_code);
        } else {
            recalcSourceBuffer(srcBuf->phys[0], data.src_phys[0], true, ctx->left, ctx->top, srcBuf->stride[0], ctx->src_code);
            recalcSourceBuffer(srcBuf->phys[1], data.src_phys[1], false, ctx->left, ctx->top, srcBuf->stride[1], ctx->src_code);
            recalcSourceBuffer(srcBuf->phys[2], data.src_phys[2], false, ctx->left, ctx->top, srcBuf->stride[2], ctx->src_code);
        }
    } else {
        memcpy(data.src_phys, srcBuf->phys, sizeof(unsigned long) * 3);
    }

    memcpy(data.src_stride, srcBuf->stride, sizeof(unsigned long) * 3);
    data.src_width = ctx->src_width;
    data.src_height = ctx->src_height;
    data.src_code = ctx->src_code;
    data.dst_phys[0] = dstHandle->phys;
    data.dst_phys[1] = dstHandle->phys1;
    data.dst_phys[2] = dstHandle->phys2;
    data.dst_stride[0] = ctx->dst_width;
    /* TODO : check format */
    switch (ctx->dst_code) {
    case PIXCODE_YUV420_PLANAR:
        data.dst_stride[1] = ctx->dst_width >> 1;
        data.dst_stride[2] = ctx->dst_width >> 1;
        break;
    case PIXCODE_YUV422_PLANAR:
        data.dst_stride[1] = ctx->dst_width >> 1;
        data.dst_stride[2] = ctx->dst_width >> 1;
        break;
    default:
        data.dst_stride[1] = ctx->dst_width;
        data.dst_stride[2] = ctx->dst_width;
        break;
    }
    data.dst_width = ctx->dst_width;
    data.dst_height = ctx->dst_height;
    data.dst_code = ctx->dst_code;

    return ioctl(scaler_fd, IOCTL_SCALER_SET_AND_RUN, &data);

}

int nxScalerRun(private_handle_t const *srcHandle, private_handle_t const *dstHandle, const struct scale_ctx *ctx)
{
    if (scaler_fd < 0) {
        scaler_fd = open("/dev/nxp-scaler", O_RDWR);
        if (scaler_fd < 0) {
            ALOGE("failed to open scaler device node");
            return -EINVAL;
        }
    }

    struct nxp_scaler_ioctl_data data;
    bzero(&data, sizeof(struct nxp_scaler_ioctl_data));

    unsigned long srcPhys[3];

    if (ctx->left > 0 || ctx->top > 0) {
        if (ctx->src_code == PIXCODE_YUV422_PACKED) {
            recalcSourceBuffer(srcHandle->phys, data.src_phys[0], true, ctx->left, ctx->top, ctx->src_width, ctx->src_code);
        } else {
            recalcSourceBuffer(srcHandle->phys, data.src_phys[0], true, ctx->left, ctx->top, ctx->src_width, ctx->src_code);
            recalcSourceBuffer(srcHandle->phys1, data.src_phys[1], false, ctx->left, ctx->top, ctx->src_width >> 1, ctx->src_code);
            recalcSourceBuffer(srcHandle->phys2, data.src_phys[2], false, ctx->left, ctx->top, ctx->src_width >> 1, ctx->src_code);
        }
    } else {
        data.src_phys[0] = srcHandle->phys;
        data.src_phys[1] = srcHandle->phys1;
        data.src_phys[2] = srcHandle->phys2;
    }

    data.src_stride[0] = ctx->src_width;
    data.src_stride[1] = ctx->src_width >> 1;
    data.src_stride[2] = ctx->src_width >> 1;
    data.src_width = ctx->src_width;
    data.src_height = ctx->src_height;
    data.src_code = ctx->src_code;
    data.dst_phys[0] = dstHandle->phys;
    data.dst_phys[1] = dstHandle->phys1;
    data.dst_phys[2] = dstHandle->phys2;
    data.dst_stride[0] = ctx->dst_width;
    /* TODO : check format */
    switch (ctx->dst_code) {
    case PIXCODE_YUV420_PLANAR:
        data.dst_stride[1] = ctx->dst_width >> 1;
        data.dst_stride[2] = ctx->dst_width >> 1;
        break;
    case PIXCODE_YUV422_PLANAR:
        data.dst_stride[1] = ctx->dst_width >> 1;
        data.dst_stride[2] = ctx->dst_width >> 1;
        break;
    default:
        data.dst_stride[1] = ctx->dst_width;
        data.dst_stride[2] = ctx->dst_width;
        break;
    }
    data.dst_width = ctx->dst_width;
    data.dst_height = ctx->dst_height;
    data.dst_code = ctx->dst_code;

    return ioctl(scaler_fd, IOCTL_SCALER_SET_AND_RUN, &data);
}

int nxScalerRun(private_handle_t const *srcHandle, const struct nxp_vid_buffer *dstBuf, const struct scale_ctx *ctx)
{
    if (scaler_fd < 0) {
        scaler_fd = open("/dev/nxp-scaler", O_RDWR);
        if (scaler_fd < 0) {
            ALOGE("failed to open scaler device node");
            return -EINVAL;
        }
    }

    struct nxp_scaler_ioctl_data data;
    bzero(&data, sizeof(struct nxp_scaler_ioctl_data));

    if (ctx->left > 0 || ctx->top > 0) {
        if (ctx->src_code == PIXCODE_YUV422_PACKED) {
            recalcSourceBuffer(srcHandle->phys, data.src_phys[0], true, ctx->left, ctx->top, ctx->src_width, ctx->src_code);
        } else {
            recalcSourceBuffer(srcHandle->phys, data.src_phys[0], true, ctx->left, ctx->top, ctx->src_width, ctx->src_code);
            recalcSourceBuffer(srcHandle->phys1, data.src_phys[1], false, ctx->left, ctx->top, ctx->src_width >> 1, ctx->src_code);
            recalcSourceBuffer(srcHandle->phys2, data.src_phys[2], false, ctx->left, ctx->top, ctx->src_width >> 1, ctx->src_code);
        }
    } else {
        data.src_phys[0] = srcHandle->phys;
        data.src_phys[1] = srcHandle->phys1;
        data.src_phys[2] = srcHandle->phys2;
    }

#if 0
    data.src_stride[0] = ctx->src_width;
    data.src_stride[1] = ctx->src_width >> 1;
    data.src_stride[2] = ctx->src_width >> 1;
#else
    data.src_stride[0] = ctx->src_width;
    data.src_stride[1] = ctx->src_width >> 1;
    data.src_stride[2] = ctx->src_width >> 1;
#endif
    data.src_width = ctx->src_width;
    data.src_height = ctx->src_height;
    data.src_code = ctx->src_code;
    memcpy(data.dst_phys, dstBuf->phys, sizeof(unsigned long) * 3);
    memcpy(data.dst_stride, dstBuf->stride, sizeof(unsigned long) * 3);
    data.dst_width = ctx->dst_width;
    data.dst_height = ctx->dst_height;
    data.dst_code = ctx->dst_code;

    ALOGD("%s: src_stride(%lu,%lu,%lu), src_width(%d), src_height(%d), dst_stride(%lu,%lu,%lu), dst_width(%d), dst_height(%d)",
            __func__, data.src_stride[0], data.src_stride[1], data.src_stride[2], data.src_width, data.src_height,
            data.dst_stride[0], data.dst_stride[1], data.dst_stride[2], data.dst_width, data.dst_height);


    return ioctl(scaler_fd, IOCTL_SCALER_SET_AND_RUN, &data);
}
