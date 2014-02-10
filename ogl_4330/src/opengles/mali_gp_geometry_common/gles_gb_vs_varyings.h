/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_GB_VS_VARYINGS_H
#define GLES_GB_VS_VARYINGS_H

#include <mali_system.h>
#include "gles_geometry_backend_context.h"

/**
 * Set-up the output streams
 * @param ctx       The pointer to the GLES context
 * @param streams   The array of the out streams
 * @param stride_array The array to store the stride number from the output streams
 * @return          Error code, either MALI_ERR_NO_ERROR, or MALI_ERR_OUT_OF_MEMORY.
 */
MALI_CHECK_RESULT mali_err_code _gles_gb_setup_output_streams(gles_gb_context *gb_ctx, u32 *streams, u32 *stride_array);

#endif /* GLES_GB_VS_VARYINGS_H */
