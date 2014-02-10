/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_multisampling.h
 * @brief Defines the multisampling structures and functions that are used by both the OpenGL ES drivers.
 */

#ifndef _GLES_MULTISAMPLING_H_
#define _GLES_MULTISAMPLING_H_

#include <gles_base.h>
#include <gles_ftype.h>

struct gles_context;

/**
 * @brief Sets the sample-coverage values
 * @param ctx The pointer to the GLES context
 * @param value Specifies the coverage of the modification mask
 * @param invert Specifies if the modification mask is inverted or not
 * @note This implements the GLES-entrypoints glSampleCoverage()
 */
void _gles_sample_coverage( struct gles_context *ctx, GLftype value, GLboolean invert );

#endif /* _GLES_MULTISAMPLING_H_ */
