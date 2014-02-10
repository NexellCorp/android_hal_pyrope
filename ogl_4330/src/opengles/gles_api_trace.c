/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_api_trace.c
 * @brief Helper functions common for both OpenGL ES 1.1 and 2.0 API tracing
 */

#include "gles_api_trace.h"
#include "gles_common_state/gles_vertex_array.h"
#include "gles_buffer_object.h"
#include "mali_gp_geometry_common/gles_gb_buffer_object.h"
#include "mali_gp_geometry_common/gles_geometry_backend_context.h"
#include "gles_vtable.h"

u32 mali_api_trace_gl_texparameter_count(GLenum pname)
{
	u32 count = 0;

	switch(pname)
	{
	case GL_TEXTURE_MIN_FILTER:
	case GL_TEXTURE_MAG_FILTER:
	case GL_TEXTURE_WRAP_S:
	case GL_TEXTURE_WRAP_T:
#if MALI_USE_GLES_1
	case GL_GENERATE_MIPMAP:
#endif
#if MALI_USE_GLES_2
#if EXTENSION_TEXTURE_3D_OES_ENABLE
	case GL_TEXTURE_WRAP_R_OES:
#endif
#endif
		count = 1;
		break;
#if MALI_USE_GLES_1
#if EXTENSION_DRAW_TEX_OES_ENABLE
	case GL_TEXTURE_CROP_RECT_OES:
		count = 4;
		break;
#endif
#endif
	default:
		count = 0;
		break;
	}

	return count;
}

/**
 * Find the higest index in the specified index buffer
 * @param indices Buffer with indices
 * @param type The format of indices buffer
 * @param count Number of indices in buffer
 * @return The highest index
 */
static u32 find_highest_index(const void* indices, GLenum type, int count)
{
	u32 highest_index = 0;
	int i;

	if (type == GL_UNSIGNED_BYTE)
	{
		const u8* ptr = (const u8*)indices;
		for (i = 0; i < count; i++)
		{
			if (ptr[i] > highest_index)
			{
				highest_index = ptr[i];
			}
		}
	}
	else if (type == GL_UNSIGNED_SHORT)
	{
		const u16* ptr = (const u16*)indices;
		for (i = 0; i < count; i++)
		{
			if (ptr[i] > highest_index)
			{
				highest_index = ptr[i];
			}
		}
	}
#if MALI_USE_GLES_2
	else if (type == GL_UNSIGNED_INT)
	{
		const u32* ptr = (const u32*)indices;
		for (i = 0; i < count; i++)
		{
			if (ptr[i] > highest_index)
			{
				highest_index = ptr[i];
			}
		}
	}
#endif
	else
	{
		return 0;
	}

	return highest_index;
}

void mali_api_trace_gl_draw_arrays(struct gles_context *ctx, u32 nvert)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		/*
		 * Now we need to copy all the vertex attrib array
		 * which is enabled and not connected to a VBO.
		 * VBO data is already grabbed in glBufferData() and/or glBufferSubData().
		 */
		char *buffers[GLES_VERTEX_ATTRIB_COUNT];
		u32 sizes[GLES_VERTEX_ATTRIB_COUNT];
		u32 count = 0;
		u32 i;

		_gles_gb_get_non_vbo_buffers(ctx, nvert, buffers, sizes, &count);

		for (i = 0; i < count; i++)
		{
			mali_api_trace_dump_user_buffer(buffers[i], sizes[i]);
		}
	}
}

