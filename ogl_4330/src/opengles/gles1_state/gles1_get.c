/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <shared/mali_surface.h>
#include <gles_context.h>
#include <gles_fb_interface.h>
#include <gles_sg_interface.h>
#include <gles_buffer_object.h>
#include <gles_config_extension.h>
#include <gles_convert.h>
#include <gles_ftype.h>

#include "gles1_state.h"
#include "gles1_error.h"
#include "gles1_get.h"

#include <opengles/gles_sg_interface.h>


#define MALI_GLES_1_MAJOR_VERSION 1

/* Internal function declaration */
MALI_STATIC void _gles1_convert_matrix(mali_matrix4x4 *mat, GLvoid *params, gles_datatype type);

/* External function definitions */
GLenum _gles1_getv( struct gles_context *ctx, GLenum pname, GLvoid *params, gles_datatype type )
{
	GLint i;
	GLuint active_texture;
	struct gles_state *state;
	const GLenum compressed_formats[] = {

#if EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE
				GL_PALETTE4_RGB8_OES, GL_PALETTE4_RGBA8_OES, GL_PALETTE4_R5_G6_B5_OES, GL_PALETTE4_RGBA4_OES,
				GL_PALETTE4_RGB5_A1_OES, GL_PALETTE8_RGB8_OES, GL_PALETTE8_RGBA8_OES, GL_PALETTE8_R5_G6_B5_OES,
				GL_PALETTE8_RGBA4_OES, GL_PALETTE8_RGB5_A1_OES,
#endif
#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
				GL_ETC1_RGB8_OES,
#endif
#if HARDWARE_ISSUE_3615 && EXTENSION_FLXTC_FLX_ARM_ENABLE
				GL_RGB_COMPRESSED_2_FLX, GL_RGBA_COMPRESSED_2_FLX,
#endif
	};
	const GLint num_compressed_formats = MALI_ARRAY_SIZE( compressed_formats );
	gles_common_state *state_common;
	gles1_state       *state_gles1;

	state = &ctx->state;

	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles1 );

	state_common = &(state->common);
	state_gles1  = state->api.gles1;

	/* these are the only legal datatypes we're supporting GetBooleanv( ), GetFixedv( ), GetFloatv( ) and GetIntegerv( ) */
	MALI_DEBUG_ASSERT( (GLES_FIXED == type) || (GLES_INT == type) || (GLES_BOOLEAN == type) || (GLES_FLOAT == type),
			( "illegal datatype\n" ) );

	/* the get'ing below is ordered after their storage type, as defined in section 6.2 of the GLES 1.1 spec */
	switch( pname )
	{
		/* the following states are stored as ints */
		/* table 6.6 */
		case GL_CLIENT_ACTIVE_TEXTURE:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.client_active_texture + ( GLint )GL_TEXTURE0, type );
		break;
		case GL_VERTEX_ARRAY_SIZE:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POSITION ].size, type );
		break;
		case GL_VERTEX_ARRAY_STRIDE:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POSITION ].given_stride, type );
		break;
		case GL_VERTEX_ARRAY_TYPE:
			_gles_convert_from_enum( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POSITION ].type, type );
		break;
		case GL_NORMAL_ARRAY_STRIDE:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_NORMAL ].given_stride, type );
		break;
		case GL_NORMAL_ARRAY_TYPE:
			_gles_convert_from_enum( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_NORMAL ].type, type );
		break;
		case GL_COLOR_ARRAY_SIZE:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_COLOR ].size, type );
		break;
		case GL_COLOR_ARRAY_STRIDE:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_COLOR ].given_stride, type );
		break;
		case GL_COLOR_ARRAY_TYPE:
			_gles_convert_from_enum( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_COLOR ].type, type );
		break;
		case GL_TEXTURE_COORD_ARRAY_SIZE:
			_gles_convert_from_int( params, 0,
					( GLint )state_common->vertex_array.attrib_array[ ( GLuint )GLES_VERTEX_ATTRIB_TEX_COORD0 +
					                                                  ( GLuint )state_common->vertex_array.client_active_texture ].size, type );
		break;
		case GL_TEXTURE_COORD_ARRAY_STRIDE:
			_gles_convert_from_int( params, 0,
					( GLint )state_common->vertex_array.attrib_array[ ( GLuint )GLES_VERTEX_ATTRIB_TEX_COORD0 +
					                                                  ( GLuint )state_common->vertex_array.client_active_texture ].given_stride, type );
		break;
		case GL_TEXTURE_COORD_ARRAY_TYPE:
			_gles_convert_from_enum( params, 0,
					state_common->vertex_array.attrib_array[ ( GLuint )GLES_VERTEX_ATTRIB_TEX_COORD0 +
					                                         ( GLuint )state_common->vertex_array.client_active_texture ].type, type );
		break;
		case GL_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.array_buffer_binding, type );
		break;
		case GL_VERTEX_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( params, 0,
					( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POSITION ].buffer_binding, type );
		break;
		case GL_NORMAL_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( params, 0,
					( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_NORMAL ].buffer_binding, type );
		break;
		case GL_COLOR_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( params, 0,
					( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_COLOR ].buffer_binding, type );
		break;
		case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( params, 0,
					( GLint )state_common->vertex_array.attrib_array[ ( GLuint )GLES_VERTEX_ATTRIB_TEX_COORD0 +
					                                                  ( GLuint )state_common->vertex_array.client_active_texture ].buffer_binding, type );
		break;
		case GL_ELEMENT_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.element_array_buffer_binding, type );
		break;

		/** Transforming-state( including viewport state ) */
		case GL_MODELVIEW_MATRIX:
			_gles1_convert_matrix(&state_gles1->transform.modelview_matrix[ ( GLuint )state_gles1->transform.modelview_matrix_stack_depth-1 ], params, type);
		break;
		case GL_PROJECTION_MATRIX:
			_gles1_convert_matrix(&state_gles1->transform.projection_matrix[ ( GLuint )state_gles1->transform.projection_matrix_stack_depth-1 ], params, type);
		break;
		case GL_TEXTURE_MATRIX:
			/** uses only active_texture, not as an offset, since the array spans over max number of textures */
			active_texture = state_common->texture_env.active_texture;
			_gles1_convert_matrix(&state_gles1->transform.texture_matrix[ active_texture ][ ( GLuint )state_gles1->transform.texture_matrix_stack_depth[ active_texture ] -1], params, type);
		break;
		case GL_VIEWPORT:
			_gles_convert_from_int( params, 0, ( GLint )state_common->viewport.x, type );
			_gles_convert_from_int( params, 1, ( GLint )state_common->viewport.y, type );
			_gles_convert_from_int( params, 2, ( GLint )state_common->viewport.width, type );
			_gles_convert_from_int( params, 3, ( GLint )state_common->viewport.height, type );
		break;
		case GL_DEPTH_RANGE:
			/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;

			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->viewport.z_nearfar[0], type );
			_gles_convert_from_ftype( params, 1, ( GLftype )state_common->viewport.z_nearfar[1], type );
		break;
		case GL_MODELVIEW_STACK_DEPTH:
			_gles_convert_from_int( params, 0, ( GLint )state_gles1->transform.modelview_matrix_stack_depth, type );
		break;
		case GL_PROJECTION_STACK_DEPTH:
			_gles_convert_from_int( params, 0, ( GLint )state_gles1->transform.projection_matrix_stack_depth, type );
		break;
		case GL_TEXTURE_STACK_DEPTH:
			_gles_convert_from_int( params, 0, ( GLint )state_gles1->transform.texture_matrix_stack_depth[ ( GLuint )state_common->texture_env.active_texture ], type );
		break;
		case GL_MATRIX_MODE:
			_gles_convert_from_enum( params, 0, state_gles1->transform.matrix_mode, type );
		break;

		/** Coloring-state */
		case GL_FOG_COLOR:
			/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;

			for (i = 0; i < 4; i++)
			{
				_gles_convert_from_ftype( params, i, ( GLftype )state_gles1->coloring.fog_color[ i ], type );
			}
		break;
		case GL_FOG_DENSITY:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_gles1->coloring.fog_density, type );
		break;
		case GL_FOG_START:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_gles1->coloring.fog_start, type );
		break;
		case GL_FOG_END:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_gles1->coloring.fog_end, type );
		break;
		case GL_FOG_MODE:
			_gles_convert_from_enum( params, 0, state_gles1->coloring.fog_mode, type );
		break;
		case GL_SHADE_MODEL:
			{
				mali_bool flatshading = fragment_shadergen_decode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_FLATSHADED_COLORS );
				GLenum shade_model = (flatshading ? GL_FLAT : GL_SMOOTH);
				_gles_convert_from_enum( params, 0, shade_model, type );
			}
		break;

		/** Lighting-state */
		case GL_LIGHTING:
			_gles_convert_from_boolean( params, 0, ( GLboolean )state_gles1->lighting.enabled, type );
		break;
		case GL_LIGHT_MODEL_AMBIENT:
			/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;

			for (i = 0; i < 4; i++)
			{
				_gles_convert_from_ftype( params, i, ( GLftype )state_gles1->lighting.light_model_ambient[ i ], type );
			}
		break;
		case GL_LIGHT_MODEL_TWO_SIDE:
			_gles_convert_from_boolean( params, 0, ( GLboolean ) vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_TWOSIDED), type );
		break;

		/** Rasterization-state */
		case GL_POINT_SIZE:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->rasterization.point_size, type );
		break;
		case GL_POINT_SIZE_MIN:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->rasterization.point_size_min, type );
		break;
		case GL_POINT_SIZE_MAX:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->rasterization.point_size_max, type );
		break;
		case GL_POINT_FADE_THRESHOLD_SIZE:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_gles1->rasterization.point_fade_threshold_size, type );
		break;
		case GL_POINT_DISTANCE_ATTENUATION:
			for (i = 0; i < 3; i++)
			{
				_gles_convert_from_ftype( params, i, ( GLftype )state_gles1->rasterization.point_distance_attenuation[ i ], type );
			}
		break;
		case GL_LINE_WIDTH:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->rasterization.line_width, type );
		break;
		case GL_CULL_FACE_MODE:
			_gles_convert_from_enum( params, 0, state_common->rasterization.cull_face_mode, type );
		break;
		case GL_FRONT_FACE:
			_gles_convert_from_enum( params, 0, state_common->rasterization.front_face, type );
		break;
		case GL_POLYGON_OFFSET_FACTOR:
			_gles_convert_from_ftype( params, 0, _gles_fb_get_polygon_offset_factor( ctx ), type );
		break;
		case GL_POLYGON_OFFSET_UNITS:
			_gles_convert_from_ftype( params, 0, _gles_fb_get_polygon_offset_units( ctx ), type );
		break;

		/** Multisampling-state */
		case GL_SAMPLE_COVERAGE_VALUE:
			_gles_convert_from_ftype( params, 0, _gles_fb_get_sample_coverage_value( ctx ), type );
		break;
		case GL_SAMPLE_COVERAGE_INVERT:
			_gles_convert_from_boolean( params, 0, ( GLboolean )_gles_fb_get_sample_coverage_invert( ctx ), type );
		break;

		/** Texture-state */
		case GL_TEXTURE_BINDING_2D:
			_gles_convert_from_int( params, 0,
				( GLint ) state_common->texture_env.unit[ state_common->texture_env.active_texture ]
					.current_texture_id[GLES_TEXTURE_TARGET_2D], type);
		break;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		case GL_TEXTURE_BINDING_EXTERNAL_OES:
			_gles_convert_from_int( params, 0,
				( GLint ) state_common->texture_env.unit[ state_common->texture_env.active_texture ]
					.current_texture_id[GLES_TEXTURE_TARGET_EXTERNAL], type);
		break;
