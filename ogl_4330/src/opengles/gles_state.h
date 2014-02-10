/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_STATE_H_
#define _GLES_STATE_H_

/**
 * @file gles_state.h
 * @brief Wrapper header for glesN_state, allowing a uniform interface towards either state.
 */

#if !MALI_USE_GLES_1 && !MALI_USE_GLES_2
#error "Neither MALI_USE_GLES_1 nor MALI_USE_GLES_2 are defined!\n"
#endif

#include <mali_system.h>
#include "gles_base.h"

#include "gles_common_state/gles_rasterization.h"
#include "gles_common_state/gles_vertex_array.h"
#include "gles_common_state/gles_tex_state.h"
#include "gles_common_state/gles_pixel.h"
#include "gles_common_state/gles_pixel_operations.h"
#include "gles_common_state/gles_rasterization.h"
#include "gles_common_state/gles_multisampling.h"
#include "gles_common_state/gles_viewport.h"
#include "gles_common_state/gles_framebuffer_control.h"
#include "gles_common_state/gles_framebuffer_state.h"
#include "gles_common_state/gles_program_rendering_state.h"
#include "gles_renderbuffer_object.h"

struct gles1_state;
struct gles2_state;

/* The intended idiom for GL state (prefix GLES_STATE) use is:
 *
 * Use mali_statebit_set(), mali_statebit_unset(), and
 * mali_statebit_exchange() as needed in GL API code.
 *
 * Use mali_statebit_get() ONLY in the driver backend.
 *
 *
 * The intended idiom for 'dirty' bit use is:
 *
 * Use mali_statebit_set() wherever it is necessary.  Use
 * mali_statebit_get() anywhere, but it must be accompanied by
 * mali_statebit_unset().  Use of mali_statebit_exchange() in a
 * conditional expression is considered equivalent.
 *
 * The side-effect of this idiom is that it may increase the number of
 * booleans in the codebase, but ensures that there can only be one legal
 * 'owner' of each flag.
 *
 *
 * The intended idiom for MALI_STATE_MODE flags is:
 *
 * Use mali_statebit_exchange() to conditionally set/unset the flag
 * as needed on a per-drawcall basis (i.e. based on the mode argument to
 * glDraw[Arrays|Elements]).  Elsewhere in the driver these flags should
 * be considered read-only.
 */
