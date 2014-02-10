/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <base/mali_memory.h>
#include <base/mali_runtime.h>
#include <shared/binary_shader/bs_object.h>
#include <shared/binary_shader/bs_symbol.h>
#include <shared/binary_shader/bs_loader.h>
#include <shared/binary_shader/link_gp2.h>
#include <shared/shadergen_interface.h>
#include "gles_shader_generator_context.h"
#include "gles_vertex_state_extractor.h"
#include "gles_fragment_state_extractor.h"
#include "../gles_gb_interface.h"
#include "../gles_fb_interface.h"
#include <gles_instrumented.h>
#include "compiler.h"

#if MALI_INSTRUMENTED
#include "dump_vert_shader_info.h"
#include "dump_frag_shader_info.h"
static int mali_dump_shadergen = -1;
#endif

#if defined USING_MALI200
#define MALI_HWVER (HW_REV_CORE_MALI200 << 16) | MALI200_HWVER
#elif defined USING_MALI400
#define MALI_HWVER (HW_REV_CORE_MALI400 << 16) | MALI400_HWVER
#elif defined USING_MALI450
/* TODO: Mali-450: Use 450 spefici value */
#define MALI_HWVER (HW_REV_CORE_MALI400 << 16) | MALI450_HWVER
#else
#error "No supported mali core defined"
#endif

/**
 * Creates a new vertex shader into the sg_ctx
 * @param ctx The GLES-context
 * @param sg_ctx The sg_context holding the shaders
 * @return An errorcode if unsuccesful, MALI_ERR_NO_ERROR if succesful
 */
MALI_STATIC mali_err_code make_vertex_shader(gles_sg_context *sg_ctx)
{
	mali_err_code error_code;
	unsigned *shader_instructions;
	unsigned shader_size;
	gles_sg_vertex_shader **shp;
	gles_sg_vertex_shader *new_shader = sg_ctx->fresh_vertex_shader;
	gles_sg_vertex_shader *new_fresh_shader;


	/* Find existing shader */
	for (shp = &sg_ctx->vertex_shaders ; *shp != 0 ; shp = &(*shp)->next) {
		if (COMPARE_VERTEX_SHADERGEN_STATES_EQUIVALENT((*shp)->state, sg_ctx->current_vertex_state))
		{
			/* Found existing shader. Move it to the front of the list to make it
			 * faster to find next time. */
			gles_sg_vertex_shader *sh = *shp;
			*shp = sh->next;
			sh->next = sg_ctx->vertex_shaders;
			sg_ctx->vertex_shaders = sh;
			return MALI_ERR_NO_ERROR;
		}
	}

	/* a new shader must be generated */
	_profiling_count( INST_GL_NUM_VSHADERS_GENERATED, 1);

	_profiling_enter( INST_GL_VSHADER_GEN_TIME);

	/* Copy state to fresh shader struct */
	new_shader->state = sg_ctx->current_vertex_state;

	/* Allocate new fresh shader */
	new_fresh_shader = _mali_sys_malloc(sizeof(gles_sg_vertex_shader));
	if (NULL == new_fresh_shader) {
		_profiling_leave( INST_GL_VSHADER_GEN_TIME);
		return MALI_ERR_OUT_OF_MEMORY;
	}

	/* Generate new shader */
	shader_instructions = _gles_vertex_shadergen_generate_shader(&sg_ctx->current_vertex_state, &shader_size, _mali_sys_malloc, _mali_sys_free);
	if (NULL == shader_instructions) {
		_mali_sys_free( new_fresh_shader );
		new_fresh_shader = NULL;
		_profiling_leave( INST_GL_VSHADER_GEN_TIME);
		return MALI_ERR_OUT_OF_MEMORY;
	}
#if MALI_INSTRUMENTED
    if(mali_dump_shadergen == -1)
        mali_dump_shadergen = (int)_mali_sys_config_string_get_s64("MALI_DUMP_SHADERGEN", 0, 0, 1);
    if(mali_dump_shadergen == 1)
        dump_vert_shader_info(&sg_ctx->current_vertex_state);
#endif

	sg_ctx->fresh_vertex_shader = new_fresh_shader;

	/* Set new shader as current */
	new_shader->next = sg_ctx->vertex_shaders;
	sg_ctx->vertex_shaders = new_shader;
	new_shader->delete_mark = MALI_FALSE;

	/* Initialize shader object */
	__mali_shader_binary_state_init(&new_shader->shader);

	error_code = __mali_binary_shader_load(&new_shader->shader, BS_VERTEX_SHADER, (char*)shader_instructions, shader_size);

	_mali_sys_free(shader_instructions);

	_profiling_leave( INST_GL_VSHADER_GEN_TIME);

	return error_code;
}

