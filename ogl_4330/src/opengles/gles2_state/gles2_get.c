/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_base.h>
#include <gles_context.h>
#include <gles_fb_interface.h>
#include <gles_config_extension.h>
#include <gles_buffer_object.h>
#include <gles_util.h>
#include <gles_convert.h>
#include <gles_ftype.h>

#include "gles2_state.h"
#include "gles2_get.h"

#define MALI_GLES_2_MAJOR_VERSION 2


GLenum _gles2_getv( gles_context *ctx, GLenum pname, GLvoid *params, gles_datatype type )
{
	int i;
	gles_texture_unit *tex_unit;
	gles_state *state;

	const GLenum compressed_formats[] =
	{
#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
		 GL_ETC1_RGB8_OES,
#endif
		 0 /* zero-sized arrays are not allowed, so make sure we always have at least one entry */
	};

	/* calculate the number of formats from the array size. Subtract 1 for the dummy entry */
	const int num_compressed_formats = MALI_ARRAY_SIZE( compressed_formats ) - 1;
	gles_common_state  *state_common;
	gles2_state        *state_gles2;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	state = &ctx->state;

	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles2 );

	state_common = &(state->common);
	state_gles2 = state->api.gles2;

	/* these are the only legal datatypes we're supporting GetBooleanv( ), GetFixedv( ), GetFloatv( ) and GetIntegerv( ) */
	MALI_DEBUG_ASSERT( ( type == GLES_FIXED ) || ( type == GLES_INT ) || ( type == GLES_BOOLEAN ) || ( type == GLES_FLOAT ),
						  ( "illegal datatype\n" ) );

	tex_unit = &state_common->texture_env.unit[ state_common->texture_env.active_texture ];

	/* the get'ing below is ordered after their storage type, as defined in section 6.2 of the GLES 1.1 spec */
	switch( pname )
	{
		/* the following states are stored as ints */
		/* table 6.6 */
		case GL_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.array_buffer_binding, type );
		break;
		case GL_ELEMENT_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( params, 0, ( GLint )state_common->vertex_array.element_array_buffer_binding, type );
		break;

		/** Transform state( including viewport state ) */
		case GL_VIEWPORT:
			_gles_convert_from_int( params, 0, ( GLint )state_common->viewport.x, type );
			_gles_convert_from_int( params, 1, ( GLint )state_common->viewport.y, type );
			_gles_convert_from_int( params, 2, ( GLint )state_common->viewport.width, type );
			_gles_convert_from_int( params, 3, ( GLint )state_common->viewport.height, type );
		break;
		case GL_DEPTH_RANGE:
			if(GLES_INT == type) type = GLES_NORMALIZED_INT;

			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->viewport.z_nearfar[0], type );
			_gles_convert_from_ftype( params, 1, ( GLftype )state_common->viewport.z_nearfar[1], type );
		break;
		case GL_DEPTH_TEST:
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_depth_test( ctx ), type );
		break;

		/** Rasterization state */
		case GL_LINE_WIDTH:
			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->rasterization.line_width, type );
		break;
		case GL_CULL_FACE: 
			_gles_convert_from_boolean( params, 0, (GLboolean)state_common->rasterization.cull_face, type );
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
		case GL_POLYGON_OFFSET_FILL: 
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_polygon_offset_fill( ctx ), type );
		break;
		
		/** Multisampling-state */
		case GL_SAMPLE_ALPHA_TO_COVERAGE: 
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_sample_alpha_to_coverage( ctx ), type );
		break;
		case GL_SAMPLE_COVERAGE: 
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_sample_coverage( ctx ), type );
		break;
		case GL_SAMPLE_COVERAGE_VALUE:
			_gles_convert_from_ftype( params, 0, _gles_fb_get_sample_coverage_value( ctx ), type );
		break;
		case GL_SAMPLE_COVERAGE_INVERT:
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_sample_coverage_invert( ctx ), type );
		break;

		/** Texture-state */
		case GL_TEXTURE_BINDING_2D:
			_gles_convert_from_int( params, 0, ( GLint ) tex_unit->current_texture_id[ GLES_TEXTURE_TARGET_2D ], type);
		break;
		/** Texture-env-state */
		case GL_ACTIVE_TEXTURE:
			_gles_convert_from_enum( params, 0, (GLenum)((GLint)GL_TEXTURE0 + (GLint)state_common->texture_env.active_texture), type );
		break;

		case GL_TEXTURE_BINDING_CUBE_MAP:
			_gles_convert_from_int( params, 0, ( GLint ) tex_unit->current_texture_id[ GLES_TEXTURE_TARGET_CUBE ], type);
		break;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		case GL_TEXTURE_BINDING_EXTERNAL_OES:
			_gles_convert_from_int( params, 0, ( GLint ) tex_unit->current_texture_id[ GLES_TEXTURE_TARGET_EXTERNAL ], type);
		break;
