/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_M200_RSW_SETUP_H
#define GLES_M200_RSW_SETUP_H

#include "gles_fb_context.h"

struct gles_context;

/**
 * @brief Set up all fields in a rsw, based on the data in the gles fb context and drawing mode.
 * @param state_common The common part of the OpenGL ES state
 * @param fb_ctx The fragment backend context
 * @param rsw The rsw we want to fill in
 * @param mode The current drawmode
 */
void _gles_m200_set_rsw_parameters( struct gles_context *ctx, gles_fb_context *fb_ctx, m200_rsw *rsw, u32 mode );

#endif /* GLES_M200_RSW_SETUP_H */

