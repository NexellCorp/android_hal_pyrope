/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef M200_GP_FRAME_BUILDER_DEPENDENCY_H
#define M200_GP_FRAME_BUILDER_DEPENDENCY_H

#include <shared/m200_gp_frame_builder.h>
#include "m200_gp_frame_builder_struct.h"

/**
 * @brief -
 * @param base_ctx The base context with which to perform all Mali resource (pp/gp jobs, mali mem) allocations.
 * @param status Error status.
 *
 */
void _mali_frame_builder_frame_dependency_activated(mali_base_ctx_handle base_ctx, void * owner, mali_ds_error status);

/**
 * @brief -
 * @param base_ctx The base context with which to perform all Mali resource (pp/gp jobs, mali mem) allocations.
 * @param status Error status.
 *
 */
mali_ds_release _mali_frame_builder_frame_dependency_release(mali_base_ctx_handle base_ctx, void * owner, mali_ds_error status);

/**
 * @brief -
 * @param status Error status.
 *
 */
mali_ds_resource_handle _mali_frame_builder_surface_do_copy_on_write(void* resource_owner, void* consumer_owner);

/**
 * @brief -
 * @param base_ctx The base context with which to perform all Mali resource (pp/gp jobs, mali mem) allocations.
 * @param status Error status.
 *
 */
void _mali_frame_builder_activate_gp_consumer_callback ( mali_base_ctx_handle base_ctx, void * owner, mali_ds_error status );

#endif /* M200_GP_FRAME_BUILDER_DEPENDENCY_H */