#endif

		/** Framebuffer-state */
		case GL_COLOR_WRITEMASK:
			for( i = 0; i < 4; i++ )
			{
				_gles_convert_from_boolean( params, i, ( GLboolean )state_common->framebuffer_control.color_is_writeable[ i ], type );
			}
		break;
		case GL_DEPTH_WRITEMASK:
			_gles_convert_from_boolean( params, 0, ( GLboolean )state_common->framebuffer_control.depth_is_writeable, type );
		break;

		case GL_COLOR_CLEAR_VALUE:
			if( GLES_INT == type ) type = GLES_NORMALIZED_INT;

			for( i = 0; i < 4; i++ )
			{
				_gles_convert_from_ftype( params, i, ( GLftype )state_common->framebuffer_control.color_clear_value[ i ], type );
			}
		break;
		case GL_DEPTH_CLEAR_VALUE:
			if(GLES_INT == type) type = GLES_NORMALIZED_INT;

			_gles_convert_from_ftype( params, 0, ( GLftype )state_common->framebuffer_control.depth_clear_value, type );

		break;

		/** Pixel-operations */
		case GL_SCISSOR_BOX:
			_gles_convert_from_int( params, 0, ( GLint )state_common->scissor.x, type );
			_gles_convert_from_int( params, 1, ( GLint )state_common->scissor.y, type );
			_gles_convert_from_int( params, 2, ( GLint )state_common->scissor.width, type );
			_gles_convert_from_int( params, 3, ( GLint )state_common->scissor.height, type );
		break;
		case GL_SCISSOR_TEST:
			_gles_convert_from_enum( params, 0, (GLboolean)mali_statebit_get( state_common, GLES_STATE_SCISSOR_ENABLED), type );
		break;
		case GL_STENCIL_TEST: 
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_stencil_test( ctx ), type );
		break;
		case GL_STENCIL_FUNC:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_conditional( _gles_fb_get_stencil_func( ctx ) ), type );
		break;
		case GL_STENCIL_BACK_FUNC:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_conditional( _gles_fb_get_stencil_back_func( ctx ) ), type );
		break;
		case GL_STENCIL_FAIL:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_fail( ctx ) ), type );
		break;
		case GL_STENCIL_BACK_FAIL:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_back_fail( ctx ) ), type );
		break;
		case GL_STENCIL_PASS_DEPTH_FAIL:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_depth_fail( ctx ) ), type );
		break;
		case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_back_depth_fail( ctx ) ), type );
		break;
		case GL_STENCIL_PASS_DEPTH_PASS:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_depth_pass( ctx ) ), type );
		break;
		case GL_STENCIL_BACK_PASS_DEPTH_PASS:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_stencil_operation( _gles_fb_get_stencil_back_depth_pass( ctx ) ), type );
		break;
		case GL_STENCIL_REF:
			_gles_convert_from_int( params, 0, _gles_fb_get_stencil_ref( ctx ), type );
		break;
		case GL_STENCIL_BACK_REF:
			_gles_convert_from_int( params, 0, _gles_fb_get_stencil_back_ref( ctx ), type );
		break;
		case GL_STENCIL_VALUE_MASK:
			_gles_convert_from_int( params, 0, _gles_fb_get_stencil_value_mask( ctx ), type );
		break;
		case GL_STENCIL_BACK_VALUE_MASK:
			_gles_convert_from_int( params, 0, _gles_fb_get_stencil_back_value_mask( ctx ), type );
		break;
		case GL_STENCIL_WRITEMASK:
			_gles_convert_from_int( params, 0, _gles_fb_get_stencil_write_mask( ctx ), type );
		break;
        case GL_STENCIL_BACK_WRITEMASK:
			_gles_convert_from_int( params, 0, _gles_fb_get_stencil_back_write_mask( ctx ), type );
		break;
		
		case GL_STENCIL_CLEAR_VALUE:
			_gles_convert_from_int( params, 0, ( GLint )state_common->framebuffer_control.stencil_clear_value, type );
		break;

		case GL_DEPTH_FUNC:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_conditional( _gles_fb_get_depth_func( ctx ) ), type );
		break;

		case GL_BLEND: 
			_gles_convert_from_boolean( params, 0, (GLboolean)_gles_fb_get_blend( ctx ), type );
		break;
		case GL_BLEND_SRC_RGB:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_blend_func( _gles_fb_get_blend_src_rgb( ctx ) ), type );
		break;
		case GL_BLEND_DST_RGB:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_blend_func( _gles_fb_get_blend_dst_rgb( ctx ) ), type );
		break;
		case GL_BLEND_SRC_ALPHA:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_blend_func( _gles_fb_get_blend_src_alpha( ctx ) ), type );
		break;
		case GL_BLEND_DST_ALPHA:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_blend_func( _gles_fb_get_blend_dst_alpha( ctx ) ), type );
		break;
		case GL_BLEND_EQUATION_RGB:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_blend_equation( _gles_fb_get_blend_blend_equation_rgb( ctx ) ), type );
		break;
		case GL_BLEND_EQUATION_ALPHA:
			_gles_convert_from_enum( params, 0, _gles_m200_mali_to_gles_blend_equation( _gles_fb_get_blend_equation_alpha( ctx ) ), type );
		break;
		case GL_BLEND_COLOR:
			if(GLES_INT == type) type = GLES_NORMALIZED_INT;

			_gles_convert_from_ftype( params, 0, _gles_fb_get_blend_color_r( ctx ), type );
			_gles_convert_from_ftype( params, 1, _gles_fb_get_blend_color_g( ctx ), type );
			_gles_convert_from_ftype( params, 2, _gles_fb_get_blend_color_b( ctx ), type );
			_gles_convert_from_ftype( params, 3, _gles_fb_get_blend_color_a( ctx ), type );
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
		case GL_GENERATE_MIPMAP_HINT:
			_gles_convert_from_enum( params, 0, state_gles2->hint.generate_mipmap, type );
		break;