void mali_api_trace_gl_draw_elements(struct gles_context *ctx, GLenum mode, int count, GLenum type, const void *indices)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		unsigned int element_size;
		struct gles_buffer_object *vbo;

		if (count <= 0)
		{
			return; /* no point in copying buffers */
		}

		switch (type)
		{
		case GL_UNSIGNED_BYTE:
			element_size = 1;
			break;
		case GL_UNSIGNED_SHORT:
			element_size = 2;
			break;
#if MALI_USE_GLES_2
		case GL_UNSIGNED_INT:
			element_size = 4;
			break;
#endif
		default:
			return;
		}

		/*
		 * First we need to find the highest index in the indices buffer.
		 * This will tell us how much attribute data we need to copy
		 */

		vbo = ctx->state.common.vertex_array.element_vbo;
		if (vbo != NULL)
		{
			struct gles_gb_buffer_object_data *gb_vbo = vbo->gb_data;
			if (NULL != gb_vbo) /* check if data is actually uploaded to the buffer object */
			{
				if (element_size * (unsigned int)count <= vbo->size) /* verify that there is enough data in buffer object */
				{
					index_range idx_range[MALI_LARGEST_INDEX_RANGE];
					index_range * idx_range_first = &idx_range[0];
					u32 range_count = 1;
                    u32 coherence;
					/* (We do not need to dump these indices, since it's in a VBO) */

					/* Find the max index */
					_gles_gb_buffer_object_data_range_cache_get(gb_vbo, mode, (int)indices, count, type, &idx_range_first, &coherence, &range_count);

					/* now, do the same as we do for glDrawArrays (dump non-VBO attrib array buffers) */
					mali_api_trace_gl_draw_arrays(ctx, idx_range[range_count-1].max + 1);
				}
			}
		}
		else
		{
			if (NULL != indices)
			{
				u32 max_idx = 0;

				/* We need to dump this buffer, since it isn't a VBO */
				mali_api_trace_dump_user_buffer(indices, count * element_size);

				/* Find the highest index */
				max_idx = find_highest_index(indices, type, count);

				/* now, do the same as we do for glDrawArrays (dump non-VBO attrib array buffers) */
				mali_api_trace_gl_draw_arrays(ctx, max_idx + 1);
			}
		}
	}
}