/**
 * Creates a new fragment shader into the sg_ctx
 * @param ctx The GLES-context
 * @param sg_ctx The sg_context holding the shaders
 * @return An errorcode if unsuccesful, MALI_ERR_NO_ERROR if succesful
 */
MALI_STATIC mali_err_code make_fragment_shader(gles_context *ctx, gles_sg_context *sg_ctx, GLenum primitive_type)
{
	mali_err_code error_code;
	unsigned *shader_instructions;
	unsigned shader_size;
	gles_sg_fragment_shader **shp;
	gles_sg_fragment_shader *new_shader = sg_ctx->fresh_fragment_shader;
	gles_sg_fragment_shader *new_fresh_shader;

	/* Extract state */
	_gles_sg_extract_fragment_state(ctx, primitive_type);

	/* Find existing shader */
	for (shp = &sg_ctx->fragment_shaders ; *shp != 0 ; shp = &(*shp)->next)
	{
		if (_gles_fragment_shadergen_states_equivalent(&(*shp)->state, &sg_ctx->current_fragment_state))
		{
			/* Found existing shader. Move it to the front of the list to make it
			 * faster to find next time. */
			gles_sg_fragment_shader *sh = *shp;
			*shp = sh->next;
			sh->next = sg_ctx->fragment_shaders;
			sg_ctx->fragment_shaders = sh;
			return MALI_ERR_NO_ERROR;
		}
	}

	/* a new shader must be generated */
	_profiling_count( INST_GL_NUM_FSHADERS_GENERATED, 1);

	_profiling_enter( INST_GL_FSHADER_GEN_TIME);

	/* Copy state to fresh shader struct */
	new_shader->state = sg_ctx->current_fragment_state;

	/* Allocate new fresh shader */
	new_fresh_shader = _mali_sys_malloc(sizeof(gles_sg_fragment_shader));
	if (NULL == new_fresh_shader) {
		return MALI_ERR_OUT_OF_MEMORY;
	}

	/* Generate new shader */
	shader_instructions = _gles_fragment_shadergen_generate_shader(&sg_ctx->current_fragment_state, &shader_size, MALI_HWVER, _mali_sys_malloc, _mali_sys_free);
	if (NULL == shader_instructions) {
		_mali_sys_free( new_fresh_shader );
		return MALI_ERR_OUT_OF_MEMORY;
	}

	sg_ctx->fresh_fragment_shader = new_fresh_shader;

	/* Set new shader as current */
	new_shader->next = sg_ctx->fragment_shaders;
	sg_ctx->fragment_shaders = new_shader;
	new_shader->delete_mark = MALI_FALSE;

	/* Initialize shader object */
	__mali_shader_binary_state_init(&new_shader->shader);

	error_code = __mali_binary_shader_load(&new_shader->shader, BS_FRAGMENT_SHADER, (char*)shader_instructions, shader_size);

#if MALI_INSTRUMENTED
    if(mali_dump_shadergen == -1)
        mali_dump_shadergen = (int)_mali_sys_config_string_get_s64("MALI_DUMP_SHADERGEN", 0, 0, 1);
    if(mali_dump_shadergen == 1)
        dump_frag_shader_info(&sg_ctx->current_fragment_state, &new_shader->shader);
#endif

	_mali_sys_free(shader_instructions);

	_profiling_leave( INST_GL_FSHADER_GEN_TIME);

	return error_code;
}

/**
 * Destroys the supplied shader
 * @param shader The shader to be destroyed
 */
MALI_STATIC void destroy_shader(bs_shader *shader)
{
	MALI_DEBUG_ASSERT_POINTER( shader );
	__mali_shader_binary_state_reset(shader);

	_mali_sys_free(shader->shader_block);
	shader->shader_block = NULL;

	if(shader->binary_block != NULL)
	{
		_mali_sys_free(shader->binary_block);
		shader->binary_block = NULL;
		shader->binary_size = 0;
	}
}

/**
 * Remaps and compresses attributes
 * @param program The program object to be remapped and compressed
 * @return An errorcode if unsuccesful, MALI_ERR_NO_ERROR if succesful
 */
