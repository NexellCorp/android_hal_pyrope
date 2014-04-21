#include <string.h>
#include <asm/page.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <ion/ion.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <gralloc_priv.h>
#include <nexell_format.h>
#include <ion-private.h>

#define NXPFB_GET_FB_FD _IOWR('N', 101, __u32)
static int gralloc_alloc_framebuffer_locked(private_module_t *m, size_t size, int usage, buffer_handle_t *pHandle, int *pStride)
{
    int i;
    for (i = 0; i < NUM_FB_BUFFERS; i++) {
        if (m->framebuffer[i] == NULL)
            break;
    }

    if (i >= NUM_FB_BUFFERS) {
         ALOGE("%s: ran out of index", __func__);
         return -ENOMEM;
    }

    int share_fd = i;
    int ret = ioctl(m->fb_fd, NXPFB_GET_FB_FD, &share_fd);
    //share_fd = dup(share_fd);
    if (ret < 0) {
         ALOGE("%s: failed to ioctl NXPFB_GET_FB_FD for index %d", __func__, share_fd);
         return -EINVAL;
    }

    //void *base = mmap(0, size, PROT_READ | PROT_WRITE, share_fd, 0);
    //if (MAP_FAILED == base) {
         //ALOGE("%s: failed to mmap for index %d, share_fd %d, size %d", __func__, i, share_fd, size);
         //close(share_fd);
         //return -ENOMEM;
    //}

    ALOGD("Allocate private handle for framebuffer %d", i);
    private_handle_t *hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_FRAMEBUFFER,
                                                 usage,
                                                 size,
                                                 HAL_PIXEL_FORMAT_BGRA_8888,
                                                 m->xres,
                                                 m->yres,
                                                 m->xres<<2,
                                                 0,
                                                 MALI_YUV_NO_INFO,
                                                 share_fd);

    //if (!hnd) {
        //ALOGE("%s: failed to allocate private_handle_t", __func__);
        //munmap(base, size);
    //}

    m->framebuffer[i] = hnd;
    *pHandle = hnd;
    *pStride = m->xres;
    return 0;
}

static int gralloc_alloc_framebuffer(alloc_device_t *dev, size_t size, int usage, buffer_handle_t *pHandle, int *pStride)
{
    private_module_t *m = reinterpret_cast<private_module_t *>(dev->common.module);
    pthread_mutex_lock(&m->lock);
    int err = gralloc_alloc_framebuffer_locked(m, size, usage, pHandle, pStride);
    pthread_mutex_unlock(&m->lock);
    return err;
}

static int gralloc_alloc_rgb(int ionfd, int w, int h, int format, int usage, private_handle_t **hnd, int *pStride)
{
    int bpp, stride, size;

    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        bpp = 4;
        break;
    case HAL_PIXEL_FORMAT_RGB_888:
        bpp = 3;
        break;
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RAW_SENSOR:
        bpp = 2;
        break;
    case HAL_PIXEL_FORMAT_BLOB:
        bpp = 1;
        break;
    default:
        return -EINVAL;
    }

    if (format != HAL_PIXEL_FORMAT_BLOB) {
         stride = ALIGN(w*bpp, 64);
         size = stride * h;
         *pStride = stride / bpp;
    } else {
        *pStride = w;
        size = w * h;
        stride = w;
    }

    int share_fd;
    int ret;
    if (usage & GRALLOC_USAGE_HW_COMPOSER)
        ret = ion_alloc_fd(ionfd, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &share_fd);
    else
        ret = ion_alloc_fd(ionfd, size, 0, ION_HEAP_SYSTEM_MASK, ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC, &share_fd);
    if (ret != 0) {
        ALOGE("failed to ion_alloc_fd");
        return -ENOMEM;
    }

    *hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_ION,
                                usage,
                                size,
                                format,
                                w,
                                h,
                                stride,
                                0,
                                MALI_YUV_NO_INFO,
                                share_fd);
    if (*hnd == NULL) {
         ALOGE("%s: failed to alloc private_handle_t", __func__);
         return -ENOMEM;
    }

    return 0;
}

