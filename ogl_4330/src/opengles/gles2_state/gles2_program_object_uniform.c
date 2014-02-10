/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_context.h"
#include "gles_context.h"
#include "gles2_program_object.h"
#include "gles_state.h"
#include "gles_state.h"

mali_err_code _gles2_create_fp16_fs_uniform_cache(struct gles_program_rendering_state* render_state)
{
	int num_uniforms;

	MALI_DEBUG_ASSERT_POINTER(render_state);
	num_uniforms = render_state->binary.fragment_shader_uniforms.cellsize;

	if ( num_uniforms > 0 )
	{
		render_state->fp16_cached_fs_uniforms = _mali_sys_malloc( num_uniforms * sizeof(gles_fp16) );
		if ( NULL == render_state->fp16_cached_fs_uniforms )
		{
			return MALI_ERR_OUT_OF_MEMORY;
		}
		_mali_sys_memset( render_state->fp16_cached_fs_uniforms, 0x0, num_uniforms * sizeof( gles_fp16 ) );
	}

	return MALI_ERR_NO_ERROR;
}

mali_err_code _gles2_fill_fp16_fs_uniform_cache(struct gles_program_rendering_state* render_state)
{
	u32 i;
	gles_fp16* fp16_uniforms;
	float* uniforms;
	u32 num_uniforms;

	MALI_DEBUG_ASSERT_POINTER(render_state);
	num_uniforms = render_state->binary.fragment_shader_uniforms.cellsize;

	if ( num_uniforms > 0 )
	{
		MALI_DEBUG_ASSERT_POINTER(render_state->fp16_cached_fs_uniforms);
		fp16_uniforms = render_state->fp16_cached_fs_uniforms;
		uniforms = render_state->binary.fragment_shader_uniforms.array;

		for(i = 0; i < num_uniforms; i++)
		{
			fp16_uniforms[i] = _gles_fp32_to_fp16(uniforms[i]);
		}
	}

	return MALI_ERR_NO_ERROR;
}

mali_err_code _gles2_create_gl_uniform_location_table(gles2_program_object* po)
{
	u32 i;
	gles_program_rendering_state* rs;

	MALI_DEBUG_ASSERT_POINTER(po->render_state);
	rs = po->render_state;

	MALI_DEBUG_ASSERT(rs->binary.uniform_symbols, ("Uniform Symbol table loaded"));
	MALI_DEBUG_ASSERT(rs->binary.samplers.info || rs->binary.samplers.num_samplers == 0,
	                  ("Sampler info table must be sized 0 (is %i), or be allocated", rs->binary.samplers.num_samplers));

	/* Get amount of locations, create locations array */
	rs->gl_uniform_location_size = bs_symbol_count_locations( rs->binary.uniform_symbols, _gles_active_filters, GLES_ACTIVE_FILTER_SIZE );
	if ( rs->gl_uniform_location_size > 0 )
	{
		/* We need an array with one empty space on the end */
		rs->gl_uniform_locations = _mali_sys_malloc( (rs->gl_uniform_location_size) * sizeof(bs_uniform_location) );

		if ( NULL == rs->gl_uniform_locations )
		{
			rs->gl_uniform_location_size = 0;
			return MALI_ERR_OUT_OF_MEMORY;
		}

		i = bs_symbol_fill_location_table( rs->binary.uniform_symbols, rs->gl_uniform_location_size, rs->gl_uniform_locations, _gles_active_filters, GLES_ACTIVE_FILTER_SIZE );

		MALI_DEBUG_ASSERT(i == rs->gl_uniform_location_size, ("Uniform location table is not filled correctly!"));
		MALI_IGNORE(i);
	}

	return MALI_ERR_NO_ERROR;
}

/**
 * @brief setup magic uniform locations
 * @note A magical uniform is a uniform which have a special mening in gles.
 * @param po The program object in which magic uniforms should be handled.
 */
