/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles1_matrix.h"
#include "gles_util.h"
#include "gles_state.h"
#include "gles1_state/gles1_state.h"
#include "gles_context.h"
#include <opengles/gles_sg_interface.h>
#include <shadergen_state.h>

MALI_STATIC_INLINE void _gles1_dirty_matrix_state( struct gles_context *ctx, const GLenum mode )
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	switch ( mode )
	{
		case GL_PROJECTION:
			mali_statebit_set( & ctx->state.common, MALI_STATE_PROJECTION_VIEWPORT_MATRIX_UPDATE_PENDING );
			mali_statebit_set( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_GROUP_UPDATE_PENDING );
		break;
		case GL_TEXTURE:
			MALI_DEBUG_ASSERT( ctx->state.api.gles1->transform.current_texture_matrix_id < GLES_MAX_TEXTURE_UNITS, ("Invalid texture unit selected\n") );

			mali_statebit_set( & ctx->state.common, MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING0 + ctx->state.api.gles1->transform.current_texture_matrix_id );
		break;
		case GL_MODELVIEW:
			mali_statebit_set( & ctx->state.common, MALI_STATE_MODELVIEW_PROJECTION_MATRIX_UPDATE_PENDING );
			mali_statebit_set( & ctx->state.common, MALI_STATE_NORMAL_MATRIX_UPDATE_PENDING );
		break;
		case GL_MATRIX_PALETTE_OES:
			MALI_DEBUG_ASSERT( ctx->state.api.gles1->transform.matrix_palette_current < GLES1_MAX_PALETTE_MATRICES_OES, ("Invalid matrix-palette selected\n") );

			mali_statebit_set( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_GROUP_UPDATE_PENDING );
			mali_statebit_set( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING0 + ( ctx->state.api.gles1->transform.matrix_palette_current >> 2 ) );
		break;

		/* no default since other values are valid, but do not require any action */
	}

}

/**
 * @brief Returns the current matrix
 * @param state GLES context pointer
 */
MALI_STATIC_INLINE mali_matrix4x4 *_gles1_get_current_matrix(struct gles_state *state)
{
	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles1 );

	return state->api.gles1->transform.current_matrix;
}

/**
 * @brief Returns true if current matrix is identity
 * @param state GLES context pointer
 */
MALI_STATIC_INLINE mali_bool _gles1_get_current_matrix_is_identity(struct gles_state *state)
{
	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles1 );

	return *state->api.gles1->transform.current_matrix_is_identity;
}


/**
 * @brief Sets the identity flag of the current matrix
 * @param state GLES context pointer
 */
MALI_STATIC_INLINE void _gles1_set_current_matrix_is_identity(struct gles_context *ctx, mali_bool identity)
{
	struct gles_state *state;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	state = &ctx->state;
	*state->api.gles1->transform.current_matrix_is_identity = identity;
	if ( GL_TEXTURE == state->api.gles1->transform.matrix_mode )
	{
		int idx = state->api.gles1->transform.current_texture_matrix_id;
		u32 small_identity_field = state->api.gles1->transform.texture_identity_field & ( 1 << idx );
		u32 *transform_field = &(ctx->state.api.gles1->transform.texture_transform_field);
		mali_bool previous_identity;

		if ( 0x0 == small_identity_field )
		{
			previous_identity = GL_TRUE;
		}
		else
		{
			previous_identity = GL_FALSE;
		}

		if ( identity != previous_identity )
		{
			state->api.gles1->transform.texture_identity_field &= ~( 1 << idx );
			state->api.gles1->transform.texture_identity_field |= ( ( MALI_TRUE == identity ) ? 0 : ( 1 << idx ) );
			if ( MALI_TRUE == identity )
			{
				vertex_shadergen_encode( &ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_TRANSFORM(idx), TEX_IDENTITY);
				(*transform_field) &= ~( 1 << idx );
			} else {
				vertex_shadergen_encode( &ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_TRANSFORM(idx), TEX_TRANSFORM);
				(*transform_field) |= ( 1 << idx );
			}
		}

	}
}