static int gralloc_alloc_framework_yuv(int ionfd, int w, int h, int format, int usage, private_handle_t **hnd, int *pStride)
{
    size_t size;
    int stride, vstride;

    stride = ALIGN(w, 32);
    vstride = ALIGN(h, 16);

    switch (format) {
    case HAL_PIXEL_FORMAT_YV12:
        //size = (stride * vstride) + (ALIGN(stride >> 1, 16) * ALIGN(h >> 1, 16)) * 2;
        size = (stride * h) + ((stride * h) >> 1);
        break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        //size = (stride * vstride * 3) >> 1; // y size * 1.5
        size = (stride * vstride) + (stride * ALIGN(h >> 1, 16));
        break;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        size = (stride * vstride) << 1;
        break;
    default:
        ALOGE("%s: not supported format 0x%x", __func__, format);
        return -EINVAL;
    }

    int share_fd;
    int ret = ion_alloc_fd(ionfd, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &share_fd);
    if (ret) {
         ALOGE("%s: failed to ion_alloc_fd %s", __func__, strerror(errno));
         return ret;
    }

    *hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_ION,
                                usage,
                                size,
                                format,
                                w,
                                h,
                                stride,
                                0,
                                MALI_YUV_NO_INFO,
                                share_fd);
    if (*hnd == NULL) {
         ALOGE("%s: failed to allocate private_handle_t", __func__);
         return -ENOMEM;
    }

    *pStride = stride;
    return 0;
}

static int gralloc_alloc_nexell_yuv(int ionfd, int w, int h, int format, int usage, private_handle_t **hnd, int *pStride)
{
    size_t ySize, chromaSize;
    int stride, vstride;
    int planeNum;

    stride = ALIGN(w, 16);
    vstride = ALIGN(h, 16);
    ySize = stride * vstride;

    switch (format) {
    case HAL_PIXEL_FORMAT_NEXELL_YV12:
        chromaSize = ALIGN(stride >> 1, 16) * ALIGN(vstride >> 1, 16);
        planeNum = 3;
        break;
    case HAL_PIXEL_FORMAT_NEXELL_YCbCr_422_SP:
        chromaSize = ySize;
        planeNum = 2;
        break;
    case HAL_PIXEL_FORMAT_NEXELL_YCrCb_420_SP:
        chromaSize = ySize * ALIGN(vstride >> 1, 16);
        planeNum = 2;
        break;
    default:
        ALOGE("%s: Not supported format 0x%x", __func__, format);
        return -EINVAL;
    }

    int share_fd, share_fd1, share_fd2 = -1;
    // allocate Y
    int ret = ion_alloc_fd(ionfd, ySize, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &share_fd);
    if (ret) {
         ALOGE("%s: failed to ion_alloc_fd %s", __func__, strerror(errno));
         return ret;
    }
    // allocate cb
    ret = ion_alloc_fd(ionfd, chromaSize, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &share_fd1);
    if (ret) {
         ALOGE("%s: failed to ion_alloc_fd %s", __func__, strerror(errno));
         return ret;
    }
    // allocate cr
    if (planeNum > 2) {
        ret = ion_alloc_fd(ionfd, chromaSize, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &share_fd2);
        if (ret) {
            ALOGE("%s: failed to ion_alloc_fd %s", __func__, strerror(errno));
            return ret;
        }
    }
    *hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_ION,
                                usage,
                                ySize,
                                format,
                                w,
                                h,
                                stride,
                                0,
                                MALI_YUV_NO_INFO,
                                share_fd,
                                share_fd1,
                                share_fd2);

    if (*hnd == NULL) {
         ALOGE("%s: failed to allocate private_handle_t", __func__);
         return -ENOMEM;
    }

    *pStride = stride;
    return 0;
}

static int gralloc_alloc_yuv(int ionfd, int w, int h, int format, int usage, private_handle_t **hnd, int *pStride)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        return gralloc_alloc_framework_yuv(ionfd, w, h, format, usage, hnd, pStride);

    case HAL_PIXEL_FORMAT_NEXELL_YV12:
    case HAL_PIXEL_FORMAT_NEXELL_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_NEXELL_YCrCb_420_SP:
        return gralloc_alloc_nexell_yuv(ionfd, w, h, format, usage, hnd, pStride);

    default:
        ALOGE("%s: unsupported yuv format 0x%x", __func__, format);
        return -EINVAL;
    }
}

