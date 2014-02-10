/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_FRAGMENT_BACKEND_CONTEXT_H
#define GLES_FRAGMENT_BACKEND_CONTEXT_H

#include "mali_system.h"

#include "../gles_config.h"
#include "../gles_context.h"
#include <mali_rsw.h>
#include "shared/binary_shader/bs_object.h"
#include "base/mali_debug.h"

typedef struct gles_fb_context
{
	/* These values are modified by the vertex memory management system */
	mali_addr position_addr;             /**< Buffer of vertex data, filled by the GP, read by the PP */
	mali_addr varyings_addr;             /**< Varyings per vertex, filled by the GP, read by the PP */

	/* set after the RSW has been commited, used by the GB */
	mali_addr rsw_addr;

	/* set depending on the index range of the current draw call and the amount of
	* vertices processed in previous drawcall */
	u32 prev_vertex_count;   /**< Number of vertices in the last draw call */

	u32             current_td_remap_table_address; 	/**< The address of the remap array for TD's */

	mali_addr       current_uniform_remap_table_addr;           /**< The address of the table that points to uniform-blocks */

	mali_base_frame_id fb_uniform_last_frame_id;		/**< A check of the frame id on last frame updated */

} gles_fb_context;

#endif /* GLES_FRAGMENT_BACKEND_CONTEXT_H */

