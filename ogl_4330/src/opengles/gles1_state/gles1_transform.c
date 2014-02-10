/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles1_transform.h"

#include <gles_context.h>
#include <gles_convert.h>
#include "../gles_config_extension.h"
#include "../gles_base.h"
#include "gles1_state/gles1_state.h"
#include <gles_state.h>
#include <opengles/gles_sg_interface.h>

/* External function definitions */
void _gles1_transform_init(gles1_transform *transform)
{
	int i;
	MALI_DEBUG_ASSERT_POINTER( transform );

	for (i = 0; i < GLES1_MODELVIEW_MATRIX_STACK_DEPTH; ++i)
	{
		__mali_matrix4x4_make_identity(transform->modelview_matrix[i]);
		transform->modelview_matrix_identity[i] = MALI_TRUE;
	}

	for (i = 0; i < GLES1_PROJECTION_MATRIX_STACK_DEPTH; ++i)
	{
		__mali_matrix4x4_make_identity(transform->projection_matrix[i]);
		transform->projection_matrix_identity[i] = MALI_TRUE;
	}

	for (i = 0; i < GLES_MAX_TEXTURE_UNITS; ++i)
	{
		int j;
		for (j = 0; j < GLES1_TEXTURE_MATRIX_STACK_DEPTH; ++j)
		{
			__mali_matrix4x4_make_identity(transform->texture_matrix[i][j]);
			transform->texture_matrix_identity[i][j] = MALI_TRUE;
		}
	}

	transform->modelview_matrix_stack_depth  = 1;
	transform->projection_matrix_stack_depth = 1;

	for (i = 0; i < GLES_MAX_TEXTURE_UNITS; ++i)
	{
		transform->texture_matrix_stack_depth[i]    = 1;
	}

	transform->matrix_mode = GL_MODELVIEW;
	transform->normalize      = GL_FALSE;
	transform->rescale_normal = GL_FALSE;

	transform->current_matrix = _gles1_transform_get_current_modelview_matrix(transform);
	transform->current_matrix_is_identity = &transform->modelview_matrix_identity[transform->modelview_matrix_stack_depth - 1];
	transform->texture_identity_field = 0x0;

	for (i = 0; i < GLES1_MAX_CLIP_PLANES; ++i)
	{
		transform->clip_plane[i][0] = FLOAT_TO_FTYPE( 0.f );
		transform->clip_plane[i][1] = FLOAT_TO_FTYPE( 0.f );
		transform->clip_plane[i][2] = FLOAT_TO_FTYPE( 0.f );
		transform->clip_plane[i][3] = FLOAT_TO_FTYPE( 0.f );
	}
	/* initialize the matrix palette to identity */
	for (i = 0; i < GLES1_MAX_PALETTE_MATRICES_OES; ++i)
	{
		__mali_matrix4x4_make_identity(transform->matrix_palette[i]);
		transform->matrix_palette_identity[i] = MALI_TRUE;
	}

	transform->matrix_palette_current = 0;

	/* initialize cached matrices */
	__mali_matrix4x4_make_identity(transform->projection_viewport_matrix);
	__mali_matrix4x4_make_identity(transform->modelview_projection_matrix);
}

GLenum _gles1_clip_plane( struct gles_context *ctx, GLenum plane, const GLvoid *equation, gles_datatype type )
{
	gles1_transform *transform;
	u32 index;
	mali_matrix4x4	*mat;
	mali_matrix4x4	inv_mv;
	mali_vector4	plane_eqn;

	mali_matrix4x4	m;
	u32 singular;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	transform = &ctx->state.api.gles1->transform;

	index = plane - GL_CLIP_PLANE0;
	if (index > (GLES1_MAX_CLIP_PLANES - 1)) MALI_ERROR( GL_INVALID_ENUM );
	if (NULL == equation) GL_SUCCESS;

	_gles_convert_array_to_ftype( MALI_REINTERPRET_CAST(GLftype*) ((void*)(&plane_eqn)), equation, 4, type );

	mat = _gles1_transform_get_current_modelview_matrix( transform );

	__mali_matrix4x4_copy( m, *mat );

	singular = MALI_ERR_NO_ERROR != __mali_matrix4x4_invert( inv_mv, m );
	if (singular)
	{
		/* GL spec says that singular MV matrices cause
		 * undefined behavior.  In this case we choose not to
		 * adopt the new clip plane.
		 */
		GL_SUCCESS;
	}

	__mali_matrix4x4_transpose( inv_mv, inv_mv );

	plane_eqn = __mali_matrix4x4_transform_vector4( inv_mv, plane_eqn );

	*(MALI_REINTERPRET_CAST(mali_vector4 *) &transform->clip_plane[ index ][0] ) = plane_eqn;

	if (0 == index && vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_CLIPPLANE_PLANE0))
	{
		_mali_gles_sg_write_clip_plane_patched( ctx, &transform->clip_plane[ index ][ 0 ] );
	}
	GL_SUCCESS;
}

