/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_SHADER_GENERATOR_CONTEXT_H_
#define _GLES_SHADER_GENERATOR_CONTEXT_H_

#include <opengles/gles_context.h>
#include <shared/binary_shader/bs_object.h>
#include <shared/shadergen_interface.h>
#include "../gles_common_state/gles_program_rendering_state.h"
#include <opengles/gles_util.h>

static const float log2_e = 1.4426950408889634f;
static const float sqrt_log2_e = 1.2011224087864498f;

/* NOTE: These need to be tuned for performance */
#define MAX_NUM_CACHED_VERTEX_SHADERS   25
#define MAX_NUM_CACHED_FRAGMENT_SHADERS 25

/* NOTE: This needs to match the range of the timestamp variable in gles_sg_context */
#define MAX_TIMESTAMP           ((unsigned int)0xFFFFFFFF)

#define SG_N_VERTEX_UNIFORMS 1216     /* number of vertex shader uniforms (measured in floats), MALIGP2_MAX_VS_CONSTANT_REGISTERS * 4 */
#define SG_N_FRAGMENT_UNIFORMS (1024) /* number of fragment shader uniforms (measured in floats) */

/* Internal positions for fragment shader uniforms */
#define FRAGMENT_UNIFORM_Samplers 0
#define FRAGMENT_UNIFORM_ConstantColor 32
#define FRAGMENT_UNIFORM_FogColor 36
#define FRAGMENT_UNIFORM_gl_mali_PointCoordScaleBias 40
#define FRAGMENT_UNIFORM_ClipPlaneTie 39
#define FRAGMENT_UNIFORM_Constants 44
#define FRAGMENT_UNIFORM_YUV_Constants 80
#define FRAGMENT_UNIFORM_AddConstToRes 76

/* Value to add or subtract to the signed clip plane distance for
   right-on-clip-edge fragments to avoid overlap between geometry
   drawn with opposite clip planes.
*/
#define CLIP_PLANE_TIE_EPSILON 1.0f

#define MALI_GLES_SG_WRITE_FLOAT(name, value) ctx->sg_ctx->vertex_uniform_array[VERTEX_UNIFORM_POS(name)] = value

#define MALI_GLES_SG_WRITE_VEC3(name, src) _gles_copy_vec3(&ctx->sg_ctx->vertex_uniform_array[VERTEX_UNIFORM_POS(name)], src)

#define MALI_GLES_SG_WRITE_VEC4(name, src) _gles_copy_vec4(&ctx->sg_ctx->vertex_uniform_array[VERTEX_UNIFORM_POS(name)], src)

#define MALI_GLES_SG_WRITE_ARRAY_FLOAT(name, index, value) ctx->sg_ctx->vertex_uniform_array[VERTEX_UNIFORM_POS(name)+(index)*4] = value

#define MALI_GLES_SG_WRITE_ARRAY_VEC3(name, index, src) _gles_copy_vec3(&ctx->sg_ctx->vertex_uniform_array[VERTEX_UNIFORM_POS(name)+(index)*4], src)

#define MALI_GLES_SG_WRITE_ARRAY_VEC4(name, index, src) _gles_copy_vec4(&ctx->sg_ctx->vertex_uniform_array[VERTEX_UNIFORM_POS(name)+(index)*4], src)

#define MALI_GLES_SG_FETCH_ARRAY_VEC( name, index )  &ctx->sg_ctx->vertex_uniform_array[ VERTEX_UNIFORM_POS( name ) + ( index ) * 4 ]

#define MALI_GLES_SG_FETCH_ARRAY_FLOAT( name, index ) ctx->sg_ctx->vertex_uniform_array[ VERTEX_UNIFORM_POS( name ) + ( index ) * 4 ]

#define MALI_GLES_SG_FETCH_FLOAT( name ) ctx->sg_ctx->vertex_uniform_array[ VERTEX_UNIFORM_POS( name ) ]


#define MALI_GLES_SG_WRITE_MAT3(name, src) \
  MALI_GLES_SG_WRITE_ARRAY_VEC3(name, 0, src[0]); \
  MALI_GLES_SG_WRITE_ARRAY_VEC3(name, 1, src[1]); \
  MALI_GLES_SG_WRITE_ARRAY_VEC3(name, 2, src[2]);

#if (! MALI_OSU_MATH)
#define MALI_GLES_SG_WRITE_MAT4(name, src) \
  MALI_GLES_SG_WRITE_ARRAY_VEC4(name, 0, src[0]); \
  MALI_GLES_SG_WRITE_ARRAY_VEC4(name, 1, src[1]); \
  MALI_GLES_SG_WRITE_ARRAY_VEC4(name, 2, src[2]); \
  MALI_GLES_SG_WRITE_ARRAY_VEC4(name, 3, src[3]); 
#else 
  #define MALI_GLES_SG_WRITE_MAT4(name, src)  _mali_osu_matrix4x4_copy(&ctx->sg_ctx->vertex_uniform_array[VERTEX_UNIFORM_POS(name)], (const float*)src)
  #define MALI_GLES_SG_WRITE_MAT4_WITH_OFFSET(name, index, src)  _mali_osu_matrix4x4_copy(&ctx->sg_ctx->vertex_uniform_array[VERTEX_UNIFORM_POS(name)+(index)*4], (const float*)src)
