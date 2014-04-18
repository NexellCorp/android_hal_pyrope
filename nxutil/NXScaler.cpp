#define LOG_TAG "NXScaler"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/ioctl.h>

#include <utils/Log.h>

#include <ion/ion.h>
#include <ion-private.h>

#include <system/graphics.h>
#include <nexell_format.h>

#include "NXScaler.h"

static int scaler_fd = -1;
static int ion_fd = -1;

static void recalcSourceBuffer(const unsigned long srcPhys, unsigned long &calcPhys,
        bool isY, const uint32_t left, const uint32_t top, const uint32_t stride, const uint32_t code)
{
    if (isY) {
        if (code == PIXCODE_YUV422_PACKED) {
            calcPhys = ALIGN(srcPhys + (top * stride) + (left << 1), 16);
        } else {
            calcPhys = ALIGN(srcPhys + (top * stride) + left, 16);
        }
    } else {
        switch (code) {
        case PIXCODE_YUV420_PLANAR:
            calcPhys = ALIGN(srcPhys + ((top >> 1) * stride) + (left >> 1), 16);
            break;
        case PIXCODE_YUV422_PLANAR:
            calcPhys = ALIGN(srcPhys + (top * stride) + (left >> 1), 16);
            break;
        case PIXCODE_YUV444_PLANAR:
            calcPhys = ALIGN(srcPhys + (top * stride) + left, 16);
            break;
        }
    }
}

static inline int baseCheck()
{
    if (scaler_fd < 0) {
        scaler_fd = open("/dev/nxp-scaler", O_RDWR);
        if (scaler_fd < 0) {
            ALOGE("failed to open scaler device node");
            return -EINVAL;
        }
    }

    if (ion_fd < 0) {
         ion_fd = ion_open();
         if (ion_fd < 0) {
             ALOGE("failed to open ion device node");
             return -EINVAL;
         }
    }

    return 0;
}

int getPhysForHandle(private_handle_t const *handle, unsigned long *phys)
{
    int ret = 0;
    switch (handle->format) {
    case HAL_PIXEL_FORMAT_YV12:
        ret = ion_get_phys(ion_fd, handle->share_fd, &phys[0]);
        if (ret) {
             ALOGE("%s: failed to ion_get_phys for handle %p, fd %d", __func__, handle, handle->share_fd);
             return -EINVAL;
        }
        phys[1] = phys[0] + (handle->stride * ALIGN(handle->height, 16));
        phys[2] = phys[1] + (ALIGN(handle->stride >> 1, 16) * ALIGN(handle->height >> 1, 16));
        break;
#if 0
        // TODO
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
        break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        break;
#endif
    default:
        ALOGE("%s: not supported format 0x%x", __func__, handle->format);
        return -EINVAL;
    }

    return 0;
}

int getVirtForHandle(private_handle_t const *handle, unsigned long *virt)
{
    void *ptr = mmap(0, handle->size, PROT_READ | PROT_WRITE, MAP_SHARED, handle->share_fd, 0);
    if (MAP_FAILED == ptr) {
         ALOGE("%s: failed to mmap for %d", __func__, handle->share_fd);
         return -EINVAL;
    }
    *virt = (unsigned long)ptr;
    return 0;
}

void releaseVirtForHandle(private_handle_t const *handle, unsigned long virt)
{
     munmap((void *)virt, handle->size);
}