void _gles2_setup_magic_uniforms(struct gles2_program_object* po)
{
	struct bs_symbol* symbol;
	gles_program_rendering_state* rs;

	MALI_DEBUG_ASSERT_POINTER(po->render_state);
	rs = po->render_state;

	/* Viewport transform */
	symbol = bs_symbol_lookup(rs->binary.uniform_symbols, "gl_mali_ViewportTransform", &rs->viewport_uniform_vs_location, NULL, NULL);
	if(!symbol || symbol->datatype != DATATYPE_FLOAT || symbol->type.primary.vector_size != 4 || symbol->array_size != 2)
	{
		rs->viewport_uniform_vs_location = -1;
	}

	/* Point size clamping, and scaling for use when AA is enabled */
	symbol = bs_symbol_lookup(rs->binary.uniform_symbols, "gl_mali_PointSizeParameters", &rs->pointsize_parameters_uniform_vs_location, NULL, NULL);
	if(!symbol || symbol->datatype != DATATYPE_FLOAT || symbol->type.primary.vector_size != 4 || symbol->array_size != 0)
	{
		rs->pointsize_parameters_uniform_vs_location = -1;
	}

	/* Point coord flipping, for use with point sprite rendering to textures */
	symbol = bs_symbol_lookup(rs->binary.uniform_symbols, "gl_mali_PointCoordScaleBias", NULL, &rs->pointcoordscalebias_uniform_fs_location, NULL);
	if(!symbol || symbol->datatype != DATATYPE_FLOAT || symbol->type.primary.vector_size != 4 || symbol->array_size != 0)
	{
		rs->pointcoordscalebias_uniform_fs_location = -1;
	}

	/* Flipping and scaling of the derivative fragment shader functions dFdx, dFdy and fwidth */
	symbol = bs_symbol_lookup(rs->binary.uniform_symbols, "gl_mali_DerivativeScale", NULL, &rs->derivativescale_uniform_fs_location, NULL);
	if(!symbol || symbol->datatype != DATATYPE_FLOAT || symbol->type.primary.vector_size != 2 || symbol->array_size != 0)
	{
		rs->derivativescale_uniform_fs_location = -1;
	}

	/* Depth range struct */
	bs_symbol_lookup(rs->binary.uniform_symbols, "gl_DepthRange.near", &rs->depthrange_uniform_vs_location_near,
	                 &rs->depthrange_uniform_fs_location_near,
	                 NULL);
	bs_symbol_lookup(rs->binary.uniform_symbols, "gl_DepthRange.far", &rs->depthrange_uniform_vs_location_far,
	                 &rs->depthrange_uniform_fs_location_far,
	                 NULL);
	bs_symbol_lookup(rs->binary.uniform_symbols, "gl_DepthRange.diff", &rs->depthrange_uniform_vs_location_diff,
	                 &rs->depthrange_uniform_fs_location_diff,
	                 NULL);

	/* fragcoordscale locations */
	symbol = bs_symbol_lookup(rs->binary.uniform_symbols, "gl_mali_FragCoordScale", NULL, &rs->fragcoordscale_uniform_fs_location, NULL);
	if(!symbol || symbol->datatype != DATATYPE_FLOAT || symbol->type.primary.vector_size != 3 || symbol->array_size != 0)
	{
		rs->fragcoordscale_uniform_fs_location = -1;
	}

	/* update overall state flags */
	if ( (rs->depthrange_uniform_fs_location_near != -1) ||
	     (rs->depthrange_uniform_fs_location_far  != -1) ||
	     (rs->depthrange_uniform_fs_location_diff != -1))
	{
		rs->depthrange_locations_fs_in_use = MALI_TRUE;
	} else
	{
		rs->depthrange_locations_fs_in_use = MALI_FALSE;
	}

	if ( (rs->pointcoordscalebias_uniform_fs_location != -1) ||
	     (rs->derivativescale_uniform_fs_location     != -1) ||
	     (rs->fragcoordscale_uniform_fs_location      != -1))
	{
		rs->flip_scale_bias_locations_fs_in_use = MALI_TRUE;
	} else
	{
		rs->flip_scale_bias_locations_fs_in_use = MALI_FALSE;
	}
}