#endif

		/** Texture-env-state */
		case GL_ACTIVE_TEXTURE:
			_gles_convert_from_enum( params, 0, (GLenum)((GLint)GL_TEXTURE0 + (GLint)state_common->texture_env.active_texture), type );
		break;

		case GL_TEXTURE_BINDING_CUBE_MAP:
			_gles_convert_from_int( params, 0, ( GLint ) state_common->texture_env.unit[ state_common->texture_env.active_texture ].current_texture_id[ GLES_TEXTURE_TARGET_CUBE ], type);
		break;

		/** Framebuffer-state */
		case GL_COLOR_WRITEMASK:
			for (i = 0; i < 4; i++)
			{
				_gles_convert_from_boolean( params, i, ( GLboolean )state_common->framebuffer_control.color_is_writeable[ i ], type );
			}
		break;
		case GL_DEPTH_WRITEMASK:
			_gles_convert_from_boolean( params, 0, ( GLboolean )state_common->framebuffer_control.depth_is_writeable, type );
		break;
		case GL_COLOR_CLEAR_VALUE:
			/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;

			for (i = 0; i < 4; i++)
			{
				_gles_convert_from_ftype( params, i, ( GLftype )state_common->framebuffer_control.color_clear_value[ i ], type );
			}
		break;
		case GL_DEPTH_CLEAR_VALUE:
			/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;

			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->framebuffer_control.depth_clear_value, type );

		break;

		/** Pixel-operations */
		case GL_SCISSOR_BOX:
			_gles_convert_from_int( params, 0, ( GLint )state_common->scissor.x, type );
			_gles_convert_from_int( params, 1, ( GLint )state_common->scissor.y, type );
			_gles_convert_from_int( params, 2, ( GLint )state_common->scissor.width, type );
			_gles_convert_from_int( params, 3, ( GLint )state_common->scissor.height, type );
		break;
		/** Pixel operations */
		case GL_SCISSOR_TEST:
			_gles_convert_from_enum( params, 0, (GLboolean)mali_statebit_get( state_common, GLES_STATE_SCISSOR_ENABLED), type );
		break;
		case GL_ALPHA_TEST:
			_gles_convert_from_enum( params, 0, _gles_fb_get_alpha_test( ctx ), type );
		break;
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
			_gles_convert_from_enum( params, 0, _gles_fb_get_sample_alpha_to_coverage( ctx ), type );
		break;
		case GL_SAMPLE_ALPHA_TO_ONE:
			_gles_convert_from_enum( params, 0, _gles_fb_get_sample_alpha_to_one( ctx ), type );
		break;
		case GL_SAMPLE_COVERAGE:
			_gles_convert_from_enum( params, 0, _gles_fb_get_sample_coverage( ctx ), type );
		break;
		case GL_MULTISAMPLE:
			_gles_convert_from_enum( params, 0, _gles_fb_get_multisample( ctx ), type );
		break;
		case GL_ALPHA_TEST_FUNC:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_conditional( _gles_fb_get_alpha_test_func( ctx ) ), type );
		break;
		case GL_STENCIL_TEST:
			_gles_convert_from_enum( params, 0, (GLboolean)_gles_fb_get_stencil_test( ctx ), type );
		break;
		case GL_ALPHA_TEST_REF:
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;
			_gles_convert_from_ftype( params, 0, _gles_fb_get_alpha_test_ref( ctx ), type );
		break;

		case GL_STENCIL_FUNC:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_conditional( _gles_fb_get_stencil_func( ctx ) ), type );
		break;
		case GL_STENCIL_FAIL:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_fail( ctx ) ), type );
		break;
		case GL_STENCIL_PASS_DEPTH_FAIL:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_depth_fail( ctx ) ), type );
		break;
		case GL_STENCIL_PASS_DEPTH_PASS:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_depth_pass( ctx ) ), type );
		break;
		case GL_STENCIL_REF:
			{
				unsigned int stencilbits = _gles_fbo_get_bits(state->common.framebuffer.current_object, GL_STENCIL_BITS);
				unsigned int stencilmask = (1 << stencilbits) - 1;
				_gles_convert_from_int( params, 0, _gles_fb_get_stencil_ref( ctx ) & stencilmask, type );
			}
		break;
		case GL_STENCIL_VALUE_MASK:
                        _gles_convert_from_int( params, 0, _gles_fb_get_stencil_value_mask( ctx ), type );
		break;
		case GL_STENCIL_WRITEMASK:
			_gles_convert_from_int( params, 0, _gles_fb_get_stencil_write_mask( ctx ), type );
		break;

		case GL_STENCIL_CLEAR_VALUE:
			_gles_convert_from_int( params, 0, ( GLint )state_common->framebuffer_control.stencil_clear_value, type );
		break;

		case GL_DEPTH_FUNC:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_conditional( _gles_fb_get_depth_func( ctx ) ), type );
		break;

		case GL_BLEND_SRC:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_blend_func( _gles_fb_get_blend_src_rgb( ctx ) ), type );
		break;

		case GL_BLEND_DST:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_blend_func( _gles_fb_get_blend_dst_rgb( ctx ) ), type );
		break;

		case GL_BLEND:
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_blend( ctx ), type );
		break;

		case GL_LOGIC_OP_MODE:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_logicop( _gles_fb_get_logic_op_mode( ctx ) ), type );
		break;

		case GL_DITHER:
			_gles_convert_from_int(params, 0, ( GLint )_gles_fb_get_dither( ctx ), type);
		break;

		/** Pixels state */
		case GL_UNPACK_ALIGNMENT:
			_gles_convert_from_int( params, 0, ( GLint )state_common->pixel.unpack_alignment, type );
		break;
		case GL_PACK_ALIGNMENT:
			_gles_convert_from_int( params, 0, ( GLint )state_common->pixel.pack_alignment, type );
		break;

		/** Hint state */
		case GL_PERSPECTIVE_CORRECTION_HINT:
			_gles_convert_from_enum( params, 0, state_gles1->hint.perspective_correction, type );
		break;
		case GL_POINT_SMOOTH_HINT:
			_gles_convert_from_enum( params, 0, state_gles1->hint.point_smooth, type );
		break;
		case GL_FOG_HINT:
			_gles_convert_from_enum( params, 0, state_gles1->hint.fog, type );
		break;
		case GL_GENERATE_MIPMAP_HINT:
			_gles_convert_from_enum( params, 0, state_gles1->hint.generate_mipmap, type );
		break;
		case GL_LINE_SMOOTH_HINT:
			_gles_convert_from_enum( params, 0, state_gles1->hint.line_smooth, type );
		break;

		/** Implementation dependent values */
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
		case MALI_GL_MAX_SAMPLES_EXT_V1: /* fall-through */
		case MALI_GL_MAX_SAMPLES_EXT_V2:
			_gles_convert_from_int( params, 0, ( GLint )GLES_MAX_FBO_SAMPLES, type );
		break;