static int gralloc_alloc(alloc_device_t *dev, int w, int h, int format, int usage, buffer_handle_t *pHandle, int *pStride)
{
    if (usage & GRALLOC_USAGE_HW_FB)
        return gralloc_alloc_framebuffer(dev, w*h<<2, usage, pHandle, pStride);

    private_module_t *m = reinterpret_cast<private_module_t *>(dev->common.module);

    int ret = gralloc_alloc_rgb(m->ion_client, w, h, format, usage, (private_handle_t **)pHandle, pStride);
    if (ret == 0)
        return ret;

    if (ret == -EINVAL)
         ret =  gralloc_alloc_yuv(m->ion_client, w, h, format, usage, (private_handle_t **)pHandle, pStride);

    return ret;
}

// framebuffer_device.cpp
extern int framebuffer_device_open(hw_module_t const *module, const char *name, hw_device_t **device);

static int gralloc_close(struct hw_device_t *device)
{
    alloc_device_t *dev = reinterpret_cast<alloc_device_t *>(device);
    if (dev) {
        private_module_t *m = reinterpret_cast<private_module_t *>(dev);
        if (m->ion_client >= 0)
            close(m->ion_client);
        delete dev;
    }
    return 0;
}

extern int gralloc_unmap(gralloc_module_t const *module, buffer_handle_t handle);

static int gralloc_free(alloc_device_t* dev, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
    gralloc_module_t* module = reinterpret_cast<gralloc_module_t*>(dev->common.module);
    if (hnd->base)
        gralloc_unmap(module, const_cast<private_handle_t*>(hnd));

    close(hnd->share_fd);
    if (hnd->share_fd1 >= 0)
        close(hnd->share_fd1);
    if (hnd->share_fd2 >= 0)
        close(hnd->share_fd2);

    delete hnd;
    return 0;
}

static int gralloc_device_open(const hw_module_t *module, const char *name, hw_device_t **device)
{
    if (!strcmp(name, GRALLOC_HARDWARE_GPU0)) {
        alloc_device_t *dev = new alloc_device_t;
        if (!dev) {
             ALOGE("%s: failed to allocate alloc_devic_t", __func__);
             return -ENOMEM;
        }
        memset(dev, 0, sizeof(*dev));

        dev->common.tag         = HARDWARE_DEVICE_TAG;
        dev->common.version     = 0;
        dev->common.module      = const_cast<hw_module_t *>(module);
        dev->common.close       = gralloc_close;
        dev->alloc              = gralloc_alloc;
        dev->free               = gralloc_free;

        private_module_t *m = reinterpret_cast<private_module_t *>(dev->common.module);
        m->ion_client = ion_open();

        *device = &dev->common;
        return 0;
    } else {
        return framebuffer_device_open(module, name, device);
    }
}

// mapper.cpp
extern int gralloc_lock(gralloc_module_t const *module, buffer_handle_t handle, int usage, int l, int t, int w, int h, void **vaddr);
extern int gralloc_unlock(gralloc_module_t const *module, buffer_handle_t handle);
extern int gralloc_register_buffer(gralloc_module_t const *module, buffer_handle_t handle);
extern int gralloc_unregister_buffer(gralloc_module_t const *module, buffer_handle_t handle);

static struct hw_module_methods_t gralloc_module_methods = {
open: gralloc_device_open,
};

struct private_module_t HAL_MODULE_INFO_SYM = {
base: {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: GRALLOC_HARDWARE_MODULE_ID,
        name: "Graphics Memory Allocator Module",
        author: "Nexell",
        methods: &gralloc_module_methods,
        dso: NULL,
        reserved: {0, },
    },
    registerBuffer: gralloc_register_buffer,
    unregisterBuffer: gralloc_unregister_buffer,
    lock: gralloc_lock,
    unlock: gralloc_unlock,
    perform: NULL,
},
fb_fd: -1,
framebuffer: {NULL, },
lock: PTHREAD_MUTEX_INITIALIZER,
ion_client: -1,
xres: 0,
yres: 0,
xdpi: 0.0f,
ydpi: 0.0f,
fps: 0.0f
};