MALI_STATIC mali_err_code compress_attributes(gles_program_rendering_state *prs) {
	mali_err_code error_code;
	int shader_to_final[MALIGP2_MAX_VS_INPUT_REGISTERS];
	int next = 0;
	int i;
	int *remap = prs->attribute_remap_table;
	for (i = 0 ; i < MALIGP2_MAX_VS_INPUT_REGISTERS ; i++) {
		shader_to_final[i] = -1;
	}
	for (i = 0 ; i < GLES_VERTEX_ATTRIB_COUNT ; i++) {
		if (remap[i] >= 0) {
			shader_to_final[remap[i]] = next;
			remap[i] = next;
			next++;
		}
	}
	error_code = _mali_gp2_link_attribs(&prs->binary, shader_to_final, MALI_TRUE);
	return error_code;
}

/**
 * Marks a texture source as in use.
 * @param textures_enabled   a boolean array describing which texture units are in use and not
 * @param a                  A source argument describing some combiner source.
 */
MALI_STATIC_INLINE void mark_texture_source_required(mali_bool* textures_enabled, arg_source a)
{
	if(a >= ARG_TEXTURE_0 && a < ARG_TEXTURE_0 + MAX_TEXTURE_STAGES) textures_enabled[a - ARG_TEXTURE_0] = MALI_TRUE;
}

/**
 * Enables textures based on which combine mode is used
 * @param textures_enabled A boolean table to hold which textures are enabled
 * @param combiner The combine function used
 * @param sources The source textures
 */

static void mark_stage_sources(mali_bool *textures_enabled, const fragment_shadergen_state *state, unsigned int stagenumber)
{
	arg_source source;
	texture_combiner combiner;

	combiner = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_COMBINER(stagenumber) );

	switch (combiner) {
	case COMBINE_INTERPOLATE:
		source = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stagenumber, 2) );
		mark_texture_source_required(textures_enabled, source);
		/* fall-through */
	case COMBINE_MODULATE:
		/* fall-through */
	case COMBINE_ADD:
		/* fall-through */
	case COMBINE_ADD_SIGNED:
		/* fall-through */
	case COMBINE_SUBTRACT:
		/* fall-through */
	case COMBINE_DOT3_RGB:
		/* fall-through */
	case COMBINE_DOT3_RGBA:
		source = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stagenumber, 1) );
		mark_texture_source_required(textures_enabled, source);
		/* fall-through */
	case COMBINE_REPLACE:
		source = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stagenumber, 0) );
		mark_texture_source_required(textures_enabled, source);
		/* fall-through */
	default:
		break;
	}


	combiner = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_COMBINER(stagenumber) );
	switch (combiner) {
	case COMBINE_INTERPOLATE:
		source = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stagenumber, 2) );
		mark_texture_source_required(textures_enabled, source);
		/* fall-through */
	case COMBINE_MODULATE:
		/* fall-through */
	case COMBINE_ADD:
		/* fall-through */
	case COMBINE_ADD_SIGNED:
		/* fall-through */
	case COMBINE_SUBTRACT:
		/* fall-through */
	case COMBINE_DOT3_RGB:
		/* fall-through */
	case COMBINE_DOT3_RGBA:
		source = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stagenumber, 1) );
		mark_texture_source_required(textures_enabled, source);
		/* fall-through */
	case COMBINE_REPLACE:
		source = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stagenumber, 0) );
		mark_texture_source_required(textures_enabled, source);
		/* fall-through */
	default:
		break;
	}
}

/**
 * Finds enabled textures
 * @param textures_enabled A boolean table holding which textures are enabled
 * @param state The fragment shader state
 */
static void find_enabled_textures(mali_bool *textures_enabled, const fragment_shadergen_state *state) {
	int i;
	arg_source result_source;
	for (i = 0 ; i < MAX_TEXTURE_STAGES ; i++)
	{
		textures_enabled[i] = 0;
	}

	for (i = 0 ; i < MAX_TEXTURE_STAGES ; i++)
	{
		if(ESSL_FALSE == fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_ENABLE(i)) &&
		   ESSL_FALSE ==  fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_ENABLE(i)) ) continue;
		mark_stage_sources(textures_enabled, state, i );
	}
	result_source = fragment_shadergen_decode( state, FRAGMENT_SHADERGEN_RESULT_COLOR_SOURCE );
	mark_texture_source_required(textures_enabled, result_source);
}

/**
 * Initialises the program
 * @param sg_ctx The shader generator-context
 * @return An errorcode if unsuccesful, MALI_ERR_NO_ERROR if succesful
 */