#endif
		case GL_MAX_LIGHTS:
			_gles_convert_from_int( params, 0, ( GLint )GLES1_MAX_LIGHTS, type );
		break;
		case GL_MAX_CLIP_PLANES:
			_gles_convert_from_int( params, 0, ( GLint )GLES1_MAX_CLIP_PLANES, type );
		break;
		case GL_MAX_MODELVIEW_STACK_DEPTH:
			_gles_convert_from_int( params, 0, ( GLint )GLES1_MODELVIEW_MATRIX_STACK_DEPTH, type );
		break;
		case GL_MAX_PROJECTION_STACK_DEPTH:
			_gles_convert_from_int( params, 0, ( GLint )GLES1_PROJECTION_MATRIX_STACK_DEPTH, type );
		break;
		case GL_MAX_TEXTURE_STACK_DEPTH:
			_gles_convert_from_int( params, 0, ( GLint )GLES1_TEXTURE_MATRIX_STACK_DEPTH, type );
		break;
		case GL_SUBPIXEL_BITS:
			_gles_convert_from_int( params, 0, ( GLint )GLES1_SUBPIXEL_BITS, type );
		break;
		case GL_MAX_TEXTURE_SIZE:
			_gles_convert_from_int( params, 0, ( GLint )GLES_MAX_TEXTURE_SIZE, type );
		break;
		case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
			_gles_convert_from_int( params, 0, ( GLint )GLES_MAX_CUBE_MAP_TEXTURE_SIZE, type );
		break;
		case GL_MAX_VIEWPORT_DIMS:
			_gles_convert_from_int( params, 0, ( GLint )GLES1_MAX_VIEWPORT_DIM_X, type );
			_gles_convert_from_int( params, 1, ( GLint )GLES1_MAX_VIEWPORT_DIM_Y, type );
		break;
		case GL_ALIASED_POINT_SIZE_RANGE:
			_gles_convert_from_ftype( params, 0, ( GLftype )GLES_ALIASED_POINT_SIZE_MIN, type );
			_gles_convert_from_ftype( params, 1, ( GLftype )GLES_ALIASED_POINT_SIZE_MAX, type );
		break;
		case GL_SMOOTH_POINT_SIZE_RANGE:
			_gles_convert_from_ftype( params, 0, ( GLftype )GLES1_SMOOTH_POINT_SIZE_MIN, type );
			_gles_convert_from_ftype( params, 1, ( GLftype )GLES1_SMOOTH_POINT_SIZE_MAX, type );
		break;
		case GL_ALIASED_LINE_WIDTH_RANGE:
			_gles_convert_from_ftype( params, 0, ( GLftype )GLES_ALIASED_LINE_WIDTH_MIN, type );
			_gles_convert_from_ftype( params, 1, ( GLftype )GLES_ALIASED_LINE_WIDTH_MAX, type );
		break;
		case GL_SMOOTH_LINE_WIDTH_RANGE:
			_gles_convert_from_ftype( params, 0, ( GLftype )GLES1_SMOOTH_LINE_WIDTH_MIN, type );
			_gles_convert_from_ftype( params, 1, ( GLftype )GLES1_SMOOTH_LINE_WIDTH_MAX, type );
		break;
		case GL_MAX_TEXTURE_UNITS:
			_gles_convert_from_int( params, 0, ( GLint )GLES_MAX_TEXTURE_UNITS, type );
		break;
		case GL_COMPRESSED_TEXTURE_FORMATS:
			for (i = 0; i < num_compressed_formats; i++)
			{
				_gles_convert_from_enum( params, i, compressed_formats[ i ], type );
			}
		break;
		case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
			_gles_convert_from_int( params, 0, ( GLint )num_compressed_formats, type );
		break;
		case GL_RED_BITS:
			 /* Fallthrough */
		case GL_GREEN_BITS:
			/* Fallthrough */
		case GL_BLUE_BITS:
			/* Fallthrough */
		case GL_ALPHA_BITS:
			/* Fallthrough */
		case GL_DEPTH_BITS:
			/* Fallthrough */
		case GL_STENCIL_BITS:
			/* Fallthrough */
		case GL_SAMPLE_BUFFERS:
			/* Fallthrough */
		case GL_SAMPLES:
			_gles_convert_from_int( params, 0, ( GLint )_gles_fbo_get_bits(state->common.framebuffer.current_object, pname), type );
		break;

		/** Core additions and extensions */
		case GL_IMPLEMENTATION_COLOR_READ_TYPE_OES:
			_gles_convert_from_enum( params, 0, GLES_IMPLEMENTATION_COLOR_READ_TYPE, type );
		break;
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES:
			_gles_convert_from_enum( params, 0, GLES_IMPLEMENTATION_COLOR_READ_FORMAT, type );
		break;