u32 mali_api_trace_get_getter_out_count(gles_context *ctx, GLenum pname)
{
	u32 count = 0;

	switch(pname)
	{
	case GL_ACTIVE_TEXTURE:
	case GL_ALPHA_BITS:
	case GL_ARRAY_BUFFER_BINDING:
	case GL_BLEND:
	case GL_BLUE_BITS:
	case GL_CULL_FACE:
	case GL_CULL_FACE_MODE:
	case GL_DEPTH_BITS:
	case GL_DEPTH_CLEAR_VALUE:
	case GL_DEPTH_FUNC:
	case GL_DEPTH_TEST:
	case GL_DEPTH_WRITEMASK:
	case GL_DITHER:
	case GL_ELEMENT_ARRAY_BUFFER_BINDING:
	case GL_FRONT_FACE:
	case GL_GENERATE_MIPMAP_HINT:
	case GL_GREEN_BITS:
	case GL_LINE_WIDTH:
	case GL_MAX_TEXTURE_SIZE:
	case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
	case GL_PACK_ALIGNMENT:
	case GL_POLYGON_OFFSET_FACTOR:
	case GL_POLYGON_OFFSET_FILL:
	case GL_POLYGON_OFFSET_UNITS:
	case GL_RED_BITS:
	case GL_SAMPLE_BUFFERS:
	case GL_SAMPLE_COVERAGE_INVERT:
	case GL_SAMPLE_COVERAGE_VALUE:
	case GL_SAMPLES:
	case GL_SCISSOR_TEST:
	case GL_STENCIL_BITS:
	case GL_STENCIL_CLEAR_VALUE:
	case GL_STENCIL_FAIL:
	case GL_STENCIL_FUNC:
	case GL_STENCIL_PASS_DEPTH_FAIL:
	case GL_STENCIL_PASS_DEPTH_PASS:
	case GL_STENCIL_REF:
	case GL_STENCIL_TEST:
	case GL_STENCIL_VALUE_MASK:
	case GL_STENCIL_WRITEMASK:
	case GL_SUBPIXEL_BITS:
	case GL_TEXTURE_BINDING_2D:
	case GL_UNPACK_ALIGNMENT:
#if MALI_USE_GLES1
	case GL_ALPHA_TEST_FUNC:
	case GL_ALPHA_TEST_REF:
	case GL_BLEND_DST:
	case GL_BLEND_SRC:
	case GL_CLIENT_ACTIVE_TEXTURE:
	case GL_COLOR_ARRAY_BUFFER_BINDING:
	case GL_COLOR_ARRAY_SIZE:
	case GL_COLOR_ARRAY_STRIDE:
	case GL_COLOR_ARRAY_TYPE:
	case GL_FOG_DENSITY:
	case GL_FOG_END:
	case GL_FOG_HINT:
	case GL_FOG_MODE:
	case GL_FOG_START:
#if !MALI_USE_GLES2
	/* These two have a different name in GLES2 (no _OES at the end), so use those instead */
	case GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES:
	case GL_IMPLEMENTATION_COLOR_READ_TYPE_OES:
#endif
	case GL_LIGHT_MODEL_TWO_SIDE:
	case GL_LINE_SMOOTH_HINT:
	case GL_LOGIC_OP_MODE:
	case GL_MATRIX_INDEX_ARRAY_BUFFER_BINDING_OES:
	case GL_MATRIX_INDEX_ARRAY_SIZE_OES:
	case GL_MATRIX_INDEX_ARRAY_STRIDE_OES:
	case GL_MATRIX_INDEX_ARRAY_TYPE_OES:
	case GL_MATRIX_MODE:
	case GL_MAX_CLIP_PLANES:
	case GL_MAX_LIGHTS:
	case GL_MAX_MODELVIEW_STACK_DEPTH:
	case GL_MAX_PALETTE_MATRICES_OES:
	case GL_MAX_PROJECTION_STACK_DEPTH:
	case GL_MAX_TEXTURE_STACK_DEPTH:
	case GL_MAX_TEXTURE_UNITS:
	case GL_MAX_VERTEX_UNITS_OES:
	case GL_MODELVIEW_STACK_DEPTH:
	case GL_NORMAL_ARRAY_BUFFER_BINDING:
	case GL_NORMAL_ARRAY_STRIDE:
	case GL_NORMAL_ARRAY_TYPE:
	case GL_PERSPECTIVE_CORRECTION_HINT:
	case GL_POINT_SIZE:
	case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
	case GL_POINT_SIZE_ARRAY_STRIDE_OES:
	case GL_POINT_SIZE_ARRAY_TYPE_OES:
	case GL_POINT_SMOOTH_HINT:
	case GL_PROJECTION_STACK_DEPTH:
	case GL_SHADE_MODEL:
	case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
	case GL_TEXTURE_COORD_ARRAY_SIZE:
	case GL_TEXTURE_COORD_ARRAY_STRIDE:
	case GL_TEXTURE_COORD_ARRAY_TYPE:
	case GL_TEXTURE_STACK_DEPTH:
	case GL_VERTEX_ARRAY_BUFFER_BINDING:
	case GL_VERTEX_ARRAY_SIZE:
	case GL_VERTEX_ARRAY_STRIDE:
	case GL_VERTEX_ARRAY_TYPE:
	case GL_WEIGHT_ARRAY_BUFFER_BINDING_OES:
	case GL_WEIGHT_ARRAY_SIZE_OES:
	case GL_WEIGHT_ARRAY_STRIDE_OES:
	case GL_WEIGHT_ARRAY_TYPE_OES:
#endif
#if MALI_USE_GLES_2
	case GL_BLEND_DST_ALPHA:
	case GL_BLEND_DST_RGB:
	case GL_BLEND_EQUATION_ALPHA:
	case GL_BLEND_EQUATION_RGB:
	case GL_BLEND_SRC_ALPHA:
	case GL_BLEND_SRC_RGB:
	case GL_CURRENT_PROGRAM:
	case GL_FRAMEBUFFER_BINDING:
	case GL_IMPLEMENTATION_COLOR_READ_FORMAT: /* same as GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES */
	case GL_IMPLEMENTATION_COLOR_READ_TYPE: /* same as GL_IMPLEMENTATION_COLOR_READ_TYPE_OES */
	case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
	case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
	case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
	case GL_MAX_RENDERBUFFER_SIZE:
	case GL_MAX_TEXTURE_IMAGE_UNITS:
	case GL_MAX_VARYING_VECTORS:
	case GL_MAX_VERTEX_ATTRIBS:
	case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
	case GL_MAX_VERTEX_UNIFORM_VECTORS:
	case GL_NUM_SHADER_BINARY_FORMATS:
	case GL_RENDERBUFFER_BINDING:
	case GL_SHADER_COMPILER:
	case GL_STENCIL_BACK_FAIL:
	case GL_STENCIL_BACK_FUNC:
	case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
	case GL_STENCIL_BACK_PASS_DEPTH_PASS:
	case GL_STENCIL_BACK_REF:
	case GL_STENCIL_BACK_VALUE_MASK:
	case GL_STENCIL_BACK_WRITEMASK:
	case GL_TEXTURE_BINDING_CUBE_MAP:
#endif
		count = 1;
		break;
	case GL_ALIASED_LINE_WIDTH_RANGE:
	case GL_ALIASED_POINT_SIZE_RANGE:
	case GL_DEPTH_RANGE:
	case GL_MAX_VIEWPORT_DIMS:
#if MALI_USE_GLES1
	case GL_SMOOTH_LINE_WIDTH_RANGE:
	case GL_SMOOTH_POINT_SIZE_RANGE:
#endif
		count = 2;
		break;
#if MALI_USE_GLES1
	case GL_CURRENT_NORMAL:
		count = 3;
		break;
#endif
	case GL_COLOR_CLEAR_VALUE:
	case GL_COLOR_WRITEMASK:
	case GL_SCISSOR_BOX:
	case GL_VIEWPORT:
#if MALI_USE_GLES1
	case GL_CURRENT_COLOR:
	case GL_CURRENT_TEXTURE_COORDS:
	case GL_FOG_COLOR:
	case GL_LIGHT_MODEL_AMBIENT:
#endif
#if MALI_USE_GLES_2
	case GL_BLEND_COLOR:
#endif
		count = 4;
		break;
	case GL_COMPRESSED_TEXTURE_FORMATS:
		{
			GLint param = 0;
			ctx->vtable->fp_glGetIntegerv(ctx, GL_NUM_COMPRESSED_TEXTURE_FORMATS, &param, GLES_INT);
			count = (u32)param;
		}
		break;
#if MALI_USE_GLES_2
	case GL_SHADER_BINARY_FORMATS:
		{
			GLint param = 0;
			ctx->vtable->fp_glGetIntegerv(ctx, GL_NUM_SHADER_BINARY_FORMATS, &param, GLES_INT);
			count = (u32)param;
		}
		break;
#endif
#if MALI_USE_GLES1
	case GL_MODELVIEW_MATRIX:
	case GL_PROJECTION_MATRIX:
	case GL_TEXTURE_MATRIX:
		count = 16;
		break;
#endif
	default:
		count = 0;
		break;
	}

	return count;
}

