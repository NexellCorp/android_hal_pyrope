/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_viewport.h
 * @brief Contains the gles_viewport structure that is used by both the OpenGL ES 1.1 and 2.0 APIs.
 */

#ifndef _GLES_VIEWPORT_H_
#define _GLES_VIEWPORT_H_

struct gles_context;

/**
 * structure for keeping track of viewport dimensions
 */
typedef struct gles_viewport
{
	GLint x, y;				/**< Viewport offset */
	GLint width, height;			/**< Viewport size */

	GLftype    z_nearfar[2];		/**< depth range state: viewport-mapping depends on depth range */
	mali_float z_minmax[2];			/**< depth range state: RSW and PLBU need min/max of near/far */

	mali_float viewport_transform[8];	/**< viewport transform derived from near/far planes and viewport */

	mali_float half_width, half_height;	/**< half the width/height of the viewport */
	mali_float offset_x, offset_y;		/**< the x/y coordinates (in window coordinates) of the viewport center */

} gles_viewport;

/**
 * Sets the current depth range
 * @param ctx The pointer to the GLES context
 * @param z_near near bound of depth range
 * @param z_far far bound of depth range
 * @note This implements the GLES-entrypoint glDepthRangef/x().
 */
void _gles_depth_range( struct gles_context *ctx, GLftype z_near, GLftype z_far );

#endif /* _GLES_VIEWPORT_H_ */
