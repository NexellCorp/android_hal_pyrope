#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cutils/log.h>

#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <ion-private.h>

#define LOG_TAG "nexell-ion"

static int ion_ioctl(int fd, int req, void *arg)
{
    int ret = ioctl(fd, req, arg);
    if (ret < 0) {
        ALOGE("ioctl %d failed with code %d: %d\n", req,
                ret, strerror(errno));
    }
    return ret;
}

int ion_get_phys(int fd, int buf_fd, unsigned long *phys)
{
    int ret;
    struct ion_custom_data data;
    struct nxp_ion_physical custom_data;

    custom_data.ion_buffer_fd = buf_fd;
    data.cmd = NXP_ION_GET_PHY_ADDR;
    data.arg = (unsigned long)&custom_data;

    ret = ion_ioctl(fd, ION_IOC_CUSTOM, &data);
    if (ret) {
        ALOGE("%s error: failed ION_IOC_CUSTOM\n", __func__);
        return ret;
    }   
    *phys = custom_data.phys;
    return 0;
}