/* converts a binary shader datatype to a gltype - used by both glGetActive* functions */
GLenum _gles2_convert_datatype_to_gltype(u32 type, u32 size)
{
	switch(type)
	{
	case DATATYPE_FLOAT:
		switch(size)
		{
		case 1: return GL_FLOAT;
		case 2: return GL_FLOAT_VEC2;
		case 3: return GL_FLOAT_VEC3;
		case 4: return GL_FLOAT_VEC4;
		default: return GL_INVALID_ENUM;
		}
	case DATATYPE_INT:
		switch(size)
		{
		case 1: return GL_INT;
		case 2: return GL_INT_VEC2;
		case 3: return GL_INT_VEC3;
		case 4: return GL_INT_VEC4;
		default: return GL_INVALID_ENUM;
		}
	case DATATYPE_BOOL:
		switch(size)
		{
		case 1: return GL_BOOL;
		case 2: return GL_BOOL_VEC2;
		case 3: return GL_BOOL_VEC3;
		case 4: return GL_BOOL_VEC4;
		default: return GL_INVALID_ENUM;
		}
	case DATATYPE_MATRIX:
		switch(size)
		{
		case 2: return GL_FLOAT_MAT2;
		case 3: return GL_FLOAT_MAT3;
		case 4: return GL_FLOAT_MAT4;
		default: return GL_INVALID_ENUM;
		}
	case DATATYPE_SAMPLER:
		switch(size)
		{
		case 2: return GL_SAMPLER_2D;
#if EXTENSION_TEXTURE_3D_OES_ENABLE
		case 3: return GL_SAMPLER_3D_OES;
#endif
		default: return GL_INVALID_ENUM;
		}
	case DATATYPE_SAMPLER_CUBE:
		return GL_SAMPLER_CUBE;
	case DATATYPE_SAMPLER_SHADOW:
	  /* this is not supported. */
		return GL_INVALID_ENUM;
	case DATATYPE_SAMPLER_EXTERNAL:
		return GL_SAMPLER_EXTERNAL_OES;
	case DATATYPE_STRUCT:
	default:
		return GL_INVALID_ENUM;
	}
}


/* Uniform part of gles2_program_object.c
 * In this file, you will find everything related to uniforms */

GLenum _gles2_get_active_uniform(mali_named_list* program_object_list, GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint *size, GLenum* type, char* name)
{
	gles2_program_object *po;
	GLenum object_type;
	gles_program_rendering_state* rs;

	/* assert bufsize positive */
	if(bufsize < 0) return GL_INVALID_VALUE;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);

	/* assert that name is valid */
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that object is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	MALI_DEBUG_ASSERT_POINTER(po->render_state);
	rs = po->render_state;

	/* assert that the index isn't out of range (also, if linked failed so bad that we never
	 * made a uniform symbol table, or we never even linked,  all indices are illegal) */
	MALI_CHECK( rs->binary.uniform_symbols, GL_INVALID_VALUE );
	MALI_CHECK( bs_symbol_count_actives(rs->binary.uniform_symbols, _gles_active_filters, GLES_ACTIVE_FILTER_SIZE) > index, GL_INVALID_VALUE );

	/* lookup values */
	{
		struct bs_symbol* symbol = NULL;
		int ilength = 0;

		/* get the symbol! */
		symbol = bs_symbol_get_nth_active(rs->binary.uniform_symbols, index, name, bufsize,  _gles_active_filters, GLES_ACTIVE_FILTER_SIZE);

		MALI_DEBUG_ASSERT(symbol, ("At this point, the symbol should be legal"));

		/* write outputs */

		if (NULL != name && bufsize > 0)
		{
			ilength = _mali_sys_strlen(name);

			/* Bugzilla 4916 - array types must be appended with [0] */
			if(symbol->array_size > 0)
			{
				if((ilength+1) < bufsize) { name[ilength] = '['; ilength++; }
				if((ilength+1) < bufsize) { name[ilength] = '0'; ilength++; }
				if((ilength+1) < bufsize) { name[ilength] = ']'; ilength++; }
				if( ilength    < bufsize) { name[ilength] = '\0'; }
			}
		}

		if(NULL != length)
		{
			*length = ilength;
		}

		/* the rest of the output params */
		if(size)
		{
			*size = symbol->array_size;
			if(!symbol->array_size) (*size) = 1;
		}
		if(type) *type = _gles2_convert_datatype_to_gltype(symbol->datatype, symbol->type.primary.vector_size);
		if(length) *length = ilength;
		/* and, we're done. Cleanup */
	}

	return GL_NO_ERROR;
}

