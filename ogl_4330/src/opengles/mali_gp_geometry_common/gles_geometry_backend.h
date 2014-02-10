/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_GEOMETRY_BACKEND_H
#define GLES_GEOMETRY_BACKEND_H

/**
 * @file gles_geometry_backend.h
 */

/* used for splitting glDrawArrays-calls */
#define GLES_GB_MAX_VERTICES (1 << 16)

/* used when the rsw/varying base address is set for each drawcall,
 * in which case the RSW index is always 0
 */
#define GLES_GB_RSW_INDEX_ZERO (0)

/* used when the rsw/varying base address is set for each drawcall,
 * in which case the post index bias is always 0
 */
#define GLES_GB_RSW_POST_INDEX_BIAS_ZERO (0)


#include <mali_system.h>
#include <regs/MALI200/mali_render_regs.h>

struct gles_context;
struct gles_gb_context;
struct mali_frame_builder;

/**
 * Calculates the viewport based on the gles state and frame dimensions. Also scales the viewport
 * there is 16xAA is enabled
 * @param ctx The pointer to the GLES context
 * @param vp Pointer to the viewport to transform
 */
void _gles_gb_calculate_vs_viewport_transform(struct gles_context * const ctx, float * const vp);


#endif /* GLES_GEOMETRY_BACKEND_H */
