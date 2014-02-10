/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_ukk.c
 * File implements the user side of the user-kernel interface
 */

#define _XOPEN_SOURCE 600

#include <base/mali_types.h>
#include <base/mali_debug.h>
#include <base/mali_runtime.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "errno.h"
#include "mali_osu.h"
#include "mali_uku.h"
#ifdef NEXELL_FEATURE_ENABLE
#include "vr_utgard_ioctl.h"
#else
#include "mali_utgard_ioctl.h"
#endif

/**
 * Internal function that calls the ioctl handler of the MALI device driver
 */
MALI_NOTRACE static _mali_osk_errcode_t mali_driver_ioctl(void *context, u32 command, void *args);
static _mali_osk_errcode_t map_errcode(int err);

/**
 * The device file to access the device driver by
 * This is a character special file giving access to the device driver.
 * Usually created using the mknod command line utility.
 */
#ifdef NEXELL_FEATURE_ENABLE
#define MALI_DEVICE_FILE_NAME    "/dev/vr"
unsigned int gOpenDrvCnt = 0;
#else
#define MALI_DEVICE_FILE_NAME    "/dev/mali"
#endif
_mali_osk_errcode_t _mali_uku_open( void **context )
{
	struct stat filestat;
    int mali_file_descriptor;

#ifdef NEXELL_FEATURE_ENABLE
    MALI_CHECK_NON_NULL(context, _MALI_OSK_ERR_INVALID_ARGS);
	/* open a file descriptor to the device driver */
	gOpenDrvCnt = 0;
	do{
		++gOpenDrvCnt;
		mali_file_descriptor = open(MALI_DEVICE_FILE_NAME, O_RDWR);
		if(gOpenDrvCnt == 500)
			break;
		usleep(10000);		
	}
	while(-1 == mali_file_descriptor);		
#else /* org */
    MALI_CHECK_NON_NULL(context, _MALI_OSK_ERR_INVALID_ARGS);

	/* open a file descriptor to the device driver */
	mali_file_descriptor = open(MALI_DEVICE_FILE_NAME, O_RDWR);
#endif

	/* first verify that we were able to open the file */
	if (-1 == mali_file_descriptor)
	{
		MALI_DEBUG_ERROR(("Failed to open device file %s\nPlease verify that the file exists and that the driver has been loaded\n", MALI_DEVICE_FILE_NAME));
        return _MALI_OSK_ERR_FAULT;
	}

	/* query the file for information */
	if (0 != fstat(mali_file_descriptor, &filestat))
	{
		close(mali_file_descriptor);
		MALI_DEBUG_ERROR(("Failed to query device file %s for type information.", MALI_DEVICE_FILE_NAME));
		return _MALI_OSK_ERR_FAULT;
	}

	/* verify that it is a character special file */
	if (0 == S_ISCHR(filestat.st_mode))
	{
		close(mali_file_descriptor);
		MALI_DEBUG_ERROR(("File %s not a character device file.", MALI_DEVICE_FILE_NAME));
		return _MALI_OSK_ERR_FAULT;
	}

    *context = (void *)mali_file_descriptor;

	return _MALI_OSK_ERR_OK;
}

_mali_osk_errcode_t _mali_uku_close(void **context)
{
    MALI_CHECK_NON_NULL(context, _MALI_OSK_ERR_INVALID_ARGS);
    MALI_CHECK(-1 != (int)*context, _MALI_OSK_ERR_INVALID_ARGS);

	close((int)*context);
	*context = (void *)-1;

    return _MALI_OSK_ERR_OK;
}

_mali_osk_errcode_t _mali_uku_wait_for_notification(_vr_uk_wait_for_notification_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_WAIT_FOR_NOTIFICATION, args);
}

_mali_osk_errcode_t _mali_uku_post_notification(_vr_uk_post_notification_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_POST_NOTIFICATION, args);
}

_mali_osk_errcode_t _mali_uku_get_api_version(_vr_uk_get_api_version_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_GET_API_VERSION, args);
}

_mali_osk_errcode_t _mali_uku_get_user_setting(_vr_uk_get_user_setting_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_GET_USER_SETTING, args);
}

_mali_osk_errcode_t _mali_uku_get_user_settings(_vr_uk_get_user_settings_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_GET_USER_SETTINGS, args);
}

_mali_osk_errcode_t _mali_uku_stream_create(_vr_uk_stream_create_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_STREAM_CREATE, args);
}