GLenum _gles2_get_uniform_location( mali_named_list* program_object_list, GLuint program, const char* name, GLint *retval )
{
	gles2_program_object *po;
	GLenum object_type;
	s32 index;
	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	if(retval) *retval = -1;

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);

	/* assert that name is valid */
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that object is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	/* assert program has been linked */
	MALI_CHECK( MALI_TRUE == po->render_state->binary.linked, GL_INVALID_OPERATION );

	index = bs_lookup_uniform_location(po->render_state->binary.uniform_symbols, name, _gles_active_filters, GLES_ACTIVE_FILTER_SIZE );

	/* not present? */
	MALI_CHECK( (index != -1) , GL_NO_ERROR );

	MALI_DEBUG_ASSERT((u32) index < po->render_state->gl_uniform_location_size,
	                  ("location was located by lookup, but not found in location list - THIS SHOULD NEVER HAPPEN!"));

	if(retval) *retval = index;

	return GL_NO_ERROR;
}

MALI_STATIC GLenum _gles_get_uniform_internal(mali_named_list* program_object_list, GLuint program, GLint location, void* output_array, GLenum output_type, GLsizei bufSize)
{
	gles2_program_object *po;
	GLenum object_type;
	struct bs_uniform_location* bs_loc;
	float* array;
	u32 x, y, height, width, vector_stride;
	s32 uniform_calculated_location = 0;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);

	/* assert that name is valid */
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that object is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	/* assert program has been linked */
	MALI_DEBUG_ASSERT_POINTER( po->render_state );
	MALI_CHECK( MALI_TRUE == po->render_state->binary.linked, GL_INVALID_OPERATION );

	/* location must be valid */
	MALI_CHECK( location >= 0, GL_INVALID_OPERATION );
	MALI_CHECK( (u32)location < po->render_state->gl_uniform_location_size, GL_INVALID_OPERATION );

	bs_loc = &(po->render_state->gl_uniform_locations[location]);

	/* if it's a sampler, retrieve the value from the sampler array instead of the uniform array */
	if( bs_symbol_is_sampler( bs_loc->symbol ) )
	{
		if(output_type == GLES_FLOAT)
		{
			*((float*)output_array) = (float)po->render_state->binary.samplers.info[bs_loc->extra.sampler_location].API_value;
		}
		if(output_type == GLES_INT)
		{
			*((s32*)output_array) = po->render_state->binary.samplers.info[bs_loc->extra.sampler_location].API_value;
		}
		return GL_NO_ERROR;
	}

	/* otherwise, retrieve from the vertex or fragment array, whichever is available */
	if( bs_loc->reg_location.vertex != -1 )
	{ /* get data from the vertex array */
		array = po->render_state->binary.vertex_shader_uniforms.array;
		vector_stride = bs_loc->symbol->type.primary.vector_stride.vertex;
		uniform_calculated_location = bs_loc->reg_location.vertex;
	} else {
		/* get data from the fragment array */
		array = po->render_state->binary.fragment_shader_uniforms.array;
		vector_stride = bs_loc->symbol->type.primary.vector_stride.fragment;
		uniform_calculated_location = bs_loc->reg_location.fragment;
	}
	width = bs_loc->symbol->type.primary.vector_size;
	height = 1; if(bs_loc->symbol->datatype == DATATYPE_MATRIX) height *= width;

	MALI_DEBUG_ASSERT(output_type == GLES_FLOAT || output_type == GLES_INT, ("Output type must be int or float"));

	/* if bufSize < 0, this is called by glGetUniform */
	if( bufSize >= 0 && (GLuint)bufSize < width * height)
	{
		return GL_INVALID_OPERATION;
	}	

	/* start copying data */
	/* we are ready to write to the uniform arrays */
	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			int internal_index =  (y * vector_stride) + x;
			int output_index =  (y * width) + x;
			if(output_type == GLES_FLOAT) ((float*)output_array)[output_index] = array[uniform_calculated_location + internal_index];
			if(output_type == GLES_INT) ((s32*)output_array)[output_index] = (s32) array[uniform_calculated_location + internal_index];
		}
	}

	/* voila */
	return GL_NO_ERROR;
}