MALI_STATIC mali_err_code init_program(gles_sg_context *sg_ctx, gles_sg_program *program)
{
	mali_err_code error_code;
	unsigned i;
	mali_bool textures_enabled[MAX_TEXTURE_STAGES];
	gles_program_rendering_state *prs = program->prs;

	if( NULL != prs->binary.vertex_shader_uniforms.array ) _mali_sys_free(prs->binary.vertex_shader_uniforms.array);
	prs->binary.vertex_shader_uniforms.array = sg_ctx->vertex_uniform_array;
	prs->binary.vertex_shader_uniforms.cellsize = _gles_sg_get_vertex_uniform_array_size(&program->vertex_shader->state);

	if( NULL != prs->binary.fragment_shader_uniforms.array ) _mali_sys_free(prs->binary.fragment_shader_uniforms.array);
	prs->binary.fragment_shader_uniforms.array = sg_ctx->fragment_uniform_array;
	prs->binary.fragment_shader_uniforms.cellsize = _gles_sg_get_fragment_uniform_array_size(&program->fragment_shader->state);

	prs->fp16_cached_fs_uniforms = sg_ctx->fragment_fp16_uniform_array;

	/* Set up identity mapping from sampler index to texture stage */
	find_enabled_textures(textures_enabled, &program->fragment_shader->state);
	
	/* In GLES1, Each sampler is hardcoded to match its predefined Texture Unit. 
	 * This code sets that relation up. Once set up, they will not be changed. 
	 * Note: There are two kinds of samplers exposed:
	 * 	- StageSamplerNormal0-7 
	 * 	- StageSamplerExternal0-7
	 */
	MALI_DEBUG_ASSERT(MAX_TEXTURE_STAGES<10, ("The name comparison code below only works if MAX_TEXTURE_STAGES is one digit. Change the code below if this is not true"));
	for (i = 0 ; i < prs->binary.samplers.num_samplers ; i++)
	{
		const char* symbolname = prs->binary.samplers.info[i].symbol->name;

		MALI_DEBUG_ASSERT(
			(0 == _mali_sys_memcmp("StageSamplerNormal", symbolname, 18) && _mali_sys_strlen(symbolname) == 19 && symbolname[18] >= '0' && symbolname[18] < '0'+MAX_TEXTURE_STAGES)||
			(0 == _mali_sys_memcmp("StageSamplerExternal", symbolname, 20) && _mali_sys_strlen(symbolname) == 21 && symbolname[20] >= '0' && symbolname[20] < '0'+MAX_TEXTURE_STAGES),
			("GLES1 generated sampler name must start with 'StageSamplerNormal' or 'StageSamplerExternal'; found '%s',"
			 "Also it must end in exactly one digit ", symbolname));
		if( 0 == _mali_sys_memcmp("StageSamplerNormal", symbolname, 18) && _mali_sys_strlen(symbolname) == 19 && 
		    symbolname[18] >= '0' && symbolname[18] < '0'+MAX_TEXTURE_STAGES) 
		{
			int stage = symbolname[18] - '0';
			prs->binary.samplers.info[i].API_value = stage;
			prs->binary.samplers.info[i].td_remap_table_index = stage;
			prs->binary.samplers.td_remap_table_size = MAX(prs->binary.samplers.td_remap_table_size, MAX_TEXTURE_STAGES);
		} else
		if( 0 == _mali_sys_memcmp("StageSamplerExternal", symbolname, 20) && _mali_sys_strlen(symbolname) == 21 && 
		    symbolname[20] >= '0' && symbolname[20] < '0'+MAX_TEXTURE_STAGES)
		{
			int stage = symbolname[20] - '0';
			prs->binary.samplers.info[i].API_value = stage;
			prs->binary.samplers.info[i].YUV_metadata_index = (FRAGMENT_UNIFORM_YUV_Constants) + 16*stage; /* 16 cells per uniform*/
			prs->binary.samplers.info[i].td_remap_table_index = (3*stage) + MAX_TEXTURE_STAGES;
			prs->binary.samplers.td_remap_table_size = MAX(prs->binary.samplers.td_remap_table_size, 4*MAX_TEXTURE_STAGES); /* 1x normal samplers + 3x YUV samplers */
		} else {
			MALI_DEBUG_ASSERT(0, ("Sampler found was neither normal or external. What is it?"));
		}
	}

	if (vertex_shadergen_decode(&program->vertex_shader->state, VERTEX_SHADERGEN_FEATURE_POINTSIZE_COPY) == MALI_FALSE)
	{
		/* nuke the point size location to prevent the backend from using it */
		prs->binary.vertex_pass.flags.vertex.pointsize_register = -1;
		if(NULL != prs->binary.varying_pointsize_symbol) bs_symbol_free(prs->binary.varying_pointsize_symbol);

		prs->binary.varying_pointsize_symbol = 0;
	}

	/* Only set gl_mali_PointCoordScaleBias location if it is actually used by the shader */
	if (prs->binary.fragment_shader_uniforms.cellsize >= FRAGMENT_UNIFORM_gl_mali_PointCoordScaleBias+4)
	{
		prs->pointcoordscalebias_uniform_fs_location = FRAGMENT_UNIFORM_gl_mali_PointCoordScaleBias;
		prs->flip_scale_bias_locations_fs_in_use = MALI_TRUE;
	}

	prs->viewport_uniform_vs_location = VERTEX_UNIFORM_POS(gl_mali_ViewportTransform);
	prs->pointsize_parameters_uniform_vs_location = VERTEX_UNIFORM_POS(gl_mali_PointSizeParameters);


#if HARDWARE_ISSUE_4326
	/* The symbol data is lying. It is listing absolutely all possible streams, whether the shader use it or not.
	 * So as a workaround for this issue, we'll nuke all symbols. This is required for the rewrite later */
	{
		int i;
		struct bs_symbol_table* attributes = prs->binary.attribute_symbols;
		for(i = 0; i < attributes->member_count; i++)
		{
			bs_symbol_free(attributes->members[i]);
			attributes->members[i] = NULL; /* holes in this list is okay, this operation makes it 100% empty */
		}
		for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
		{
			prs->binary.attribute_streams.info[i].index = i;
			prs->binary.attribute_streams.info[i].symbol  = NULL;
		}
	}
#endif
	_gles_sg_make_attribute_remap_table(&program->vertex_shader->state, prs->attribute_remap_table);
	error_code = compress_attributes(prs);
#if HARDWARE_ISSUE_4326
	{
		mali_err_code err;
		/* rewrite the shader */
		err = __mali_gp2_rewrite_attributes_for_bug_4326(&prs->binary);
		MALI_DEBUG_ASSERT(err == MALI_ERR_NO_ERROR, ("Any generated program should always be legal on R0P1"));
		if(err != MALI_ERR_NO_ERROR) return err;
	}
#endif

	/* setup reverse mapping and attribute stream data */
	prs->binary.attribute_streams.count = 0;
	for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
	{
		if( prs->attribute_remap_table[i] > -1 )
		{
			prs->reverse_attribute_remap_table[prs->attribute_remap_table[i]] = i;
			prs->binary.attribute_streams.count++;
		}
		prs->binary.attribute_streams.info[i].index = i;
		prs->binary.attribute_streams.info[i].symbol  = NULL;
	}
	prs->binary.vertex_pass.flags.vertex.num_input_registers = prs->binary.attribute_streams.count;


	/* set up the backend specific part of the program rendering state */
	prs->fb_prs = _gles_fb_alloc_program_rendering_state( prs );
	if( prs->fb_prs == NULL ) return MALI_ERR_OUT_OF_MEMORY;
	prs->gb_prs = _gles_gb_alloc_program_rendering_state( prs );
	if( prs->gb_prs == NULL ) return MALI_ERR_OUT_OF_MEMORY;

	return error_code;
}

