/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles1_context.h>
#include "gles1_state.h"
#include "gles1_current.h"

#include "gles1_lighting.h"
#include <opengles/gles_sg_interface.h>

/* Array index usage */
#define RED_INDEX	0
#define GREEN_INDEX	1
#define BLUE_INDEX	2
#define ALPHA_INDEX	3

#define S_INDEX		0
#define T_INDEX		1
#define R_INDEX		2
#define Q_INDEX		3

#define X_INDEX		0
#define Y_INDEX		1
#define Z_INDEX		2



void _gles1_current_init(gles1_current *current)
{
	s32 i;
	MALI_DEBUG_ASSERT_POINTER( current );

	current->color[ RED_INDEX	] = FLOAT_TO_FTYPE(1.0f);
	current->color[ GREEN_INDEX ] = FLOAT_TO_FTYPE(1.0f);
	current->color[ BLUE_INDEX  ] = FLOAT_TO_FTYPE(1.0f);
	current->color[ ALPHA_INDEX ] = FLOAT_TO_FTYPE(1.0f);

	for (i = 0; i < GLES_MAX_TEXTURE_UNITS; ++i)
	{
		current->tex_coord[i][ S_INDEX ] = FLOAT_TO_FTYPE(0.0f);
		current->tex_coord[i][ T_INDEX ] = FLOAT_TO_FTYPE(0.0f);
		current->tex_coord[i][ R_INDEX ] = FLOAT_TO_FTYPE(0.0f);
		current->tex_coord[i][ Q_INDEX ] = FLOAT_TO_FTYPE(1.0f);
	}

	current->normal[ X_INDEX ] = FLOAT_TO_FTYPE(0.0f);
	current->normal[ Y_INDEX ] = FLOAT_TO_FTYPE(0.0f);
	current->normal[ Z_INDEX ] = FLOAT_TO_FTYPE(1.0f);
}

GLenum _gles1_color4( gles_context *ctx, GLftype red, GLftype green, GLftype blue, GLftype alpha )
{
	gles1_current *current;
	gles1_lighting *lighting;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	current = &ctx->state.api.gles1->current;
	lighting = &ctx->state.api.gles1->lighting;

	if ( (current->color[ RED_INDEX   ] != red)   ||
		 (current->color[ GREEN_INDEX ] != green) ||
		 (current->color[ BLUE_INDEX  ] != blue)  ||
		 (current->color[ ALPHA_INDEX ] != alpha))
	{
		current->color[ RED_INDEX   ] = red;
		current->color[ GREEN_INDEX ] = green;
		current->color[ BLUE_INDEX  ] = blue;
		current->color[ ALPHA_INDEX ] = alpha;

		mali_statebit_set( &ctx->state.common, GLES_STATE_CURRENT_COLOR_DIRTY);


		if ( COLORMATERIAL_AMBIENTDIFFUSE == vertex_shadergen_decode( &ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL))
		{
			int i;
			for ( i = 0; i < 4; i++ )
			{
				lighting->ambient[i] = current->color[i];
				lighting->diffuse[i] = current->color[i];
			}
			mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
		}
	}

	GL_SUCCESS;
}

GLenum _gles1_normal3( gles1_current *current, GLftype nx, GLftype ny, GLftype nz )
{
	MALI_DEBUG_ASSERT_POINTER( current );

	current->normal[ X_INDEX ] = nx;
	current->normal[ Y_INDEX ] = ny;
	current->normal[ Z_INDEX ] = nz;

	GL_SUCCESS;
}

GLenum _gles1_multi_tex_coord4( gles1_current *current, GLenum target, GLftype s, GLftype t, GLftype r, GLftype q )
{
	u32 unit;
	MALI_DEBUG_ASSERT_POINTER( current );

	unit = (u32) target - GL_TEXTURE0;
	if (unit >= GLES_MAX_TEXTURE_UNITS) MALI_ERROR( GL_INVALID_ENUM );

	current->tex_coord[ unit ][ S_INDEX ] = s;
	current->tex_coord[ unit ][ T_INDEX ] = t;
	current->tex_coord[ unit ][ R_INDEX ] = r;
	current->tex_coord[ unit ][ Q_INDEX ] = q;

	GL_SUCCESS;
}