/* push the matrix stack */
GLenum _gles1_push_matrix(struct gles_context *ctx)
{
	GLuint *depth = NULL;
	mali_matrix4x4 *old_mat = NULL;
	mali_matrix4x4 *new_mat = NULL;
	mali_matrix4x4 *curr_matrix = NULL;
	mali_bool old_is_identity;
	mali_bool *is_identity_matrix = NULL;

	gles1_transform *transform;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	transform = &ctx->state.api.gles1->transform;

	/*
	 * get depth, and check if it's equal or bigger than max depth.
	 * if it's equal, we'll be trying to push outside of the stack
	 * as the next step, so it's an overflow
	 */
	switch (transform->matrix_mode)
	{
		case GL_MODELVIEW:
			depth = &transform->modelview_matrix_stack_depth;
			is_identity_matrix = transform->modelview_matrix_identity;
			curr_matrix = transform->modelview_matrix;
			if (*depth >= GLES1_MODELVIEW_MATRIX_STACK_DEPTH) MALI_ERROR( GL_STACK_OVERFLOW );
		break;

		case GL_PROJECTION:
			depth = &transform->projection_matrix_stack_depth;
			is_identity_matrix = transform->projection_matrix_identity;
			curr_matrix = transform->projection_matrix;
			if (*depth >= GLES1_PROJECTION_MATRIX_STACK_DEPTH) MALI_ERROR( GL_STACK_OVERFLOW );
		break;

		case GL_TEXTURE:
			depth = &transform->texture_matrix_stack_depth[ctx->state.common.texture_env.active_texture];
			is_identity_matrix = transform->texture_matrix_identity[ctx->state.common.texture_env.active_texture];
			curr_matrix = transform->texture_matrix[ctx->state.common.texture_env.active_texture];
			if (*depth >= GLES1_TEXTURE_MATRIX_STACK_DEPTH) MALI_ERROR( GL_STACK_OVERFLOW );
		break;
		case GL_MATRIX_PALETTE_OES:
			return GL_STACK_OVERFLOW; /* We have no stack for MATRIX_PALETTE, so just return GL-error */

		default: MALI_DEBUG_ASSERT( MALI_FALSE, ("corrupted matrix mode"));
	}
	MALI_DEBUG_ASSERT_POINTER(depth);

 	/* Check if valid pointer */
	if ( NULL != depth)
	{
		/* get old matrix */
		old_mat = &curr_matrix[(*depth) - 1];
		old_is_identity = _gles1_get_current_matrix_is_identity(&ctx->state);

		/* do the actual pushing */
		(*depth)++;

		/* get new matrix */
		new_mat = &curr_matrix[(*depth) - 1];

		/* copy old matrix to new matrix */
		__mali_matrix4x4_copy(*new_mat, *old_mat);

		/* set current matrix pointers */
		transform->current_matrix = new_mat;
		transform->current_matrix_is_identity = &is_identity_matrix[(*depth) - 1];

		_gles1_set_current_matrix_is_identity( ctx, old_is_identity);
	}

	/* it all went well! */
	GL_SUCCESS;
}

/* pop the matrix stack */
GLenum _gles1_pop_matrix(struct gles_context *ctx)
{
	/* get the depth based on the current matrix mode */
	GLuint *depth = NULL;
	gles1_transform *transform;
	mali_bool *is_identity_matrix = NULL;
	mali_matrix4x4 *curr_matrix = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	transform = &ctx->state.api.gles1->transform;

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

	switch (transform->matrix_mode)
	{
		case GL_MODELVIEW:
			depth = &transform->modelview_matrix_stack_depth;
			is_identity_matrix = transform->modelview_matrix_identity;
			curr_matrix = transform->modelview_matrix;
		break;
		case GL_PROJECTION:
			depth = &transform->projection_matrix_stack_depth;
			is_identity_matrix = transform->projection_matrix_identity;
			curr_matrix = transform->projection_matrix;
		break;
		case GL_TEXTURE:
			depth = &transform->texture_matrix_stack_depth[ctx->state.common.texture_env.active_texture];
			is_identity_matrix = transform->texture_matrix_identity[ctx->state.common.texture_env.active_texture];
			curr_matrix = transform->texture_matrix[ctx->state.common.texture_env.active_texture];
		break;
		case GL_MATRIX_PALETTE_OES: return GL_STACK_UNDERFLOW; /* We have no stack for MATRIX_PALETTE, so just return GL-error */
		default: MALI_DEBUG_ASSERT( MALI_FALSE, ("corrupted matrix mode"));
	}
	MALI_DEBUG_ASSERT_POINTER(depth);


	/* check if valid pointer */
	if ( NULL != depth)
	{
		/* check for underflow */
		if (*depth <= 1) MALI_ERROR( GL_STACK_UNDERFLOW );

		/* pop */
		(*depth)--;

		/* set current matrix pointers */
		transform->current_matrix = &curr_matrix[(*depth) - 1];
		transform->current_matrix_is_identity = &is_identity_matrix[(*depth) - 1];

		_gles1_set_current_matrix_is_identity(ctx, *transform->current_matrix_is_identity);
	}

	/* it all went swell! */
	GL_SUCCESS;
}