GLenum _gles2_get_uniform( mali_named_list* program_object_list, GLuint program, GLint location, void* output_array, GLenum output_type )
{
	return _gles_get_uniform_internal( program_object_list, program, location, output_array, output_type, -1);
}

MALI_STATIC_INLINE float transform_to_float(const void* value, int index, gles_datatype datatype)
{
	switch(datatype)
	{
		/*case GL_INT:*/
	case GLES_INT:   return (float) ((s32*)value)[index];
	case GLES_FLOAT: return         ((float*)value)[index];

	default:
		MALI_DEBUG_ASSERT(0, ( "CRITICAL ERROR: gles2_uniform: error transforming uniform value to float32 - THIS SHOULD NEVER HAPPEN! type: 0x%04x\n", datatype) );
		break;
	}
	return 0;
}

MALI_STATIC void _gles_set_uniforms(
	float* MALI_RESTRICT dst, int vector_stride,
	const float* MALI_RESTRICT src, gles_fp16* MALI_RESTRICT cache,
	int input_width, int input_height,
	gles_datatype datatype, mali_bool is_bool)
{
	int y;
	int src_index = 0;
	for (y = 0; y < input_height; y++)
	{ /* for each row */
		int x;
		for(x = 0; (s32)x < input_width; x++)
		{ /* for each column */
			float fvalue = transform_to_float(src, src_index++, datatype);
			/* target is a bool, all values != 0 must become 1.0 */
			if( is_bool && fvalue != 0.f ) fvalue = 1.f;

			if (dst[x] != fvalue)
			{
				dst[x] = fvalue;
				if (NULL != cache)
				{
					cache[x] = _gles_fp32_to_fp16(fvalue);
				}
			}
		}
		dst += vector_stride;
		if (NULL != cache)
		{
			cache += vector_stride;
		}
	}
}

/**
 * Special-case of uniform setting for a float vertex uniform.
 * Setting vertex and fragment float uniforms differ in that fragment float uniforms must be converted to 16 bits and cached.
 */
MALI_STATIC_INLINE void _gles_set_float_uniforms_vertex(
	float* MALI_RESTRICT dst, int vector_stride,
	const float* MALI_RESTRICT src,
	int input_width, int input_height)
{
	int y;
	const int src_stride = input_width*sizeof(float);
	for(y = input_height; y > 0; y--)
	{
		_mali_sys_memcpy_runtime_32(dst, src, src_stride);
		dst += vector_stride;
		src += input_width;
	}
}

/**
 * Special-case of uniform setting for a float fragment uniform.
 * Setting vertex and fragment float uniforms differ in that fragment float uniforms must be converted to 16 bits and cached.
 */
MALI_STATIC void _gles_set_float_uniforms_fragment(
	float* MALI_RESTRICT dst, int vector_stride,
	const float* MALI_RESTRICT src, gles_fp16* MALI_RESTRICT cache,
	int input_width, int input_height)
{
	int x, y;

	MALI_DEBUG_ASSERT_POINTER(cache);
	for(y = 0; (s32)y < input_height; y++)
	{ /* for each row */
		for(x = 0; (s32)x < input_width; x++)
		{ /* for each column */
			float fvalue = *src++;

			if( dst[x] != fvalue ) 
			{
				dst[x] = fvalue;
				cache[x] = _gles_fp32_to_fp16(fvalue);
			}
		}
		dst += vector_stride;
		cache += vector_stride;
	}
}

/**
 * Special-case of uniform setting for a float vertex uniform where stride == width.
 * Setting vertex and fragment float uniforms differ in that fragment float uniforms must be converted to 16 bits and cached.
 */
MALI_STATIC_INLINE void _gles_set_float_uniforms_vertex_linear(
	float* MALI_RESTRICT dst,
	const float* MALI_RESTRICT src,
	int size)
{
	_mali_sys_memcpy_runtime_32(dst, src, size);
}

/**
 * Special-case of uniform setting for a float fragment uniform where stride == width.
 * Setting vertex and fragment float uniforms differ in that fragment float uniforms must be converted to 16 bits and cached.
 */