#endif

/**
  * A cached vertex shader and the state from which it was generated
  */
typedef struct gles_sg_vertex_shader {
	struct gles_sg_vertex_shader *next; /**< linkage to next vertex-shader */
	vertex_shadergen_state state;		/**< The state variables of the vertex shader */
	unsigned int last_used;				/**< When this shader was last used, used for deleting oldest shader when needed */
	shadergen_boolean delete_mark;				/**< Is this shader marked for deletion? */
	bs_shader shader;					/**< The shader object */
} gles_sg_vertex_shader;

/**
  * A cached fragment shader and the state from which it was generated
  */
typedef struct gles_sg_fragment_shader {
	struct gles_sg_fragment_shader *next; 	/**< linkage to next fragment shader */
	fragment_shadergen_state state;			/**< The state variable of the fragment shader */
	unsigned int last_used;					/**< When this shader was last used, used for deleting oldest shader when needed */
	shadergen_boolean delete_mark;					/**< Is this shader marked for deletion? */
	bs_shader shader;						/**< The shader object */
} gles_sg_fragment_shader;

/**
  * A cached program and the shaders it is linked from
  */
typedef struct gles_sg_program {
	struct gles_sg_program *next;				/**< Linkage to the next program object */
	gles_sg_vertex_shader *vertex_shader;		/**< The vertex shader used in this program */
	gles_sg_fragment_shader *fragment_shader;	/**< The fragment shader used in this program */
	gles_program_rendering_state *prs;			/**< The program object */
} gles_sg_program;

/**
  * The sg_context, holding information about new and cached shaders and programs and the shared uniform arrays
  */
typedef struct gles_sg_context {
	mali_base_ctx_handle base_ctx;					/**< The base context */

	gles_sg_vertex_shader *fresh_vertex_shader;		/**< Scratchpad for new vertex shader */
	gles_sg_fragment_shader *fresh_fragment_shader;	/**< Scratchpad for new fragment shader */

	gles_sg_vertex_shader *vertex_shaders;			/**< The cached list of vertex shaders that have been constructed, first entry is most recently used */
	gles_sg_fragment_shader *fragment_shaders;		/**< The cached list of fragment shaders that have been constructed, first entry is most recently used */
	gles_sg_program *programs;						/**< The cached list of programs that have been constructed, first entry is most recently used */

	unsigned int current_timestamp;					/**< Timestamp to keep track of shader aging */

	vertex_shadergen_state current_vertex_state;	/**< The vertex state of the current draw call */
	fragment_shadergen_state current_fragment_state;/**< The fragment state of the current draw call */

	float vertex_uniform_array[SG_N_VERTEX_UNIFORMS];/**< Shared vertex uniform array for all bs_programs */
	float fragment_uniform_array[SG_N_FRAGMENT_UNIFORMS];/**< Shared fragment uniform array for all bs_programs */
	gles_fp16 fragment_fp16_uniform_array[SG_N_FRAGMENT_UNIFORMS];/**< Shared fp16 cache of fragment uniforms for all bs_programs */

	u32 last_light_enabled_field;                           /**< Tells us which lights were enabled last, used to determine if we should update the packed array of lights */
	u32 last_texture_transform_field;                       /**< Tells us which textures last had a transform matrix set, used to determine if we should update the packed array of texture transform data */

} gles_sg_context;

/* @brief Calculates the epsilon for patching clip-planes
 * @param coefs The clip plane to calculate epsilon for
 * @return The epsilon
 */
MALI_STATIC_INLINE float _mali_gles_sg_clip_plane_sign(const float *coefs) {
	int i;
	float largest = 0.0f;
	float epsilon = 0.0f;
	for (i = 0 ; i < 4 ; i++) {
		if (coefs[i] > largest) {
			epsilon = CLIP_PLANE_TIE_EPSILON;
			largest = coefs[i];
		} else if (-coefs[i] > largest) {
			epsilon = -CLIP_PLANE_TIE_EPSILON;
			largest = -coefs[i];
		}
	}
	return epsilon;
}

/* @brief Calcualates the patched clip-plane coefficients and sets them in the uniform array
 * @param ctx The pointer to the GLES-context
 * @param clip_plane The original coefficiants of the clip-plane
 */
MALI_STATIC_INLINE void _mali_gles_sg_write_clip_plane_patched( struct gles_context *ctx, GLftype *clip_plane )
{
	int i;
	float patched_clip_coefs[4];
	float epsilon;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( clip_plane );

	epsilon = _mali_gles_sg_clip_plane_sign( clip_plane );
	for (i = 0 ; i < 4 ; i++)
	{
		/* Adding of 0.0f is investigated in bugzilla #8884 */
		patched_clip_coefs[i] = clip_plane[ i ] * epsilon + 0.0f;
	}
	MALI_GLES_SG_WRITE_VEC4(ClipPlaneCoefficients, patched_clip_coefs);
}

#endif /* _GLES_SHADER_GENERATOR_CONTEXT_H_ */