_mali_osk_errcode_t _mali_uku_stream_destroy(_vr_uk_stream_destroy_s *args)
{
	close(args->fd);
	return _MALI_OSK_ERR_OK;
}

_mali_osk_errcode_t _mali_uku_fence_validate(_vr_uk_fence_validate_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_FENCE_VALIDATE, args);
}

_mali_osk_errcode_t _mali_uku_mem_mmap( _vr_uk_mem_mmap_s *args )
{
	int flags;
    MALI_CHECK_NON_NULL(args, _MALI_OSK_ERR_INVALID_ARGS);

    /* The Linux implementation uses the mmap mechanism at this time (as it provides functionality to reserve the process virtual memory necessary).
     * The mmap handler in the driver will route it to the corresponding _mali_ukk_mem_mmap function in the kernel driver.
     * Other OSs are most likely to use the ioctl interface.
     */

    /* check for a valid file descriptor */
	/** @note manual type safety check-point */
	MALI_CHECK(-1 != (int)args->ctx, _MALI_OSK_ERR_INVALID_ARGS);

	switch( args->cache_settings )
	{
		case VR_CACHE_GP_READ_ALLOCATE:
		flags = MAP_PRIVATE;
		MALI_DEBUG_PRINT(3, ("%s - GP L2 cache alloc. Size: %d kB\n", __FUNCTION__, args->size/1024));
		break;
		case VR_CACHE_STANDARD:
		/* falltrough */
		default:
		MALI_DEBUG_ASSERT(args->cache_settings==VR_CACHE_STANDARD, ("Illegal cache settings."));
		flags = MAP_SHARED;
		MALI_DEBUG_PRINT(3, ("%s - Standard alloc. Size: %d kB\n", __FUNCTION__, args->size/1024));
	}

/* MAP_SHARED */
    args->mapping = mmap( 0, args->size, PROT_WRITE | PROT_READ, flags, (int)args->ctx, args->phys_addr );

    args->cookie = 0; /* Cookie is not used in munmap */

    MALI_CHECK(MAP_FAILED != args->mapping, _MALI_OSK_ERR_NOMEM);

    MALI_SUCCESS;
}

_mali_osk_errcode_t  _mali_uku_mem_munmap( _vr_uk_mem_munmap_s *args )
{
    int rc;

    MALI_CHECK_NON_NULL(args, _MALI_OSK_ERR_INVALID_ARGS);
    MALI_CHECK_NON_NULL(args->mapping, _MALI_OSK_ERR_INVALID_ARGS);

    /* check for a valid file descriptor */
	/** @note manual type safety check-point */
	MALI_CHECK(-1 != (int)args->ctx, _MALI_OSK_ERR_INVALID_ARGS);

    rc = munmap( args->mapping, args->size );

    MALI_CHECK(-1 != rc, _MALI_OSK_ERR_FAULT);

    MALI_SUCCESS;
}

_mali_osk_errcode_t _mali_uku_query_mmu_page_table_dump_size( _vr_uk_query_mmu_page_table_dump_size_s *args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_QUERY_MMU_PAGE_TABLE_DUMP_SIZE, args);
}

_mali_osk_errcode_t _mali_uku_dump_mmu_page_table( _vr_uk_dump_mmu_page_table_s * args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_DUMP_MMU_PAGE_TABLE, args);
}

_mali_osk_errcode_t _mali_uku_map_external_mem( _vr_uk_map_external_mem_s *args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_MAP_EXT, args);
}

_mali_osk_errcode_t _mali_uku_unmap_external_mem( _vr_uk_unmap_external_mem_s *args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_UNMAP_EXT, args);
}

#if MALI_USE_DMA_BUF
_mali_osk_errcode_t _mali_uku_dma_buf_get_size( _vr_uk_dma_buf_get_size_s *args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_DMA_BUF_GET_SIZE, args);
}

_mali_osk_errcode_t _mali_uku_attach_dma_buf( _vr_uk_attach_dma_buf_s *args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_ATTACH_DMA_BUF, args);
}

_mali_osk_errcode_t _mali_uku_release_dma_buf( _vr_uk_release_dma_buf_s *args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_RELEASE_DMA_BUF, args);
}
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
_mali_osk_errcode_t _mali_uku_attach_ump_mem( _vr_uk_attach_ump_mem_s *args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_ATTACH_UMP, args);
}