#if EXTENSION_FRAGMENT_SHADER_DERIVATIVE_HINT_OES_ENABLE
		case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
			_gles_convert_from_enum( params, 0, state_gles2->hint.fragment_shader_derivative, type );
		break;
#endif

		/** Implementation dependent values */
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
		case MALI_GL_MAX_SAMPLES_EXT_V1: /* fall-through */
		case MALI_GL_MAX_SAMPLES_EXT_V2:
			_gles_convert_from_int( params, 0, ( GLint )GLES_MAX_FBO_SAMPLES, type );
		break;
#endif
		case GL_SUBPIXEL_BITS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_SUBPIXEL_BITS, type );
		break;
		case GL_MAX_TEXTURE_SIZE:
			_gles_convert_from_int( params, 0, ( GLint )GLES_MAX_TEXTURE_SIZE, type );
		break;
		case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
			_gles_convert_from_int( params, 0, ( GLint )GLES_MAX_CUBE_MAP_TEXTURE_SIZE, type );
		break;
		case GL_MAX_VIEWPORT_DIMS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_MAX_VIEWPORT_DIM_X, type );
			_gles_convert_from_int( params, 1, ( GLint )GLES2_MAX_VIEWPORT_DIM_Y, type );
		break;
		case GL_ALIASED_POINT_SIZE_RANGE:
			_gles_convert_from_ftype( params, 0, ( GLftype )GLES_ALIASED_POINT_SIZE_MIN, type );
			_gles_convert_from_ftype( params, 1, ( GLftype )GLES_ALIASED_POINT_SIZE_MAX, type );
		break;
		case GL_ALIASED_LINE_WIDTH_RANGE:
			_gles_convert_from_ftype( params, 0, ( GLftype )GLES_ALIASED_LINE_WIDTH_MIN, type );
			_gles_convert_from_ftype( params, 1, ( GLftype )GLES_ALIASED_LINE_WIDTH_MAX, type );
		break;
		case GL_COMPRESSED_TEXTURE_FORMATS:
			for( i = 0; i < num_compressed_formats; i++)
			{
				_gles_convert_from_enum( params, i, compressed_formats[ i ], type );
			}
		break;
		case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
			_gles_convert_from_int( params, 0, ( GLint )num_compressed_formats, type );
		break;
		case GL_MAX_VERTEX_ATTRIBS:
			_gles_convert_from_int( params, 0, ( GLint )GLES_VERTEX_ATTRIB_COUNT, type );
		break;
		case GL_MAX_VERTEX_UNIFORM_VECTORS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_MAX_VERTEX_UNIFORM_VECTORS, type );
		break;
		case GL_MAX_VARYING_VECTORS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_MAX_VARYING_VECTORS, type );
		break;
		case GL_MAX_TEXTURE_IMAGE_UNITS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_MAX_TEXTURE_IMAGE_UNITS, type );
		break;
		case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_MAX_COMBINED_TEXTURE_IMAGE_UNITS, type );
		break;
		case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_MAX_VERTEX_TEXTURE_IMAGE_UNITS, type );
		break;
		case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_MAX_FRAGMENT_UNIFORM_VECTORS, type );
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
			_gles_convert_from_int( params, 0, ( GLint )_gles_fbo_get_bits(state->common.framebuffer.current_object, pname), type);
		break;

		/** Core additions and extensions */
		case GL_IMPLEMENTATION_COLOR_READ_TYPE:
			_gles_convert_from_enum( params, 0, GLES_IMPLEMENTATION_COLOR_READ_TYPE, type );
		break;
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
			_gles_convert_from_enum( params, 0, GLES_IMPLEMENTATION_COLOR_READ_FORMAT, type );
		break;

		/* shaders */
		case GL_CURRENT_PROGRAM:
			_gles_convert_from_int( params, 0, (GLint) state_gles2->program_env.current_program, type);
		break;

		/* render buffer */
		case GL_RENDERBUFFER_BINDING:
			_gles_convert_from_int( params, 0, (GLint)state_common->renderbuffer.current_object_id, type);
		break;

		/* render buffer */
		case GL_FRAMEBUFFER_BINDING:
			_gles_convert_from_int( params, 0, (GLint)state_common->framebuffer.current_object_id, type);
		break;

		/* Renderbuffer size - implementation specific. Typically 4096 for M100 and M200  */
		case GL_MAX_RENDERBUFFER_SIZE:
			_gles_convert_from_int( params, 0, (GLint)GLES_MAX_RENDERBUFFER_SIZE, type);
		break;

		case GL_SHADER_COMPILER:
			/* Currently we always support online compilation */
			_gles_convert_from_boolean( params, 0, GL_TRUE, type );
		break;

		case GL_NUM_SHADER_BINARY_FORMATS:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_NUM_SHADER_BINARY_FORMATS, type );
		break;
		case GL_SHADER_BINARY_FORMATS:
			for( i = 0; i < ( GLint )GLES2_NUM_SHADER_BINARY_FORMATS; i++)
			{
				/* Only one format supported at the moment */
				_gles_convert_from_enum( params, i, GL_MALI_SHADER_BINARY_ARM, type );
			}
		break;
		case GL_PROGRAM_BINARY_FORMATS_OES:
			for( i = 0; i < ( GLint )GLES2_NUM_PROGRAM_BINARY_FORMATS; i++ )
			{
				_gles_convert_from_enum( params, i, GL_MALI_PROGRAM_BINARY_ARM, type );
			}
		break;
		case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
			_gles_convert_from_int( params, 0, ( GLint )GLES2_NUM_PROGRAM_BINARY_FORMATS, type );
		break;
