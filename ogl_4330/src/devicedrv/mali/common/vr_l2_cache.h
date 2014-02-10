/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_KERNEL_L2_CACHE_H__
#define __VR_KERNEL_L2_CACHE_H__

#include "vr_osk.h"

#define VR_MAX_NUMBER_OF_L2_CACHE_CORES  3
/* Maximum 1 GP and 4 PP for an L2 cache core (VR-400 Quad-core) */
#define VR_MAX_NUMBER_OF_GROUPS_PER_L2_CACHE 5

struct vr_l2_cache_core;
struct vr_group;

_vr_osk_errcode_t vr_l2_cache_initialize(void);
void vr_l2_cache_terminate(void);

struct vr_l2_cache_core *vr_l2_cache_create(_vr_osk_resource_t * resource);
void vr_l2_cache_delete(struct vr_l2_cache_core *cache);

void vr_l2_cache_power_is_enabled_set(struct vr_l2_cache_core *core, vr_bool power_is_enabled);
vr_bool vr_l2_cache_power_is_enabled_get(struct vr_l2_cache_core * core);

u32 vr_l2_cache_get_id(struct vr_l2_cache_core *cache);

vr_bool vr_l2_cache_core_set_counter_src0(struct vr_l2_cache_core *cache, u32 counter);
vr_bool vr_l2_cache_core_set_counter_src1(struct vr_l2_cache_core *cache, u32 counter);
u32 vr_l2_cache_core_get_counter_src0(struct vr_l2_cache_core *cache);
u32 vr_l2_cache_core_get_counter_src1(struct vr_l2_cache_core *cache);
void vr_l2_cache_core_get_counter_values(struct vr_l2_cache_core *cache, u32 *src0, u32 *value0, u32 *src1, u32 *value1);
struct vr_l2_cache_core *vr_l2_cache_core_get_glob_l2_core(u32 index);
u32 vr_l2_cache_core_get_glob_num_l2_cores(void);

void vr_l2_cache_reset(struct vr_l2_cache_core *cache);
void vr_l2_cache_reset_all(void);

struct vr_group *vr_l2_cache_get_group(struct vr_l2_cache_core *cache, u32 index);

_vr_osk_errcode_t vr_l2_cache_invalidate_all(struct vr_l2_cache_core *cache);
vr_bool vr_l2_cache_invalidate_all_conditional(struct vr_l2_cache_core *cache, u32 id);
void vr_l2_cache_invalidate_all_force(struct vr_l2_cache_core *cache);
_vr_osk_errcode_t vr_l2_cache_invalidate_pages(struct vr_l2_cache_core *cache, u32 *pages, u32 num_pages);
void vr_l2_cache_invalidate_pages_conditional(u32 *pages, u32 num_pages);

vr_bool vr_l2_cache_lock_power_state(struct vr_l2_cache_core *cache);
void vr_l2_cache_unlock_power_state(struct vr_l2_cache_core *cache);

#endif /* __VR_KERNEL_L2_CACHE_H__ */
