/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_transform.h
 * @brief All routines related to transform-state. matrix-manipulations and other routines that require the context-pointer is defined in gles_matrix.h
 */

#ifndef _GLES1_TRANSFORM_H_
#define _GLES1_TRANSFORM_H_

#include "../gles_base.h"
#include "../gles_config.h"
#include "../gles_util.h"
#include "../gles_state.h"
#include "../gles_float_type.h"

/**
  * structure for keeping track of all the matrices, clip planes and other GL-state regarding transformation
  */
typedef struct gles1_transform
{
	/* matrix stacks */
	mali_matrix4x4 modelview_matrix[GLES1_MODELVIEW_MATRIX_STACK_DEPTH];	/**< The modelview-matrix-stack */
	mali_matrix4x4 projection_matrix[GLES1_PROJECTION_MATRIX_STACK_DEPTH];	/**< The projection-matrix-stack */
	mali_matrix4x4 texture_matrix[GLES_MAX_TEXTURE_UNITS][GLES1_TEXTURE_MATRIX_STACK_DEPTH]; /**< The texture-matrices stacks */

	/* matrix stacks */
	mali_matrix4x4 *current_matrix;	            /**< The current matrix*/
	mali_bool *current_matrix_is_identity;	    /**< The current matrix*/
	u32 current_texture_matrix_id;              /**< What is the index of the current_texture_matrix(only set if this is a texture_matrix */
	u32 texture_identity_field;                 /**< The field saying which texture_units have identity matrix, ( texture_identity_field & 0x2 ) == 0x0 --> texture_unit 1 is identity */
	u32 texture_transform_field;                 /**< The field saying which texture_units have some transform matrix, ( texture_transform_field & 0x2 ) == 0x0 --> texture_unit 1 has a transform enabled */

	mali_bool      modelview_matrix_identity[GLES1_MODELVIEW_MATRIX_STACK_DEPTH];   /**< A flag to tell if the modelview-matrix is identity */
	mali_bool      projection_matrix_identity[GLES1_PROJECTION_MATRIX_STACK_DEPTH];   /**< A flag to tell if the projection_matrix is identity */
	mali_bool      texture_matrix_identity[GLES_MAX_TEXTURE_UNITS][GLES1_TEXTURE_MATRIX_STACK_DEPTH];   /**< A flag to tell if the texture_matrix is identity */

	/* matrix stack pointers
	   NOTE: Since this is stack depth rather than an index pointer, it is 1-based rather than 0-based
	         Stack accesses via these indices must subtract 1
	*/
	GLuint         modelview_matrix_stack_depth;                       /**< The current modelview-matrix-stack-depth */
	GLuint         projection_matrix_stack_depth;                      /**< The current projection-matrix-stack-depth */
	GLuint         texture_matrix_stack_depth[GLES_MAX_TEXTURE_UNITS]; /**< The current texture-matrices stack-depths */

	GLenum         matrix_mode;                                        /**< The current matrix-mode */
	GLboolean      normalize;                                          /**< Is GL_NORMALIZE enabled? */
	GLboolean      rescale_normal;                                     /**< Is GL_RESCALE_NORMAL enabled? */
	GLftype        clip_plane[GLES1_MAX_CLIP_PLANES][4];               /**< The different user defined clip-planes */

	/* matrix palette */
	mali_matrix4x4 matrix_palette[GLES1_MAX_PALETTE_MATRICES_OES];     /**< The matrix palette */
	mali_bool      matrix_palette_identity[GLES1_MAX_PALETTE_MATRICES_OES];     /**< The matrix palette identity flag */
	GLuint         matrix_palette_current;                             /**< The current matrix palette selected */

	mali_matrix4x4 projection_viewport_matrix;                         /**< The projection-viewport matrix */
	mali_matrix4x4 modelview_projection_matrix;                        /**< The modelview*projection-viewport matrix */

} gles1_transform;

/**
 * @brief Initialize the transform-state
 * @param transform Pointer to the transform-state
 */
void _gles1_transform_init(gles1_transform *transform);