void _gles1_load_identity(struct gles_context *ctx)
{
	mali_matrix4x4 *mat = NULL;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	mat = _gles1_get_current_matrix(&ctx->state);
	MALI_DEBUG_ASSERT_POINTER(mat);

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

	__mali_matrix4x4_make_identity(*mat);
	_gles1_set_current_matrix_is_identity( ctx, MALI_TRUE);
}

void _gles1_load_matrixf(struct gles_context *ctx, const GLfloat *m)
{
	mali_matrix4x4 *mat = NULL;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	mat = _gles1_get_current_matrix(&ctx->state);
	MALI_DEBUG_ASSERT_POINTER(mat);

	if (NULL == m) return; /* no defined error */

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

	_mali_sys_memcpy_32((void*)(*mat), (void*)m, sizeof(mali_matrix4x4));
	_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
}


void _gles1_mult_matrixf(struct gles_context *ctx, const GLfloat *m)
{
	mali_matrix4x4 *mat = NULL;
	mali_bool is_identity;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	mat = _gles1_get_current_matrix(&ctx->state);
	is_identity = _gles1_get_current_matrix_is_identity(&ctx->state);
	MALI_DEBUG_ASSERT_POINTER(mat);

	if (NULL == m) return; /* no defined error */

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);
	if( is_identity == MALI_TRUE )
	{
		_mali_sys_memcpy_32((void*)mat, (void*)m, sizeof(mali_matrix4x4));
		_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
	}
	else
	{
		__mali_matrix4x4_multiply(*mat, *mat, *(MALI_REINTERPRET_CAST(const mali_matrix4x4*)m));
	}
}

void _gles1_load_matrixx(struct gles_context *ctx, const GLfixed *m)
{
	mali_matrix4x4 *mat = NULL;
	float *pointer;
	int i;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	mat = _gles1_get_current_matrix(&ctx->state);
	MALI_DEBUG_ASSERT_POINTER(mat);
	pointer = (GLfloat *)mat;
	if (NULL == m) return; /* no defined error */

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

	for(i = 0; i < 16; i++)
	{
		*(pointer++) = FIXED_TO_FTYPE( *(m++) );
	}
	_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
}

void _gles1_mult_matrixx(struct gles_context *ctx, const GLfixed *m)
{
	mali_matrix4x4 *mat = NULL;
	mali_matrix4x4 temp;
	mali_bool is_identity;
	int i;
	float *pointer = (float*) &temp;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	mat = _gles1_get_current_matrix(&ctx->state);
	is_identity = _gles1_get_current_matrix_is_identity(&ctx->state);
	MALI_DEBUG_ASSERT_POINTER(mat);

	if (NULL == m) return; /* no defined error */

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);


	/* Convert fixed matrix to float matrix and then multiply */
	for( i = 0; i < 16; i++)
	{
		*(pointer++) = FIXED_TO_FTYPE( *(m++) );
	}
	if( is_identity == MALI_TRUE )
	{
		_mali_sys_memcpy_32((void*)mat, (void*)temp, sizeof(mali_matrix4x4));
		_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
	}
	else
	{
		__mali_matrix4x4_multiply(*mat, *mat, *(MALI_REINTERPRET_CAST(const mali_matrix4x4*)&temp));
	}
}

GLbitfield _gles1_query_matrixx(struct gles_context *ctx, GLfixed *mantissa, GLint *exponent)
{
	int i, j;
	mali_matrix4x4 *mat = NULL;
	MALI_DEBUG_ASSERT_POINTER(&ctx->state);
	mat = _gles1_get_current_matrix(&ctx->state);
	MALI_DEBUG_ASSERT_POINTER(mat);


	if ( NULL == mantissa) return 0;
	if ( NULL == exponent) return 0;

	for (i = 0; i < 4; ++i)
	{
		for (j = 0; j < 4; ++j)
		{
			int index = i*4+j;
			mantissa[index] = _gles_fp32_get_mantissa((*mat)[i][j]);
			exponent[index] = _gles_fp32_get_exponent((*mat)[i][j]);
		}
	}

	return 0; /* we don't track any under/overflow */
}

