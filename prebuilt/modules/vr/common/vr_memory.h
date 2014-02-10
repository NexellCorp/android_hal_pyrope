/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_MEMORY_H__
#define __VR_MEMORY_H__

#include "vr_osk.h"
#include "vr_session.h"

/** @brief Initialize VR memory subsystem
 *
 * Allocate and initialize internal data structures. Must be called before
 * allocating VR memory.
 *
 * @return On success _VR_OSK_ERR_OK, othervise some error code describing the error.
 */
_vr_osk_errcode_t vr_memory_initialize(void);

/** @brief Terminate VR memory system
 *
 * Clean up and release internal data structures.
 */
void vr_memory_terminate(void);

/** @brief Start new VR memory session
 *
 * Allocate and prepare session specific memory allocation data data. The
 * session page directory, lock, and descriptor map is set up.
 *
 * @param vr_session_data pointer to the session data structure
 */
_vr_osk_errcode_t vr_memory_session_begin(struct vr_session_data *vr_session_data);

/** @brief Close a VR memory session
 *
 * Release session specific memory allocation related data.
 *
 * @param vr_session_data pointer to the session data structure
 */
void vr_memory_session_end(struct vr_session_data *vr_session_data);

/** @brief Allocate a page table page
 *
 * Allocate a page for use as a page directory or page table. The page is
 * mapped into kernel space.
 *
 * @return _VR_OSK_ERR_OK on success, othervise an error code
 * @param table_page GPU pointer to the allocated page
 * @param mapping CPU pointer to the mapping of the allocated page
 */
_vr_osk_errcode_t vr_mmu_get_table_page(u32 *table_page, vr_io_address *mapping);

/** @brief Release a page table page
 *
 * Release a page table page allocated through \a vr_mmu_get_table_page
 *
 * @param pa the GPU address of the page to release
 */
void vr_mmu_release_table_page(u32 pa);


/** @brief Parse resource and prepare the OS memory allocator
 *
 * @param size Maximum size to allocate for VR GPU.
 * @return _VR_OSK_ERR_OK on success, otherwise failure.
 */
_vr_osk_errcode_t vr_memory_core_resource_os_memory(u32 size);

/** @brief Parse resource and prepare the dedicated memory allocator
 *
 * @param start Physical start address of dedicated VR GPU memory.
 * @param size Size of dedicated VR GPU memory.
 * @return _VR_OSK_ERR_OK on success, otherwise failure.
 */
_vr_osk_errcode_t vr_memory_core_resource_dedicated_memory(u32 start, u32 size);

vr_allocation_engine vr_mem_get_memory_engine(void);

#endif /* __VR_MEMORY_H__ */