#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
		case GL_MAX_PALETTE_MATRICES_OES:
			_gles_convert_from_int( params, 0, ( GLint ) GLES1_MAX_PALETTE_MATRICES_OES, type );
		break;
		case GL_MAX_VERTEX_UNITS_OES:
			_gles_convert_from_int( params, 0, ( GLint ) GLES1_MAX_VERTEX_UNITS_OES, type );
		break;
		case GL_MATRIX_INDEX_ARRAY_SIZE_OES:
			_gles_convert_from_int( params, 0, ( GLint ) state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_MATRIX_INDEX ].size, type );
		break;
		case GL_MATRIX_INDEX_ARRAY_TYPE_OES:
			_gles_convert_from_enum( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_MATRIX_INDEX ].type, type );
		break;
		case GL_MATRIX_INDEX_ARRAY_STRIDE_OES:
			_gles_convert_from_int( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_MATRIX_INDEX ].given_stride, type );
		break;
		case GL_MATRIX_INDEX_ARRAY_BUFFER_BINDING_OES:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_MATRIX_INDEX ].buffer_binding, type );
		break;
		case GL_WEIGHT_ARRAY_SIZE_OES:
			_gles_convert_from_int( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_WEIGHT ].size, type );
		break;
		case GL_WEIGHT_ARRAY_TYPE_OES:
			_gles_convert_from_enum( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_WEIGHT ].type, type );
		break;
		case GL_WEIGHT_ARRAY_STRIDE_OES:
			_gles_convert_from_int( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_WEIGHT ].given_stride, type );
		break;
		case GL_WEIGHT_ARRAY_BUFFER_BINDING_OES:
			_gles_convert_from_int( params, 0, ( GLint ) state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_WEIGHT ].buffer_binding, type );
		break;
		case GL_CURRENT_PALETTE_MATRIX_OES:
			_gles_convert_from_int( params, 0, (GLint) state_gles1->transform.matrix_palette_current, type );
		break;
#endif

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
		/* render buffer */
		case GL_RENDERBUFFER_BINDING:
			_gles_convert_from_int( params, 0, (GLint)state_common->renderbuffer.current_object_id, type);
		break;

		/* frame buffer */
		case GL_FRAMEBUFFER_BINDING:
			_gles_convert_from_int( params, 0, (GLint)state_common->framebuffer.current_object_id, type);
		break;

		/* Renderbuffer size - implementation specific. Typically 4096 for M100 and M200  */
		case GL_MAX_RENDERBUFFER_SIZE:
			_gles_convert_from_int( params, 0, (GLint)GLES_MAX_RENDERBUFFER_SIZE, type);
		break;
#endif
		case GL_POINT_SIZE_ARRAY_TYPE_OES:
			_gles_convert_from_enum( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POINT_SIZE ].type, type );
		break;
		case GL_POINT_SIZE_ARRAY_STRIDE_OES:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POINT_SIZE ].given_stride, type );
		break;
		case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POINT_SIZE ].buffer_binding, type );
		break;
#if EXTENSION_TEXTURE_LOD_BIAS_EXT_ENABLE
		case GL_MAX_TEXTURE_LOD_BIAS_EXT:
			_gles_convert_from_ftype( params, 0, FIXED_TO_FTYPE( GLES1_MAX_TEXTURE_LOD_BIAS ), type );
		break;