MALI_STATIC void _gles_set_float_uniforms_fragment_linear(
	float* MALI_RESTRICT dst,
	const float* MALI_RESTRICT src, gles_fp16* MALI_RESTRICT cache,
	int input_count)
{
	int i;

	MALI_DEBUG_ASSERT_POINTER(cache);

	for(i = input_count; i > 0; --i, ++src, ++dst, ++cache)
	{ /* for each column */
		if( *dst != *src )
		{
			*dst = *src;
			*cache = _gles_fp32_to_fp16(*src);
 		}
 	}
}

GLenum _gles2_uniform( struct gles_context *ctx, gles_datatype input_type,
                       GLuint input_width, GLuint input_height, GLint count, GLint location, const void* value)
{
	bs_uniform_location *uniform;
	struct bs_symbol       *symbol;
	GLint                   c;
	GLint                   startindex;
	void                   *vertex_array;
	void                   *fragment_array;
	mali_bool               uniform_is_bool;
	mali_bool               uniform_is_float;
	gles_program_rendering_state* prs;

	/* NOTE: this works ONLY because input type must be either int or float, both which are of the same size */
	const u32 src_stride = input_height * input_width * sizeof(float);

	MALI_DEBUG_ASSERT_POINTER(ctx);

	prs = ctx->state.common.current_program_rendering_state;


	MALI_DEBUG_ASSERT(input_type == GLES_INT || input_type == GLES_FLOAT, ("Input type must be int or float"));

	/* ensure that count is not below zero */
	if(count < 0) return GL_INVALID_VALUE;

	/* if location is -1, silently ignore it. */
	if(location == -1) return GL_NO_ERROR;

	/* ensure there is a current object (ie, prs not NULL) */
	if(prs == NULL) return GL_INVALID_OPERATION;

	/* location must be a legal location */
	if(location < 0 || (u32)location >= prs->gl_uniform_location_size) return GL_INVALID_OPERATION;

	uniform = &(prs->gl_uniform_locations[location]);

	/* get uniform location and corresponding active variable */
	symbol = uniform->symbol;
	startindex = uniform->extra.base_index;
	vertex_array = prs->binary.vertex_shader_uniforms.array;
	fragment_array = prs->binary.fragment_shader_uniforms.array;

	{
		/* assert that array size match */
		GLint symbolcount;

		symbolcount = symbol->array_size;
		if( symbolcount == 0 && 1 != count) return GL_INVALID_OPERATION;

		/* modify count to set no more than the symbolcount number of elements */
		if( 0 == symbolcount ) symbolcount = 1;

		/* limit the access given the starting index */
		symbolcount -= startindex;	
		MALI_DEBUG_ASSERT(symbolcount > 0, ("glUniform array access attempting to write to an index above the member count"));

		count = MIN(count, symbolcount);
	}

	/* if the active uniform is a sample type, we are actually trying to set a texture */
	if( bs_symbol_is_sampler( symbol ) )
	{
		int c;
		/* only integer glUniform1i[v] (input_type=GLES_INT, input_width/height=1) calls are allowed to write to samplers */
		if( input_type != GLES_INT || input_width != 1 || input_height != 1 ) return GL_INVALID_OPERATION;

		for(c = 0; c < count; c++)
		{
			u32 sampler_info_index = uniform->extra.sampler_location + c;
			u32 new_value = ((u32*)value)[c];
			MALI_DEBUG_ASSERT(sampler_info_index < prs->binary.samplers.num_samplers,
			                  ("Attempting to write to a too high sampler index - attempting %i, limit is %i", sampler_info_index, prs->binary.samplers.num_samplers));

			prs->binary.samplers.info[sampler_info_index].API_value = new_value;
			mali_statebit_set( &ctx->state.common, MALI_STATE_VS_PRS_UPDATE_PENDING );
		}

		/* at this point, early out. Don't touch the uniform table at all regardless of whether the sampler reads it or not */
		return GL_NO_ERROR;
	}

	/* we don't have a sampler; run a setter for normal uniforms */
	{
		u32 symbolheight;

		/* make sure that the datatype is consistent;
		 * int symbols must be written to by int setters,
		 * float symbols must be written to by float setters,
		 * matrix symbols must be written to by float setters,
		 * samplers are already handled,
		 * structs cannot be locations and will thus never occur,
		 * all other types (this means bool) can be written to by either setter */
		if((( input_type != GLES_INT )   && ( symbol->datatype == DATATYPE_INT )) ||
		   (( input_type != GLES_FLOAT ) && ( symbol->datatype == DATATYPE_FLOAT )) ||
		   (( input_type != GLES_FLOAT ) && ( symbol->datatype == DATATYPE_MATRIX )))
		{
			return GL_INVALID_OPERATION;
		}

		/* assert that symbol width match: vec3 must have 3, mat2 must have 2 etc. */
		if(input_width != symbol->type.primary.vector_size) return GL_INVALID_OPERATION;

		/* assert that height is good as well */
		symbolheight = 1; /* scalars are actually one row high */
		if(symbol->datatype == DATATYPE_MATRIX)
		{
			symbolheight *= input_width; /* matrices have N rows per entry, where N is the dimension of the symbol */
		}
		if(input_height != symbolheight) return GL_INVALID_OPERATION;
	}

	/* set the uniforms - in both the vertex and fragment data tables */

	/* if the targets are booleans we must explicitly make sure every value is either 0.0 or 1.0 */
	uniform_is_bool = (symbol->datatype == DATATYPE_BOOL);

	/* if the uniform input and destination are float we can take the fast paths */
	uniform_is_float = (uniform_is_bool == MALI_FALSE && input_type == GLES_FLOAT) ? MALI_TRUE : MALI_FALSE;

	/* we are ready to write to the uniform arrays */

	if (uniform->reg_location.vertex >= 0)
	{
		const u32 vector_stride = symbol->type.primary.vector_stride.vertex;
		const u32 array_element_stride = symbol->array_element_stride.vertex;

		const unsigned char *src = value;
		float *dst = &((float*)vertex_array)[uniform->reg_location.vertex];

		if (uniform_is_float && (vector_stride == input_width))
		{
			for(c = count; c > 0; c--)
			{
				_gles_set_float_uniforms_vertex_linear(dst, (float*)src, src_stride);
				dst += array_element_stride;
				src += src_stride;
			}
		} else if (uniform_is_float)
		{
			for(c = count; c > 0; c--)
			{
				_gles_set_float_uniforms_vertex(dst, vector_stride, (float*)src, input_width, input_height);
				dst += array_element_stride;
				src += src_stride;
			}
		} else
		{
			for(c = count; c > 0; c--)
			{
				_gles_set_uniforms(
					     dst, vector_stride,
					     (void*)src, NULL, input_width, input_height, (gles_datatype)input_type,
					     uniform_is_bool);
				dst += array_element_stride;
				src += src_stride;
			}
		}
		mali_statebit_set( &ctx->state.common, MALI_STATE_VS_VERTEX_UNIFORM_UPDATE_PENDING );
	}

	if (uniform->reg_location.fragment >= 0)
	{
		u32 vector_stride = symbol->type.primary.vector_stride.fragment;
		u32 array_element_stride = symbol->array_element_stride.fragment;

		const unsigned char *src = value;
		float *dst = &((float*)fragment_array)[uniform->reg_location.fragment];
		gles_fp16 *cache = &(prs->fp16_cached_fs_uniforms[uniform->reg_location.fragment]);

		if (uniform_is_float && (vector_stride == input_width))
		{
			for(c = count; c > 0; c--)
			{
				_gles_set_float_uniforms_fragment_linear(dst, (float*)src, cache, input_width*input_height);
				dst += array_element_stride;
				cache += array_element_stride;
				src += src_stride;
			}
		} else if (uniform_is_float)
		{
			for(c = count; c > 0; c--)
			{
				_gles_set_float_uniforms_fragment(dst, vector_stride, (float*)src, cache, input_width, input_height);
				dst += array_element_stride;
				cache += array_element_stride;
				src += src_stride;
			}
		} else
		{
			for(c = count; c > 0; c--)
			{
				_gles_set_uniforms(
				     dst, vector_stride,
				     (void*)src, cache, input_width, input_height, (gles_datatype)input_type,
				     uniform_is_bool);
				dst += array_element_stride;
				cache += array_element_stride;
				src += src_stride;
			}
		}
		mali_statebit_set( &ctx->state.common, MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING );
	}

	return GL_NO_ERROR;
}

