/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_base.h"

#include "gles_sg_interface.h"
#include "gles_fb_interface.h"
#include "gles_gb_interface.h"
#include "gles1_state/gles1_get.h"
#include "gles1_state/gles1_enable.h"
#include "gles1_state/gles1_state.h"
#include "gles_config.h"
#include "shared/m200_quad.h"

#if EXTENSION_DRAW_TEX_OES_ENABLE

struct gles_draw_tex_state 
{
	/* A bunch of state can be temporarily disabled by a call to glDrawTex. 
	 * This struct tracks what was disabled, so that it can be re-enabled later */

	mali_bool restore_fog; 
	mali_bool restore_clip_plane0;
	mali_bool restore_lighting;
	mali_bool restore_color_array;

};


MALI_STATIC void disable_drawtex_state(struct gles_context* ctx, struct gles_draw_tex_state *work)
{
	GLboolean retval = GL_FALSE;
	GLenum err;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(work);

	/* clear all state */
	_mali_sys_memset(work, 0, sizeof(struct gles_draw_tex_state));

	/* If fog is enabled, disable it.
	 * This state must be disabled to ensure that the vertex color is constant. */
	MALI_IGNORE(err);
	err = _gles1_is_enabled( ctx, GL_FOG, &retval );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("isenabled failed")); 
	if(retval) 
	{
		err = _gles1_enable( ctx, GL_FOG, GL_FALSE );
		MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("enable failed")); 
		work->restore_fog = MALI_TRUE;
	}

	/* If clip plane 0 is enabled, disable it 
	 * This state must be disabled to ensure that the clip plane varying is not used. */
	err = _gles1_is_enabled( ctx, GL_CLIP_PLANE0, &retval );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("isenabled failed")); 
	if(retval) 
	{
		err = _gles1_enable( ctx, GL_CLIP_PLANE0, GL_FALSE );
		MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("enable failed")); 
		work->restore_clip_plane0 = MALI_TRUE;
	}

	/* If lighting is enabled, disable it. 
	 * This state must be enabled to ensure that the vertex color is constant */
	err = _gles1_is_enabled( ctx, GL_LIGHTING, &retval );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("isenabled failed")); 
	if(retval) 
	{
		err = _gles1_enable( ctx, GL_LIGHTING, GL_FALSE );
		MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("enable failed")); 
		work->restore_lighting = MALI_TRUE;
	}

	/* All enabled texture units will have a matching 4C-FP32 varying stream.
	 * The vertex shader typically copies and/or transforms the matching
	 * attribute streams into the varying streams. 
	 *
	 * This code should not disable any attribute streams. Since there is no 
	 * vertex shader, the data in those attribute streams just won't be read
	 * anyway. But the varying streams are set up, and exists. Later code will 
	 * fill in those varying streams with cropbox-based texture coordinates. 
	 *
	 * The exception here is the color array stream. 
	 * If this is set, the fragment shader is set to read colors from
	 * a varying instead of a constant uniform, which would force us to 
	 * fill out additional attribute streams. Simpler to just disable it. 
	 **/

	err = _gles1_is_enabled( ctx, GL_COLOR_ARRAY, &retval );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("isenabled failed")); 
	if(retval) 
	{
		err = _gles1_client_state( ctx, GL_COLOR_ARRAY, GL_FALSE );
		MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("enable failed")); 
		work->restore_color_array = MALI_TRUE;
	}


}

MALI_STATIC void restore_drawtex_state(struct gles_context* ctx, struct gles_draw_tex_state *work)
{
	GLenum err = GL_NO_ERROR; 
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(work);

	MALI_IGNORE(err);
	err = _gles1_enable( ctx, GL_FOG, work->restore_fog );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("enable failed")); 
	err = _gles1_enable( ctx, GL_CLIP_PLANE0, work->restore_clip_plane0 );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("enable failed")); 
	err = _gles1_enable( ctx, GL_LIGHTING, work->restore_lighting );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("enable failed")); 
	err = _gles1_client_state( ctx, GL_COLOR_ARRAY, work->restore_color_array);
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("enable failed")); 

}