typedef enum
{
	/* GLES scissor enable. This bit is special and must be one less than MALI_STATE_SCISSOR_DIRTY
	 */
	GLES_STATE_SCISSOR_ENABLED,

	/* PLBU viewport/scissor calculation depends on the scissor when enabled
	 */
	MALI_STATE_SCISSOR_DIRTY,

	/* 16x supersampling is enabled
	 */
	MALI_STATE_MODE_SUPERSAMPLE_16X,

	/* The vertex-shader viewport calculation depends on depth and viewport
	 */
	MALI_STATE_VS_VIEWPORT_UPDATE_PENDING,

	/* The PLBU viewport/scissor calculation depends on only the viewport
	 */
	MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING,

	/* Viewport/scissor PLBU commands must be sent
	 */
	MALI_STATE_VIEWPORT_SCISSOR_UPDATE_PENDING,

	/* PLBU commands for vertex-offset or depthrange must be emitted
	 */
	MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING,
	MALI_STATE_PLBU_FRAME_DIRTY,

	/* The Vertex program has changed. Uniform locations and buffers may have changed */
	MALI_STATE_VS_PRS_UPDATE_PENDING,

	/* The depth range has changed. This affect the Vertex Shader constant uniforms */
	MALI_STATE_VS_DEPTH_RANGE_UPDATE_PENDING,

	/* If we have to only realloc mali mem for constant registers without update anything additional dirty bit will help to save performance */
	MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING,

	/* The polygon offset has changed. This affects Vertex shader constant uniforms */
	MALI_STATE_VS_POLYGON_OFFSET_UPDATE_PENDING,

	/* An FBO completeness check has recently been performed:
	 * supersampling, width, or height may have changed.
	 */
	MALI_STATE_FBO_DIRTY,

	/* The vertex uniforms have changed. This affect the Vertex Shader uniforms */
	MALI_STATE_VS_VERTEX_UNIFORM_UPDATE_PENDING,

	/* The fragment uniforms have changed. This affects Fragment Shader cached uniforms */
	MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING,

	/* The frame mem_pool has been reset. This means all mem_pool allocs locations are no longer valid */
	MALI_STATE_FRAME_MEM_POOL_RESET,

	/* Fast 'current mode' check;
	 *
	 * POINT_TYPE for points
	 * LINE_TYPE for lines, line-strips, line-loops
	 * TRIANGLE_TYPE for triangles, triangle-strips, triangle-fans
	 */
	MALI_STATE_MODE_POINT_TYPE,
	MALI_STATE_MODE_LINE_TYPE,
	MALI_STATE_MODE_TRIANGLE_TYPE,

	/* Indicates that the current drawcall is an indexed one
	 * (i.e. glDrawElements rather than glDrawArrays)
	 */
	MALI_STATE_MODE_INDEXED,

	/* Placeholder to check that the dirty flags will fit in a u32
	 Note: Fast access flags should be added before this marker.
	 */
	MALI_STATE_MODE_AND_DIRTY_END = 32,

	/* GL state providing per-light enables.  Owned by GL state
	 * management, read-only elsewhere.
	 */
	GLES_STATE_LIGHT0_ENABLED,
	GLES_STATE_LIGHT1_ENABLED,
	GLES_STATE_LIGHT2_ENABLED,
	GLES_STATE_LIGHT3_ENABLED,
	GLES_STATE_LIGHT4_ENABLED,
	GLES_STATE_LIGHT5_ENABLED,
	GLES_STATE_LIGHT6_ENABLED,
	GLES_STATE_LIGHT7_ENABLED,

	/* Set this to true if all lights are to be updated(after a material-diffuse/ambient change) */
	GLES_STATE_LIGHT_ALL_DIRTY,

	/* Set to true when the state of the light changes */
	GLES_STATE_LIGHT0_DIRTY,
	GLES_STATE_LIGHT1_DIRTY,
	GLES_STATE_LIGHT2_DIRTY,
	GLES_STATE_LIGHT3_DIRTY,
	GLES_STATE_LIGHT4_DIRTY,
	GLES_STATE_LIGHT5_DIRTY,
	GLES_STATE_LIGHT6_DIRTY,
	GLES_STATE_LIGHT7_DIRTY,

	/* Set to true when lighting-material-state changes(there are exceptions) */
	GLES_STATE_LIGHTING_DIRTY,

	/* Separating [spot_cutoff == 180.0f] from the range [0.0f <=
	 * spot_cutoff <= 90.0f] allows us to precompute cosf(spot_cutoff)
	 * once and only when it is updated by the application.
	 *
	 * When disabled, cosf( spot_cutoff ) should be -1.0f
	 *
	 * Since we need to glGet the cutoff (and arc-cos cannot
	 * guarantee verbatim result) we also need to store the cutoff
	 * angle as originally supplied.  There is no need to keep the
	 * glGet data with the uniform data.
	 */
	GLES_STATE_LIGHT0_SPOT_ENABLED,
	GLES_STATE_LIGHT1_SPOT_ENABLED,
	GLES_STATE_LIGHT2_SPOT_ENABLED,
	GLES_STATE_LIGHT3_SPOT_ENABLED,
	GLES_STATE_LIGHT4_SPOT_ENABLED,
	GLES_STATE_LIGHT5_SPOT_ENABLED,
	GLES_STATE_LIGHT6_SPOT_ENABLED,
	GLES_STATE_LIGHT7_SPOT_ENABLED,

	GLES_STATE_CURRENT_COLOR_DIRTY,
	GLES_STATE_TEXENV_COLOR_DIRTY,
	GLES_STATE_FOG_COLOR_DIRTY,
	GLES_STATE_COLOR_ATTACHMENT_DIRTY,

	/* GP2 stream specifier must be recalculated when any part of
	 * an attribute stream is altered.
	 */
	MALI_STATE_GLESATTRIB0_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB1_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB2_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB3_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB4_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB5_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB6_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB7_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB8_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB9_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB10_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB11_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB12_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB13_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB14_STREAM_SPEC_DIRTY,
	MALI_STATE_GLESATTRIB15_STREAM_SPEC_DIRTY,

	/* Indicates that matrices must be recalculated
	 */
	MALI_STATE_MODELVIEW_PROJECTION_MATRIX_UPDATE_PENDING,
	MALI_STATE_PROJECTION_VIEWPORT_MATRIX_UPDATE_PENDING,
	MALI_STATE_NORMAL_MATRIX_UPDATE_PENDING,
	MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING0,
	MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING1,
	MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING2,
	MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING3,
	MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING4,
	MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING5,
	MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING6,
	MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING7,

	MALI_STATE_TEXTURE_MATRIX_UPDATE_END,
	/* Indicates if matrix-palette has been changed, either projection matrix or one of skinning-matrices
	 */
	MALI_STATE_SKINNING_MATRIX_GROUP_UPDATE_PENDING = MALI_STATE_TEXTURE_MATRIX_UPDATE_END,

	/* Indicates which of the skinning-matrices has changed, grouped in 4, so that matrix-index 5 is ...PENDING1
	 */
	MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING0,
	MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING1,
	MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING2,
	MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING3,
	MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING4,
	MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING5,
	MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING6,
	MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING7,


	/* This enumerant *must* be the last used since it is used to
	 * size array storage
	 */
	MALI_STATE_ENUM_CHECK

} mali_statebit;