/**
 * Destroys the program
 * @param sg_ctx The sg_context holding the shaders
 * @param program The program to destroy
 */
MALI_STATIC void destroy_program(gles_sg_context *sg_ctx, gles_program_rendering_state *prs)
{
	MALI_IGNORE(sg_ctx);

	MALI_DEBUG_ASSERT(prs->binary.vertex_shader_uniforms.array == sg_ctx->vertex_uniform_array, ("Program not created by us"));
	prs->binary.vertex_shader_uniforms.array = 0;
	prs->binary.vertex_shader_uniforms.cellsize = 0;

	MALI_DEBUG_ASSERT(prs->binary.fragment_shader_uniforms.array == sg_ctx->fragment_uniform_array, ("Program not created by us"));
	prs->binary.fragment_shader_uniforms.array = 0;
	prs->binary.fragment_shader_uniforms.cellsize = 0;

	MALI_DEBUG_ASSERT(prs->fp16_cached_fs_uniforms == sg_ctx->fragment_fp16_uniform_array,
	                  ("Uniform cache not created by us"));
	prs->fp16_cached_fs_uniforms = 0;
	_gles_program_rendering_state_deref(prs);
}

/**
 * Constructs the program
 * @param sg_ctx The sg-context holding the shaders
 * @return An errorcode if unsuccesful, MALI_ERR_NO_ERROR if succesful
 */