#endif


		/* the following states are stored as ftype ( float/fixed ) */
		/* table 6.5 */
		case GL_CURRENT_COLOR:
			/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;

			for (i = 0; i < 4; i++)
			{
				_gles_convert_from_ftype( params, i, ( GLftype )state_gles1->current.color[ i ], type );
			}
		break;

		case GL_CURRENT_TEXTURE_COORDS:
			active_texture = ( GLuint )state_common->texture_env.active_texture;
			for (i = 0; i < 4; i++)
			{
				_gles_convert_from_ftype( params, i, ( GLftype )state_gles1->current.tex_coord[ active_texture ][ i ], type );
			}
		break;
		case GL_CURRENT_NORMAL:
			/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;

			for (i = 0; i < 3; i++)
			{
				_gles_convert_from_ftype( params, i, ( GLftype )state_gles1->current.normal[ i ], type);
			}
		break;
		case GL_MODELVIEW_MATRIX_FLOAT_AS_INT_BITS_OES:
			/* the type is set to GLES_FLOAT because the spec. states that we should return the IEEE float representation of the matrix in an integer array */
			_gles1_convert_matrix(&state_gles1->transform.modelview_matrix[ ( GLuint )state_gles1->transform.modelview_matrix_stack_depth-1 ], params, GLES_FLOAT );
		break;
		case GL_PROJECTION_MATRIX_FLOAT_AS_INT_BITS_OES:
			/* the type is set to GLES_FLOAT because the spec. states that we should return the IEEE float representation of the matrix in an integer array */
			_gles1_convert_matrix(&state_gles1->transform.projection_matrix[ ( GLuint )state_gles1->transform.projection_matrix_stack_depth-1 ], params, GLES_FLOAT );
		break;
		case GL_TEXTURE_MATRIX_FLOAT_AS_INT_BITS_OES:
			/* the type is set to GLES_FLOAT because the spec. states that we should return the IEEE float representation of the matrix in an integer array */
			/** uses only active_texture, not as an offset, since the array spans over max number of textures */
			active_texture = state_common->texture_env.active_texture;
			_gles1_convert_matrix(&state_gles1->transform.texture_matrix[ active_texture ][ ( GLuint )state_gles1->transform.texture_matrix_stack_depth[ active_texture ] -1], params, GLES_FLOAT );
		break;
		/** Transformation state */
		case GL_NORMALIZE:
			_gles_convert_from_boolean( params, 0, state_gles1->transform.normalize, type );
		break;
		case GL_RESCALE_NORMAL:
			_gles_convert_from_boolean( params, 0, state_gles1->transform.rescale_normal, type );
		break;
		case GL_CLIP_PLANE0:
		case GL_CLIP_PLANE1:
		case GL_CLIP_PLANE2:
		case GL_CLIP_PLANE3:
		case GL_CLIP_PLANE4:
		case GL_CLIP_PLANE5:
		{
			GLint index;
			index = (GLint) pname - (GLint) GL_CLIP_PLANE0;

			/* Need to handle the situation that the number of clip-planes
			 * actually supported by the driver is less than the number of
			 * planes we have defines for.
			 */
			MALI_CHECK_RANGE( index, 0, GLES1_MAX_CLIP_PLANES - 1, GL_INVALID_ENUM);
			MALI_DEBUG_ASSERT( 1 == GLES1_MAX_CLIP_PLANES, ("Invalid number of clip planes, need to change code here to work") );
			_gles_convert_from_boolean( params, 0, vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_CLIPPLANE_PLANE0), type );
		}
		break;
		case GL_COLOR_LOGIC_OP:
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_color_logic_op( ctx ), type );;
		break;
		case GL_COLOR_MATERIAL:
			_gles_convert_from_boolean( params, 0, COLORMATERIAL_AMBIENTDIFFUSE == vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL), type );
		break;
		case GL_DEPTH_TEST:
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_depth_test( ctx ), type );
		break;
		case GL_FOG:
			_gles_convert_from_boolean( params, 0, vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST) ? 1 : 0, type );
		break;
		case GL_LIGHT0:
		case GL_LIGHT1:
		case GL_LIGHT2:
		case GL_LIGHT3:
		case GL_LIGHT4:
		case GL_LIGHT5:
		case GL_LIGHT6:
		case GL_LIGHT7:
		{
			GLint index;
			index = (GLint) pname - (GLint) GL_LIGHT0;
			MALI_CHECK_RANGE( index, 0, GLES1_MAX_LIGHTS - 1, GL_INVALID_ENUM );

			_gles_convert_from_boolean( params, 0, (GLboolean)mali_statebit_get( state_common, GLES_STATE_LIGHT0_ENABLED + index ), type );
		}
		break;
		/** Rasterization state */
		case GL_POINT_SMOOTH:
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_point_smooth( ctx ), type );
		break;
		case GL_LINE_SMOOTH:
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_line_smooth( ctx ), type );
		break;
		case GL_POLYGON_OFFSET_FILL:
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_polygon_offset_fill( ctx ), type );
		break;
		case GL_CULL_FACE:
			_gles_convert_from_boolean( params, 0, state_common->rasterization.cull_face, type );
		break;

		/** Texture state */
		case GL_TEXTURE_2D:
			_gles_convert_from_boolean( params, 0, state_common->texture_env.unit[ state_common->texture_env.active_texture ].enable_vector[GLES_TEXTURE_TARGET_2D], type );
		break;
		case GL_TEXTURE_EXTERNAL_OES:
			_gles_convert_from_boolean( params, 0, state_common->texture_env.unit[ state_common->texture_env.active_texture ].enable_vector[GLES_TEXTURE_TARGET_EXTERNAL], type );
		break;


		/** Vertex array state */
		case GL_VERTEX_ARRAY:
			_gles_convert_from_boolean( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POSITION ].enabled, type );
		break;
		case GL_NORMAL_ARRAY:
			_gles_convert_from_boolean( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_NORMAL ].enabled, type );
		break;
		case GL_COLOR_ARRAY:
			_gles_convert_from_boolean( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_COLOR ].enabled, type );
		break;
		case GL_TEXTURE_COORD_ARRAY:
			_gles_convert_from_boolean( params, 0, state_common->vertex_array.attrib_array[ ( GLuint )GLES_VERTEX_ATTRIB_TEX_COORD0 + ( GLuint )state_common->vertex_array.client_active_texture ].enabled, type );
		break;
		case  GL_POINT_SIZE_ARRAY_OES:
			_gles_convert_from_boolean( params, 0, state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POINT_SIZE ].enabled, type );
		break;
#if EXTENSION_ROBUSTNESS_EXT_ENABLE
		case GL_CONTEXT_ROBUST_ACCESS_EXT:
			_gles_convert_from_boolean( params, 0, ctx->robust_strategy ? GL_TRUE:GL_FALSE, type );
		break;
		case GL_RESET_NOTIFICATION_STRATEGY_EXT:
			_gles_convert_from_int( params, 0, (GLint)ctx->robust_strategy, type );
		break;
#endif
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

GLenum _gles1_get_tex_param( struct gles_common_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type )
{
	gles_texture_unit  *tex_unit;
	gles_texture_object *tex;
	enum gles_texture_target dimensionality;

	MALI_DEBUG_ASSERT_POINTER( state );

	dimensionality = _gles_get_dimensionality(target, GLES_API_VERSION_1);
	if ((int)dimensionality == GLES_TEXTURE_TARGET_INVALID) return GL_INVALID_ENUM;

	/** Get the texture-object from the context */
	tex_unit = &state->texture_env.unit[ state->texture_env.active_texture ];
	tex = tex_unit->current_texture_object[dimensionality];

	switch(pname)
	{
		case GL_TEXTURE_MIN_FILTER:
			_gles_convert_from_enum( params, 0, tex->min_filter, type );
		break;
		case GL_TEXTURE_MAG_FILTER:
			_gles_convert_from_enum( params, 0, tex->mag_filter, type );
		break;
		case GL_TEXTURE_WRAP_S:
			_gles_convert_from_enum( params, 0, tex->wrap_s, type );
		break;
		case GL_TEXTURE_WRAP_T:
			_gles_convert_from_enum( params, 0, tex->wrap_t, type );
		break;
		case GL_GENERATE_MIPMAP:
			_gles_convert_from_boolean( params, 0, ( GLboolean )tex->generate_mipmaps, type );
		break;
#if EXTENSION_DRAW_TEX_OES_ENABLE
		case GL_TEXTURE_CROP_RECT_OES:
			_gles_convert_from_int( params, 0, tex->crop_rect[ 0 ], type );
			_gles_convert_from_int( params, 1, tex->crop_rect[ 1 ], type );
			_gles_convert_from_int( params, 2, tex->crop_rect[ 2 ], type );
			_gles_convert_from_int( params, 3, tex->crop_rect[ 3 ], type );
		break;
#endif

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		/* external images uses the texture image units */
		case GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES:
		{
			int num_image_units = 1;
			if ( GL_TEXTURE_EXTERNAL_OES == target )
			{
				num_image_units = tex->yuv_info.image_units_count;
			}
			_gles_convert_from_enum( params, 0, num_image_units, type );
		}
		break;
#endif

		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}
	GL_SUCCESS;
}