/* The number of 32-bit words required to contain the mali_statebit
 * bitvector.
 */
#define MALI_STATE_BITVECTOR_WORDS (( MALI_STATE_ENUM_CHECK + 31 ) / 32 )


/**
 * @brief The part of the OpenGL ES state that is shared between OpenGL ES 1.x and OpenGL ES 2.x.
 * @note For the rest of the OpenGL ES 1.x state, see the struct gles1_state in gles1_state.h
 * @note For the rest of the OpenGL ES 2.x state, see the struct gles2_state in gles2_state.h
 */
typedef struct gles_common_state
{
	u32 state_flags[ MALI_STATE_BITVECTOR_WORDS ];   /**< Bitvector for boolean state */

	GLenum                   errorcode;           /**< Holds the last errorcode set by the driver */

	gles_vertex_array        vertex_array;        /**< The vertex-array-state */
	gles_texture_environment texture_env;         /**< The texture-environment-state */
	gles_pixel               pixel;               /**< The pixel-state */
	gles_scissor             scissor;             /**< The scissor-state */
	gles_rasterization       rasterization;       /**< The rasterization-state */
	gles_viewport            viewport;            /**< The viewport-state */
	gles_framebuffer_control framebuffer_control; /**< The framebuffer-control-state */
	gles_renderbuffer_state  renderbuffer;        /**< The renderbuffer state */
	gles_framebuffer_state   framebuffer;         /**< The framebuffer state  */

	gles_program_rendering_state* current_program_rendering_state; /* The current program rendering state */

} gles_common_state;

/**
 * @brief The OpenGL ES state. The common state is the part of the state that is shared between
 *        the APIs. The api union will contain the API specific state of either OpenGL ES 1.x or
 *        OpenGL ES 2.x depending on which API the current context is created for.
 */
typedef struct gles_state
{
	struct gles_common_state common;

	union
	{
		struct gles1_state * gles1;
		struct gles2_state * gles2;

	} api;

} gles_state;

/**
 * @brief Return a state flag value
 */
