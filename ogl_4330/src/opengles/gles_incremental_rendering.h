/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_INCREMENTAL_RENDERING_H_
#define _GLES_INCREMENTAL_RENDERING_H_

#include <mali_system.h>
#include <shared/m200_gp_frame_builder.h>
#include <shared/m200_incremental_rendering.h>
#include "gles_context.h"
#include "gles_framebuffer_object.h"

/**
 * Performs incremental rendering and then re-initializes the gles frame and the framepool since incremental
 * rendering triggers a frame reset.
 *
 * @see _mali_incremental_render
 * @see _gles_begin_frame
 *
 * @param ctx           The gles_context to re-initialize
 * @param fbo           The fbo to perform incremental rendering on, thereby
 *                      resolving all previously registered draw calls/geometry
 */
mali_err_code _gles_incremental_render( gles_context       *ctx,
                                        gles_framebuffer_object *fbo ) MALI_CHECK_RESULT;

#endif /* _GLES_INCREMENTAL_RENDERING_H_ */