/**
 * @brief Gets a pointer to the current modelview-matrix
 * @param transform Pointer to the transform-state
 */
MALI_STATIC_FORCE_INLINE mali_matrix4x4 *_gles1_transform_get_current_modelview_matrix(gles1_transform *transform)
{
	MALI_DEBUG_ASSERT_POINTER(transform);
	MALI_DEBUG_ASSERT_RANGE(transform->modelview_matrix_stack_depth, 1, GLES1_MODELVIEW_MATRIX_STACK_DEPTH);
	return &(transform->modelview_matrix[transform->modelview_matrix_stack_depth - 1]);
}

/**
 * @brief Gets a pointer to the current projection-matrix
 * @param transform Pointer to the transform-state
 */
MALI_STATIC_FORCE_INLINE mali_matrix4x4 *_gles1_transform_get_current_projection_matrix(gles1_transform *transform)
{
	MALI_DEBUG_ASSERT_POINTER(transform);
	MALI_DEBUG_ASSERT_RANGE(transform->projection_matrix_stack_depth, 1, GLES1_PROJECTION_MATRIX_STACK_DEPTH);
	return &(transform->projection_matrix[transform->projection_matrix_stack_depth - 1]);
}

/**
 * @brief Gets a pointer to the current texture-matrix
 * @param transform Pointer to the transform-state
 * @param texture_unit Specifies which texture-unit is active and thus which texture-matrix-stack we should get the matrix from
 */
MALI_STATIC_FORCE_INLINE mali_matrix4x4 *_gles1_transform_get_current_texture_matrix(gles1_transform *transform, GLint texture_unit)
{
	MALI_DEBUG_ASSERT_POINTER(transform);
	MALI_DEBUG_ASSERT_RANGE(texture_unit, 0, (GLES_MAX_TEXTURE_UNITS - 1 ));
	MALI_DEBUG_ASSERT_RANGE(transform->texture_matrix_stack_depth[texture_unit], 1, GLES1_TEXTURE_MATRIX_STACK_DEPTH);
	return &(transform->texture_matrix[texture_unit][transform->texture_matrix_stack_depth[texture_unit] - 1]);
}


/**
 * @brief Gets a pointer to the current palette matrix
 * @param transform Pointer to the transform-state
 */
MALI_STATIC_FORCE_INLINE mali_matrix4x4 *_gles1_transform_get_current_palette_matrix(gles1_transform *transform)
{
	MALI_DEBUG_ASSERT_POINTER(transform);
	return &(transform->matrix_palette[transform->matrix_palette_current]);
}

/**
 * Sets current matrix-mode
 * @param transform the gles1_transform-structure to pick the matrix mode from
 * @param mode the new matrix mode. accepted values are GL_MODELVIEW, GL_PROJECTION and GL_TEXTURE
 * @note This implements the GLES-entrypoint glMatrixMode()
 */
GLenum _gles1_matrix_mode(gles_state *state, GLenum mode) MALI_CHECK_RESULT;

/**
 * Specifies a user-defined clip plane
 * @param ctx the GLES context
 * @param plane the user clip plane which is defined by the equation
 * @param equation pointer to an array of 4 entries containing the plane equation
 * @param type the data type used to specify the equation
 * @note This implements the GLES-entrypoints glClipPlane[f/x]
 */
GLenum _gles1_clip_plane( struct gles_context *ctx, GLenum plane, const GLvoid *equation, gles_datatype type ) MALI_CHECK_RESULT;


/**
 * @brief Sets the current palette matrix
 * @param transform Pointer to the transform-state
 * @param palette_index Specifies the matrix palette to be set
 */
GLenum _gles1_set_current_palette_matrix_oes( gles1_transform *transform, GLint palette_index) MALI_CHECK_RESULT;

/**
 * @brief Loads the model view matrix into the currently sellected palette matrix
 * @param transform Pointer to the transform-state
 */
void _gles1_load_palette_from_model_view_matrix_oes( struct gles_context *ctx, gles1_transform *transform);

#endif /* _GLES1_TRANSFORM_H_ */
