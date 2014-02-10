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
 * @file mali_uku.h
 * Defines the user-side interface of the user-kernel interface
 */

#ifndef __MALI_UKU_H__
#define __MALI_UKU_H__

#ifdef NEXELL_FEATURE_ENABLE
#include "vr_utgard_uk_types.h"
#else
#include "mali_utgard_uk_types.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @addtogroup uddapi Unified Device Driver (UDD) APIs
 *
 * @{
 */

/**
 * @addtogroup u_k_api UDD User/Kernel Interface (U/K) APIs
 *
 * @{
 */

/** @addtogroup _vr_uk_context U/K Context management
 * @{ */

/** Make a U/K call to _mali_ukk_open().
 * @param context pointer to storage to return a (void*)context handle.
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_open( void **context );

/** Make a U/K call to _mali_ukk_close().
 * @param context pointer a stored (void*)context handle.
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_close( void **context );

/** @} */ /* end group _vr_uk_context */

/** @addtogroup _vr_uk_core U/K Core
 * @{ */

/** Make a U/K call to _mali_ukk_wait_for_notification().
 * @param args see _vr_uk_wait_for_notification_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_wait_for_notification( _vr_uk_wait_for_notification_s *args );

/** Make a U/K call to _mali_ukk_post_notification().
 * @param args see _vr_uk_post_notification_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_post_notification( _vr_uk_post_notification_s *args );

/** Make a U/K call to _mali_ukk_get_api_version().
 * @param args see _vr_uk_get_api_version_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_get_api_version( _vr_uk_get_api_version_s *args );

/* Make a U/K call to _mali_uku_get_user_setting().
 * @param args see _vr_uk_get_user_setting_s in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_get_user_setting(_vr_uk_get_user_setting_s *args);

/* Make a U/K call to _mali_uku_get_user_settings().
 * @param args see _vr_uk_get_user_settings_s in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_get_user_settings(_vr_uk_get_user_settings_s *args);

/* Make a U/K call to _mali_uku_stream_create().
 * @params args see _vr_uk_stream_create_s in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_stream_create(_vr_uk_stream_create_s *args);

/* Make a U/K call to _mali_uku_stream_destroy().
 * @params args see _vr_uk_stream_destroy_s in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_stream_destroy(_vr_uk_stream_destroy_s *args);

/* Make a U/K call to _mali_uku_fence_validate().
 * @params args see _vr_uk_fence_validate in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_fence_validate(_vr_uk_fence_validate_s *args);

/** @} */ /* end group _vr_uk_core */

/** @addtogroup _vr_uk_memory U/K Memory
 * @{ */

/** Make a U/K call to _mali_ukk_mem_mmap().
 * @param args see _vr_uk_mem_mmap_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_mem_mmap( _vr_uk_mem_mmap_s *args );

/** Make a U/K call to _mali_ukk_mem_munmap().
 * @param args see _vr_uk_mem_munmap_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_mem_munmap( _vr_uk_mem_munmap_s *args );

/** Make a U/K call to _mali_ukk_query_mmu_page_table_dump_size().
 * @param args see _vr_uk_query_mmu_page_table_dump_size_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_query_mmu_page_table_dump_size( _vr_uk_query_mmu_page_table_dump_size_s *args );

/** Make a U/K call to _mali_ukk_dump_mmu_page_table().
 * @param args see _vr_uk_dump_mmu_page_table_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_dump_mmu_page_table( _vr_uk_dump_mmu_page_table_s * args );

/** Make a U/K call to _mali_ukk_map_external_mem().
 * @param args see _vr_uk_map_external_mem_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_map_external_mem( _vr_uk_map_external_mem_s *args );

/** Make a U/K call to _mali_ukk_unmap_external_mem().
 * @param args see _vr_uk_unmap_external_mem_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_unmap_external_mem( _vr_uk_unmap_external_mem_s *args );

#if MALI_USE_DMA_BUF
/** Make a U/K call to _mali_ukk_dma_buf_get_size().
 * @param args see _vr_uk_dma_buf_get_size_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_dma_buf_get_size( _vr_uk_dma_buf_get_size_s *args );

/** Make a U/K call to _mali_ukk_attach_dma_buf().
 * @param args see _vr_uk_attach_dma_buf_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_attach_dma_buf( _vr_uk_attach_dma_buf_s *args );

/** Make a U/K call to _mali_ukk_release_dma_buf().
 * @param args see _vr_uk_release_dma_buf_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_release_dma_buf( _vr_uk_release_dma_buf_s *args );
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
/** Make a U/K call to _mali_ukk_attach_ump_mem().
 * @param args see _vr_uk_attach_ump_mem_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_attach_ump_mem( _vr_uk_attach_ump_mem_s *args );

/** Make a U/K call to _mali_ukk_release_ump_mem().
 * @param args see _vr_uk_release_ump_mem_s in mali_utgard_uk_types.h
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_release_ump_mem( _vr_uk_release_ump_mem_s *args );
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER */

/** Make a U/K call to _mali_ukk_va_to_mali_pa().
 * @param args see _vr_uk_va_to_mali_pa_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_va_to_mali_pa( _vr_uk_va_to_vr_pa_s * args );

/** @} */ /* end group _vr_uk_memory */

/** @addtogroup _vr_uk_pp U/K Fragment Processor
 * @{ */

/** Make a U/K call to _mali_ukk_pp_start_job().
 * @param args see _vr_uk_pp_start_job_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_pp_start_job( _vr_uk_pp_start_job_s *args );

/** Make a U/K call to _mali_ukk_get_pp_number_of_cores().
 * @param args see _vr_uk_get_pp_number_of_cores_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_get_pp_number_of_cores( _vr_uk_get_pp_number_of_cores_s *args );

/** Make a U/K call to _mali_ukk_get_pp_core_version().
 * @param args see _vr_uk_get_pp_core_version_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_get_pp_core_version( _vr_uk_get_pp_core_version_s *args );

/** Make a U/K call to _mali_ukk_pp_job_disable_wb().
 * @param args see _vr_uk_pp_disable_wb_s in "mali_utgard_uk_types.h"
 */
void _mali_uku_pp_job_disable_wb( _vr_uk_pp_disable_wb_s *args );

/** @} */ /* end group _vr_uk_pp */

/** @addtogroup _vr_uk_gp U/K Vertex Processor
 * @{ */

/** Make a U/K call to _mali_ukk_gp_start_job().
 * @param args see _vr_uk_gp_start_job_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_gp_start_job( _vr_uk_gp_start_job_s *args );

/** Make a U/K call to _mali_ukk_get_gp_number_of_cores().
 * @param args see _vr_uk_get_gp_number_of_cores_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_get_gp_number_of_cores( _vr_uk_get_gp_number_of_cores_s *args );

/** Make a U/K call to _mali_ukk_get_gp_core_version().
 * @param args see _vr_uk_get_gp_core_version_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_get_gp_core_version( _vr_uk_get_gp_core_version_s *args );

/** Make a U/K call to _mali_ukk_gp_suspend_response().
 * @param args see _vr_uk_gp_suspend_response_s in "mali_utgard_uk_types.h"
 * @return _MALI_OSK_ERR_OK on success, otherwise a suitable _mali_osk_errcode_t on failure.
 */
_mali_osk_errcode_t _mali_uku_gp_suspend_response( _vr_uk_gp_suspend_response_s *args );

/** @} */ /* end group _vr_uk_gp */

#if MALI_TIMELINE_PROFILING_ENABLED
/** @addtogroup _vr_uk_profiling U/K Timeline profiling module
 * @{ */

/* Make a U/K call to _mali_ukk_profiling_start().
 * @param args see _vr_uk_profiling_start_s in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_profiling_start(_vr_uk_profiling_start_s *args);

/* Make a U/K call to _mali_uku_profiling_add_event().
 * @param args see _vr_uk_profiling_add_event_s in "mali_utgard_uk_types.h"
 */
MALI_NOTRACE _mali_osk_errcode_t _mali_uku_profiling_add_event(_vr_uk_profiling_add_event_s *args);

/* Make a U/K call to _mali_uku_profiling_stop().
 * @param args see _vr_uk_profiling_stop_s in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_profiling_stop(_vr_uk_profiling_stop_s *args);

/* Make a U/K call to _mali_uku_profiling_get_event().
 * @param args see _vr_uk_profiling_get_event_s in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_profiling_get_event(_vr_uk_profiling_get_event_s *args);

/* Make a U/K call to _mali_uku_profiling_clear().
 * @param args see _vr_uk_profiling_clear_s in "mali_utgard_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_profiling_clear(_vr_uk_profiling_clear_s *args);

#endif /* MALI_TIMELINE_PROFILING_ENABLED */

#if MALI_SW_COUNTERS_ENABLED

/** @brief Make a U/K call to _mali_uku_sw_counters_report().
 * @param args see _vr_uk_sw_counters_report_s in "mali_uk_types.h"
 */
_mali_osk_errcode_t _mali_uku_profiling_report_sw_counters(_vr_uk_sw_counters_report_s *args);

#endif /* MALI_SW_COUNTERS_ENABLED */

/** @} */ /* end group _vr_uk_profiling */

/** @addtogroup _vr_uk_vsync U/K VSYNC event reporting module
 * @{ */

/** @brief Make a U/K call to _mali_uku_vsync_event_report().
 * @param args see _vr_uk_vsync_event_report_s in "mali_utgard_uk_types.h"
 */
void _mali_uku_vsync_event_report(_vr_uk_vsync_event_report_s *args);

/* @} */ /* end group _vr_uk_vsync */

/** @} */ /* end group u_k_api */

/** @} */ /* end group uddapi */

#ifdef __cplusplus
}
#endif

#endif /* __MALI_UKU_H__ */
