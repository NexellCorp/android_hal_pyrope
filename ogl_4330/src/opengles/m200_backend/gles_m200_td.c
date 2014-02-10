/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_m200_td.h"
#include "gles_fb_texture_object.h"

#include "shared/binary_shader/bs_symbol.h"
#include "base/mali_dependency_system.h"

#include "../gles_texture_object.h"

#include "base/mali_debug.h"
#include "util/mali_math.h"
#include "gles_fb_context.h"
#include "shared/mali_shared_mem_ref.h"
#include "shared/m200_gp_frame_builder_inlines.h"

#define FROM_ADDR( x ) ( x >> 6 )
/**
 * @brief Fills out a 'disabled' TD which will cause Mali200 to render black
 * @param dest The mali texture descriptor, in client memory, to fill out, a 512 bit block.
 * @param mali_td A pointer to the pre-allocated texture descriptor in mali memory. This pointer or
 *                its value will not be changed and is only necessary for a workaround for hardware
 *                issue 2363
 */
MALI_STATIC void _gles_m200_make_disabled_td( m200_td dest, u32 mali_address )
{
	MALI_IGNORE( mali_address );

	_gles_m200_td_set_defaults( dest );

#ifdef HARDWARE_ISSUE_2363
	/** setup dimensions - this is to get somewhat predictable addressing,
	*  M200_TEXEL_FORMAT_NO_TEXTURE is treated as 1bpp by mali200, but the
	*  result is never used.
	*/
	MALI_TD_SET_TEXTURE_DIMENSIONALITY( dest, M200_TEXTURE_DIMENSIONALITY_2D );
	MALI_TD_SET_TEXTURE_FORMAT( dest, M200_TEXTURE_FORMAT_NORMALIZED );
	MALI_TD_SET_TEXTURE_ADDRESSING_MODE( dest, M200_TEXTURE_ADDRESSING_MODE_LINEAR );

	MALI_TD_SET_TEXTURE_DIMENSION_S( dest, 1 );
	MALI_TD_SET_TEXTURE_DIMENSION_T( dest, 1 );
	MALI_TD_SET_TEXTURE_DIMENSION_R( dest, 1 );

	MALI_TD_SET_LAMBDA_LOW_CLAMP( dest, 0 );
	MALI_TD_SET_LAMBDA_HIGH_CLAMP( dest, 0 );
	MALI_TD_SET_MIPMAPPING_MODE( dest, M200_MIPMAP_MODE_NEAREST );

	/* Make sure M200 gets a valid address - the texture descriptor should be OK */
	mali_address = FROM_ADDR( mali_address );
	MALI_TD_SET_MIPMAP_ADDRESS_0( dest, mali_address );
#endif
}

#undef FROM_ADDR

mali_bool _gles_m200_is_texture_usable(gles_texture_object *tex_obj)
{
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	/* if there's no data in level 0 or the texture is incomplete, we should
	   disable the entire texture */
	if (
		(NULL == tex_obj->mipchains[0]) ||
		(NULL == tex_obj->mipchains[0]->levels[0]) ||
		( MALI_FALSE == _gles_texture_object_is_complete(tex_obj))
	)
	{
		return MALI_FALSE;
	}

	return MALI_TRUE;
}

/**
 * @brief updates one TD based on info from a tex_obj.
 * @param ctx the context
 * @param dst the TD where we place the result
 * @param tex_obj the object which contains our info
 * @param mali_td_memory_address
 * @param td_idx the texture plane this texture descriptor is associated with
 */
