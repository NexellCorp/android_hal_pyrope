/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _EGL_COMMON_MEMORY_H_
#define _EGL_COMMON_MEMORY_H_

#include <sys/ioctl.h>
#include <unistd.h>
#include "base/mali_runtime.h"
#include "base/mali_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * egl_memory_handle refers to ump_handle if MALI_USE_UNIFIED_MEMORY_PROVIDER
 *                   refers to dma_buf    if MALI_USE_DMA_BUF
 * egl_buffer_name	refers to ump_secure_id if MALI_USE_UNIFIED_MEMORY_PROVIDER
 *					refers to gem_name		if MALI_USE_DMA_BUF
 */
typedef void *       egl_memory_handle;
typedef unsigned int egl_buffer_name;

#define EGL_INVALID_MEMORY_HANDLE   NULL
#define EGL_INVALID_BUFFER_NAME     ((egl_buffer_name)-1)

/**
 *@brief create egl_memory_handle from egl_buffer_name
 *@param drm_fd for x11, it's drm_fd, for others, please pass -1
 *@param buf_name egl_buffer_name, refers to either ump_secure_id or gem_name
 *@return egl_memory_handle
 */
egl_memory_handle _egl_memory_create_handle_from_name( int drm_fd, egl_buffer_name buf_name );

/**
 *@brief create mali_mem_handle from egl_memory_handle
 *@param ctx mali base context handle
 *@param mem_handle egl memory handle, from the handle create mali_memory
 *@param offset offset to the buffer
 *@return mali_mem_handle
 */
mali_mem_handle _egl_memory_create_mali_memory_from_handle( mali_base_ctx_handle ctx, egl_memory_handle mem_handle, u32 offset );

/**
 *@brief get egl_memory_handle from mali_mem_handle
 *@param mali_mem mali memory handle
 *@return egl_memory_handle
 */
egl_memory_handle _egl_memory_get_handle_from_mali_memory( mali_mem_handle mali_mem );

/**
 *@brief get egl_buffer_name from egl_memory_handle
 *@param drm_fd for x11, it's drm_fd, for others, please pass -1
 *@param mem_handle egl memory handle refers to ump_handle in case of ump, or dma_buf fd in case of dma_buf
 *@return egl_buffer_name
 */
egl_buffer_name _egl_memory_get_name_from_handle( int drm_fd, egl_memory_handle mem_handle );

/**
 *@brief release reference on egl_memory_handle
 *@param mem_handle egl memory handle refers to ump_handle in case of ump, or dma_buf fd in case of dma_buf
 *@return void
 */
void _egl_memory_release_reference( egl_memory_handle mem_handle );

#ifdef __cplusplus
}
#endif

#endif /* _EGL_COMMON_MEMORY_H_ */

