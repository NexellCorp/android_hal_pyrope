/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_ARCH_MAIN_H_
#define _BASE_ARCH_MAIN_H_

#include <base/mali_types.h>
#include "mali_osu.h"
#include "mali_uku.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The user-kernel context shared among the common base_arch_ modules
 */
extern void *mali_uk_ctx;

/**
 * Arch common open handler
 * Handles the connection to the device driver.
 * Keeps track of the usage count and opens
 * the file descriptor when needed.
 * @return Standard mali_err_code
 */
mali_err_code _mali_base_arch_open(void) MALI_CHECK_RESULT;

/**
 * Arch common close handler
 * Keeps track of the usage count and closes
 * the file descriptor when no references exists.
 */
void _mali_base_arch_close(void);

/**
 * Get memory info reported by the device driver
 * Access the linked list of memory info structs
 * retrieved by the device driver
 * @return Pointer to the first entry, NULL if no info
 */
_vr_mem_info * _mali_base_arch_get_mem_info(void);

/**
 * PP event handler
 * Called by arch main when a PP event is received from the device driver
 * @param event_id Event id received
 * @param event_data Pointer to user defined event data block
 */
void _mali_base_arch_pp_event_handler(u32 event_id, void * event_data);

/**
 * GP event handler
 * Called by arch main when a GP event is received from the device driver
 * @param event_id Event id received
 * @param event_data Pointer to user defined event data block
 */
void _mali_base_arch_gp_event_handler(u32 event_id, void * event_data);

#if !defined(USING_MALI200)
/**
 * Check if Mali-400 MP L2 counters needs to be reset
 * Mali-400 MP L2 counters needs to be reset when the first GP or PP job starts.
 * This function tracks this.
 * @return MALI_TRUE if there has been no previous jobs with Mali-400 MP L2 counter enabled.
 */
mali_bool arch_l2_counters_needs_reset(void);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_BASE_ARCH_MAIN_H_ */