GLenum _gles1_is_enabled( gles_context *ctx, GLenum cap, GLboolean *enabled )
{
	GLint index;
	gles_common_state *state_common;
	gles1_state       *state_gles1;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );
	MALI_DEBUG_ASSERT_POINTER( enabled );

	state_common = &(ctx->state.common);
	state_gles1  = ctx->state.api.gles1;

	switch( cap )
	{
		/** Vertex array state */
		case GL_VERTEX_ARRAY:
			*enabled = state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POSITION ].enabled;
		break;
		case GL_NORMAL_ARRAY:
			*enabled = state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_NORMAL ].enabled;
		break;
		case GL_COLOR_ARRAY:
			*enabled = state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_COLOR ].enabled;
		break;
		case GL_TEXTURE_COORD_ARRAY:
			*enabled = state_common->vertex_array.attrib_array[ ( GLuint )GLES_VERTEX_ATTRIB_TEX_COORD0 +
			                                                              ( GLuint )state_common->vertex_array.client_active_texture ].enabled;
		break;

		/** Transformation state */
		case GL_NORMALIZE:
			*enabled = state_gles1->transform.normalize;
		break;
		case GL_RESCALE_NORMAL:
			*enabled = state_gles1->transform.rescale_normal;
		break;
		case GL_CLIP_PLANE0:
		case GL_CLIP_PLANE1:
		case GL_CLIP_PLANE2:
		case GL_CLIP_PLANE3:
		case GL_CLIP_PLANE4:
		case GL_CLIP_PLANE5:
			index = (GLint) cap - (GLint) GL_CLIP_PLANE0;

			/* Need to handle the situation that the number of clip-planes
			 * actually supported by the driver is less than the number of
			 * planes we have defines for.
			 */
			MALI_CHECK_RANGE( index, 0, GLES1_MAX_CLIP_PLANES - 1, GL_INVALID_ENUM);
			MALI_DEBUG_ASSERT( 1 == GLES1_MAX_CLIP_PLANES, ("Invalid number of clip planes, need to change code here to work") );
			*enabled = vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_CLIPPLANE_PLANE0);
		break;

		/** Coloring state */
		case GL_FOG:
			*enabled = vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST) ? 1 : 0;
		break;
		case GL_BLEND:
			*enabled = (GLboolean)_gles_fb_get_blend( ctx );
		break;

		/** Lighting state */
		case GL_LIGHTING:
			*enabled = state_gles1->lighting.enabled;
		break;
		case GL_COLOR_MATERIAL:
			*enabled = COLORMATERIAL_AMBIENTDIFFUSE == vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL);
		break;
		case GL_LIGHT0:
		case GL_LIGHT1:
		case GL_LIGHT2:
		case GL_LIGHT3:
		case GL_LIGHT4:
		case GL_LIGHT5:
		case GL_LIGHT6:
		case GL_LIGHT7:
			index = (GLint) cap - (GLint) GL_LIGHT0;
			MALI_CHECK_RANGE( index, 0, GLES1_MAX_LIGHTS - 1, GL_INVALID_ENUM );

			*enabled = (GLboolean)mali_statebit_get( state_common, GLES_STATE_LIGHT0_ENABLED + index );
		break;

		/** Rasterization state */
		case GL_POINT_SMOOTH:
			*enabled = (GLboolean)_gles_fb_get_point_smooth( ctx );
		break;
		case GL_LINE_SMOOTH:
			*enabled = (GLboolean)_gles_fb_get_line_smooth( ctx );
		break;
		case GL_POLYGON_OFFSET_FILL:
			*enabled = (GLboolean)_gles_fb_get_polygon_offset_fill( ctx );
		break;
		case GL_CULL_FACE:
			*enabled = state_common->rasterization.cull_face;
		break;

		/** Multisampling state */
		case GL_MULTISAMPLE:
			*enabled = (GLboolean)_gles_fb_get_multisample( ctx );
		break;
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
			*enabled = (GLboolean)_gles_fb_get_sample_alpha_to_coverage( ctx );
		break;
		case GL_SAMPLE_ALPHA_TO_ONE:
			*enabled = (GLboolean)_gles_fb_get_sample_alpha_to_one( ctx );
		break;
		case GL_SAMPLE_COVERAGE:
			*enabled = (GLboolean)_gles_fb_get_sample_coverage( ctx );
		break;

		/** Texture state */
		case GL_TEXTURE_2D:
			*enabled = state_common->texture_env.unit[ state_common->texture_env.active_texture ].enable_vector[GLES_TEXTURE_TARGET_2D];
		break;
		case GL_TEXTURE_EXTERNAL_OES:
			*enabled = state_common->texture_env.unit[ state_common->texture_env.active_texture ].enable_vector[GLES_TEXTURE_TARGET_EXTERNAL];
		break;


		/** Pixel operations */
		case GL_SCISSOR_TEST:
			*enabled = (GLboolean)mali_statebit_get( state_common, GLES_STATE_SCISSOR_ENABLED );
		break;
		case GL_ALPHA_TEST:
			*enabled = (GLboolean)_gles_fb_get_alpha_test( ctx );
		break;
		case GL_STENCIL_TEST:
			*enabled = (GLboolean)_gles_fb_get_stencil_test( ctx );
		break;

		case GL_DEPTH_TEST:
			*enabled = (GLboolean)_gles_fb_get_depth_test( ctx );
		break;
		case GL_DITHER:
			*enabled = (GLboolean)_gles_fb_get_dither( ctx );
		break;
		case GL_COLOR_LOGIC_OP:
			*enabled = (GLboolean)_gles_fb_get_color_logic_op( ctx );
		break;
		case  GL_POINT_SIZE_ARRAY_OES:
			*enabled = state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POINT_SIZE ].enabled;
		break;
		/* point_sprite is a global enable/disable, not per tex unit! */
		case GL_POINT_SPRITE_OES:
			*enabled = state_gles1->texture_env.point_sprite_enabled;
		break;
		case GL_COORD_REPLACE_OES:
			*enabled = state_gles1->texture_env.unit[ 0 ].coord_replace;
		break;
#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
		case GL_MATRIX_PALETTE_OES:
			*enabled = vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING);
		break;
		case GL_MATRIX_INDEX_ARRAY_OES:
			*enabled = state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_MATRIX_INDEX ].enabled;
		break;
		case GL_WEIGHT_ARRAY_OES:
			*enabled = state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_WEIGHT ].enabled;
		break;
#endif

		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