#if EXTENSION_ROBUSTNESS_EXT_ENABLE
		case GL_CONTEXT_ROBUST_ACCESS_EXT:
			_gles_convert_from_boolean( params, 0, ctx->robust_strategy ? GL_TRUE : GL_FALSE, type );
		break;
		case GL_RESET_NOTIFICATION_STRATEGY_EXT:
			_gles_convert_from_int( params, 0, (GLint)ctx->robust_strategy, type );
		break;
#endif
		default:
			return GL_INVALID_ENUM;
	}

	return GL_NO_ERROR;
}

GLenum _gles2_get_tex_param( struct gles_common_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type )
{
	enum gles_texture_target gles_target;
	gles_texture_unit *tex_unit;
	gles_texture_object *tex;
	MALI_DEBUG_ASSERT_POINTER( state );

	gles_target = _gles_get_dimensionality(target, GLES_API_VERSION_2);

	/** Check if target is valid */
	if ( GLES_TEXTURE_TARGET_INVALID == gles_target )
	{
		return GL_INVALID_ENUM;
	}

	/** Get the texture-object from the context */
	tex_unit = & state->texture_env.unit[state->texture_env.active_texture];
	tex = tex_unit->current_texture_object[gles_target];

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
#if EXTENSION_TEXTURE_3D_OES_ENABLE
		case GL_TEXTURE_WRAP_R_OES:
			_gles_convert_from_enum( params, 0, tex->wrap_r, type );
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
			return GL_INVALID_ENUM;
	}
	return GL_NO_ERROR;
}