/* works just like the normal atoi */
MALI_STATIC int local_atoi(const char* str)
{
	int retval = 0;
	int multiplier = 1;
	/* handle leading spaces */
	while(*str == ' ') str++;

	/* handle sign */
	if(*str == '-')
	{
		multiplier = -1;
		str++;
	}

	/* parse numbers */
	while(*str != '\0')
	{
		/* end at non-digits */
		if(*str < '0' || *str > '9') break;
	
		/* increase number */
		retval *= 10;
		retval += *str - '0';
		str++;
	}
	return retval * multiplier;
}

MALI_STATIC mali_err_code allocate_and_fill_in_varying_buffer(
                    mali_mem_pool* pool, 
                    struct gles_context* ctx, 
                    mali_addr *out_varying_address)
{
	struct gles_program_rendering_state* prs;
	u32 block_stride;
	void* varyings;
	u32 i;

	MALI_DEBUG_ASSERT_POINTER(pool);
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(out_varying_address);

	/* fetch the current PRS (this was set by the shader step done earlier */
	prs = ctx->state.common.current_program_rendering_state;
	MALI_DEBUG_ASSERT_POINTER(prs); /* if NULL, the shader step failed and we should not be here */
	block_stride = prs->binary.varying_streams.block_stride; /* bytewise stride between the varyign data from one vertex to the next */

	if(block_stride == 0) 
	{
		*out_varying_address = 0; /* no varyings, just encode a NULL address */
		return MALI_ERR_NO_ERROR;
	}

#ifndef NDEBUG  /* debug mode only verification code to ensure that only texcoord streams exist*/
	{
		struct bs_symbol* vsymbol;
		s32 flocation = -1;
		
		/* there should also be no other streams available */
		vsymbol = bs_symbol_lookup( prs->binary.varying_symbols, "var_FogDist", NULL, &flocation, NULL);
		if(vsymbol) MALI_DEBUG_ASSERT(flocation == -1,("FogDist stream should be disabled!")); 
		vsymbol = bs_symbol_lookup( prs->binary.varying_symbols, "var_ClipPlaneSignedDist", NULL, &flocation, NULL);
		if(vsymbol) MALI_DEBUG_ASSERT(flocation == -1,("ClipPlaneSignedDist stream should be disabled!")); 
		vsymbol = bs_symbol_lookup( prs->binary.varying_symbols, "var_PrimaryColor", NULL, &flocation, NULL);
		if(vsymbol) MALI_DEBUG_ASSERT(flocation == -1,("PrimaryColor stream should be disabled, since the shader state should be overridden to read the uniform color instead!")); 
		vsymbol = bs_symbol_lookup( prs->binary.varying_symbols, "var_PrimaryColorTwosided", NULL, &flocation, NULL);
		if(vsymbol) MALI_DEBUG_ASSERT(flocation == -1,("PrimaryColorTwosided stream should be disabled, since the shader state should be overridden to read the uniform color instead!")); 
	}
#endif

	/* allocate a buffer holding the varying memory.
	 * There is exactly 3 vertices, each needing prs->binary.varying_streams.block_stride bytes */
	varyings = _mali_mem_pool_alloc(pool, (3 * block_stride), out_varying_address);
	if( NULL == varyings) return MALI_ERR_OUT_OF_MEMORY;

	_mali_sys_memset(varyings, 0, (3 * block_stride)); /* clear it. */

	/* Just sanity checking: every one of the varying streams in the PRS are in FP32 precision mode,
	 * since they should all be 4-component texture coordinates, and texture coordinates are in fp32 precision. */ 
	MALI_DEBUG_ASSERT(prs->binary.varying_streams.count * 4 * sizeof(float) == prs->binary.varying_streams.block_stride, ("Incorrect block stride compared to stream count"));

	/* write to each of the varying streams. There are up to GLES_MAX_TEXTURE_UNITS + 2 streams to write to. 
	 * Each of the GLES texture units addressed will have a texcoord stream enabled. 
	 * Then there is the PrimaryColor stream, and the PrimaryColorTwosided streams; of which one or none may be in use */

	/* "Fill out" algorithm: 
	 *  - For each varying symbol not at location -1
	 *  - Find the matching stream by taking the location and divide by 4 (aka cells / vector). This gives a stream spec with offset, precision and component count. 
	 *  - Write the correct data to the stream at that offset + block_stride*vertex_id, for each of the three vertices. 
	 *
	 * Special care regarding color streams: 
	 *  - These are currently in FP16 format. Conversion required. 
	 *
	 * Special care regarding texture coordinates
	 *  - From the stream name, a texture unit can be found. 
	 *  - From the texture unit, a dominant texture target can be found
	 *  - If that texture is complete (already checked), use it to find the crop rect. 
	 *  - Otherwise use the disabled texture, where the crop rect is irrelevant (only 1 pixel anyway)
	 */

	MALI_DEBUG_ASSERT_POINTER(prs->binary.varying_symbols); /* cannot legally be NULL after a successful link */	
	for(i = 0; i < prs->binary.varying_symbols->member_count; i++)
	{
		struct bs_symbol* vsymbol = prs->binary.varying_symbols->members[i];
		u32 stream_id;
		struct bs_varying_stream_info* stream_spec;
		void* v0_ptr;
		void* v1_ptr;
		void* v2_ptr;
		u32 tex_unit_id;
		struct gles_texture_object* tex_obj;
		struct gles_texture_unit *texture_unit;
		mali_surface* surf; 

		MALI_DEBUG_ASSERT_POINTER(vsymbol);
		MALI_DEBUG_ASSERT_POINTER(vsymbol->name);

		if(vsymbol->location.fragment == -1) continue; /* skip the symbol due to not being in use */

		/* find the stream in question */
		stream_id = vsymbol->location.fragment  / 4; /* find stream index from symbol location. Divide by number of cells per stream */
		MALI_DEBUG_ASSERT((s32) (stream_id * 4) == vsymbol->location.fragment, ("All GLES1 streams must start at stream boundaries"));
		MALI_DEBUG_ASSERT(stream_id < prs->binary.varying_streams.count, ("Stream index out of bounds"));
		stream_spec = &prs->binary.varying_streams.info[stream_id];
		MALI_DEBUG_ASSERT_POINTER(stream_spec);

		/* find the location of each vertex */
		v0_ptr = (u8 *)varyings + stream_spec->offset;
		v1_ptr = (u8 *)v0_ptr + block_stride;
		v2_ptr = (u8 *)v1_ptr + block_stride;

	/*	fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE) */

		MALI_DEBUG_ASSERT( vsymbol->name &&
		                   vsymbol->name[0] == 'v' &&
		                   vsymbol->name[1] == 'a' &&
		                   vsymbol->name[2] == 'r' &&
		                   vsymbol->name[3] == '_' &&
		                   vsymbol->name[4] == 'T' &&
		                   vsymbol->name[5] == 'e' &&
		                   vsymbol->name[6] == 'x' &&
		                   vsymbol->name[7] == 'C' &&
		                   vsymbol->name[8] == 'o' &&
		                   vsymbol->name[9] == 'o' &&
		                   vsymbol->name[10] == 'r' &&
		                   vsymbol->name[11] == 'd', ("Only remaining possible enabled varying symbols are texcoord symbols"));
		MALI_DEBUG_ASSERT(stream_spec->component_size == 4, ("All texcoord streams are FP32"));
		MALI_DEBUG_ASSERT(stream_spec->component_count == 4, ("All texcoords have 4 components"));

		/* turn the symbol name into a tex unit ID */	
		tex_unit_id = local_atoi(vsymbol->name + 12);
		MALI_DEBUG_ASSERT(tex_unit_id < GLES_MAX_TEXTURE_UNITS, ("Unit out of range"));

		/* find object from unit */
		texture_unit = &ctx->state.common.texture_env.unit[tex_unit_id];
		MALI_DEBUG_ASSERT_POINTER(texture_unit);
		tex_obj = _gles1_texture_env_get_dominant_texture_object(texture_unit, NULL);
		MALI_DEBUG_ASSERT_POINTER(tex_obj); /* if it is enabled, it's allocated... not necessarily complete, but allocated */

		if( ! _gles_m200_is_texture_usable(tex_obj) ) continue; /* if it is not complete, don't care what the texcoords are (they will be 0) */
		surf = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, 0, 0); /* find base level mali_surface size */
		MALI_DEBUG_ASSERT_POINTER(surf); /* must exist, or completeness check should have failed */
			
		/* turn the unit into a texture object */

		/* From the spec: Each texcoord should be written as follows 
		    s = (Ucr + (X - Xs)*(Wcr/Ws)) / Wt
			t = (Vcr + (Y - Ys)*(Hcr/Hs)) / Ht
			r = 0
			q = 1

		   Wt and Ht is the texture width / height.
		   Ucr, Vcr, Hcr and Wcr is the four dimensions of the crop rectangle. 
		   X and Y is the fragment coordinate. 
		   Xs, Yx, Hs and Ws are all inputs to the glDrawTex function.
		*/

		/* Mali want normalized texture coordinates in all out GLES lookups. 
		 * This basically means:
		 * t0 = { Ucr/Wt,      Vcr/Ht,      0,1 } 
		 * t1 = { (Ucr+Wcr)/Wt, Vcr/Ht,      0,1 }
		 * t2 = { Ucr+Wcr/Wt,      (Vcr+Hcr)/Ht, 0,1 }
		 */
		{
			float Ucr = tex_obj->crop_rect[0];
			float Vcr = tex_obj->crop_rect[1];
			float Wcr = tex_obj->crop_rect[2];
			float Hcr = tex_obj->crop_rect[3];
			float UcrWt = Ucr / surf->format.width;
			float VcrHt = Vcr / surf->format.height;
			float UcrWcrWt = (Ucr + Wcr) / surf->format.width;
			float VcrHcrHt = (Vcr + Hcr) / surf->format.height;

			#define WRITE_FP32(ptr,x,y,z,w) \
				do {                        \
				((float*)(ptr))[0] = (x);   \
				((float*)(ptr))[1] = (y);   \
				((float*)(ptr))[2] = (z);   \
				((float*)(ptr))[3] = (w);   \
				} while(0)

			WRITE_FP32(v0_ptr, UcrWt,    VcrHt,    0.0, 1.0);
			WRITE_FP32(v1_ptr, UcrWcrWt, VcrHt,    0.0, 1.0);
			WRITE_FP32(v2_ptr, UcrWcrWt, VcrHcrHt, 0.0, 1.0);

			#undef WRITE_FP32
		}

	} /* for each varying symbol */


	return MALI_ERR_NO_ERROR;
}


