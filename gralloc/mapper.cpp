#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <gralloc_priv.h>

#include <ion/ion.h>
#include <linux/ion.h>
#include <ion-private.h>

static int gralloc_map(gralloc_module_t const *module, buffer_handle_t handle)
{
    private_handle_t *hnd = (private_handle_t *)handle;

    void *mappedAddress = mmap(0, hnd->size, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->share_fd, 0);
    if (MAP_FAILED == mappedAddress) {
        ALOGE("%s: could not mmap at %p, err %s", __func__, hnd, strerror(errno));
        return -errno;
    }
    hnd->base = (int)mappedAddress;
    return 0;
}

int gralloc_unmap(gralloc_module_t const *module, buffer_handle_t handle)
{
    private_handle_t *hnd = (private_handle_t *)handle;

    if (!hnd->base)
        return 0;

    if (munmap((void *)hnd->base, hnd->size) < 0)
        ALOGE("%s: could not unmap %s 0x%x %d", __func__, strerror(errno), hnd->base, hnd->size);

    hnd->base = 0;
    return 0;
}

static int getIonClient(gralloc_module_t const *module)
{
    private_module_t *m = const_cast<private_module_t *>(reinterpret_cast<const private_module_t *>(module));
    if (m->ion_client == -1)
        m->ion_client = ion_open();
    return m->ion_client;
}

int gralloc_register_buffer(gralloc_module_t const *module, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;
    return gralloc_map(module, handle);
}

int gralloc_unregister_buffer(gralloc_module_t const *module, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;
    gralloc_unmap(module, handle);
    return 0;
}

int gralloc_lock(gralloc_module_t const *module, buffer_handle_t handle, int usage, int l, int t, int w, int h, void **vaddr)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;

    if (vaddr) {
        private_handle_t *hnd = (private_handle_t *)handle;
        if (!hnd->base)
            gralloc_map(module, hnd);
        *vaddr = (void *)hnd->base;

        if ((usage & GRALLOC_USAGE_SW_READ_MASK) && (hnd->format != HAL_PIXEL_FORMAT_BLOB)) {
            ion_sync_from_device(getIonClient(module), hnd->share_fd);
        }
    }
    return 0;
}

int gralloc_unlock(gralloc_module_t const *module, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;

    private_handle_t *hnd = (private_handle_t *)handle;

    ion_sync_fd(getIonClient(module), hnd->share_fd);
    if (hnd->share_fd1 >= 0)
        ion_sync_fd(getIonClient(module), hnd->share_fd1);
    if (hnd->share_fd2 >= 0)
        ion_sync_fd(getIonClient(module), hnd->share_fd2);

    return 0;
}