int nxScalerRun(const struct nxp_vid_buffer *srcBuf, const struct nxp_vid_buffer *dstBuf, const struct scale_ctx *ctx)
{
    if (0 != baseCheck()) {
        return -EINVAL;
    }

    struct nxp_scaler_ioctl_data data;
    bzero(&data, sizeof(struct nxp_scaler_ioctl_data));

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
    if (0 != baseCheck()) {
        return -EINVAL;
    }

#if 1
    struct nxp_scaler_ioctl_data data;
    bzero(&data, sizeof(struct nxp_scaler_ioctl_data));

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
    int ret = getPhysForHandle(dstHandle, data.dst_phys);
    if (ret)
         return ret;
#if 0
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
#else
    data.dst_stride[0] = dstHandle->stride;
    /* TODO : check format */
    switch (ctx->dst_code) {
    case PIXCODE_YUV420_PLANAR:
        data.dst_stride[1] =
        data.dst_stride[2] = ALIGN(dstHandle->stride >> 1, 16);
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
#endif
    data.dst_width = ctx->dst_width;
    data.dst_height = ctx->dst_height;
    data.dst_code = ctx->dst_code;

    return ioctl(scaler_fd, IOCTL_SCALER_SET_AND_RUN, &data);
#else
    // psw0523 : this is test code
    unsigned long virtY, virtCb, virtCr;
    getVirtForHandle(dstHandle, &virtY);
    virtCb = virtY + srcBuf->sizes[0];
    virtCr = virtCb + srcBuf->sizes[1];
    memcpy((void *)virtY, srcBuf->virt[0], srcBuf->sizes[0]);
    memcpy((void *)virtCb, srcBuf->virt[1], srcBuf->sizes[1]);
    memcpy((void *)virtCr, srcBuf->virt[2], srcBuf->sizes[2]);
    releaseVirtForHandle(dstHandle, virtY);
    return 0;
#endif
}

int nxScalerRun(private_handle_t const *srcHandle, private_handle_t const *dstHandle, const struct scale_ctx *ctx)
{
    if (0 != baseCheck()) {
        return -EINVAL;
    }

    struct nxp_scaler_ioctl_data data;
    bzero(&data, sizeof(struct nxp_scaler_ioctl_data));

    int ret = getPhysForHandle(srcHandle, data.src_phys);
    if (ret)
        return ret;

    if (ctx->left > 0 || ctx->top > 0) {
        if (ctx->src_code == PIXCODE_YUV422_PACKED) {
            recalcSourceBuffer(data.src_phys[0], data.src_phys[0], true, ctx->left, ctx->top, ctx->src_width, ctx->src_code);
        } else {
            recalcSourceBuffer(data.src_phys[0], data.src_phys[0], true, ctx->left, ctx->top, ctx->src_width, ctx->src_code);
            recalcSourceBuffer(data.src_phys[1], data.src_phys[1], false, ctx->left, ctx->top, ctx->src_width >> 1, ctx->src_code);
            recalcSourceBuffer(data.src_phys[2], data.src_phys[2], false, ctx->left, ctx->top, ctx->src_width >> 1, ctx->src_code);
        }
    }

    data.src_stride[0] = ctx->src_width;
    data.src_stride[1] = ctx->src_width >> 1;
    data.src_stride[2] = ctx->src_width >> 1;
    data.src_width = ctx->src_width;
    data.src_height = ctx->src_height;
    data.src_code = ctx->src_code;

    ret = getPhysForHandle(dstHandle, data.dst_phys);
    if (ret)
        return ret;

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
    if (0 != baseCheck()) {
        return -EINVAL;
    }
    struct nxp_scaler_ioctl_data data;
    bzero(&data, sizeof(struct nxp_scaler_ioctl_data));

    int ret = getPhysForHandle(srcHandle, data.src_phys);
    if (ret)
        return ret;

    if (ctx->left > 0 || ctx->top > 0) {
        if (ctx->src_code == PIXCODE_YUV422_PACKED) {
            recalcSourceBuffer(data.src_phys[0], data.src_phys[0], true, ctx->left, ctx->top, ctx->src_width, ctx->src_code);
        } else {
            recalcSourceBuffer(data.src_phys[0], data.src_phys[0], true, ctx->left, ctx->top, ctx->src_width, ctx->src_code);
            recalcSourceBuffer(data.src_phys[1], data.src_phys[1], false, ctx->left, ctx->top, ctx->src_width >> 1, ctx->src_code);
            recalcSourceBuffer(data.src_phys[2], data.src_phys[2], false, ctx->left, ctx->top, ctx->src_width >> 1, ctx->src_code);
        }
    }

    data.src_stride[0] = ctx->src_width;
    data.src_stride[1] = ctx->src_width >> 1;
    data.src_stride[2] = ctx->src_width >> 1;

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