GLenum _gles2_uniform1i( struct gles_context *ctx, GLint location, GLint val)
{
	bs_uniform_location *uniform;
	struct bs_symbol       *symbol;
	void                   *vertex_array;
	void                   *fragment_array;
	gles_fp16              *cache;
	float fval;
	gles_program_rendering_state* prs = ctx->state.common.current_program_rendering_state;

	/* if location is -1, silently ignore it. */
	if(location == -1) return GL_NO_ERROR;

	/* ensure there is a current object (ie, prs not NULL) */
	if(prs == NULL) return GL_INVALID_OPERATION;

	/* location must be a legal location */
	if(location < 0 || (u32)location >= prs->gl_uniform_location_size) return GL_INVALID_OPERATION;

	uniform = &(prs->gl_uniform_locations[location]);

	/* get uniform location and corresponding active variable */
	symbol = uniform->symbol;
	vertex_array = prs->binary.vertex_shader_uniforms.array;
	fragment_array = prs->binary.fragment_shader_uniforms.array;
	cache = prs->fp16_cached_fs_uniforms;

	/* if the active uniform is a sample type, we are actually trying to set a texture */
	if ( bs_symbol_is_sampler(symbol) )
	{
		u32 sampler_info_index = uniform->extra.sampler_location;
		MALI_DEBUG_ASSERT(
			sampler_info_index < prs->binary.samplers.num_samplers,
			("Attempting to write to a too high sampler index - attempting %i, limit is %i", sampler_info_index, prs->binary.samplers.num_samplers)
			);
	
		/* disallowing setting of samplers to values > GLES_MAX_TEXTURE_UNTS.
		 * This is undefined in the spec, and we're now handling it equal to Mali 600. 
		 * See bugzilla entry 11069 for details. */
		if( val >= GLES_MAX_TEXTURE_UNITS ) return GL_INVALID_VALUE;

		prs->binary.samplers.info[sampler_info_index].API_value = val;
		mali_statebit_set( &ctx->state.common, MALI_STATE_VS_PRS_UPDATE_PENDING );

		/* at this point, early out. Don't touch the uniform table at all regardless of whether the sampler reads it or not */
		return GL_NO_ERROR;

	}

	/* check that the datatype is correct; only integer-compatible symbols may be touched by glUniform1i  */
	switch(symbol->datatype)
	{
		case DATATYPE_BOOL: 	val = !!val; /* convert input to 0 or 1 */
								/* fallthrough */
		case DATATYPE_INT:      break;
		default:                return GL_INVALID_OPERATION;
	}

	/* assert that symbol width is infact 1 */
	if (1 != symbol->type.primary.vector_size) return GL_INVALID_OPERATION;

	/* we are ready to write to the uniform arrays */
	fval = (float)val;
	if (uniform->reg_location.vertex >= 0)
	{
		((float*)vertex_array)[uniform->reg_location.vertex] = fval;
		mali_statebit_set( &ctx->state.common, MALI_STATE_VS_VERTEX_UNIFORM_UPDATE_PENDING );
	}

	if (uniform->reg_location.fragment >= 0)
	{
		if (fval != ((float*)fragment_array)[uniform->reg_location.fragment])
		{
			((float*)fragment_array)[uniform->reg_location.fragment] = fval;
			cache[uniform->reg_location.fragment] = _gles_fp32_to_fp16(fval);

			mali_statebit_set( &ctx->state.common, MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING );
		}
	}

	return GL_NO_ERROR;
}

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
GLenum _gles2_get_n_uniform_ext( struct gles_context *ctx, mali_named_list* program_object_list, GLuint program, GLint location, GLsizei bufSize, void* output_array, GLenum output_type )
{
	if( ctx->robust_strategy)
	{
		return _gles_get_uniform_internal( program_object_list, program, location, output_array, output_type, bufSize );
	}
	else
	{
		return GL_INVALID_OPERATION;
	}
}
#endif