MALI_STATIC_FORCE_INLINE mali_bool mali_statebit_get( const gles_common_state * const ctx, const mali_statebit which )
{
	const u32 wordof = which >> 5;
	const u32 bitof  = which & 0x1F;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_LEQ( which, MALI_STATE_ENUM_CHECK - 1 );

	return ( ctx->state_flags[wordof] >> bitof ) & 1;
}

/**
 * @brief Set a state flag
 */
MALI_STATIC_FORCE_INLINE void mali_statebit_set( gles_common_state * const ctx, const mali_statebit which )
{
	const u32 wordof = which >> 5;
	const u32 bitof  = which & 0x1F;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_LEQ( which, MALI_STATE_ENUM_CHECK - 1 );

	ctx->state_flags[wordof] |= ( 1 << bitof );
}

/**
 * @brief Clear a state flag
 */
MALI_STATIC_FORCE_INLINE void mali_statebit_unset( gles_common_state * const ctx, const mali_statebit which )
{
	const u32 wordof = which >> 5;
	const u32 bitof  = which & 0x1F;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_LEQ( which, MALI_STATE_ENUM_CHECK - 1 );

	ctx->state_flags[wordof] &= ~( 1 << bitof );
}

/**
 * @brief Exchange (non-atomic) boolean state flag
 */
MALI_STATIC_INLINE mali_bool mali_statebit_exchange( gles_common_state * const ctx, const mali_statebit which, mali_bool value )
{
	mali_bool ret;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_LEQ( which, MALI_STATE_ENUM_CHECK - 1 );

	ret = mali_statebit_get( ctx, which );

	if( value )
	{
		mali_statebit_set( ctx, which );
	}
	else
	{
		mali_statebit_unset( ctx, which );
	}
	return ret;
}

/**
 * @brief Initialise all state flags to zero/false.
 */
MALI_STATIC_INLINE void mali_state_init( gles_common_state * const ctx )
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	for( i = 0; i < MALI_ARRAY_SIZE( ctx->state_flags ); ++i )
	{
		ctx->state_flags[i] = 0;
	}
}

/**
 * @brief Return the first state_flags word masked with the input parameter.
 */
MALI_STATIC_FORCE_INLINE u32 mali_statebit_raw_get( const gles_common_state * const ctx, const u32 mask )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	return ( ctx->state_flags[0] & mask );
}

/**
 * @brief Set the first state_flags word within the mask values to the input value.
 */
MALI_STATIC_FORCE_INLINE void mali_statebit_raw_set( gles_common_state * const ctx, const u32 mask, const u32 values )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	{
		u32 reg = ctx->state_flags[0];
		u32 reg2 = mask & values;
		reg &=  ~mask;
		reg ^= reg2;

		ctx->state_flags[0] = reg;
	}
}

/**
 * @brief Make a backup copy of the state bitvector.
 *
 * Use this with mali_state_restore() to rollback state changes in
 * error paths.
 *
 * Argument ordering is memcpy()-like
 */
MALI_STATIC_INLINE void mali_state_backup( u32 * const dst, gles_common_state * const ctx )
{
	MALI_DEBUG_ASSERT_POINTER( dst );
	MALI_DEBUG_ASSERT_POINTER( ctx );
	{
		u32* src = ctx->state_flags;
		u32* tmpdst = dst;
		u32 i = MALI_STATE_BITVECTOR_WORDS;

		while(i--)
		{
			*tmpdst++ = *src++;
		}
	}
}

/**
 * @brief Restore a backup copy of the state bitvector.
 *
 * Use this with mali_state_backup() to rollback state changes in
 * error paths.
 *
 * Argument ordering is memcpy()-like
 */
MALI_STATIC_INLINE void mali_state_restore( gles_common_state * const ctx, u32 * const src )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( src );

	(void) _mali_sys_memcpy( ctx->state_flags, src, MALI_ARRAY_SIZE( ctx->state_flags ) );
}

#endif /* _GLES_STATE_H_ */