MALI_STATIC mali_err_code make_program(gles_sg_context *sg_ctx)
{
	mali_err_code error_code = MALI_ERR_NO_ERROR;
	gles_sg_program **progp;
	gles_sg_program *program;

	/* Find existing program */
	for (progp = &sg_ctx->programs ; *progp != 0 ; progp = &(*progp)->next)
	{
		if ((*progp)->vertex_shader == sg_ctx->vertex_shaders &&
			(*progp)->fragment_shader == sg_ctx->fragment_shaders)
		{
			/* Found existing program */
			gles_sg_program *prog = *progp;
			*progp = prog->next;
			prog->next = sg_ctx->programs;
			sg_ctx->programs = prog;
			return MALI_ERR_NO_ERROR;
		}
	}

	/* Allocate a new program */
	program = _mali_sys_malloc(sizeof(*program));
	if (NULL == program) return MALI_ERR_OUT_OF_MEMORY;

	program->prs = _gles_program_rendering_state_alloc();
	if (NULL == program->prs )
	{
		_mali_sys_free(program);
		return MALI_ERR_OUT_OF_MEMORY;
	}

	/* Link shaders */
	__mali_program_binary_state_init(&program->prs->binary);
	error_code = __mali_link_binary_shaders( sg_ctx->base_ctx, &program->prs->binary,
											&sg_ctx->vertex_shaders->shader,
											&sg_ctx->fragment_shaders->shader);

	/* handle linker error */
	if (error_code != MALI_ERR_NO_ERROR)
	{
		__mali_program_binary_state_reset(&program->prs->binary);
		_mali_sys_free(program->prs);
		_mali_sys_free(program);
		return error_code;
	}

	/* complete program */
	program->vertex_shader = sg_ctx->vertex_shaders;
	program->fragment_shader = sg_ctx->fragment_shaders;

	error_code = init_program(sg_ctx, program);

	if ( MALI_ERR_NO_ERROR != error_code )
	{
		destroy_program(sg_ctx, program->prs);
		program->prs = NULL;
		_mali_sys_free(program);

		MALI_ERROR( error_code );
	}

	/* add new program to list */
	program->next = sg_ctx->programs;
	sg_ctx->programs = program;

	MALI_SUCCESS;
}

/**
 * Cleans up and frees shaders and programs marked for deletion
 * @param sg_ctx The sg_context holding the shaders
 * @param only_marked Set to false if all shaders should be deleted
 */
MALI_STATIC void free_shaders_and_programs(gles_sg_context *sg_ctx, shadergen_boolean only_marked) {
	gles_sg_vertex_shader **vsp;
	gles_sg_fragment_shader **fsp;
	gles_sg_program **pp;

	for (pp = &sg_ctx->programs ; *pp != NULL ;) {
		if (!only_marked || (*pp)->vertex_shader->delete_mark || (*pp)->fragment_shader->delete_mark) {
			gles_sg_program *prog = *pp;
			*pp = prog->next;
			destroy_program(sg_ctx, prog->prs);
			prog->prs = NULL;
			_mali_sys_free(prog);
		} else {
			pp = &(*pp)->next;
		}
	}

	for (vsp = &sg_ctx->vertex_shaders ; *vsp != NULL ;) {
		if (!only_marked || (*vsp)->delete_mark) {
			gles_sg_vertex_shader *vshader = *vsp;
			*vsp = vshader->next;
			destroy_shader(&vshader->shader);
			_mali_sys_free(vshader);
		} else {
			vsp = &(*vsp)->next;
		}
	}

	for (fsp = &sg_ctx->fragment_shaders ; *fsp != NULL ;) {
		if (!only_marked || (*fsp)->delete_mark) {
			gles_sg_fragment_shader *fshader = *fsp;
			*fsp = fshader->next;
			destroy_shader(&fshader->shader);
			_mali_sys_free(fshader);
		} else {
			fsp = &(*fsp)->next;
		}
	}
}

/**
 * Cleans the oldest shaders
 * @param sg_ctx The sg_context holding the shaders
 */