GLenum _gles2_is_enabled( gles_context *ctx, GLenum cap, GLboolean *enabled )
{
	gles_common_state  *state_common;
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( enabled );
	state_common = &(ctx->state.common);

	switch( cap )
	{
		/** Coloring state */
		case GL_BLEND:
			*enabled = (GLboolean)_gles_fb_get_blend( ctx );
		break;

		/** Rasterization state */
		case GL_CULL_FACE:
			*enabled = state_common->rasterization.cull_face;
		break;
		case GL_POLYGON_OFFSET_FILL:
			*enabled = (GLboolean)_gles_fb_get_polygon_offset_fill( ctx );
		break;

		/** Multisampling state */
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
			*enabled = (GLboolean)_gles_fb_get_sample_alpha_to_coverage( ctx );
		break;
		case GL_SAMPLE_COVERAGE:
			*enabled = (GLboolean)_gles_fb_get_sample_coverage( ctx );
		break;

		/** Pixel operations */
		case GL_SCISSOR_TEST:
			*enabled = (GLboolean)mali_statebit_get( state_common, GLES_STATE_SCISSOR_ENABLED );
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

		default:
			*enabled = GL_FALSE;
			return GL_INVALID_ENUM;
	}

	return GL_NO_ERROR;
}

GLenum _gles2_get_buffer_parameter( struct gles_common_state *state, GLenum target, GLenum pname, GLint *params )
{
	gles_buffer_object *vbo = NULL;

	MALI_DEBUG_ASSERT_POINTER( state );

	switch (target)
	{
		case GL_ARRAY_BUFFER:         vbo = state->vertex_array.vbo; break;
		case GL_ELEMENT_ARRAY_BUFFER: vbo = state->vertex_array.element_vbo; break;
		default: return GL_INVALID_ENUM;
	}

	/* Return invalid operation if zero is bound */
	if (NULL == vbo) return GL_INVALID_OPERATION;

	switch( pname )
	{
		case GL_BUFFER_SIZE:
			if (NULL != params) *params = ( GLint )vbo->size;
		break;
		case GL_BUFFER_USAGE:
			if (NULL != params) *params = ( GLint )vbo->usage;
		break;
		default:
			return GL_INVALID_ENUM;
	}

	return GL_NO_ERROR;
}