GLenum _gles1_get_tex_env( struct gles_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type )
{
	gles1_texture_unit *tex_unit;
	GLint i;

	MALI_DEBUG_ASSERT_POINTER( state );

	tex_unit = &state->api.gles1->texture_env.unit[ state->common.texture_env.active_texture ];

	if (target != GL_TEXTURE_ENV)
	{
		if ((GL_POINT_SPRITE_OES == target) && (GL_COORD_REPLACE_OES == pname))
		{
			_gles_convert_from_int( params, 0, ( GLint ) tex_unit->coord_replace, type );
			GL_SUCCESS;
		}

		MALI_ERROR( GL_INVALID_ENUM );
	}

	switch( pname )
	{
		case GL_TEXTURE_ENV_MODE:
			_gles_convert_from_enum( params, 0, tex_unit->env_mode, type );
		break;
		case GL_TEXTURE_ENV_COLOR:
			/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
			if (GLES_INT == type) type = GLES_NORMALIZED_INT;

			for (i = 0; i < 4; i++)
			{
				_gles_convert_from_ftype( params, i, ( GLftype )tex_unit->env_color[ i ], type );
			}
		break;
		case GL_COMBINE_RGB:
			_gles_convert_from_enum( params, 0, tex_unit->combine_rgb, type );
		break;
		case GL_COMBINE_ALPHA:
			_gles_convert_from_enum( params, 0, tex_unit->combine_alpha, type );
		break;
		case GL_SRC0_RGB:
		case GL_SRC1_RGB:
		case GL_SRC2_RGB:
			i = ( GLint )pname - ( GLint )GL_SRC0_RGB;
			_gles_convert_from_enum( params, 0, tex_unit->src_rgb[ i ], type );
		break;
		case GL_SRC0_ALPHA:
		case GL_SRC1_ALPHA:
		case GL_SRC2_ALPHA:
			i = ( GLint )pname - ( GLint )GL_SRC0_ALPHA;
			_gles_convert_from_enum( params, 0, tex_unit->src_alpha[ i ], type );
		break;
		case GL_OPERAND0_RGB:
		case GL_OPERAND1_RGB:
		case GL_OPERAND2_RGB:
			i = ( GLint )pname - ( GLint )GL_OPERAND0_RGB;
			_gles_convert_from_enum( params, 0, tex_unit->operand_rgb[ i ], type );
		break;
		case GL_OPERAND0_ALPHA:
		case GL_OPERAND1_ALPHA:
		case GL_OPERAND2_ALPHA:
			i = ( GLint )pname - ( GLint )GL_OPERAND0_ALPHA;
			_gles_convert_from_enum( params, 0, tex_unit->operand_alpha[ i ], type );
		break;
		case GL_RGB_SCALE:
			_gles_convert_from_int( params, 0, ( GLint )tex_unit->rgb_scale, type );
		break;
		case GL_ALPHA_SCALE:
			_gles_convert_from_int( params, 0, ( GLint )tex_unit->alpha_scale, type );
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}
	GL_SUCCESS;

}


GLenum _gles1_get_clip_plane( struct gles_state *state, GLenum plane, GLvoid *equation, gles_datatype type )
{
	GLint i;
	GLint p_nr = ( GLint )plane - ( GLint )GL_CLIP_PLANE0;

	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->	api.gles1 );

	if ((plane < GL_CLIP_PLANE0) || (plane>=GL_CLIP_PLANE0+GLES1_MAX_CLIP_PLANES)) MALI_ERROR( GL_INVALID_ENUM );

	for (i = 0; i < 4; i++)
	{
		MALI_CHECK_RANGE(p_nr, 0, GLES1_MAX_CLIP_PLANES-1, GL_INVALID_ENUM);
		_gles_convert_from_ftype( equation, i, ( GLftype )state->api.gles1->transform.clip_plane[ p_nr ][ i ], type );
	}
	GL_SUCCESS;
}

GLenum _gles1_get_pointer( struct gles_state *state, GLenum pname, GLvoid **pointer )
{
	gles_common_state *state_common;
	MALI_DEBUG_ASSERT_POINTER( state );
	state_common  = &(state->common);

	switch( pname )
	{
		case GL_VERTEX_ARRAY_POINTER:
			( *pointer ) = ( GLvoid * ) state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POSITION ].pointer;
		break;
		case GL_NORMAL_ARRAY_POINTER:
			( *pointer ) = ( GLvoid * ) state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_NORMAL ].pointer;
		break;
		case GL_COLOR_ARRAY_POINTER:
			( *pointer ) = ( GLvoid * ) state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_COLOR ].pointer;
		break;
		case GL_TEXTURE_COORD_ARRAY_POINTER:
			( *pointer ) = ( GLvoid * ) state_common->vertex_array.attrib_array[ ( GLuint )GLES_VERTEX_ATTRIB_TEX_COORD0 +
			                                                                           ( GLuint )state_common->vertex_array.client_active_texture ].pointer;
		break;
#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
		case GL_MATRIX_INDEX_ARRAY_POINTER_OES:
			( *pointer ) = ( GLvoid * ) state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_MATRIX_INDEX ].pointer;
		break;
		case GL_WEIGHT_ARRAY_POINTER_OES:
			( *pointer ) = ( GLvoid * ) state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_WEIGHT ].pointer;
		break;
#endif
		case GL_POINT_SIZE_ARRAY_POINTER_OES:
			(*pointer) = ( GLvoid * )state_common->vertex_array.attrib_array[ GLES_VERTEX_ATTRIB_POINT_SIZE ].pointer;
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

