/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <linux/nxp_ion.h>
#include "ion-private.h"

#include "alloc_device.h"
#include "gralloc_priv.h"
#include "gralloc_helper.h"
#include "framebuffer_device.h"

#if GRALLOC_ARM_UMP_MODULE
#include <ump/ump.h>
#include <ump/ump_ref_drv.h>
#endif

#if GRALLOC_ARM_DMA_BUF_MODULE
#include <linux/ion.h>
#include <ion/ion.h>
#endif

// for debugging
// #include <utils/CallStack.h>
// using namespace android;
// end debugging

#define GRALLOC_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

// flag : mali use system memory
#define MALI_USE_SYSTEM_MEMORY

#if GRALLOC_SIMULATE_FAILURES
#include <cutils/properties.h>

/* system property keys for controlling simulated UMP allocation failures */
#define PROP_MALI_TEST_GRALLOC_FAIL_FIRST     "mali.test.gralloc.fail_first"
#define PROP_MALI_TEST_GRALLOC_FAIL_INTERVAL  "mali.test.gralloc.fail_interval"

static int __ump_alloc_should_fail()
{

    static unsigned int call_count  = 0;
    unsigned int        first_fail  = 0;
    int                 fail_period = 0;
    int                 fail        = 0;

    ++call_count;

    /* read the system properties that control failure simulation */
    {
        char prop_value[PROPERTY_VALUE_MAX];

        if (property_get(PROP_MALI_TEST_GRALLOC_FAIL_FIRST, prop_value, "0") > 0)
        {
            sscanf(prop_value, "%u", &first_fail);
        }

        if (property_get(PROP_MALI_TEST_GRALLOC_FAIL_INTERVAL, prop_value, "0") > 0)
        {
            sscanf(prop_value, "%u", &fail_period);
        }
    }

    /* failure simulation is enabled by setting the first_fail property to non-zero */
    if (first_fail > 0)
    {
        LOGI("iteration %u (fail=%u, period=%u)\n", call_count, first_fail, fail_period);

        fail = 	(call_count == first_fail) ||
            (call_count > first_fail && fail_period > 0 && 0 == (call_count - first_fail) % fail_period);

        if (fail)
        {
            ALOGE("failed ump_ref_drv_allocate on iteration #%d\n", call_count);
        }
    }
    return fail;
}
#endif

static bool is_rgb_format(int format)
{
    switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_RGB_888:
        case HAL_PIXEL_FORMAT_RGB_565:
        //case HAL_PIXEL_FORMAT_RGBA_5551:
        //case HAL_PIXEL_FORMAT_RGBA_4444:
            return true;
            break;
        default:
            return false;
    }
    return false;
}