GLenum _gles2_get_string( gles_context *ctx, GLenum name, const GLubyte **return_string )
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
	/* MALI_GLES_2_MINOR_VERSION is defined by the build system */
	static const GLubyte version[] = "OpenGL ES " GLES_MAKE_VERSION_STRING(MALI_GLES_2_MAJOR_VERSION, MALI_GLES_2_MINOR_VERSION);
	static const GLubyte shader_version[] = "OpenGL ES GLSL ES 1.00";
	static const GLubyte extensions[] =
		"GL_OES_texture_npot "
#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
		"GL_OES_compressed_ETC1_RGB8_texture "
#endif
#if EXTENSION_FRAGMENT_SHADER_DERIVATIVE_HINT_OES_ENABLE
		"GL_OES_standard_derivatives "
#endif
#if EXTENSION_EGL_IMAGE_OES_ENABLE
		"GL_OES_EGL_image "
#endif
#if EXTENSION_DEPTH24_OES_ENABLE
		"GL_OES_depth24 "
#endif
#if EXTENSION_ARM_RGBA8_ENABLE
		"GL_ARM_rgba8 "
#endif
#if EXTENSION_ARM_MALI_SHADER_BINARY_ENABLE
		"GL_ARM_mali_shader_binary "
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
#if EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE
		"GL_OES_texture_half_float "
#endif
#if EXTENSION_TEXTURE_HALF_FLOAT_LINEAR_OES_ENABLE
		"GL_OES_texture_half_float_linear "
#endif
#if EXTENSION_BLEND_MINMAX_EXT_ENABLE
		"GL_EXT_blend_minmax "
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
#if EXTENSION_DISCARD_FRAMEBUFFER
		"GL_EXT_discard_framebuffer "
#endif
#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
		"GL_OES_get_program_binary "
		"GL_ARM_mali_program_binary "
#endif
#if EXTENSION_EXT_SHADER_TEXTURE_LOD
		"GL_EXT_shader_texture_lod "
#endif
#if EXTENSION_ROBUSTNESS_EXT_ENABLE
		"GL_EXT_robustness "
#endif
		;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	switch (name)
	{
		case GL_VENDOR:
			*return_string = (GLubyte *)&vendor;
			break;
		case GL_VERSION:
			*return_string = (GLubyte *)&version;
			break;
		case GL_RENDERER:
			*return_string = ctx->renderer;
			break;
		case GL_EXTENSIONS:
			*return_string = (GLubyte *)&extensions;
			break;
		case GL_SHADING_LANGUAGE_VERSION:
			*return_string = (GLubyte *)&shader_version;
			break;
		default:
			*return_string = NULL;
			return ( GL_INVALID_ENUM );
	}
	return GL_NO_ERROR;
}