GLenum _gles1_get_buffer_parameter( struct gles_common_state *state, GLenum target, GLenum pname, GLint *params )
{
	gles_buffer_object *dst_ptr;

	MALI_DEBUG_ASSERT_POINTER( state );

	switch (target)
	{
		case GL_ARRAY_BUFFER:
			dst_ptr     = state->vertex_array.vbo;
		break;
		case GL_ELEMENT_ARRAY_BUFFER:
			dst_ptr     = state->vertex_array.element_vbo;
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	/* Return invalid operation if zero is bound */
	if (NULL == dst_ptr) return GL_INVALID_OPERATION;

	switch( pname )
	{
		case GL_BUFFER_SIZE:
			if (NULL == params) GL_SUCCESS;
			( *params ) = ( GLint )dst_ptr->size;
		break;
		case GL_BUFFER_USAGE:
			if (NULL == params) GL_SUCCESS;
			( *params ) = ( GLint )dst_ptr->usage;
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

GLenum _gles1_get_material( struct gles_state *state, GLenum face, GLenum pname, GLvoid *params, gles_datatype type )
{
	GLftype *temp;
	GLint i;
	GLint nvalues;

	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles1 );

	if (GL_FRONT != face && GL_BACK != face) MALI_ERROR( GL_INVALID_ENUM );

	switch( pname )
	{
		case GL_AMBIENT:
			temp = state->api.gles1->lighting.ambient;
			nvalues = 4;
		break;
		case GL_DIFFUSE:
			temp = state->api.gles1->lighting.diffuse;
			nvalues = 4;
		break;
		case GL_EMISSION:
			temp = state->api.gles1->lighting.emission;
			nvalues = 4;
		break;
		case GL_SPECULAR:
			temp = state->api.gles1->lighting.specular;
			nvalues = 4;
		break;
		case GL_LIGHT_MODEL_AMBIENT:
			temp = state->api.gles1->lighting.light_model_ambient;
			nvalues = 4;
		break;
		case GL_SHININESS:
			temp = &state->api.gles1->lighting.shininess;
			nvalues = 1;
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal */
	if (GLES_INT == type) type = GLES_NORMALIZED_INT;

	/** transfer the selected ftype-table to params */
	for (i = 0; i < nvalues; i++)
	{
		_gles_convert_from_ftype( params, i, ( GLftype )temp[ i ], type );
	}

	GL_SUCCESS;
}

GLenum _gles1_get_light( struct gles_state *state, GLenum light, GLenum pname, GLvoid *params, gles_datatype type)
{
	const int   lindex = (int) light - (int) GL_LIGHT0;

	int i;

	if( lindex < 0 )			MALI_ERROR(  GL_INVALID_ENUM );
	if( lindex >= GLES1_MAX_LIGHTS )	MALI_ERROR(  GL_INVALID_ENUM );

	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles1 );

	{
		gles1_lighting * const lighting = & state->api.gles1->lighting;
		const gles_light * const l = & lighting->light[ lindex ];

		switch( pname )
		{
			case GL_AMBIENT:
				/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal
				 */
				if (GLES_INT == type) type = GLES_NORMALIZED_INT;

				for (i = 0; i < 4; i++)
				{
					_gles_convert_from_ftype( params, i, ( GLftype )l->ambient[ i ], type );
				}
			break;
			case GL_DIFFUSE:
				/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal
				 */
				if (GLES_INT == type) type = GLES_NORMALIZED_INT;

				for (i = 0; i < 4; i++)
				{
					_gles_convert_from_ftype( params, i, ( GLftype )l->diffuse[ i ], type );
				}
			break;
			case GL_SPECULAR:
				/* Apply data conversion rule for RGBA color, depth range, depth clear value, and normal
				 */
				if (GLES_INT == type) type = GLES_NORMALIZED_INT;

				for (i = 0; i < 4; i++)
				{
					_gles_convert_from_ftype( params, i, ( GLftype )l->specular[ i ], type );
				}
			break;
			case GL_POSITION:
				for (i = 0; i < 4; i++)
				{
					_gles_convert_from_ftype( params, i, ( GLftype )l->position[ i ], type );
				}
			break;
			case GL_CONSTANT_ATTENUATION:
				_gles_convert_from_ftype( params, 0, ( GLftype )l->attenuation[ 0 ], type );
			break;
			case GL_LINEAR_ATTENUATION:
				_gles_convert_from_ftype( params, 0, ( GLftype )l->attenuation[ 1 ], type );
			break;
			case GL_QUADRATIC_ATTENUATION:
				_gles_convert_from_ftype( params, 0, ( GLftype )l->attenuation[ 2 ], type );
			break;
			case GL_SPOT_DIRECTION:
				for (i = 0; i < 3; i++)
				{
					_gles_convert_from_ftype( params, i, ( GLftype )l->spot_direction[ i ], type );
				}
			break;
			case GL_SPOT_EXPONENT:
				_gles_convert_from_ftype( params, 0, ( GLftype )l->spot_exponent, type );
			break;
			case GL_SPOT_CUTOFF:
				_gles_convert_from_ftype( params, 0, ( GLftype )lighting->spotlight_cutoff[lindex], type );
			break;
			default:
				MALI_ERROR( GL_INVALID_ENUM );
		}
	}
	GL_SUCCESS;
}

GLenum _gles1_get_string( struct gles_context *ctx, GLenum name, const GLubyte **return_string )
{
#ifdef NEXELL_FEATURE_ENABLE
#ifdef DEBUG
	static const GLubyte vendor[] = "Nexell-1.11D";
#else
	static const GLubyte vendor[] = "Nexell-1.11";
#endif
#else
	static const GLubyte vendor[] = "ARM";
#endif

	/* MALI_GLES_1_MINOR_VERSION is defined by the build system */
	static const GLubyte version[] = "OpenGL ES-CM " GLES_MAKE_VERSION_STRING(MALI_GLES_1_MAJOR_VERSION, MALI_GLES_1_MINOR_VERSION);

	static const GLubyte extensions[] =
		"GL_OES_byte_coordinates "
		"GL_OES_fixed_point "
		"GL_OES_single_precision "
		"GL_OES_matrix_get "
		"GL_OES_read_format "
		"GL_OES_compressed_paletted_texture "
		"GL_OES_point_size_array "
		"GL_OES_point_sprite "
		"GL_OES_texture_npot "
#if EXTENSION_QUERY_MATRIX_OES_ENABLE
		"GL_OES_query_matrix "
#endif
#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
		"GL_OES_matrix_palette "
#endif
#if EXTENSION_EXTENDED_MATRIX_PALETTE_OES_ENABLE
		"GL_OES_extended_matrix_palette "
#endif
#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
		"GL_OES_compressed_ETC1_RGB8_texture "
#endif
#if EXTENSION_EGL_IMAGE_OES_ENABLE
		"GL_OES_EGL_image "
#endif
#if EXTENSION_DRAW_TEX_OES_ENABLE
		"GL_OES_draw_texture "
#endif
#if EXTENSION_DEPTH_TEXTURE_ENABLE
		"GL_OES_depth_texture "
#endif
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		"GL_OES_packed_depth_stencil "
#endif
#if EXTENSION_BGRA8888_ENABLE
		"GL_EXT_texture_format_BGRA8888 "
#endif
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
		"GL_OES_framebuffer_object "
		"GL_OES_stencil8 "
#endif
#if EXTENSION_DEPTH24_OES_ENABLE
		"GL_OES_depth24 "
#endif
#if EXTENSION_ARM_RGBA8_ENABLE
		"GL_ARM_rgba8 "
#endif
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		"GL_OES_EGL_image_external "
#endif
#if EXTENSION_EGL_FENCE_SYNC_OES_ENABLE
		"GL_OES_EGL_sync "
#endif
#if EXTENSION_RGB8_RGBA8_ENABLE
		"GL_OES_rgb8_rgba8 "
#endif
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
		"GL_EXT_multisampled_render_to_texture "
#endif
#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE
		"GL_OES_texture_cube_map "
#endif
#if EXTENSION_DISCARD_FRAMEBUFFER
		"GL_EXT_discard_framebuffer "
#endif		
#if EXTENSION_ROBUSTNESS_EXT_ENABLE
		"GL_EXT_robustness "
#endif
	;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	switch (name)
	{
		case GL_VENDOR:
			*return_string = vendor;
			break;
		case GL_VERSION:
			*return_string = version;
			break;
		case GL_RENDERER:
				*return_string = ctx->renderer;
			break;
		case GL_EXTENSIONS:
			*return_string = extensions;
			break;
		default:
			*return_string = NULL;
			MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}


/* Internal function definitions */
MALI_STATIC void _gles1_convert_matrix(mali_matrix4x4 *mat, GLvoid *params, gles_datatype type)
{
	GLint row, col;

	for (row = 0; row < 4; row++)
	{
		for (col = 0; col < 4; col++)
		{
			_gles_convert_from_ftype( params, row*4+col, ( GLftype )( ( *mat )[ row ][ col ] ), type ) ;
		}
	}
}