MALI_CHECK_RESULT mali_err_code _gles_m200_td_update(struct gles_context *ctx, m200_td *dst, gles_texture_object *tex_obj, u32 *mali_td_memory_address, u32 td_idx)
 {
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(dst);
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_POINTER(tex_obj->internal);
	MALI_DEBUG_ASSERT_POINTER(mali_td_memory_address);
	MALI_DEBUG_ASSERT(tex_obj->dirty == MALI_FALSE, ("dirty check should be done by now"));
	MALI_DEBUG_ASSERT(tex_obj->is_complete, ("The texture object should be complete"));

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_DEBUG_ASSERT_LEQ(td_idx, 2);
#else
	MALI_DEBUG_ASSERT_LEQ(td_idx, 0);
#endif

	/* remove old mali TD if present */
	if ( MALI_NO_HANDLE != tex_obj->internal->mali_tds[td_idx] )
	{
		_mali_mem_deref(tex_obj->internal->mali_tds[td_idx]);
		tex_obj->internal->mali_tds[td_idx] = MALI_NO_HANDLE;
	}

	/* create new TD */
	/* allocate persistent TD memory */
	tex_obj->internal->mali_tds[td_idx] = _mali_mem_alloc( ctx->base_ctx, sizeof(m200_td), M200_TD_ALIGNMENT, MALI_PP_READ ); /* comes with one reference, the texobj reference */
	if ( MALI_NO_HANDLE == tex_obj->internal->mali_tds[td_idx]) return MALI_ERR_OUT_OF_MEMORY;

	/* upload TD to mali memory */
	_mali_mem_write_cpu_to_mali(tex_obj->internal->mali_tds[td_idx], 0, *dst, sizeof(m200_td), sizeof(u32) );
	*mali_td_memory_address = _mali_mem_mali_addr_get( tex_obj->internal->mali_tds[td_idx], 0 );
	
	/* since the TD was invalidated, also invalidate the TD last used frame */
	tex_obj->internal->tds_last_used_frame = MALI_BAD_FRAME_ID;

	return MALI_ERR_NO_ERROR;
}

/**
 * @brief retrieves a complete texture object from a sampler, if present
 * @param state The current gles state
 * @param prog_binary_state the current bs program struct
 * @param sampler_id the id of the sampler we want to retrieve the object from
 * @return the texture object retrieved from the sampler, or NULL if it isn't a complete texture object. 
 */
gles_texture_object* _gles_get_texobj_from_sampler(gles_state *state, bs_program * prog_binary_state, int sampler_id)
{
	gles_texture_unit *texture_unit;
	gles_texture_object *texture_object;
	int texture_unit_id;

	MALI_DEBUG_ASSERT_RANGE((s32)sampler_id, 0, (s32)prog_binary_state->samplers.num_samplers-1);
	MALI_DEBUG_ASSERT_POINTER(state);
	MALI_DEBUG_ASSERT_POINTER(prog_binary_state);

	texture_unit_id = prog_binary_state->samplers.info[sampler_id].API_value;
		
	MALI_DEBUG_ASSERT_RANGE(texture_unit_id, 0, GLES_MAX_TEXTURE_UNITS-1);

	texture_unit = &state->common.texture_env.unit[texture_unit_id];
	MALI_DEBUG_ASSERT_POINTER(texture_unit);

	{
		enum gles_texture_target texture_target = GLES_TEXTURE_TARGET_INVALID;
		struct bs_symbol *symb = prog_binary_state->samplers.info[sampler_id].symbol;
		MALI_DEBUG_ASSERT_POINTER(symb);

		switch (symb->datatype)
		{
			case DATATYPE_SAMPLER:
				MALI_DEBUG_ASSERT(2 == symb->type.primary.vector_size, ("Not a 2D texture!"));
				texture_target = GLES_TEXTURE_TARGET_2D;
			break;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
			case DATATYPE_SAMPLER_EXTERNAL:
				texture_target = GLES_TEXTURE_TARGET_EXTERNAL;
			break;
#endif

			case DATATYPE_SAMPLER_CUBE:
				texture_target = GLES_TEXTURE_TARGET_CUBE;
			break;

			/*	case DATATYPE_SAMPLER_SHADOW: */
			default:
				MALI_DEBUG_ASSERT(0, ("unknown texture type (type: %d, vector size: %d)",
					symb->datatype,
					symb->type.primary.vector_size)
				);
				return NULL;
		}

		/* GLES 1.1 special: if the texture unit target is disabled, don't set the TD. 
		 * For GLES 2.0, all targets are always enabled. */
		if(texture_unit->enable_vector[texture_target] == GL_FALSE) return NULL; 

		texture_object = texture_unit->current_texture_object[texture_target];
	}

	MALI_DEBUG_ASSERT_POINTER(texture_object);

	/* Only texture objects that are valid will actually be used */
	if( ! _gles_m200_is_texture_usable(texture_object) ) return NULL;

	return texture_object;
}