MALI_STATIC void clean_old_shaders(gles_sg_context *sg_ctx) {
	gles_sg_vertex_shader *vs, *oldest_vs;
	u32 num_vshaders = 0;

	gles_sg_fragment_shader *fs, *oldest_fs;
	u32 num_fshaders = 0;

	/* we always have at least one shader before calling this function */
	MALI_DEBUG_ASSERT_POINTER( sg_ctx->vertex_shaders );
	MALI_DEBUG_ASSERT_POINTER( sg_ctx->fragment_shaders );

	oldest_vs = sg_ctx->vertex_shaders;
	oldest_fs = sg_ctx->fragment_shaders;

	/* find the oldest vertex shader */
	for (vs = sg_ctx->vertex_shaders ; vs != NULL ; vs = vs->next) {
		num_vshaders++;
		if ( vs->last_used < oldest_vs->last_used ) {
			oldest_vs = vs;
		}
	}

	/* if the next draw call might make the number of stored vertex shaders
	 * larger than the maximum stored shaders, then mark the oldest one for
	 * deletion.
	 */
	if ( num_vshaders >= (MAX_NUM_CACHED_VERTEX_SHADERS-1) )
	{
		oldest_vs->delete_mark = MALI_TRUE;
	}

	/* .. and the same for fragment shaders */
	for (fs = sg_ctx->fragment_shaders ; fs != NULL ; fs = fs->next) {
		num_fshaders++;
		if ( fs->last_used < oldest_fs->last_used ) {
			oldest_fs = fs;
		}
	}

	if ( num_fshaders >= (MAX_NUM_CACHED_FRAGMENT_SHADERS-1) )
	{
		oldest_fs->delete_mark = MALI_TRUE;
	}

	/* if any shaders were marked for deletion, we now delete them along with
	 * all programs they are used in.
	 */
	if ( oldest_fs->delete_mark || oldest_vs->delete_mark )
	{
		free_shaders_and_programs(sg_ctx, MALI_TRUE);
	}
}

/**
 * Initialisez the shader for this draw call
 * @param ctx The GLES-context
 * @param sg_ctx The sg_context holding the shaders
 */
mali_err_code _gles_sg_init_draw_call( struct gles_context *ctx, struct gles_sg_context *sg_ctx, GLenum primitive_type)
{
	mali_err_code error_code;

	sg_ctx->current_timestamp++;
	if ( (0 == sg_ctx->current_timestamp) || (sg_ctx->current_timestamp >= MAX_TIMESTAMP) )
	{
		/* the counter has wrapped, so we flush the cache and reset the timestamp */
		free_shaders_and_programs(sg_ctx, MALI_FALSE);
		sg_ctx->current_timestamp = 0;
	}

	error_code = make_vertex_shader(sg_ctx);
	if (error_code != MALI_ERR_NO_ERROR) return error_code;
	sg_ctx->vertex_shaders->last_used = sg_ctx->current_timestamp;

	error_code = make_fragment_shader(ctx, sg_ctx, primitive_type);
	if (error_code != MALI_ERR_NO_ERROR) return error_code;
	sg_ctx->fragment_shaders->last_used = sg_ctx->current_timestamp;

	clean_old_shaders(sg_ctx);

	error_code = make_program(sg_ctx);
	if (error_code != MALI_ERR_NO_ERROR) return error_code;

	{
		vertex_shadergen_state* vertex_state = &sg_ctx->current_vertex_state;
		fragment_shadergen_state *fragment_state = &sg_ctx->current_fragment_state;
		gles_program_rendering_state *current_prs = sg_ctx->programs->prs;

		if (ctx->state.common.current_program_rendering_state != current_prs )
		{
			ctx->state.common.current_program_rendering_state = current_prs;
			mali_statebit_set( & ctx->state.common, MALI_STATE_VS_PRS_UPDATE_PENDING );
			current_prs->const_regs_to_load = VERTEX_UNIFORM_GROUP_POS_basic;
		}
              
		_gles_sg_extract_vertex_uniforms(ctx, vertex_state,
										  current_prs->binary.vertex_shader_uniforms.array, &current_prs->binary);
		_gles_sg_update_current_attribute_values(&ctx->state);
		_gles_sg_extract_fragment_uniforms(ctx, fragment_state,
		                                   current_prs->fp16_cached_fs_uniforms,
		                                   &current_prs->binary);
	}

	return MALI_ERR_NO_ERROR;
}


/**
 * Allocs a sg_context object
 * @param base_ctx The Base-context
 * @return The created sg_context
 */
struct gles_sg_context *_gles_sg_alloc( mali_base_ctx_handle base_ctx )
{
	gles_sg_context *sg_ctx;
	unsigned i;
	const uniform_initializer *uniform_inits;
	unsigned n_inits;