GLenum _gles1_matrix_mode(gles_state *state, GLenum mode)
{
	gles1_transform *transform = &state->api.gles1->transform;
	MALI_DEBUG_ASSERT_POINTER(transform);
	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles1 );

	/* validate mode */
	switch (mode)
	{
		case GL_MODELVIEW:
			transform->current_matrix = &transform->modelview_matrix[transform->modelview_matrix_stack_depth - 1];
			transform->current_matrix_is_identity = &transform->modelview_matrix_identity[transform->modelview_matrix_stack_depth - 1];
			break;
		case GL_PROJECTION:
			transform->current_matrix = &transform->projection_matrix[transform->projection_matrix_stack_depth - 1];
			transform->current_matrix_is_identity = &transform->projection_matrix_identity[transform->projection_matrix_stack_depth - 1];
			break;
		case GL_TEXTURE:
			transform->current_matrix = &transform->texture_matrix[state->common.texture_env.active_texture][transform->texture_matrix_stack_depth[state->common.texture_env.active_texture] - 1];
			transform->current_matrix_is_identity = &transform->texture_matrix_identity[state->common.texture_env.active_texture][transform->texture_matrix_stack_depth[state->common.texture_env.active_texture] - 1];
			transform->current_texture_matrix_id = state->common.texture_env.active_texture;
			break;
#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
		case GL_MATRIX_PALETTE_OES:
			transform->current_matrix = &transform->matrix_palette[transform->matrix_palette_current];
			transform->current_matrix_is_identity = &transform->matrix_palette_identity[transform->matrix_palette_current];
			break;
#endif
		default:
			/* invalid. */
			MALI_ERROR( GL_INVALID_ENUM );
	}

	/* valid. */
	transform->matrix_mode = mode;

	GL_SUCCESS;
}

GLenum _gles1_set_current_palette_matrix_oes( gles1_transform *transform, GLint palette_index)
{
	MALI_DEBUG_ASSERT_POINTER(transform);
	if (palette_index < 0 || palette_index >= GLES1_MAX_PALETTE_MATRICES_OES)
	{
		return GL_INVALID_VALUE;
	}
	transform->matrix_palette_current = palette_index;
	if ( GL_MATRIX_PALETTE_OES == transform->matrix_mode )
	{
		transform->current_matrix = &transform->matrix_palette[transform->matrix_palette_current];
		transform->current_matrix_is_identity = &transform->matrix_palette_identity[transform->matrix_palette_current];
	}
	return GL_NO_ERROR;
}

void _gles1_load_palette_from_model_view_matrix_oes( struct gles_context *ctx, gles1_transform *transform)
{
	MALI_DEBUG_ASSERT_POINTER(transform);
	MALI_DEBUG_ASSERT_RANGE(transform->modelview_matrix_stack_depth, 1, GLES1_MODELVIEW_MATRIX_STACK_DEPTH);
	__mali_matrix4x4_copy(transform->matrix_palette[transform->matrix_palette_current], transform->modelview_matrix[transform->modelview_matrix_stack_depth - 1]);
	transform->matrix_palette_identity[transform->matrix_palette_current] = MALI_FALSE;

	MALI_DEBUG_ASSERT( ctx->state.api.gles1->transform.matrix_palette_current < GLES1_MAX_PALETTE_MATRICES_OES, ("Invalid matrix-palette selected\n") );

	mali_statebit_set( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_GROUP_UPDATE_PENDING );
	mali_statebit_set( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING0 + ( ctx->state.api.gles1->transform.matrix_palette_current >> 2 ) );
}