void _gles1_rotate(struct gles_context *ctx, GLftype angle, GLftype x, GLftype y, GLftype z)
{
	mali_matrix4x4 *mat = NULL;
	mali_vector3 axis;
	mali_float len2;
	mali_bool is_identity;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	mat = _gles1_get_current_matrix( &ctx->state );
	MALI_DEBUG_ASSERT_POINTER( mat );

	is_identity = _gles1_get_current_matrix_is_identity(&ctx->state);

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

	angle *= (mali_float) MALI_DEGREES_TO_RADIANS;

	/*
	 * normalize rotation axis if necessary
	 */

	axis.x = x;
	axis.y = y;
	axis.z = z;

	len2 = __mali_vector3_dot3( axis, axis );

	/* len2 must be zero or positive - hence no need for fabs() */

	if( len2 < (1.0f - MALI_SW_EPSILON) ||
	    len2 > (1.0f + MALI_SW_EPSILON) )
	{
		mali_float len = _mali_sys_sqrt( len2 );

		/* ensure len >= MALI_SW_EPSILON */

		if( len < MALI_SW_EPSILON )
		{
			len = (mali_float)MALI_SW_EPSILON;
		}

		axis = __mali_vector3_scale( axis, 1.0f / len );
	}

	/*
	 * apply rotation to current matrix
	 */
	{
		int i;

		/* precompute sine/cosine of angle since it will be used several times */

		const mali_float c = _mali_sys_cos( angle );
		const mali_float s = _mali_sys_sin( angle );
		const mali_float ci = 1.0f - c;

		/* precompute subexpressions */

		const mali_float xs = 	axis.x * s;
		const mali_float ys = 	axis.y * s;
		const mali_float zs = 	axis.z * s;

		const mali_float yci = 	axis.y * ci;
		const mali_float zci = 	axis.z * ci;

		const mali_float xyci = axis.x * yci;
		const mali_float xzci = axis.x * zci;
		const mali_float yzci = axis.y * zci;

		/* a rotation matrix */
		const mali_matrix4x4 rot_mat = {   {axis.x * axis.x * ci + c,   xyci + zs, xzci - ys,  0},
		                                                      {xyci - zs, axis.y * yci + c,  yzci + xs,  0},
		                                                      {xzci + ys, yzci - xs, axis.z * zci + c,  0},
		                                                      {0,  0, 0, 1} };

#if (!MALI_OSU_MATH)
		if( is_identity == MALI_TRUE )
		{
		 (*mat)[0][0] = rot_mat[0][0];
		  (*mat)[0][1] = rot_mat[0][1];
		  (*mat)[0][2] = rot_mat[0][2];
		 (*mat)[1][0] = rot_mat[1][0];
		  (*mat)[1][1] = rot_mat[1][1];
		  (*mat)[1][2] = rot_mat[1][2];
		 (*mat)[2][0] = rot_mat[2][0];
		  (*mat)[2][1] = rot_mat[2][1];
		  (*mat)[2][2] = rot_mat[2][2];
		}
		else
		{
			/* optimised matrix multiply: no need to unroll since
			 * branches are predictable and smaller code is kinder to
			 * I-cache
			 */
			for( i = 0; i < 4; ++i )
			{
#ifndef __SYMBIAN32__
				const mali_float m[3] = { (*mat)[0][i], (*mat)[1][i], (*mat)[2][i] };
#else
				mali_float m[3];
				m[0] = (*mat)[0][i];
				m[1] = (*mat)[1][i];
				m[2] = (*mat)[2][i];
#endif
				(*mat)[0][i] = m[0] * rot_mat[0][0] + m[1] * rot_mat[0][1] + m[2] * rot_mat[0][2];
				(*mat)[1][i] = m[0] * rot_mat[1][0] + m[1] * rot_mat[1][1] + m[2] * rot_mat[1][2];
				(*mat)[2][i] = m[0] * rot_mat[2][0] + m[1] * rot_mat[2][1] + m[2] * rot_mat[2][2];
			}
		}
#else
			__mali_matrix4x4_multiply(*mat, *mat, rot_mat);
#endif

	}

	_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
}

void _gles1_translate(struct gles_context *ctx, GLftype x, GLftype y, GLftype z)
{
	mali_matrix4x4 *mat = NULL;
	mali_bool is_identity;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	mat = _gles1_get_current_matrix( &ctx->state );
	MALI_DEBUG_ASSERT_POINTER( mat );

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

	is_identity = _gles1_get_current_matrix_is_identity(&ctx->state);

	if( is_identity == MALI_TRUE )
	{
        (*mat)[3][0] = x;
        (*mat)[3][1] = y;
        (*mat)[3][2] = z;
	}
	else
	{
#if (!MALI_OSU_MATH)
		(*mat)[3][0] = (*mat)[0][0] * x + (*mat)[1][0] * y + (*mat)[2][0] * z + (*mat)[3][0];
		(*mat)[3][1] = (*mat)[0][1] * x + (*mat)[1][1] * y + (*mat)[2][1] * z + (*mat)[3][1];
		(*mat)[3][2] = (*mat)[0][2] * x + (*mat)[1][2] * y + (*mat)[2][2] * z + (*mat)[3][2];
		(*mat)[3][3] = (*mat)[0][3] * x + (*mat)[1][3] * y + (*mat)[2][3] * z + (*mat)[3][3];
#else
		_mali_osu_matrix4x4_translate((float*)mat, &x, &y, &z);
#endif
	}
	_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
}