GLenum _gles2_get_shader_precision_format(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision)
{
	/* All numbers provided by compiler group */
	switch( shadertype )
	{
		case GL_VERTEX_SHADER:
			switch( precisiontype )
			{
				case GL_LOW_FLOAT:
				case GL_MEDIUM_FLOAT:
				case GL_HIGH_FLOAT:
					if (NULL != precision) *precision = 23;
					if (NULL != range)
					{
						range[0] = 127;
						range[1] = 127;
					}
					break;
				case GL_LOW_INT:
				case GL_MEDIUM_INT:
				case GL_HIGH_INT:
					if (NULL != precision) *precision = 0;
					if (NULL != range)
					{
						range[0] = 24;
						range[1] = 24;
					}
					break;
				default:
					return GL_INVALID_ENUM;
			}
			break;
		case GL_FRAGMENT_SHADER:
			switch( precisiontype )
			{
				case GL_LOW_FLOAT:
				case GL_MEDIUM_FLOAT:
					if (NULL != precision) *precision = 10;
					if (NULL != range)
					{
						range[0] = 15;
						range[1] = 15;
					}
					break;
				case GL_LOW_INT:
				case GL_MEDIUM_INT:
					if (NULL != precision) *precision = 0;
					if (NULL != range)
					{
						range[0] = 11;
						range[1] = 11;
					}
					break;
				case GL_HIGH_FLOAT:
				case GL_HIGH_INT:
					/* highp not supported in the fragment shader */
					if (NULL != precision) *precision = 0;
					if (NULL != range)
					{
						range[0] = 0;
						range[1] = 0;
					}
					break;
				default:
					return GL_INVALID_ENUM;
			}
			break;

		default:
			return GL_INVALID_ENUM;
	}
	return GL_NO_ERROR;
}

GLenum _gles2_get_vertex_attrib(struct gles_vertex_array *vertex_array, GLuint index, GLenum pname, gles_datatype type, void* param)
{
	MALI_DEBUG_ASSERT_POINTER(vertex_array);

	/* verify user-input */
	if (index >= GLES_VERTEX_ATTRIB_COUNT) return GL_INVALID_VALUE;

	switch(pname)
	{
		case GL_CURRENT_VERTEX_ATTRIB:
			_gles_convert_from_ftype( param, 0, vertex_array->attrib_array[index].value[0], type );
			_gles_convert_from_ftype( param, 1, vertex_array->attrib_array[index].value[1], type );
			_gles_convert_from_ftype( param, 2, vertex_array->attrib_array[index].value[2], type );
			_gles_convert_from_ftype( param, 3, vertex_array->attrib_array[index].value[3], type );
			break;
		case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
			_gles_convert_from_boolean( param, 0, vertex_array->attrib_array[index].enabled, type );
			break;
		case GL_VERTEX_ATTRIB_ARRAY_SIZE:
			_gles_convert_from_int( param, 0, vertex_array->attrib_array[index].size, type );
			break;
		case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
			_gles_convert_from_int( param, 0, vertex_array->attrib_array[index].given_stride, type );
			break;
		case GL_VERTEX_ATTRIB_ARRAY_TYPE:
			_gles_convert_from_enum( param, 0, vertex_array->attrib_array[index].type, type );
			break;
		case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
			_gles_convert_from_boolean( param, 0, vertex_array->attrib_array[index].normalized, type );
			break;
		case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
			_gles_convert_from_int( param, 0, ( GLint )vertex_array->attrib_array[index].buffer_binding, type );
			break;
		default:
			return GL_INVALID_ENUM;
	}

	return GL_NO_ERROR;
}

GLenum _gles2_get_vertex_attrib_pointer(struct gles_vertex_array *vertex_array, GLuint index, GLenum pname, void** param)
{
	MALI_DEBUG_ASSERT_POINTER(vertex_array);

	/* check for errors */
	if (index >= GLES_VERTEX_ATTRIB_COUNT) return GL_INVALID_VALUE;
	if (GL_VERTEX_ATTRIB_ARRAY_POINTER != pname) return GL_INVALID_ENUM;

	/* fetch pointer */
	if (NULL != param) *param = (void*) vertex_array->attrib_array[index].pointer;

	return GL_NO_ERROR;
}
