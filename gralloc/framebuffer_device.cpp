#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>

#include <cutils/log.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <gralloc_priv.h>

static int get_screen_res(const char *fbname, int32_t &xres, int32_t &yres, int32_t &refreshRate)
{
    int ret = 0;
    char *path;
    char buf[128] = {0, };
    int fd;
    unsigned int x, y, r;

    asprintf(&path, "/sys/class/graphics/%s/modes", fbname);
    if (!path) {
        ALOGE("%s: failed to asprintf()", __func__);
        return -EINVAL;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s: failed to open %s", __func__, path);
        ret = -EINVAL;
        goto err_open;
    }

    ret = read(fd, buf, sizeof(buf));
    if (ret <= 0) {
         ALOGE("%s: failed to read %s", __func__, path);
         ret = -EINVAL;
         goto err_read;
    }

    ret = sscanf(buf, "U:%ux%up-%u", &x, &y, &r);
    if (ret != 3) {
         ALOGE("%s: failed to sscanf()", __func__);
         ret = -EINVAL;
         goto err_sscanf;
    }

    ALOGI("Using %ux%u %uHz resolution for %s from modes list\n", x, y, r, fbname);

    xres = (int32_t)x;
    yres = (int32_t)y;
    refreshRate = (int32_t)r;

    close(fd);
    free(path);
    return 0;

err_sscanf:
err_read:
    close(fd);
err_open:
    free(path);

    return ret;
}

static int init_fb(struct private_module_t *module)
{
     int fd = open("/dev/graphics/fb0", O_RDWR);
     if (fd < 0) {
         ALOGE("%s: failed to open /dev/graphics/fb0", __func__);
         return -errno;
     }

     struct fb_fix_screeninfo finfo;
     if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
         ALOGE("%s: failed to FBIOGET_FSCREENINFO", __func__);
         return -errno;
     }

     struct fb_var_screeninfo info;
     if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1) {
          ALOGE("%s: failed to FBIOGET_VSCREENINFO", __func__);
          return -errno;
     }

     int32_t refreshRate;
     if (get_screen_res("fb0", module->xres, module->yres, refreshRate)) {
          ALOGE("%s: failed to get_screen_res()", __func__);
          return -errno;
     }

     float xdpi = (module->xres * 25.4f) / info.width;
     float ydpi = (module->yres * 25.4f) / info.height;

     ALOGI("using (id=%s)\n"
           "xres        = %d px\n"
           "yres        = %d px\n"
           "width       = %d mm (%f dpi)\n"
           "height      = %d mm (%f dpi)\n"
           "refreshRate = %.2f Hz\n",
           finfo.id,
           module->xres,
           module->yres,
           info.width, xdpi,
           info.height, ydpi,
           (float)refreshRate);

     module->fb_fd = fd;
     module->xdpi = xdpi;
     module->ydpi = ydpi;
     module->fps  = (float)refreshRate;

     return 0;
}

static int fb_close(struct hw_device_t *device)
{
    framebuffer_device_t *dev = reinterpret_cast<framebuffer_device_t *>(device);
    if (dev)
        delete dev;
    return 0;
}

int framebuffer_device_open(hw_module_t const *module, const char *name, hw_device_t **device)
{
    int status = -EINVAL;

    alloc_device_t *gralloc_device;
    status = gralloc_open(module, &gralloc_device);
    if (status < 0) {
        ALOGE("%s: Fail to open gralloc device", __func__);
        return status;
    }

    framebuffer_device_t *dev = new framebuffer_device_t();
    memset(dev, 0, sizeof(*dev));

    private_module_t *m = (private_module_t *)module;
    status = init_fb(m);
    if (status < 0) {
         ALOGE("%s: failed to init framebuffer", __func__);
         free(dev);
         gralloc_close(gralloc_device);
         return status;
    }

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = const_cast<hw_module_t *>(module);
    dev->common.close = fb_close;
    dev->setSwapInterval = 0;
    dev->post = 0;
    dev->setUpdateRect = 0;
    dev->compositionComplete = 0;

    //uint32_t bits_per_pixel = 32;
    //int stride = m->xres * 4 / (bits_per_pixel >> 3);
    const_cast<uint32_t&>(dev->flags) = 0;
    const_cast<uint32_t&>(dev->width) = m->xres;
    const_cast<uint32_t&>(dev->height) = m->yres;
    //const_cast<int&>(dev->stride) = m->xres * 4 / (bits_per_pixel >> 3);
    const_cast<int&>(dev->stride) = m->xres;
    const_cast<int&>(dev->format) = HAL_PIXEL_FORMAT_BGRA_8888;
    const_cast<float&>(dev->xdpi) = m->xdpi;
    const_cast<float&>(dev->ydpi) = m->ydpi;
    const_cast<float&>(dev->fps)  = m->fps;
    const_cast<int&>(dev->minSwapInterval) = 1;
    const_cast<int&>(dev->maxSwapInterval) = 1;
    *device = &dev->common;

    return 0;
}