MALI_CHECK_RESULT mali_err_code _gles_texture_used_in_drawcall( mali_frame_builder* frame_builder, gles_texture_object *texture_object, int num_descriptors)
{
	u32 i;
	int descriptor_index;

	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	MALI_DEBUG_ASSERT_POINTER(texture_object);
	MALI_DEBUG_ASSERT(texture_object->is_complete, ("texture object must be complete at this point"));
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_DEBUG_ASSERT_RANGE(num_descriptors, 1, 3);
#else
	MALI_DEBUG_ASSERT(1 == num_descriptors, ("Trying to assign multiple texture planes while the image external extension is disabled"));
#endif
	for(descriptor_index = 0; descriptor_index < num_descriptors; descriptor_index++ )
	{
		for( i = 0; i < texture_object->complete_levels; i++)
		{
			mali_err_code err;
			mali_bool regenerate_td = MALI_FALSE;
			struct gles_fb_texture_memory* texmem = _gles_fb_texture_object_get_memblock(texture_object->internal, i, descriptor_index, NULL);

			MALI_DEBUG_ASSERT_POINTER(texmem);
			MALI_DEBUG_ASSERT(_gles_fb_texture_memory_valid(texmem), ("All memory must be valid at this point; a constraint resolve should have happened otherwise."));

			/* add texmem to drawcall */
			err = _gles_fb_texture_memory_draw(texmem, frame_builder, &regenerate_td);
			if(err != MALI_ERR_NO_ERROR) 
			{
				/* We failed to rebuffer something, meaning the texmem block is currently without memory. 
				 * This must be treated as a constraint resolve case the next time around it is used for drawing */
				texture_object->internal->need_constraint_resolve = MALI_TRUE;
				return err;
			}

			if(regenerate_td) _gles_m200_td_level_change(texture_object, i);

		} /* for all miplevels */
	} /* for all planes */ 
	return MALI_ERR_NO_ERROR;
}

/**
 * Allocates a set of disabled TDs and assign them to the given TD remap table at slot 0-num_descriptors. 
 * @param ctx The GLES context
 * @param td_remap The TD remap table to add TDs to. 
 * @param num_descriptors The number of descriptors to add 
 **/
MALI_CHECK_RESULT mali_err_code _gles_add_disabled_texobj_to_remap_table(struct gles_context *ctx, u32* td_remap, int num_descriptors)
{
	int descriptor_index;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(td_remap);

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_DEBUG_ASSERT_RANGE(num_descriptors, 1, 3);
#else
	MALI_DEBUG_ASSERT_RANGE(num_descriptors, 1, 1);
#endif

	for ( descriptor_index = 0; descriptor_index < num_descriptors; descriptor_index++ )
	{
		m200_td td;
		void *mali_td = _mali_mem_pool_alloc(ctx->frame_pool, sizeof(m200_td), &td_remap[descriptor_index]);
		if (NULL == mali_td) return MALI_ERR_OUT_OF_MEMORY;
		_gles_m200_make_disabled_td( td, td_remap[descriptor_index] );
		_mali_sys_memcpy_cpu_to_mali_32(mali_td, td, sizeof(m200_td), sizeof(u32) );
	}

	/* debug asserts */
	for ( descriptor_index = 0; descriptor_index < num_descriptors; descriptor_index++ )
	{
		MALI_DEBUG_ASSERT(td_remap[descriptor_index] != 0, ("Must be a legal address"));
		MALI_DEBUG_ASSERT_ALIGNMENT(td_remap[descriptor_index], M200_TD_ALIGNMENT);
	}

	return MALI_ERR_NO_ERROR;
}