MALI_STATIC mali_err_code allocate_and_fill_in_rsw(
                    mali_mem_pool* pool,
                    m200_rsw* rsw, 
                    mali_addr varying_address,
                    mali_addr *out_rsw_address)
{
	MALI_DEBUG_ASSERT_POINTER(pool);
	MALI_DEBUG_ASSERT_POINTER(rsw);
	MALI_DEBUG_ASSERT_POINTER(out_rsw_address);

	/* fill in the varying memory address into the RSW */
	__m200_rsw_encode( rsw, M200_RSW_VARYINGS_BASE_ADDRESS, varying_address );
	
	{
		u32 count = sizeof(m200_rsw) / sizeof(u32);
		u32* rsw_ptr_mirror = (u32*)rsw;

		m200_rsw* rsw_ptr = _mali_mem_pool_alloc(pool, sizeof(m200_rsw), out_rsw_address);
		u32* rsw_ptr_local = (u32*)rsw_ptr;

		if (NULL == rsw_ptr) return MALI_ERR_OUT_OF_MEMORY;

		/* copy the composed rsw to the gpu memory */
		while(count--)
		{
			*rsw_ptr_local++ = *rsw_ptr_mirror++;
		}
	}

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC mali_err_code allocate_and_fill_in_vertex_buffer(
                    mali_frame_builder* frame_builder, 
                    mali_mem_pool* pool, 
                    struct gles_context* ctx, 
                    float pretransform_x, float pretransform_y, float pretransform_z, 
                    float pretransform_width, float pretransform_height, 
                    mali_addr* out_vertex_address)
{
	float transformed_x, transformed_y, transformed_z, transformed_width, transformed_height;
	gles_framebuffer_object* fb_object;
	float *position; /* will point to a buffer holding 3 vertices, 4 components per vertex, fp32 */

	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	MALI_DEBUG_ASSERT_POINTER(pool);
	MALI_DEBUG_ASSERT_POINTER(out_vertex_address);
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* supersample scalefactor is 2 (in 16x output mode) or 1 (any other setup) */
	if( mali_statebit_get( &ctx->state.common, MALI_STATE_MODE_SUPERSAMPLE_16X ) )
	{
		transformed_x = pretransform_x * 2;
		transformed_y = pretransform_y * 2;
		transformed_z = pretransform_z * 2;
		transformed_width = pretransform_width * 2;
		transformed_height = pretransform_height * 2;
	}
	else 
	{
		transformed_x = pretransform_x;
		transformed_y = pretransform_y;
		transformed_z = pretransform_z;
		transformed_width = pretransform_width;
		transformed_height = pretransform_height;
	}
	
	/** 
	 * How to transform from GL coordinates to framebuilder coordinates: 
	 *  1: multiply GL coordinate by supersample scalefactor
	 *  2: flip y axis if the draw_flip_y flag is set. 
	 */

	fb_object = ctx->state.common.framebuffer.current_object;
	MALI_DEBUG_ASSERT_POINTER( fb_object );

    /* allocate on the frame pool */
	position = _mali_mem_pool_alloc(pool, (3 * 4 * sizeof(float)), out_vertex_address);
	if( NULL == position) return MALI_ERR_OUT_OF_MEMORY;

	/* fill in vertex coordinates for the quad */
	position[0] = transformed_x;
	position[2] = transformed_z;
	position[3] = 1.0f;

	position[4] = transformed_x+transformed_width;
	position[6] = transformed_z;
	position[7] = 1.0f;

	position[8] = transformed_x+transformed_width;
	position[10] = transformed_z;
	position[11] = 1.0f;

	if(!fb_object->draw_flip_y)
	{
		/* not flipping */
		position[1] = transformed_y;
		position[5] = transformed_y;
		position[9] = transformed_y+transformed_height;

	} else {
		/* flipping */
		float fbuilder_height = _mali_frame_builder_get_height(frame_builder); 
		position[1] = fbuilder_height - transformed_y;
		position[5] = fbuilder_height - transformed_y;
		position[9] = fbuilder_height - (transformed_y + transformed_height);
	}

	return MALI_ERR_NO_ERROR;
}


MALI_STATIC mali_err_code  setup_plbu_scissor_and_viewport( gles_context * ctx, mali_frame_builder* frame_builder)
{
	u32 scissor_boundaries[4];
	mali_bool scissor_closed = MALI_FALSE;
	mali_err_code err;

	/**
	 * Hot discussion topic: Should drawtex be subject to viewport clipping? 
	 * After all, matching calls to glDraw are. Consistency is nice. 
	 * But - glDrawTex is only supposed to run the fragment pipeline. 
	 * Viewport clipping is part of the vertex pipeline (GLES1 spec, image, page 27)
	 * So no viewport clipping should take place. 
	 */

	_gles_gb_extract_scissor_parameters( ctx, frame_builder, MALI_FALSE, scissor_boundaries, &scissor_closed );

	if(scissor_closed) return MALI_ERR_EARLY_OUT; /* if the scissor is zero sized, drop the drawcall */

	/* force the next normal drawcall to set up a new scissor/viewportbox, 
	 * which will probably be the same as the one it was before we started 
	 * messing with it here */
	mali_statebit_set(&ctx->state.common, MALI_STATE_SCISSOR_DIRTY );
	mali_statebit_set(&ctx->state.common, MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING );

	/* Setup a max viewport. It simply should not clip anything. */
	err = _mali_frame_builder_viewport(
		frame_builder,
		0, 
		0,
		GLES_MAX_RENDERBUFFER_SIZE,
		GLES_MAX_RENDERBUFFER_SIZE,
		NULL,
		NULL,
		0
	);  
	if( err != MALI_ERR_NO_ERROR ) return err;

	/* Setup a correct scissorbox. It should clip like a normal scissor */
	err = _mali_frame_builder_scissor(
		frame_builder,
		scissor_boundaries[0],
		scissor_boundaries[3],
		scissor_boundaries[1],
		scissor_boundaries[2],
		NULL,
		NULL,
		0
	);
	if( err != MALI_ERR_NO_ERROR ) return err;

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC mali_err_code _gles1_draw_tex_internal( gles_context * ctx, float x, float y, float z, float width, float height )
{
	/**
	 * There is a few things that needs to be done. 
	 *
	 * In the calling function, we disabled a bunch of vertex state. We need the 
	 * fragment shader's varying stream to only consist of texture coordinates. 
	 * Anything else must be constant (and thus not a stream). 
	 *
	 *    This means we now have a state where the following pp input streams are available. 
	 *    Since no vertex shader will be run, this function is responsible for filling in this memory. 
	 *      - gl_Position (naturally). Note that this is not part of the varying buffer, 
	 *        but is a separate buffer. 
	 *      - TexCoord0 through TexCoord7 may have one stream each, one for each unit enabled. 
	 *        Each texture stream must (in this function) be filled with texcoords based on the 
	 *        croprect in the texture object bound to the unit. 
	 *      - PrimaryColor is constant, but still a stream. Must be filled in. 
	 *      - PrimaryColorTwosided (same deal)
	 *
	 *    The following streams must never be enabled
	 *      - gl_PointSize (we're not drawing with points anyway, so this will always be disabled)
	 *      - FogDist (is always 0 in draw_tex calls, so fog must be disabled))
	 *      - ClipPlaneSignedDist (so, clip planes must always be disabled)
	 *
	 *    No other streams should be possible. 
	 *
	 * In this function, we do the following
	 *
	 *  - Fetch the framebuilder. We will need it for rendering
	 *  
	 *  - Create the shaders using the standard shader path setup. 
	 *    This will set up a vertex and fragment shader,
	 *    but we will only use the fragment shader in the draw_tex drawcall.
	 *
	 *  - Setup the drawtex-specific PLBU scissor and viewport. The scissor should
	 *    act like a normal scissorbox, but the viewport should never clip anything. 
	 *
	 *  - Create the RSW CPU copy. This should be done using the current fragment drawcall path. 
	 *    The RSW CPU copy will be saved to the gles context, statepushing style. 
	 *    So it is important that the state disabling/enabling code called outside this 
	 *    function also toggles the proper dirtyflags. 
	 *
	 *    Note: this operation may trigger GLES-specified incremental rendering operations. 
	 *
	 *  - Extract the frame pool. This must happen after the RSW extraction. 
	 *
	 *  - Fill in the varying and position buffers matching this drawcall. 
	 *    The texcoord data must be based on croprect info on the relevant textures bound on each unit.  
	 *    Then upload both items to mali memory and keep the address of each. 
	 *    Then modify the RSW to point to the varying buffer and upload that to mali memory as well. 
	 *
	 *  - Draw the texquad using the standard readback quad call using the two addresses. 
	 *
	 **/

	mali_frame_builder *frame_builder;
	mali_addr vertex_address;
	mali_addr varying_address;
	mali_addr rsw_address;
	mali_mem_pool *pool;
	mali_err_code error;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* fetch the framebuilder */
	frame_builder = _gles_get_current_draw_frame_builder(ctx);
	MALI_DEBUG_ASSERT_POINTER(frame_builder); /* if this is null, the check to fbo_setup earlier should have failed */

	/* create the shader pair */
	error = _gles_sg_init_draw_call( ctx, ctx->sg_ctx, GL_TRIANGLES); /* it's actually a readback quad, but whatever */
	if(error != MALI_ERR_NO_ERROR) return error;

	/* setup a drawtex specific scissor and viewport */
	error = setup_plbu_scissor_and_viewport(ctx, frame_builder);
	if(error != MALI_ERR_NO_ERROR) return error;

	/* setup RSW. CPU copy will be stored in the ctx->rsw_raster->mirror structure */
	error = _gles_fb_init_draw_call( ctx, GL_TRIANGLES ); 
	if(error != MALI_ERR_NO_ERROR) return error;

	/* extract the pool */
	pool = _mali_frame_builder_frame_pool_get( frame_builder );
	MALI_DEBUG_ASSERT_POINTER(pool); /* if this is null, frame_builder_write_lock failed, and that should be handled earlier */

	/* set up and fill in the position buffer. No memleak handling required since it's on the frame pool */
	error = allocate_and_fill_in_vertex_buffer(frame_builder, pool, ctx, x, y, z, width, height, &vertex_address);
	if(error != MALI_ERR_NO_ERROR) return error;
	
	/* set up and fill in the varying buffers. No memleak handling required since it's on the frame pool */
	error = allocate_and_fill_in_varying_buffer(pool, ctx, &varying_address);
	if(error != MALI_ERR_NO_ERROR) return error;

	/* set up and fill in the rsw. No memleak handling required since it's on the frame pool */
	error = allocate_and_fill_in_rsw(pool, &ctx->rsw_raster->mirror, varying_address, &rsw_address);
	if(error != MALI_ERR_NO_ERROR) return error;

	/* draw a quad using said RSW stored in the fb_ctx */
	return _mali200_draw_quad( frame_builder, vertex_address, rsw_address);

}



/* this is the function entrypoint */
GLenum _gles1_draw_tex_oes( gles_context * ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height )
{
	float clamp_z;
	struct gles_draw_tex_state draw_tex_state; /* holds all state we disable in order to draw with this function */	

	mali_err_code error = MALI_ERR_NO_ERROR;
	GLenum glerror = GL_NO_ERROR;

	/* clamp z to near/far plane according to spec */
	clamp_z = CLAMP(z, 0.0, 1.0);

	/* if width or height is negative, nothing should be drawn */
	if(width <= 0 || height <= 0) return GL_INVALID_VALUE; /* early out */

    /* Ready the bound framebuffer */
	glerror = _gles_fbo_internal_draw_setup( ctx );
	if ( glerror != GL_NO_ERROR ) return glerror;

	/* begin drawcall for real */
	error = _gles_drawcall_begin( ctx );
	if(error != MALI_ERR_NO_ERROR) 
	{
		MALI_DEBUG_ASSERT(error == MALI_ERR_OUT_OF_MEMORY, ("Only legal error code at this point"));
		return GL_OUT_OF_MEMORY;
	}

	/* disable a bunch of state, draw, and restore the state. Then check for errors */
	disable_drawtex_state(ctx, &draw_tex_state);
	error = _gles1_draw_tex_internal( ctx, x, y, clamp_z, width, height );
	restore_drawtex_state(ctx, &draw_tex_state);

	/* end drawcall regardless of internal operations outcome*/
	_gles_drawcall_end(ctx);

	if(error != MALI_ERR_NO_ERROR) /* check result from _gles1_draw_tex_internal */
	{
		if(error == MALI_ERR_EARLY_OUT) return GL_NO_ERROR;
		return GL_OUT_OF_MEMORY;
	}
	return GL_NO_ERROR;

}

#endif /* EXTENSION_DRAW_TEX_OES_ENABLE */