static int gralloc_alloc_buffer(alloc_device_t* dev, size_t size, int usage, buffer_handle_t* pHandle, int format, int plane_num)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    int ret;

    if (plane_num == 1) {
#ifdef MALI_USE_SYSTEM_MEMORY
        struct ion_handle *ion_hnd;
        unsigned char *cpu_ptr;
        int shared_fd;
        unsigned int ion_flags = 0;

        // if (size == 9216000) {
        if (usage & GRALLOC_USAGE_HW_COMPOSER) {
            ret = ion_alloc(m->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &ion_hnd);
        } else {
            if ((usage & GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_OFTEN)
                ion_flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
            ret = ion_alloc(m->ion_client, size, 0, ION_HEAP_SYSTEM_MASK, ion_flags, &ion_hnd);
        }

        ALOGV("GRALLOC: alloc from system memory!!!");
        if ( ret != 0) {
            ALOGE("Failed to ion_alloc from ion_client:%d", m->ion_client);
            return -1;
        }

        ret = ion_share( m->ion_client, ion_hnd, &shared_fd );
        if ( ret != 0 ) {
            ALOGE( "ion_share( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            return -1;
        }

        cpu_ptr = (unsigned char*)mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0 );
        if ( MAP_FAILED == cpu_ptr ) {
            ALOGE( "ion_map( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close( shared_fd );
            return -1;
        }

        uint phys;
        ret = ion_get_phys(m->ion_client, shared_fd, (unsigned long *)&phys);

        private_handle_t *hnd = new private_handle_t( private_handle_t::PRIV_FLAGS_USES_ION, size, (int)cpu_ptr, private_handle_t::LOCK_STATE_MAPPED );

        if ( NULL != hnd ) {
            hnd->share_fd = shared_fd;
            hnd->ion_hnd = ion_hnd;
            hnd->ion_client = m->ion_client;

            hnd->ion_hnd1 = NULL;
            hnd->ion_hnd2 = NULL;
            hnd->base1 = 0;
            hnd->base2 = 0;

            hnd->phys  = phys;
            hnd->phys1 = 0;
            hnd->phys2 = 0;

            *pHandle = hnd;
            hnd->format = format;
            return 0;
        } else {
            ALOGE( "Gralloc out of mem for ion_client:%d", m->ion_client );
        }

        close( shared_fd );
        ret = munmap( cpu_ptr, size );
        if ( 0 != ret ) ALOGE( "munmap failed for base:%p size: %d", cpu_ptr, size );
        ret = ion_free( m->ion_client, ion_hnd );
        if ( 0 != ret ) ALOGE( "ion_free( %d ) failed", m->ion_client );
        return -1;
#else // MALI_USE_SYSTEM_MEMORY
        struct ion_handle *ion_hnd;
        unsigned char *cpu_ptr;
        int shared_fd;

        ret = ion_alloc(m->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &ion_hnd);
        if ( ret != 0)
        {
            ALOGE("Failed to ion_alloc from ion_client:%d", m->ion_client);
            return -1;
        }

        ret = ion_share( m->ion_client, ion_hnd, &shared_fd );
        if ( ret != 0 )
        {
            ALOGE( "ion_share( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            return -1;
        }

        cpu_ptr = (unsigned char*)mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0 );
        if ( MAP_FAILED == cpu_ptr )
        {
            ALOGE( "ion_map( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close( shared_fd );
            return -1;
        }

        uint phys;
        ret = ion_get_phys(m->ion_client, shared_fd, (unsigned long *)&phys);
        if (ret < 0)
        {
            ALOGE("ion_get_phys(%d) failed", shared_fd);
            munmap(cpu_ptr, size);
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close(shared_fd);
            return -1;
        }

        private_handle_t *hnd = new private_handle_t( private_handle_t::PRIV_FLAGS_USES_ION, size, (int)cpu_ptr, private_handle_t::LOCK_STATE_MAPPED );

        if ( NULL != hnd )
        {
            hnd->share_fd = shared_fd;
            hnd->ion_hnd = ion_hnd;
            hnd->ion_client = m->ion_client;

            hnd->ion_hnd1 = NULL;
            hnd->ion_hnd2 = NULL;
            hnd->base1 = 0;
            hnd->base2 = 0;

            *pHandle = hnd;
            hnd->phys = phys;
            hnd->format = format;
            return 0;
        }
        else
        {
            ALOGE( "Gralloc out of mem for ion_client:%d", m->ion_client );
        }

        close( shared_fd );
        ret = munmap( cpu_ptr, size );
        if ( 0 != ret ) ALOGE( "munmap failed for base:%p size: %d", cpu_ptr, size );
        ret = ion_free( m->ion_client, ion_hnd );
        if ( 0 != ret ) ALOGE( "ion_free( %d ) failed", m->ion_client );
        return -1;
#endif // MALI_USE_SYSTEM_MEMORY
    } else if (plane_num == 3) {
        struct ion_handle *ion_hnd, *ion_hnd1, *ion_hnd2;
        unsigned char *cpu_ptr, *cpu_ptr1, *cpu_ptr2;
        size_t size1, size2;
        int shared_fd, shared_fd1, shared_fd2;
        uint phys, phys1, phys2;

        ALOGV("GRALLOC allocate 3plane YV12 format");

        /* allocate Y */
        ret = ion_alloc(m->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &ion_hnd);
        if ( ret != 0) {
            ALOGE("Failed to ion_alloc from ion_client:%d", m->ion_client);
            return -1;
        }

        ret = ion_share( m->ion_client, ion_hnd, &shared_fd );
        if ( ret != 0 ) {
            ALOGE( "ion_share( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            return -1;
        }

        cpu_ptr = (unsigned char*)mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0 );
        if ( MAP_FAILED == cpu_ptr ) {
            ALOGE( "ion_map( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close( shared_fd );
            return -1;
        }

        ret = ion_get_phys(m->ion_client, shared_fd, (unsigned long *)&phys);
        if (ret < 0) {
            ALOGE("ion_get_phys(%d) failed", shared_fd);
            munmap(cpu_ptr, size);
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close(shared_fd);
            return -1;
        }

        /* allocate CB */
        if (format == HAL_PIXEL_FORMAT_YV12_444)
            size1 = size;
        else
            size1 = size >> 2;
        ret = ion_alloc(m->ion_client, size1, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &ion_hnd1);
        if ( ret != 0) {
            ALOGE("Failed to ion_alloc from ion_client:%d", m->ion_client);
            return -1;
        }

        ret = ion_share( m->ion_client, ion_hnd1, &shared_fd1 );
        if ( ret != 0 ) {
            ALOGE( "ion_share( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd1 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            return -1;
        }

        cpu_ptr1 = (unsigned char*)mmap( NULL, size1, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd1, 0 );
        if ( MAP_FAILED == cpu_ptr1 ) {
            ALOGE( "ion_map( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd1 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close( shared_fd1 );
            return -1;
        }

        ret = ion_get_phys(m->ion_client, shared_fd1, (unsigned long *)&phys1);
        if (ret < 0) {
            ALOGE("ion_get_phys(%d) failed", shared_fd1);
            munmap(cpu_ptr1, size1);
            if ( 0 != ion_free( m->ion_client, ion_hnd1 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close(shared_fd1);
            return -1;
        }

        /* allocate CR */
        if (format == HAL_PIXEL_FORMAT_YV12_444)
            size2 = size;
        else
            size2 = size >> 2;
        ret = ion_alloc(m->ion_client, size2, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &ion_hnd2);
        if ( ret != 0) {
            ALOGE("Failed to ion_alloc from ion_client:%d", m->ion_client);
            return -1;
        }

        ret = ion_share( m->ion_client, ion_hnd2, &shared_fd2 );
        if ( ret != 0 ) {
            ALOGE( "ion_share( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd2 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            return -1;
        }

        cpu_ptr2 = (unsigned char*)mmap( NULL, size2, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd2, 0 );
        if ( MAP_FAILED == cpu_ptr2 ) {
            ALOGE( "ion_map( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd2 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close( shared_fd2 );
            return -1;
        }

        ret = ion_get_phys(m->ion_client, shared_fd2, (unsigned long *)&phys2);
        if (ret < 0) {
            ALOGE("ion_get_phys(%d) failed", shared_fd2);
            munmap(cpu_ptr2, size2);
            if ( 0 != ion_free( m->ion_client, ion_hnd2 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close(shared_fd2);
            return -1;
        }

        private_handle_t *hnd = new private_handle_t( private_handle_t::PRIV_FLAGS_USES_ION, size, (int)cpu_ptr, private_handle_t::LOCK_STATE_MAPPED );

        if ( NULL != hnd ) {
            hnd->share_fd = shared_fd;
            hnd->share_fds[0] = shared_fd;
            hnd->share_fds[1] = shared_fd1;
            hnd->share_fds[2] = shared_fd2;
            hnd->ion_hnd = ion_hnd;
            hnd->ion_hnd1 = ion_hnd1;
            hnd->ion_hnd2 = ion_hnd2;
            hnd->sizes[0] = size;
            hnd->sizes[1] = size1;
            hnd->sizes[2] = size2;
            hnd->base = (int)cpu_ptr;
            hnd->base1 = (int)cpu_ptr1;
            hnd->base2 = (int)cpu_ptr2;
            hnd->ion_client = m->ion_client;

            ALOGV("share_fds(%d,%d,%d), sizes(%d,%d,%d)", hnd->share_fds[0], hnd->share_fds[1], hnd->share_fds[2],
                    hnd->sizes[0], hnd->sizes[1], hnd->sizes[2]);
            hnd->phys = phys;
            hnd->phys1 = phys1;
            hnd->phys2 = phys2;
            hnd->format = format;

            hnd->numFds = 4;
            // hnd->numInts = 23;
            hnd->numInts = 22;

            *pHandle = hnd;
            return 0;
        }

        // free Y
        close( shared_fd );
        ret = munmap( cpu_ptr, size );
        if ( 0 != ret ) ALOGE( "munmap failed for base:%p size: %d", cpu_ptr, size );
        ret = ion_free( m->ion_client, ion_hnd );
        if ( 0 != ret ) ALOGE( "ion_free( %d ) failed", m->ion_client );
        // free CB
        close( shared_fd1 );
        ret = munmap( cpu_ptr1, size1 );
        if ( 0 != ret ) ALOGE( "munmap failed for base:%p size: %d", cpu_ptr1, size1);
        ret = ion_free( m->ion_client, ion_hnd1 );
        if ( 0 != ret ) ALOGE( "ion_free( %d ) failed", m->ion_client );
        // free CR
        close( shared_fd2 );
        ret = munmap( cpu_ptr2, size1 );
        if ( 0 != ret ) ALOGE( "munmap failed for base:%p size: %d", cpu_ptr2, size2);
        ret = ion_free( m->ion_client, ion_hnd2 );
        if ( 0 != ret ) ALOGE( "ion_free( %d ) failed", m->ion_client );
        return -1;
    } else if (plane_num == 2) { // for NV12, NV21
        struct ion_handle *ion_hnd, *ion_hnd1;
        unsigned char *cpu_ptr, *cpu_ptr1;
        size_t size1;
        int shared_fd, shared_fd1;
        uint phys, phys1;

        ALOGV("GRALLOC allocate 2plane for 0x%x format", format);

        /* allocate Y */
        ret = ion_alloc(m->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &ion_hnd);
        if ( ret != 0) {
            ALOGE("Failed to ion_alloc from ion_client:%d", m->ion_client);
            return -1;
        }

        ret = ion_share( m->ion_client, ion_hnd, &shared_fd );
        if ( ret != 0 ) {
            ALOGE( "ion_share( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            return -1;
        }

        cpu_ptr = (unsigned char*)mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0 );
        if ( MAP_FAILED == cpu_ptr ) {
            ALOGE( "ion_map( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close( shared_fd );
            return -1;
        }

        ret = ion_get_phys(m->ion_client, shared_fd, (unsigned long *)&phys);
        if (ret < 0) {
            ALOGE("ion_get_phys(%d) failed", shared_fd);
            munmap(cpu_ptr, size);
            if ( 0 != ion_free( m->ion_client, ion_hnd ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close(shared_fd);
            return -1;
        }

        /* allocate CbCr or CrCb */
        size1 = size >> 1;
        ret = ion_alloc(m->ion_client, size1, 0, ION_HEAP_NXP_CONTIG_MASK, 0, &ion_hnd1);
        if ( ret != 0) {
            ALOGE("Failed to ion_alloc from ion_client:%d", m->ion_client);
            return -1;
        }

        ret = ion_share( m->ion_client, ion_hnd1, &shared_fd1 );
        if ( ret != 0 ) {
            ALOGE( "ion_share( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd1 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            return -1;
        }

        cpu_ptr1 = (unsigned char*)mmap( NULL, size1, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd1, 0 );
        if ( MAP_FAILED == cpu_ptr1 ) {
            ALOGE( "ion_map( %d ) failed", m->ion_client );
            if ( 0 != ion_free( m->ion_client, ion_hnd1 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close( shared_fd1 );
            return -1;
        }

        ret = ion_get_phys(m->ion_client, shared_fd1, (unsigned long *)&phys1);
        if (ret < 0) {
            ALOGE("ion_get_phys(%d) failed", shared_fd1);
            munmap(cpu_ptr1, size1);
            if ( 0 != ion_free( m->ion_client, ion_hnd1 ) ) ALOGE( "ion_free( %d ) failed", m->ion_client );
            close(shared_fd1);
            return -1;
        }

        private_handle_t *hnd = new private_handle_t( private_handle_t::PRIV_FLAGS_USES_ION, size, (int)cpu_ptr, private_handle_t::LOCK_STATE_MAPPED );

        if ( NULL != hnd )
        {
            hnd->share_fd = shared_fd;
            hnd->share_fds[0] = shared_fd;
            hnd->share_fds[1] = shared_fd1;
            hnd->share_fds[2] = -1;
            hnd->ion_hnd = ion_hnd;
            hnd->ion_hnd1 = ion_hnd1;
            hnd->ion_hnd2 = NULL;
            hnd->sizes[0] = size;
            hnd->sizes[1] = size1;
            hnd->sizes[2] = 0;
            hnd->base = (int)cpu_ptr;
            hnd->base1 = (int)cpu_ptr1;
            hnd->base2 = 0;
            hnd->ion_client = m->ion_client;

            hnd->phys = phys;
            hnd->phys1 = phys1;
            hnd->phys2 = 0;
            hnd->format = format;

            hnd->numFds = 3;
            // hnd->numInts = 24;
            hnd->numInts = 23;

            ALOGV("share_fds(%d,%d,%d), sizes(%d,%d,%d), phys(0x%x,0x%x,0x%x)", hnd->share_fds[0], hnd->share_fds[1], hnd->share_fds[2],
                    hnd->sizes[0], hnd->sizes[1], hnd->sizes[2],
                    hnd->phys, hnd->phys1, hnd->phys2);

            *pHandle = hnd;
            return 0;
        }

        // free Y
        close( shared_fd );
        ret = munmap( cpu_ptr, size );
        if ( 0 != ret ) ALOGE( "munmap failed for base:%p size: %d", cpu_ptr, size );
        ret = ion_free( m->ion_client, ion_hnd );
        if ( 0 != ret ) ALOGE( "ion_free( %d ) failed", m->ion_client );
        // free CB
        close( shared_fd1 );
        ret = munmap( cpu_ptr1, size1 );
        if ( 0 != ret ) ALOGE( "munmap failed for base:%p size: %d", cpu_ptr1, size1);
        ret = ion_free( m->ion_client, ion_hnd1 );
        if ( 0 != ret ) ALOGE( "ion_free( %d ) failed", m->ion_client );
        return -1;
    }

    return 0;
}

static int gralloc_alloc_framebuffer_locked(alloc_device_t* dev, size_t size, int usage, buffer_handle_t* pHandle)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

    // allocate the framebuffer
    if (m->framebuffer == NULL)
    {
        // initialize the framebuffer, the framebuffer is mapped once and forever.
        int err = init_frame_buffer_locked(m);
        if (err < 0)
        {
            return err;
        }
    }

    const uint32_t bufferMask = m->bufferMask;
    const uint32_t numBuffers = m->numBuffers;
    const size_t bufferSize = m->finfo.line_length * m->info.yres;
    if (numBuffers == 1)
    {
        // If we have only one buffer, we never use page-flipping. Instead,
        // we return a regular buffer which will be memcpy'ed to the main
        // screen when post is called.
        int newUsage = (usage & ~GRALLOC_USAGE_HW_FB) | GRALLOC_USAGE_HW_2D;
        ALOGE( "fallback to single buffering. Virtual Y-res too small %d", m->info.yres );
        return gralloc_alloc_buffer(dev, bufferSize, newUsage, pHandle, 0, 1);
    }

    if (bufferMask >= ((1LU<<numBuffers)-1))
    {
        // We ran out of buffers.
        return -ENOMEM;
    }

    int vaddr = m->framebuffer->base;
    // find a free slot
    for (uint32_t i=0 ; i<numBuffers ; i++)
    {
        if ((bufferMask & (1LU<<i)) == 0)
        {
            m->bufferMask |= (1LU<<i);
            break;
        }
        vaddr += bufferSize;
    }

    // The entire framebuffer memory is already mapped, now create a buffer object for parts of this memory
    private_handle_t* hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_FRAMEBUFFER, size, vaddr,
            0, dup(m->framebuffer->fd), vaddr - m->framebuffer->base);
    *pHandle = hnd;

    return 0;
}

static int gralloc_alloc_framebuffer(alloc_device_t* dev, size_t size, int usage, buffer_handle_t* pHandle)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    pthread_mutex_lock(&m->lock);
    int err = gralloc_alloc_framebuffer_locked(dev, size, usage, pHandle);
    pthread_mutex_unlock(&m->lock);
    return err;
}

static int alloc_device_alloc(alloc_device_t* dev, int w, int h, int format, int usage, buffer_handle_t* pHandle, int* pStride)
{
    if (!pHandle || !pStride)
    {
        return -EINVAL;
    }

    size_t size;
    size_t stride;
    size_t plane_num;
    if (format == HAL_PIXEL_FORMAT_YV12 || format == HAL_PIXEL_FORMAT_YV12_444) {
        plane_num = 3;
        stride = GRALLOC_ALIGN(w, 32);
        h = GRALLOC_ALIGN(h, 16);	//	Modified by Ray Park ( VPU Request width & height x16 aligned Memory )
        size = h * stride;
        ALOGV("%s: w(%d), h(%d), stride(%d), size(%d)", __func__, w, h, stride, size);
    } else if (format == HAL_PIXEL_FORMAT_YCrCb_420_SP) {
        plane_num = 1;
        stride = GRALLOC_ALIGN(w, 32);
        h = GRALLOC_ALIGN(h + (h/2), 16); // Y + CrCb
        size = h * stride;
        ALOGV("%s: NV21 ==>  w(%d), h(%d), stride(%d), size(%d)", __func__, w, h, stride, size);
    } else {
        int bpp = 0;
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
            //case HAL_PIXEL_FORMAT_RGBA_5551:
            //case HAL_PIXEL_FORMAT_RGBA_4444:
            case HAL_PIXEL_FORMAT_YCbCr_422_I:
                bpp = 2;
                break;
            case HAL_PIXEL_FORMAT_BLOB:
                bpp = 1;
                break;
            default:
                return -EINVAL;
        }
        size_t bpr = GRALLOC_ALIGN(w * bpp, 64);
        size = bpr * h;
        stride = bpr / bpp;
        plane_num = 1;
    }

    int err;

#ifndef MALI_600
    if (usage & GRALLOC_USAGE_HW_FB)
        err = gralloc_alloc_framebuffer(dev, size, usage, pHandle);
    else
#endif
        err = gralloc_alloc_buffer(dev, size, usage, pHandle, format, plane_num);

    if (err < 0) {
        return err;
    }

    *pStride = stride;
    return 0;
}

static int alloc_device_free(alloc_device_t* dev, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0) {
        return -EINVAL;
    }

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        // free this buffer
        private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
        const size_t bufferSize = m->finfo.line_length * m->info.yres;
        int index = (hnd->base - m->framebuffer->base) / bufferSize;
        m->bufferMask &= ~(1<<index);
        close(hnd->fd);
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
#if GRALLOC_ARM_UMP_MODULE
        ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
        ump_reference_release((ump_handle)hnd->ump_mem_handle);
#else
        ALOGE( "Can't free ump memory for handle:0x%x. Not supported.", (unsigned int)hnd );
#endif
    } else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION ) {
#if GRALLOC_ARM_DMA_BUF_MODULE
        if ( 0 != munmap( (void*)hnd->base, hnd->size ) ) ALOGE( "Failed to munmap handle 0x%x", (unsigned int)hnd );
        close( hnd->share_fd );
        if ( 0 != ion_free( hnd->ion_client, hnd->ion_hnd ) ) ALOGE( "Failed to ion_free( ion_client: %d ion_hnd: %p )", hnd->ion_client, hnd->ion_hnd );

        /* handle 3 plane */
        if (hnd->ion_hnd1) {
            if ( 0 != munmap( (void*)hnd->base1, hnd->sizes[1] ) ) ALOGE( "Failed to munmap handle 0x%x", (unsigned int)hnd );
            close( hnd->share_fds[1] );
            if ( 0 != ion_free( hnd->ion_client, hnd->ion_hnd1 ) ) ALOGE( "Failed to ion_free( ion_client: %d ion_hnd: %p )", hnd->ion_client, hnd->ion_hnd1 );
        }
        if (hnd->ion_hnd2) {
            if ( 0 != munmap( (void*)hnd->base2, hnd->sizes[2] ) ) ALOGE( "Failed to munmap handle 0x%x", (unsigned int)hnd );
            close( hnd->share_fds[2] );
            if ( 0 != ion_free( hnd->ion_client, hnd->ion_hnd2 ) ) ALOGE( "Failed to ion_free( ion_client: %d ion_hnd: %p )", hnd->ion_client, hnd->ion_hnd2 );
        }
#else
        ALOGE( "Can't free dma_buf memory for handle:0x%x. Not supported.", (unsigned int)hnd );
#endif

    }

    delete hnd;

    return 0;
}

static int alloc_device_close(struct hw_device_t *device)
{
    alloc_device_t* dev = reinterpret_cast<alloc_device_t*>(device);
    if (dev)
    {
#if GRALLOC_ARM_DMA_BUF_MODULE
        private_module_t *m = reinterpret_cast<private_module_t*>(device);
        if ( 0 != ion_close(m->ion_client) ) ALOGE( "Failed to close ion_client: %d", m->ion_client );
        close(m->ion_client);
#endif
        delete dev;
#if GRALLOC_ARM_UMP_MODULE
        ump_close(); // Our UMP memory refs will be released automatically here...
#endif
    }
    return 0;
}

int alloc_device_open(hw_module_t const* module, const char* name, hw_device_t** device)
{
    alloc_device_t *dev;

    dev = new alloc_device_t;
    if (NULL == dev)
    {
        return -1;
    }

#if GRALLOC_ARM_UMP_MODULE
    ump_result ump_res = ump_open();
    if (UMP_OK != ump_res)
    {
        ALOGE( "UMP open failed with %d", ump_res );
        delete dev;
        return -1;
    }
#endif

    /* initialize our state here */
    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = const_cast<hw_module_t*>(module);
    dev->common.close = alloc_device_close;
    dev->alloc = alloc_device_alloc;
    dev->free = alloc_device_free;

#if GRALLOC_ARM_DMA_BUF_MODULE
    private_module_t *m = reinterpret_cast<private_module_t *>(dev->common.module);
    m->ion_client = ion_open();
    if ( m->ion_client < 0 )
    {
        ALOGE( "ion_open failed with %s", strerror(errno) );
        delete dev;
        return -1;
    }
#endif

    *device = &dev->common;

    return 0;
}