/**
 * Assigns a set of TDs and assign them to the given TD remap table at slot 0-num_descriptors. 
 * The TDs are taken from the input texture object. 
 * @param ctx The GLES context
 * @param frame_builder The framebuilder to add read dependencies on (used by renderable textures)
 * @param td_remap The TD remap table to add TDs to.
 * @param texture_object The texture object to get TDs from. 
 *        This function may update the mali_td field of the internal texture object as needed. 
 * @param num_descriptors The number of descriptors to add 
 **/
MALI_CHECK_RESULT mali_err_code _gles_add_texobj_to_remap_table(struct gles_context *ctx, mali_frame_builder* frame_builder, u32* td_remap, gles_texture_object *texture_object, int num_descriptors)
{
	int descriptor_index;
	mali_err_code err;
	mali_base_frame_id frameid;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(td_remap);
	MALI_DEBUG_ASSERT_POINTER(texture_object);
	MALI_DEBUG_ASSERT_POINTER(texture_object->internal);
	MALI_DEBUG_ASSERT(texture_object->is_complete, ("This function should only be called on complete texture objects"));

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_DEBUG_ASSERT_RANGE(num_descriptors, 1, 3);
#else
	MALI_DEBUG_ASSERT_RANGE(num_descriptors, 1, 1);
#endif
	frameid = _mali_frame_builder_get_current_frame_id(frame_builder); 

	/* Add the internal texture object deref to the frame callback-list. 
	 * This will cause subsequent updates to trigger CoWs where needed until the HW job is done, and prevents the object from being deleted prematurely. */
	if(frameid != texture_object->internal->to_last_used_frame)
	{
		err = _mali_frame_builder_add_callback(frame_builder, (mali_frame_cb_func)_gles_fb_texture_object_deref, (mali_frame_cb_param)texture_object->internal);
		if (MALI_ERR_NO_ERROR != err)
		{
			MALI_DEBUG_ASSERT(err == MALI_ERR_OUT_OF_MEMORY, ("inconsistent error code returned (got 0x%X, expected 0x%X)", err, MALI_ERR_OUT_OF_MEMORY));
			return err;
		}
		_gles_fb_texture_object_addref(texture_object->internal);
		texture_object->internal->to_last_used_frame = frameid;
	}

	/* If the texture object has been modified since the last drawcall, 
	 * we may need to upload a new texture descriptor. This code block 
	 * deals with last minute constraint resolving, then uploads the new TD. 
	 */
	if( texture_object->dirty || texture_object->internal->need_constraint_resolve || texture_object->internal->using_renderable_miplevels )
	{
		int descriptor_index = 0;
		
		/* First do a normal constraint resolve. 
		 * This path is only taken if the user uploaded something unreasonable,
		 * but which passed completeness testing. */
		if( texture_object->internal->need_constraint_resolve )
		{
			err = _gles_texture_object_resolve_constraints(texture_object);
			if(MALI_ERR_NO_ERROR != err)
			{
				MALI_DEBUG_ASSERT(err == MALI_ERR_OUT_OF_MEMORY, ("unexpected error code"));
				return err;
			}
		}
	
		/* Next, flag the memory as being used by a drawcall. Normally this state is never cleared. 
		 * For renderable textures, this must be done every drawcall. For normal textures
		 * this is only done at the first drawcall, when we're creating the TD. */ 
		if( texture_object->internal->using_renderable_miplevels  || texture_object->dirty )
		{
			err = _gles_texture_used_in_drawcall( frame_builder, texture_object, num_descriptors);
			if (MALI_ERR_NO_ERROR != err) return err;
		}	

		/* After handling all constraint flags, recreate all the TDs and clear the dirtyflags*/ 
		texture_object->dirty = MALI_FALSE ;
		for(descriptor_index = 0; descriptor_index < num_descriptors; descriptor_index++)
		{
			err = _gles_m200_td_update( ctx, &texture_object->internal->tds[descriptor_index], texture_object, &td_remap[descriptor_index], descriptor_index);
			if(err != MALI_ERR_NO_ERROR)
			{
				texture_object->dirty = MALI_TRUE;
				return err;
			}
		}

	}

	/* The mali_TD copy of the TD is residing in Mali memory. Depending on the result from the previous code block, 
	 * it may or may not have been replaced. To ensure that all drawcalls retain the correct TD, the TD must be addref'ed individually. 
	 * Once addref'ed, the adress of the TDs can be written into the TD remap table. 
	 */
	for(descriptor_index = 0; descriptor_index < num_descriptors; descriptor_index++)
	{
		if( (texture_object->dirty || texture_object->internal->need_constraint_resolve || texture_object->internal->using_renderable_miplevels) 
		    || frameid != texture_object->internal->tds_last_used_frame)
		{
			MALI_DEBUG_ASSERT_POINTER(texture_object->internal->mali_tds[descriptor_index]);
			err = _mali_frame_builder_add_callback(frame_builder, (mali_frame_cb_func)_mali_mem_deref, (mali_frame_cb_param)texture_object->internal->mali_tds[descriptor_index]);
			if(err != MALI_ERR_NO_ERROR) return err;
			_mali_mem_addref(texture_object->internal->mali_tds[descriptor_index]);
		}
		td_remap[descriptor_index] = _mali_mem_mali_addr_get( texture_object->internal->mali_tds[descriptor_index], 0 );
	}
	if(frameid != texture_object->internal->tds_last_used_frame)
	{
		texture_object->internal->tds_last_used_frame = frameid;
	}

	/* debug asserts */
	for ( descriptor_index = 0; descriptor_index < num_descriptors; descriptor_index++ )
	{
		MALI_DEBUG_ASSERT(td_remap[descriptor_index] != 0, ("Must be a legal address"));
		MALI_DEBUG_ASSERT_ALIGNMENT(td_remap[descriptor_index], M200_TD_ALIGNMENT);
	}

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC void _gles_m200_td_update_grad_table( gles_state *state, bs_program *prog_binary_state, int sampler_index, gles_texture_object *texture_object )
{
	gles_program_rendering_state* rs;
	s32 offset;
	int w = 1;
	int h = 1;

	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( prog_binary_state );

	rs = state->common.current_program_rendering_state;
	MALI_DEBUG_ASSERT_POINTER( rs );

	offset = prog_binary_state->samplers.info[sampler_index].grad_metadata_index;
	MALI_DEBUG_ASSERT_RANGE(offset, -1, (int)prog_binary_state->fragment_shader_uniforms.cellsize);

	if(offset == -1) return;

	if ( NULL != texture_object && NULL != texture_object->mipchains[0] && NULL != texture_object->mipchains[0]->levels[0] )
	{
		w = texture_object->mipchains[0]->levels[0]->width;
		h = texture_object->mipchains[0]->levels[0]->height;
	}

	prog_binary_state->fragment_shader_uniforms.array[offset+0] = w;
	rs->fp16_cached_fs_uniforms[offset+0] = _gles_fp32_to_fp16(w);

	prog_binary_state->fragment_shader_uniforms.array[offset+1] = h;
	rs->fp16_cached_fs_uniforms[offset+1] = _gles_fp32_to_fp16(h);
}

MALI_STATIC void _gles_set_single_fragment_uniform_yuv( struct gles_common_state* c_state,
                                                        struct gles_program_rendering_state* prs,
                                                        int loc, float val)
{
	bs_program *program_bs;

	MALI_DEBUG_ASSERT_POINTER( c_state );
	MALI_DEBUG_ASSERT_POINTER( prs );

	program_bs = &prs->binary;
	MALI_DEBUG_ASSERT_POINTER( program_bs );

	MALI_DEBUG_ASSERT_RANGE(loc, 0, (int)program_bs->fragment_shader_uniforms.cellsize - 1);
	if (program_bs->fragment_shader_uniforms.array[loc] != val)
	{
		program_bs->fragment_shader_uniforms.array[loc] = val;
		prs->fp16_cached_fs_uniforms[loc] = _gles_fp32_to_fp16(val);
		mali_statebit_set( c_state, MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING );
	}
}


MALI_STATIC void _gles_m200_update_yuv_uniforms( gles_state *state, bs_program *prog_binary_state, int sampler_index, gles_texture_object *texture_object )
{
	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( prog_binary_state );

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if ( texture_object != NULL && GLES_TEXTURE_TARGET_EXTERNAL == texture_object->dimensionality )
	{
		struct gles_common_state* c_state = &state->common;
		gles_program_rendering_state* rs = state->common.current_program_rendering_state;
		s32 offset = prog_binary_state->samplers.info[sampler_index].YUV_metadata_index;

		/* The following variable are various components we need to calculate to set up the YUV uniforms */
		float Kb, Kr;
		float ay, by, ac, bc;
		float Ab = 0.0f;
		float Ac = 1.0f;
		float As = 1.0f;
		float f0, f1, f2, f3, f4;
		float e1 = 0.0f;
		float e2 = 0.0f;
		float e3 = 0.0f;
		float e4 = 0.0f;
		float dr, dg, db;
		float q1, q2, q3, q4;

		/**
		 * The sampler, will calculate the RGBA value as:
		 *
		 * R = y*abs(f0)		+ u*e1 + v*f2 + dr
		 * G = max(y*f0, 0.0)	+ u*f3 + v*f4 + dg
		 * B = max(y*f0, 0.0)	+ u*f1 + v*e3 + db
		 * A = 1.0
		 *
		 * Where y is red on the first texture descriptor,
		 * u is green on the second texture descriptor and
		 * v is blue on the third texture descriptor.
		 */

		if(offset == -1) return;

#if EXTENSION_VIDEO_CONTROLS_ARM_ENABLE
		Ab = texture_object->yuv_info.brightness;
		Ac = texture_object->yuv_info.contrast;
		As = texture_object->yuv_info.saturation;
#endif

		if ( GLES_COLORSPACE_BT_601 == texture_object->yuv_info.colorspace )
		{
			Kb = 0.114f;
			Kr = 0.299f;
		}
		else if ( GLES_COLORSPACE_BT_709 == texture_object->yuv_info.colorspace )
		{
			Kb = 0.0722f;
			Kr = 0.2126f;
 		}
		else
		{
			Kb = 0.114f;
			Kr = 0.299f;
			MALI_DEBUG_ASSERT( MALI_FALSE, ( "invalid colorspace %d", texture_object->yuv_info.colorspace ) );
		}

		if ( GLES_COLORRANGE_FULL == texture_object->yuv_info.colorrange )
		{
			ay =  1.0f;
			by =  0.0f;
			ac =  1.0f;
			bc = -0.5f;
		}
		else if ( GLES_COLORRANGE_REDUCED == texture_object->yuv_info.colorrange )
		{
			ay = 1.164383562f;   /*  255.f / (235.f - 16.f); */
			by = -0.062745098f;  /* -16.f / 255.f; */
			ac = 1.138392857f;   /*  255.f / (240.f - 16.f);*/
			bc = -0.501960784f;  /* -128.f / 255.f; */
		}
		else
		{
			ay =  1.0f;
			by =  0.0f;
			ac =  1.0f;
			bc = -0.5f;
			MALI_DEBUG_ASSERT( MALI_FALSE, ( "invalid colorrange %d", texture_object->yuv_info.colorrange ) );
		}

		if(MALI_FALSE == texture_object->yuv_info.is_rgb)
		{
			const float one_minus_Kb = 1.0f - Kb;
			const float one_minus_Kr = 1.0f - Kr;
			const float one_minus_Kb_minus_Kr = one_minus_Kb - Kr;
			const float Ac_As_ac = Ac * As * ac;
			float f0_by;
			const float bc_plus_one_minus_Ac_half_plus_Ab = bc + (1.0f - Ac) / 2.0f + Ab;

			q1 = one_minus_Kb + one_minus_Kb;
			q2 = one_minus_Kr + one_minus_Kr;
			q3 = -q1 * Kb / one_minus_Kb_minus_Kr;
			q4 = -q2 * Kr / one_minus_Kb_minus_Kr;

			f0 = Ac * ay;
			f1 = Ac_As_ac * q1;
			f2 = Ac_As_ac * q2;
			f3 = Ac_As_ac * q3;
			f4 = Ac_As_ac * q4;

			f0_by = f0 * by;

			dr = f0_by + f2 * bc_plus_one_minus_Ac_half_plus_Ab;
			dg = f0_by + (f3 + f4) * bc_plus_one_minus_Ac_half_plus_Ab;
			db = f0_by + f1 * bc_plus_one_minus_Ac_half_plus_Ab;
		}
		else
		{
			f0 = -1.0;
			f1 = f2 = f4 = 0.0f;
			f3 = 1.0f;
			e3 = 1.0f;
			dr = dg = db = 0.0f;
		}

		_gles_set_single_fragment_uniform_yuv(c_state, rs, 0 + offset, dr );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 1 + offset, dg );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 2 + offset, db );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 3 + offset, f0 );

		_gles_set_single_fragment_uniform_yuv(c_state, rs, 4 + offset, e1 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 5 + offset, f3 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 6 + offset, f1 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 7 + offset, e2 );

		_gles_set_single_fragment_uniform_yuv(c_state, rs, 8 + offset, f2 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 9 + offset, f4 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 10 + offset, e3 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 11 + offset, e4 );

		if(texture_object->yuv_info.is_rgb)
		{
			_gles_set_single_fragment_uniform_yuv(c_state, rs, 12 + offset, 0 );
		} else {
			_gles_set_single_fragment_uniform_yuv(c_state, rs, 12 + offset, 1 );
		}
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 13 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 14 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 15 + offset, 0 );

	} else 