	MALI_DEBUG_ASSERT_POINTER( base_ctx );

	sg_ctx = ( gles_sg_context * ) _mali_sys_malloc( sizeof( gles_sg_context ) );
	if( sg_ctx == NULL ) return NULL;

	sg_ctx->base_ctx = base_ctx;
	sg_ctx->current_timestamp = 0;

	/* Allocate fresh shaders */
	sg_ctx->fresh_vertex_shader = _mali_sys_malloc(sizeof(gles_sg_vertex_shader));
	if (sg_ctx->fresh_vertex_shader == NULL) {
		_mali_sys_free(sg_ctx);
		return NULL;
	}
	sg_ctx->fresh_fragment_shader = _mali_sys_malloc(sizeof(gles_sg_fragment_shader));
	if (sg_ctx->fresh_fragment_shader == NULL) {
		_mali_sys_free(sg_ctx->fresh_vertex_shader);
		_mali_sys_free(sg_ctx);
		return NULL;
	}

	/* reset structures */
	_mali_sys_memset( sg_ctx->fresh_vertex_shader, 0, sizeof(gles_sg_vertex_shader) );
	_mali_sys_memset( sg_ctx->fresh_fragment_shader, 0, sizeof(gles_sg_fragment_shader) );
	_mali_sys_memset( &sg_ctx->current_fragment_state, 0, sizeof(struct fragment_shadergen_state) );
	_mali_sys_memset( &sg_ctx->current_vertex_state, 0, sizeof(struct vertex_shadergen_state) );
	_mali_sys_memset( sg_ctx->fragment_fp16_uniform_array, 0, SG_N_FRAGMENT_UNIFORMS * sizeof( gles_fp16 ) );

	sg_ctx->vertex_shaders = NULL;
	sg_ctx->fragment_shaders = NULL;
	sg_ctx->programs = NULL;

	_mali_sys_memset(sg_ctx->vertex_uniform_array, 0, sizeof(sg_ctx->vertex_uniform_array));
	_mali_sys_memset(sg_ctx->fragment_uniform_array, 0, sizeof(sg_ctx->fragment_uniform_array));

	uniform_inits = _gles_piecegen_get_uniform_initializers(&n_inits);
	for (i = 0 ; i < n_inits ; i++) {
		sg_ctx->vertex_uniform_array[uniform_inits[i].address] = uniform_inits[i].value;
	}

	sg_ctx->last_light_enabled_field = 0x0;
	sg_ctx->last_texture_transform_field = 0x0;

	return sg_ctx;
}

/**
 * Frees the sg-object
 * @param sg_ctx The sg_context to be free'd
 */
void _gles_sg_free( struct gles_sg_context *sg_ctx )
{
	MALI_DEBUG_ASSERT_POINTER( sg_ctx );

	if (sg_ctx->fresh_vertex_shader != NULL)
	{
		_mali_sys_free(sg_ctx->fresh_vertex_shader);
		sg_ctx->fresh_vertex_shader = NULL;
	}

	if (sg_ctx->fresh_fragment_shader != NULL)
	{
		_mali_sys_free(sg_ctx->fresh_fragment_shader);
		sg_ctx->fresh_fragment_shader = NULL;
	}

	free_shaders_and_programs(sg_ctx, MALI_FALSE);

	_mali_sys_free( sg_ctx );
}

void _gles_sg_state_init(struct gles_sg_context* sg_ctx)
{
	int i;

	MALI_DEBUG_ASSERT_POINTER( sg_ctx );

	for (i = 0; i < GLES1_MAX_CLIP_PLANES; ++i)
	{
		fragment_shadergen_encode(&sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_CLIP_PLANE_ENABLE(i), GL_FALSE);
	}

	/* default shading is smooth, so flatshading is initially false */
	fragment_shadergen_encode(&sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_FLATSHADED_COLORS, GL_FALSE);

	/* by setting the last active texture unit to ARG_PREVIOUS, the shader generator will use the last available texture unit.
	 * This state will never need to be changed, and as such adds les soverhead than calculating it per drawcall. */
	fragment_shadergen_encode(&sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_RESULT_COLOR_SOURCE, ARG_PREVIOUS_STAGE_RESULT);

	/* default color input is ARG_CONSTANT_COLOR as both lighting and color arrays are disabled by default */
	fragment_shadergen_encode(&sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE, ARG_CONSTANT_COLOR);

}