_mali_osk_errcode_t _mali_uku_release_ump_mem( _vr_uk_release_ump_mem_s *args )
{
	return mali_driver_ioctl(args->ctx, VR_IOC_MEM_RELEASE_UMP, args);
}
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER */

_mali_osk_errcode_t _mali_uku_pp_start_job(_vr_uk_pp_start_job_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PP_START_JOB, args);
}

_mali_osk_errcode_t _mali_uku_get_pp_number_of_cores(_vr_uk_get_pp_number_of_cores_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PP_NUMBER_OF_CORES_GET, args);
}

_mali_osk_errcode_t _mali_uku_get_pp_core_version(_vr_uk_get_pp_core_version_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PP_CORE_VERSION_GET, args);
}

void _mali_uku_pp_job_disable_wb( _vr_uk_pp_disable_wb_s *args )
{
	(void)mali_driver_ioctl(args->ctx, VR_IOC_PP_DISABLE_WB, args);
}

_mali_osk_errcode_t _mali_uku_gp_start_job(_vr_uk_gp_start_job_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_GP2_START_JOB, args);
}

_mali_osk_errcode_t _mali_uku_get_gp_number_of_cores(_vr_uk_get_gp_number_of_cores_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_GP2_NUMBER_OF_CORES_GET, args);
}

_mali_osk_errcode_t _mali_uku_get_gp_core_version(_vr_uk_get_gp_core_version_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_GP2_CORE_VERSION_GET, args);
}

_mali_osk_errcode_t _mali_uku_gp_suspend_response(_vr_uk_gp_suspend_response_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_GP2_SUSPEND_RESPONSE, args);
}

_mali_osk_errcode_t _mali_uku_profiling_start(_vr_uk_profiling_start_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PROFILING_START, args);
}

MALI_NOTRACE _mali_osk_errcode_t _mali_uku_profiling_add_event(_vr_uk_profiling_add_event_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PROFILING_ADD_EVENT, args);
}

_mali_osk_errcode_t _mali_uku_profiling_stop(_vr_uk_profiling_stop_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PROFILING_STOP, args);
}

_mali_osk_errcode_t _mali_uku_profiling_get_event(_vr_uk_profiling_get_event_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PROFILING_GET_EVENT, args);
}

_mali_osk_errcode_t _mali_uku_profiling_clear(_vr_uk_profiling_clear_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PROFILING_CLEAR, args);
}

_mali_osk_errcode_t _mali_uku_profiling_report_sw_counters(_vr_uk_sw_counters_report_s *args)
{
	return mali_driver_ioctl(args->ctx, VR_IOC_PROFILING_REPORT_SW_COUNTERS, args);
}

void _mali_uku_vsync_event_report(_vr_uk_vsync_event_report_s *args)
{
	mali_driver_ioctl(args->ctx, VR_IOC_VSYNC_EVENT_REPORT, args);
}

static _mali_osk_errcode_t mali_driver_ioctl(void *context, u32 command, void *args)
{
    MALI_CHECK_NON_NULL(args, _MALI_OSK_ERR_INVALID_ARGS);

    /* check for a valid file descriptor */
	/** @note manual type safety check-point */
	MALI_CHECK(-1 != (int)context, -1);

    /* call ioctl handler of driver */
    if (0 != ioctl((int)context, command, args))
    {
        MALI_DEBUG_PRINT(5, ("mali_driver_ioctl: failure - cmd 0x%x(type=0x%x nr=0x%x) rc 0x%x\n", command, _IOC_TYPE(command), _IOC_NR(command), errno));
        return map_errcode(errno);
    }
    return _MALI_OSK_ERR_OK;
}

static _mali_osk_errcode_t map_errcode(int err)
{
    switch(err)
    {
        case 0: return _MALI_OSK_ERR_OK;
        case EFAULT: return _MALI_OSK_ERR_FAULT;
        case ENOTTY: return _MALI_OSK_ERR_INVALID_FUNC;
        case EINVAL: return _MALI_OSK_ERR_INVALID_ARGS;
        case ENOMEM: return _MALI_OSK_ERR_NOMEM;
        case ETIMEDOUT: return _MALI_OSK_ERR_TIMEOUT;
        case ENOENT: return _MALI_OSK_ERR_ITEM_NOT_FOUND;
        default: return _MALI_OSK_ERR_FAULT;
    }
}