void _gles1_scale(struct gles_context *ctx, GLftype x, GLftype y, GLftype z)
{
	mali_matrix4x4 *mat = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	mat = _gles1_get_current_matrix( &ctx->state );
	MALI_DEBUG_ASSERT_POINTER( mat );

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

#if (!MALI_OSU_MATH)
	(*mat)[0][0] *= x;
	(*mat)[1][0] *= y;
	(*mat)[2][0] *= z;

	(*mat)[0][1] *= x;
	(*mat)[1][1] *= y;
	(*mat)[2][1] *= z;

	(*mat)[0][2] *= x;
	(*mat)[1][2] *= y;
	(*mat)[2][2] *= z;

	(*mat)[0][3] *= x;
	(*mat)[1][3] *= y;
	(*mat)[2][3] *= z;
#else
	_mali_osu_matrix4x4_scale((float*)mat, &x, &y, &z);
#endif

	_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
}

GLenum _gles1_frustum(struct gles_context *ctx, GLftype l, GLftype r, GLftype b, GLftype t, GLftype n, GLftype f)
{
	mali_matrix4x4 temp, *mat = NULL;
	mali_bool is_identity;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	mat = _gles1_get_current_matrix(&ctx->state);
	is_identity = _gles1_get_current_matrix_is_identity(&ctx->state);
	MALI_DEBUG_ASSERT_POINTER(mat);

	/* verify parameters */
	if ((n <= FLOAT_TO_FTYPE(0.f)) || (f <= FLOAT_TO_FTYPE(0.f)) ||
	   ((r - l) == FLOAT_TO_FTYPE(0.f)) ||
	   ((n - f) == FLOAT_TO_FTYPE(0.f)) ||
	   ((t - b) == FLOAT_TO_FTYPE(0.f)))
	{
		MALI_ERROR( GL_INVALID_VALUE );
	}

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

	/* generate matrix */
	__mali_matrix4x4_make_frustum(temp, (mali_ftype)l, (mali_ftype)r, (mali_ftype)b, (mali_ftype)t, (mali_ftype)n, (mali_ftype)f);

	if( is_identity == MALI_TRUE )
	{
		_mali_sys_memcpy_32((void*)(*mat), (void*)temp, sizeof(mali_matrix4x4));
		_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
	}
	else
	{
		/* multiply the current matrix with the new one */
		__mali_matrix4x4_multiply(*mat, *mat, *(MALI_REINTERPRET_CAST(const mali_matrix4x4*)&temp));
	}

	GL_SUCCESS;
}

GLenum _gles1_ortho(struct gles_context *ctx, GLftype l, GLftype r, GLftype b, GLftype t, GLftype n, GLftype f)
{
	mali_matrix4x4 temp, *mat = NULL;
	mali_bool is_identity;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	mat = _gles1_get_current_matrix(&ctx->state);
	is_identity = _gles1_get_current_matrix_is_identity(&ctx->state);
	MALI_DEBUG_ASSERT_POINTER(mat);

	/* verify parameters */
	if (((r - l) == FLOAT_TO_FTYPE(0.f)) ||
	   ((n - f) == FLOAT_TO_FTYPE(0.f))  ||
	   ((t - b) == FLOAT_TO_FTYPE(0.f)))
	{
		MALI_ERROR( GL_INVALID_VALUE );
	}

	_gles1_dirty_matrix_state(ctx, ctx->state.api.gles1->transform.matrix_mode);

	/* generate matrix */
	__mali_matrix4x4_make_ortho(temp, (mali_ftype)l, (mali_ftype)r, (mali_ftype)b, (mali_ftype)t, (mali_ftype)n, (mali_ftype)f);

	if( is_identity == MALI_TRUE )
	{
		_mali_sys_memcpy_32((void*)mat, (void*)temp, sizeof(mali_matrix4x4));
		_gles1_set_current_matrix_is_identity( ctx, MALI_FALSE);
	}
	else
	{
		/* multiply the current matrix with the new one */
		__mali_matrix4x4_multiply(*mat, *mat, *(MALI_REINTERPRET_CAST(const mali_matrix4x4*)&temp));
	}

	GL_SUCCESS;
}