#endif
	{
		struct gles_common_state* c_state = &state->common;
		gles_program_rendering_state* rs = state->common.current_program_rendering_state;
		s32 offset = prog_binary_state->samplers.info[sampler_index].YUV_metadata_index;
		if(offset == -1) return;

		_gles_set_single_fragment_uniform_yuv(c_state, rs, 0 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 1 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 2 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 3 + offset, -1 );

		_gles_set_single_fragment_uniform_yuv(c_state, rs, 4 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 5 + offset, 1 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 6 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 7 + offset, 0 );

		_gles_set_single_fragment_uniform_yuv(c_state, rs, 8 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 9 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 10 + offset, 1 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 11 + offset, 0 );

		_gles_set_single_fragment_uniform_yuv(c_state, rs, 12 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 13 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 14 + offset, 0 );
		_gles_set_single_fragment_uniform_yuv(c_state, rs, 15 + offset, 0 );

	}

}

mali_err_code _gles_m200_update_texture_descriptors( struct gles_context *ctx, gles_fb_context *fb_ctx, mali_frame_builder *frame_builder, gles_state *state, bs_program *prog_binary_state )
{
	u32* td_remap;
	int num_samplers;
	int td_remap_table_size;
	u32 td_remap_table_address;
	int i;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( fb_ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( prog_binary_state );

	num_samplers = prog_binary_state->samplers.num_samplers;

	/* If no samplers used, early out. We will not need any sampler array at all. */
	if( num_samplers == 0 )
	{
		fb_ctx->current_td_remap_table_address  = 0;
		return MALI_ERR_NO_ERROR;
	}

	/* Allocate the TD remap table */
 	td_remap_table_size = prog_binary_state->samplers.td_remap_table_size * sizeof( u32 * );

	td_remap = (u32*)_mali_mem_pool_alloc(ctx->frame_pool, td_remap_table_size, &td_remap_table_address);
	if (NULL == td_remap) return MALI_ERR_OUT_OF_MEMORY;
	_mali_sys_memset( td_remap, 0, td_remap_table_size );

	/**
	 * Map all samplers onto the slots of the td remap table and make them match their proper TD.
	 * Create/update TD's as required.
	 * Note that there is a 1:1 mapping between sampler ID and location in the TD remap table.
	 */
	for(i = 0; i < num_samplers; i++)
	{
		gles_texture_object *texture_object;
		mali_err_code err;
		int num_descriptors = 1;

		/* Get texture object and add it to table slot i.
		 * If the texobj is present and valid, it will be returned. 
		 * If the texobj is invalid or incomplete, this function will return NULL. */
		texture_object = _gles_get_texobj_from_sampler(state, prog_binary_state, i);

		if ( MALI_TRUE == prog_binary_state->samplers.info[i].YUV )
		{
			_gles_m200_update_yuv_uniforms(state, prog_binary_state, i, texture_object);
		}

		if( MALI_TRUE == prog_binary_state->samplers.info[i].is_grad )
		{
			_gles_m200_td_update_grad_table(state, prog_binary_state, i, texture_object);
		}

		if( NULL == texture_object ) 
		{
			err = _gles_add_disabled_texobj_to_remap_table(ctx, td_remap +
			                                              prog_binary_state->samplers.info[i].td_remap_table_index, 
														  num_descriptors);			 	
			if(err != MALI_ERR_NO_ERROR) return err;				
		} else {

			#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
			if(texture_object->dimensionality == GLES_TEXTURE_TARGET_EXTERNAL && MALI_FALSE == texture_object->yuv_info.is_rgb) num_descriptors = 3;	
			#endif

			MALI_DEBUG_ASSERT_POINTER(texture_object);
			MALI_DEBUG_ASSERT(texture_object->is_complete, ("_gles_get_texobj_from_sampler should never return an incomplete texture"));

			err = _gles_add_texobj_to_remap_table(ctx, frame_builder, td_remap +
			                                      prog_binary_state->samplers.info[i].td_remap_table_index,
			                                      texture_object, num_descriptors);
			if(err != MALI_ERR_NO_ERROR) return err;
		} /* end if texobj == NULL */
	} /* end for all samplers */

	/* save remap table and deref old remap table */
	fb_ctx->current_td_remap_table_address  = td_remap_table_address;

	return MALI_ERR_NO_ERROR;
}

void _gles_m200_td_dimensions( gles_texture_object *tex_obj, u32 width, u32 height, u32 stride, u32 bpp, mali_bool is_linear, u32 plane)
{
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_DEBUG_ASSERT_LEQ(plane, 2);
#else
	MALI_DEBUG_ASSERT_LEQ(plane, 0);
#endif

#if defined(USING_MALI400) || defined(USING_MALI450)
	MALI_TD_SET_TEXTURE_STRIDE( tex_obj->internal->tds[plane], stride );
	MALI_TD_SET_TEXTURE_DIMENSION_S( tex_obj->internal->tds[plane], width );
#endif
#if USING_MALI200
	/*
	 * Mali 200 does not support the stride in linear textures.
	 * Setting the stride as width, a black column will be sampled at right,
	 * but the texture will be used properly..
	 * Setting width as width, the texture will be sampled in a bad way and
	 * there will be no workaroud for it.
	 */
	MALI_TD_SET_TEXTURE_DIMENSION_S( tex_obj->internal->tds[plane], (MALI_TRUE==is_linear) ? stride/bpp : width );
#else
	MALI_IGNORE(bpp);
#endif
	MALI_TD_SET_TEXTURE_DIMENSION_T( tex_obj->internal->tds[plane], height );
	MALI_TD_SET_TEXTURE_DIMENSION_R( tex_obj->internal->tds[plane], 1 );

	MALI_TD_SET_TEXTURE_TOGGLE_STRIDE( tex_obj->internal->tds[plane], is_linear );

}