u32 mali_api_trace_gl_read_pixels_size(struct gles_context *ctx, int width, int height, GLenum format, GLenum type)
{
	u32 bpp = 0;
	u32 pack_alignment = ctx->state.common.pixel.pack_alignment;
	u32 line_length = 0;
	u32 size_per_line = 0;

	if (width < 0 || height < 0)
	{
		return 0;
	}

	if (format == GL_RGBA)
	{
		if (type == GL_UNSIGNED_BYTE)
		{
			bpp = 4;
		}
		else if (type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1)
		{
			bpp = 2;
		}
	}
	else if (format == GL_RGB)
	{
		if (type == GL_UNSIGNED_BYTE)
		{
			bpp = 3;
		}
		else if (type == GL_UNSIGNED_SHORT_5_6_5)
		{
			bpp = 2;
		}
	}
	else if (format == GL_ALPHA)
	{
		if (type == GL_UNSIGNED_BYTE)
		{
			return bpp = 1;
		}
	}

	if (bpp == 0)
	{
		return 0;
	}

	line_length = width * bpp;
	size_per_line = line_length + ((pack_alignment - 1) & ~(line_length - 1));

	return (size_per_line * height);
}

u32 mali_api_trace_gl_teximage2d_size(struct gles_context *ctx, int width, int height, GLenum format, GLenum type)
{
	u32 bpp = 0;
	u32 unpack_alignment = ctx->state.common.pixel.unpack_alignment;
	u32 line_length = 0;
	u32 size_per_line = 0;

	if (width < 0 || height < 0)
	{
		return 0;
	}

	if (format == GL_RGBA)
	{
		if (type == GL_UNSIGNED_BYTE)
		{
			bpp = 4;
		}
		else if (type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1)
		{
			bpp = 2;
		}
		else if (type == GL_UNSIGNED_SHORT)
		{
			bpp = 8;
		}
	}
	else if (format == GL_RGB)
	{
		if (type == GL_UNSIGNED_BYTE)
		{
			bpp = 3;
		}
		else if (type == GL_UNSIGNED_SHORT_5_6_5)
		{
			bpp = 2;
		}
	}
	else if (format == GL_ALPHA || format == GL_LUMINANCE)
	{
		if (type == GL_UNSIGNED_BYTE)
		{
			bpp = 1;
		}
	}
	else if (format == GL_LUMINANCE_ALPHA)
	{
		if (type == GL_UNSIGNED_BYTE)
		{
			bpp = 2;
		}
		else if (type == GL_UNSIGNED_SHORT)
		{
			bpp = 4;
		}
	}

	if (bpp == 0)
	{
		return 0;
	}

	line_length = width * bpp;
	size_per_line = line_length + ((unpack_alignment - 1) & ~(line_length - 1));

	return (size_per_line * height);
}
