/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles1_draw.h"

#include <base/mali_debug.h>
#include <base/mali_types.h>

#include "gles_context.h"
#include "gles_util.h"
#include "gles_draw.h"
#include "gles_gb_interface.h"
#include "gles_fb_interface.h"
#include "gles_convert.h"
#include "gles1_state/gles1_state.h"
#include "gles1_state/gles1_vertex_array.h"

#include <shared/m200_gp_frame_builder.h>
#include "gles_sg_interface.h"
#include "gles_framebuffer_object.h"
#include "mali_gp_geometry_common/gles_geometry_backend_context.h"

MALI_STATIC mali_err_code _gles1_init_draw( gles_context *ctx, GLenum mode )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	/* Let the optional shader generator generate shaders from the fixed-function state */
	MALI_CHECK_NO_ERROR(_gles_sg_init_draw_call( ctx, ctx->sg_ctx, mode ));

	/* Init specific draw call, and propagate any errors */
	MALI_CHECK_NO_ERROR(_gles_fb_init_draw_call( ctx, mode ));

	/* Init successful! */
	MALI_SUCCESS;
}

#if HARDWARE_ISSUE_7571_7572
/**
 * @brief Checks if the framebuffer contains triangles after the last dummy_quad was drawn
 * @param ctx The pointer to the GLES context
 * @param mode The current draw mode
 * @return An errorcode if it fails, MALI_ERR_NO_ERROR if it doesn't
 */
MALI_STATIC mali_err_code MALI_CHECK_RESULT _gles1_draw_current_draw_mode( gles_context *ctx, GLenum mode )
{
	if ( GL_POINTS == mode && MALI_TRUE == ctx->state.common.framebuffer.current_object->framebuffer_has_triangles )
	{
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
		/* Ready the bound framebuffer */
		GLenum err = _gles_fbo_internal_draw_setup( ctx );
		if ( err != GL_NO_ERROR )
		{
			return MALI_ERR_FUNCTION_FAILED;
		}
#endif
		MALI_CHECK_NO_ERROR( _gles_drawcall_begin( ctx ) );
		MALI_CHECK_NO_ERROR( _gles_draw_dummyquad( ctx ) );
		_gles_drawcall_end( ctx );
		ctx->state.common.framebuffer.current_object->framebuffer_has_triangles = MALI_FALSE;
	}
	if ( GL_TRIANGLES == mode || GL_TRIANGLE_STRIP == mode || GL_TRIANGLE_FAN == mode )
	{
		ctx->state.common.framebuffer.current_object->framebuffer_has_triangles = MALI_TRUE;
	}

	MALI_SUCCESS;
}
#endif

GLenum _gles1_draw_elements( gles_context *ctx, GLenum mode, GLint count, GLenum type, const void *indices )
{
    mali_err_code merr;
    GLenum err;
    u32 vs_range_count = 1;
    u32 coherence = 0;
    index_range   local_idx_range[MALI_LARGEST_INDEX_RANGE];
    index_range  *idx_vs_range = local_idx_range;

	MALI_DEBUG_ASSERT_POINTER( ctx );

#if HARDWARE_ISSUE_7571_7572
	merr = _gles1_draw_current_draw_mode( ctx, mode );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_merr );
#endif

	err = _gles_draw_elements_error_checking( mode, count, type );
	MALI_CHECK(GL_NO_ERROR == err, err);

	count = _gles_round_down_vertex_count(mode, count);
	if (count == 0) GL_SUCCESS;

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	/* Ready the bound framebuffer */
	err = _gles_fbo_internal_draw_setup( ctx );
	if ( err != GL_NO_ERROR )
	{
		return err;
	}
#endif

	merr = _gles_drawcall_begin(ctx);
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_merr );

	merr = _gles_init_draw_elements(ctx, count, type, mode, indices, &idx_vs_range, &vs_range_count, &coherence);
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_endraw_merr );

	{
		merr = _gles1_init_draw( ctx, mode );
		/* If there are indices pointing to an invalid vertex, we don't know which index it is, so we early out */
		MALI_CHECK_GOTO( ( MALI_ERR_NO_ERROR == merr ) , exit_endraw_merr );
	}

	merr = _gles_draw_elements_internal( ctx, mode, count, type, indices, idx_vs_range, local_idx_range, vs_range_count, coherence );

exit_endraw_merr:
	_gles_drawcall_end(ctx);
exit_merr:
	return _gles_convert_mali_err( merr );
}
	
GLenum _gles1_draw_arrays( gles_context *ctx, GLenum mode, GLint first, GLint count )
{
	mali_err_code merr = MALI_ERR_NO_ERROR;
	GLenum err = GL_NO_ERROR;

#if HARDWARE_ISSUE_7571_7572
	merr = _gles1_draw_current_draw_mode( ctx, mode );
	MALI_CHECK( MALI_ERR_NO_ERROR == merr, _gles_convert_mali_err( merr ) );
#endif

	err = _gles_draw_arrays_error_checking( mode, first, count );
	MALI_CHECK(GL_NO_ERROR == err, err);

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	/* Ready the bound framebuffer */
	err = _gles_fbo_internal_draw_setup( ctx );
	if ( err != GL_NO_ERROR )
	{
		return err;
	}
#endif

	count = _gles_round_down_vertex_count(mode, count);
	if (count == 0) return GL_NO_ERROR;

	merr = _gles_drawcall_begin(ctx);
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_merr );

	merr = _gles_init_draw_arrays( ctx, mode, count );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_endraw_merr );

	merr = _gles1_init_draw( ctx, mode );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_endraw_merr );

	merr = _gles_draw_arrays_internal( ctx, mode, first, count );

exit_endraw_merr:
	_gles_drawcall_end(ctx);
exit_merr:
	return _gles_convert_mali_err( merr );
}

