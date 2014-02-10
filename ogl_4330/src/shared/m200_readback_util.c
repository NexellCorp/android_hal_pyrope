/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <regs/MALIGP2/mali_gp_core.h>
#include <regs/MALI200/mali_rsw.h>
#include "m200_readback_util.h"
#include "m200_gp_frame_builder_struct.h"
#include "m200_gp_frame_builder_inlines.h"
#include "m200_quad.h"



/**
 * Sets up a texture descriptor that uses the content of src_render_target's pixel buffer/surface as a texture
 */
MALI_STATIC void setup_readback_td( m200_td      *readback_td,
                                    mali_surface *src_render_target,
                                    mali_bool     texel_order_invert,
                                    mali_bool     red_blue_swap );

/**
 * Initialize an RSW that perform of a framebuffer readback onto a quad with default values
 *
 * @note This function does _not_ add a texture descriptor that reference the actual memory area
 *       to be read back to the RSW. This has to be done after this function has run in order to
 *       use the RSW for readbacks.
 * @see add_texture_descriptors_to_rsw
 *
 * @param readback_rsw                    A pointer to the rsw to initialize
 * @param target_type                     The type (color, depth, stencil) of the attachment/render_target_set to read back
 * @param readback_shader_address         The address of the shader in mali memory
 * @param first_shader_instruction_length The length of the first shader instruction
 * @param varying_block_address           The address of the varying_block in mali memory
 */

MALI_STATIC void _mali_readback_init_readback_rsw( m200_rsw                    *readback_rsw,
                                                   u32                          usage,
                                                   m200_texel_format            texel_format,
                                                   mali_addr                    readback_shader_address,
                                                   u32                          first_shader_instruction_length,
                                                   mali_addr                    varying_block_address )
{
	_mali_sys_memset( readback_rsw, 0x0, sizeof( m200_rsw ) );

	/* Shader parameters */
	__m200_rsw_encode( readback_rsw, M200_RSW_HINT_NO_SHADER_PRESENT,          0 );
	__m200_rsw_encode( readback_rsw, M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH, first_shader_instruction_length );
	__m200_rsw_encode( readback_rsw, M200_RSW_SHADER_ADDRESS,                  readback_shader_address);


	/* z near & far */
	__m200_rsw_encode( readback_rsw, M200_RSW_Z_NEAR_BOUND, 0x0);
	__m200_rsw_encode( readback_rsw, M200_RSW_Z_FAR_BOUND,  0xffff);

	/* Write to all subsamples */
	__m200_rsw_encode( readback_rsw, M200_RSW_MULTISAMPLE_WRITE_MASK,  0xf);

	/* 1 varying, 2-component fp32 */
	__m200_rsw_encode( readback_rsw, M200_RSW_PER_VERTEX_VARYING_BLOCK_SIZE, 1 );
	__m200_rsw_encode( readback_rsw, M200_RSW_VARYING0_FORMAT,               M200_VARYING_FORMAT_FP32_2C );

	/* No uniforms */
	__m200_rsw_encode( readback_rsw, M200_RSW_UNIFORMS_REMAPPING_TABLE_SIZE,              0 );
	__m200_rsw_encode( readback_rsw, M200_RSW_HINT_FETCH_SHADER_UNIFORMS_REMAPPING_TABLE, 0 );


	/* Init some of the texture descriptor values. Note that the M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_SIZE
	 * and M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_ADDRESS parts are _not_ set by this function and has to be
	 * added before RSW is used for readbacks  */
	__m200_rsw_encode( readback_rsw, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_LOG2_SIZE,  0 );
	__m200_rsw_encode( readback_rsw, M200_RSW_HINT_FETCH_TEX_DESCRIPTOR_REMAPPING_TABLE, 1);


	/* No alpha masking */
	__m200_rsw_encode( readback_rsw, M200_RSW_ALPHA_TEST_FUNC, M200_TEST_ALWAYS_SUCCEED );

	/* Blending disabled == Source*1 + Destination*0 */
	/* rgb */
	__m200_rsw_encode( readback_rsw, M200_RSW_RGB_BLEND_FUNC,            M200_BLEND_CsS_pCdD );
	__m200_rsw_encode( readback_rsw, M200_RSW_RGB_S_SOURCE_SELECT,       M200_SOURCE_0 );
	__m200_rsw_encode( readback_rsw, M200_RSW_RGB_S_SOURCE_1_M_X,        1);
	__m200_rsw_encode( readback_rsw, M200_RSW_RGB_S_SOURCE_ALPHA_EXPAND, 0);
	__m200_rsw_encode( readback_rsw, M200_RSW_RGB_D_SOURCE_SELECT,       M200_SOURCE_0 );
	__m200_rsw_encode( readback_rsw, M200_RSW_RGB_D_SOURCE_1_M_X,        0);
	__m200_rsw_encode( readback_rsw, M200_RSW_RGB_D_SOURCE_ALPHA_EXPAND, 0);

	/* alpha */
	__m200_rsw_encode( readback_rsw, M200_RSW_ALPHA_BLEND_FUNC,          M200_BLEND_CsS_pCdD );
	__m200_rsw_encode( readback_rsw, M200_RSW_ALPHA_S_SOURCE_SELECT,     M200_SOURCE_0 );
	__m200_rsw_encode( readback_rsw, M200_RSW_ALPHA_S_SOURCE_1_M_X,      1);
	__m200_rsw_encode( readback_rsw, M200_RSW_ALPHA_D_SOURCE_SELECT,     M200_SOURCE_0 );
	__m200_rsw_encode( readback_rsw, M200_RSW_ALPHA_D_SOURCE_1_M_X,      0);

	/* Color mask */
	if( MALI_OUTPUT_COLOR & usage )
	{
		__m200_rsw_encode( readback_rsw, M200_RSW_R_WRITE_MASK, 1 );
		__m200_rsw_encode( readback_rsw, M200_RSW_G_WRITE_MASK, 1 );
		__m200_rsw_encode( readback_rsw, M200_RSW_B_WRITE_MASK, 1 );
		__m200_rsw_encode( readback_rsw, M200_RSW_A_WRITE_MASK, 1 );
	}
	else __m200_rsw_encode( readback_rsw, M200_RSW_ABGR_WRITE_MASK, 0x0 );

	/* Depth test settings */
	__m200_rsw_encode( readback_rsw, M200_RSW_Z_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
	if ( MALI_OUTPUT_DEPTH & usage )
	{
		if(texel_format == M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8)
		{
			/* verbatim copy (used by S24Z8) also need a matching write. */
			__m200_rsw_encode( readback_rsw, M200_RSW_DST_ENABLE,              0x1); 
		}
		__m200_rsw_encode( readback_rsw, M200_RSW_SHADER_Z_REPLACE_ENABLE, 0x1);
		__m200_rsw_encode( readback_rsw, M200_RSW_Z_WRITE_MASK,            0x1);
	}
	else __m200_rsw_encode( readback_rsw, M200_RSW_Z_WRITE_MASK, 0x0);

	/* Stencil test settings. Always pass. */
	__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_FRONT_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
	__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_BACK_COMPARE_FUNC,  M200_TEST_ALWAYS_SUCCEED );

	if ( MALI_OUTPUT_STENCIL & usage )
	{
		__m200_rsw_encode( readback_rsw, M200_RSW_DST_ENABLE,                    0x1);
		__m200_rsw_encode( readback_rsw, M200_RSW_SHADER_STENCIL_REPLACE_ENABLE, 0x1);
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_FRONT_WRITE_MASK,      0xff );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_BACK_WRITE_MASK,       0xff );

		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS,  M200_STENCIL_OP_SET_TO_REFERENCE );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_FRONT_OP_IF_ZFAIL,  M200_STENCIL_OP_SET_TO_REFERENCE );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_FRONT_OP_IF_SFAIL,  M200_STENCIL_OP_SET_TO_REFERENCE );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_BACK_OP_IF_ZPASS,   M200_STENCIL_OP_SET_TO_REFERENCE );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_BACK_OP_IF_ZFAIL,   M200_STENCIL_OP_SET_TO_REFERENCE );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_BACK_OP_IF_SFAIL,   M200_STENCIL_OP_SET_TO_REFERENCE );
	}
	else
	{
		/* Leave the stencil buffer as-is */
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS,  M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_FRONT_OP_IF_ZFAIL,  M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_FRONT_OP_IF_SFAIL,  M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_BACK_OP_IF_ZPASS,   M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_BACK_OP_IF_ZFAIL,   M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( readback_rsw, M200_RSW_STENCIL_BACK_OP_IF_SFAIL,   M200_STENCIL_OP_KEEP_CURRENT );
	}

	/* set the varying base address (1 FP32 vec2 per vertex) */
	__m200_rsw_encode( readback_rsw, M200_RSW_VARYING0_FORMAT,               M200_VARYING_FORMAT_FP32_2C  );
	__m200_rsw_encode( readback_rsw, M200_RSW_VARYINGS_BASE_ADDRESS,         varying_block_address );
	__m200_rsw_encode( readback_rsw, M200_RSW_PER_VERTEX_VARYING_BLOCK_SIZE, (8/8) ); /* 8 bytes (2C FP32); given in 8 byte units */
}

/**
 * Creates one (or two) texture descriptor(s) that reference the contents of the given src_render_target.
 * @note It is the callers responsibility to delete the texture descriptor(s).
 *
 * @param pool        The frame pool on where to allocate memory. 
 * @param src_target  The surface to create a readback texture descriptor for
 * @param num_td      An output parameters that will be set to the number of texture descriptors that were
 *                    created by this function
 * @param [OUT] out_td_remap_table_addr The mali adress for the TD remap table. Output parameter.
 */
MALI_STATIC mali_err_code _mali_readback_create_readback_texture_descriptors( mali_mem_pool*           pool,
                                                                              mali_surface            *src_target,
																			  u32                      usage,
                                                                              u32                     *num_td,
																			  u32*                     td_remap_local_addr)
{
	u32              td_mem_size;
	u32             *td_local_ptr, *td_remap_local_ptr;
	u32              td_mali_addr;
	u32              num_td_local;

	MALI_IGNORE( usage );
	
#if HARDWARE_ISSUE_4849
	if ( (usage & MALI_OUTPUT_DEPTH) && M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8 == src_target->format.texel_format )
	{
		/* Need memory for 2 texture descriptor and a 2-entry remapping table, 64 byte alignment and readable by Mali200 */
		num_td_local = 2;
	}
	else
#endif
	{
		/* Only need memory for 1 texture descriptor and a 1-entry remapping table, 64 byte alignment and readable by Mali200 */
		num_td_local = 1;
	}

	td_mem_size = ( num_td_local * sizeof(m200_td) ) + ( num_td_local * sizeof(u32*) );
	td_local_ptr = _mali_mem_pool_alloc(pool, td_mem_size, &td_mali_addr);
	if(td_local_ptr == NULL) return MALI_ERR_OUT_OF_MEMORY;

	_mali_sys_memset(td_local_ptr, 0, td_mem_size); /* zeroing the TD memory; this cause all pointers to be NULL by default unless otherwise set */

	/* Point to the first TD remap entry */
	td_remap_local_ptr = (u32 *) ( (u32)td_local_ptr + num_td_local * sizeof( m200_td ) );
	*td_remap_local_addr = td_mali_addr + (num_td_local * sizeof( m200_td ));

	/* Set up TD using the render attachment as texture source */
	setup_readback_td( (m200_td *) td_local_ptr, src_target,
	                   src_target->format.reverse_order, src_target->format.red_blue_swap );

	/* Set the address of the TD in the remap table */
	td_remap_local_ptr[0] = td_mali_addr;

#if HARDWARE_ISSUE_4849
	if ( (usage & MALI_OUTPUT_DEPTH) && M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8 == src_target->format.texel_format )
	{
		u32 *td_local_depth_ptr;

		/* Set up the second texture descriptor required for depth attachements */
		MALI_DEBUG_ASSERT( 2 == num_td_local, ("Must have 2 TDs for VERBATIM32 depth attachments") );
		td_local_depth_ptr = (u32 *) ( (u32)td_local_ptr + sizeof( m200_td ) );
		setup_readback_td( (m200_td *) td_local_depth_ptr, src_target, MALI_TRUE, MALI_FALSE );
		td_remap_local_ptr[1] = td_mali_addr + sizeof( m200_td );
	}
#endif

	(*num_td) = num_td_local;
	return MALI_ERR_NO_ERROR;
}

/**
 * Adds the texture descriptor table located at td_address to the RSW. num_td contains how many texture descriptor
 * there are in the table.
 *
 * @param readback_rsw The rsw to add the texture descriptor table to
 * @param td_address   The address in mali memory to the texture descriptor table to
 * @param num_td       The number of texture descriptor in the texture descriptor table
 * @param write_mask   The multisample write mask to encode into the RSW
 */

MALI_STATIC void _mali_readback_add_texture_descriptors_to_rsw( m200_rsw  *readback_rsw,
                                                                mali_addr  td_address,
                                                                u32        num_td,
                                                                u32        write_mask )
{
	/* Setup RSW for 1 remapping table entry */
	__m200_rsw_encode( readback_rsw, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_SIZE,    num_td );
	__m200_rsw_encode( readback_rsw, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_ADDRESS, td_address );

	/* Subsamples to write to */
	__m200_rsw_encode( readback_rsw, M200_RSW_MULTISAMPLE_WRITE_MASK, write_mask);
}


MALI_STATIC void setup_readback_td( m200_td      *readback_td,
                                    mali_surface *src_render_target,
                                    mali_bool     texel_order_invert,
                                    mali_bool     red_blue_swap )
{
	m200_texel_format texel_format;

	MALI_DEBUG_ASSERT_POINTER( src_render_target );
	MALI_DEBUG_ASSERT_POINTER( readback_td );

	/* initializing the texture descriptor with the default values */
	m200_texture_descriptor_set_defaults( *readback_td );

	/* setting up the texture descriptor parameters that differ from the default */
	MALI_TD_SET_TEXTURE_FORMAT( *readback_td, M200_TEXTURE_FORMAT_NON_NORMALIZED ); 
	MALI_TD_SET_POINT_SAMPLE_MINIFY( *readback_td, 1 );
	MALI_TD_SET_POINT_SAMPLE_MAGNIFY( *readback_td, 1 );
	MALI_TD_SET_TEXEL_ORDER_INVERT( *readback_td, ( MALI_TRUE == texel_order_invert) ? 1 : 0 );
	MALI_TD_SET_TEXEL_RED_BLUE_SWAP( *readback_td, ( MALI_TRUE == red_blue_swap) ? 1 : 0 );

	texel_format =  src_render_target->format.texel_format;

	/* in the case of readback the correct mapping of S8Z24 surfaces is to read them in using the verbatim copy format */
	if( texel_format == M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8) texel_format = M200_TEXEL_FORMAT_VERBATIM_COPY32;

	MALI_DEBUG_ASSERT(M200_TEXEL_FORMAT_NO_TEXTURE != texel_format, ("Readback texture does not carry a texel format. It cannot be read from using Mali."));

	MALI_TD_SET_TEXEL_FORMAT( *readback_td, texel_format );
	if ( MALI_PIXEL_LAYOUT_LINEAR == src_render_target->format.pixel_layout )
	{
		MALI_TD_SET_TEXTURE_DIMENSION_S( *readback_td, src_render_target->format.pitch / (u32) __m200_texel_format_get_size(texel_format) );
	}
	else
	{
		MALI_TD_SET_TEXTURE_DIMENSION_S( *readback_td, src_render_target->format.width );
	}
	MALI_TD_SET_TEXTURE_DIMENSION_T( *readback_td, src_render_target->format.height );

	switch( src_render_target->format.pixel_layout )
	{
		case MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS:
			MALI_TD_SET_TEXTURE_ADDRESSING_MODE( *readback_td, M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED );
		break;
		case MALI_PIXEL_LAYOUT_LINEAR:
			MALI_TD_SET_TEXTURE_ADDRESSING_MODE( *readback_td, M200_TEXTURE_ADDRESSING_MODE_LINEAR );
		break;
		default:
			MALI_DEBUG_ASSERT( 0, ("incorrect layout: %d\n", src_render_target->format.pixel_layout ));
			MALI_TD_SET_TEXTURE_ADDRESSING_MODE( *readback_td, M200_TEXTURE_ADDRESSING_MODE_LINEAR );
	}

#define FROM_ADDR( x ) ( x >> 6 )
	MALI_TD_SET_MIPMAP_ADDRESS_0( *readback_td, FROM_ADDR( _mali_mem_mali_addr_get( src_render_target->mem_ref->mali_memory, src_render_target->mem_offset ) ) );
#undef FROM_ADDR
}

/**
 * Creates a texture descriptor that reference the given rendertarget's pixel data and encodes it in the RSW. This
 * means that a readback of the given rendertarget is added to the RSW.
 */
MALI_STATIC MALI_CHECK_RESULT mali_err_code add_rendertarget_readback_td_to_rsw( 
                                     mali_mem_pool           *pool,
                                     m200_rsw                *readback_rsw,
                                     mali_surface            *src,
                                     u32                      usage,
                                     u32                      write_mask )
{
		mali_err_code err;
		/* The texture descriptor to the texture containing the buffer to read back (and its size) */
		/* The number of texture descriptor needed for the readback. Will be 1 unless HARDWARE_ISSUE_
		 * 4849 is defined in which case it will be 2 */
		u32 num_td = 0;
		u32 td_remap_addr = 0;

		/* Create the texture descriptor(s) that are needed to perform a readback of the render target */
		err = _mali_readback_create_readback_texture_descriptors( pool, src, usage, &num_td, &td_remap_addr );
		if(err != MALI_ERR_NO_ERROR) return err;

		/* Add the readback texture descriptor to the RSW */
		_mali_readback_add_texture_descriptors_to_rsw( readback_rsw,
		                                               td_remap_addr,
		                                               num_td,
		                                               write_mask );
 
		return MALI_ERR_NO_ERROR;
}

/**
 * Initializes the texture coordinates for the quad. 
 * Texture coordinates are always mapping the entire texture. 
 *
 * @param varying_mem    A mali mem handle to the memory block in mali memory to initialize with texture the quad coordinates
 * @param x1             The x coordinate of the first corner of the quad
 * @param y1             The y coordinate of the first corner of the quad
 * @param x2             The x coordinate of the second corner of the quad
 * @param y2             The y coordinate of the second corner of the quad
 */
MALI_STATIC MALI_CHECK_RESULT mali_err_code alloc_pos_and_texcoords( mali_mem_pool* pool,
                                                                     float offset_x,
                                                                     float offset_y,
                                                                     float width,
                                                                     float height,
                                                                     mali_surface* surface,
											                         mali_addr *out_position_mem_addr,
											                         mali_addr *out_varying_mem_addr)
{
	float* varying_mem;
	float* position;


	varying_mem = _mali_mem_pool_alloc(pool, 3*2*sizeof(float), out_varying_mem_addr);
	if(varying_mem == NULL) return MALI_ERR_OUT_OF_MEMORY;

	varying_mem[0] = surface->format.width;
	varying_mem[1] = 0.0;

	varying_mem[2] = 0.0;
	varying_mem[3] = 0.0;

	varying_mem[4] = 0.0;
	varying_mem[5] = surface->format.height;

	position = _mali_mem_pool_alloc(pool, (3 * 4 * sizeof(float)), out_position_mem_addr);
	if( NULL == position) return MALI_ERR_OUT_OF_MEMORY;
		
	/* set quad position and texcoord data - flipped across the first and last vertex */
	position[0] = width+offset_x;
	position[1] = offset_y;
	position[2] = 0.0;
	position[3] = 1.0;

	position[4] = offset_x;
	position[5] = offset_y;
	position[6] = 0.0;
	position[7] = 1.0;

	position[8] = offset_x;
	position[9] = height+offset_y;
	position[10] = 0.0;
	position[11] = 1.0;

	return MALI_ERR_NO_ERROR;
}


/**
 * Allocate a shader binary on the frame pool. The shader is then set up to match the 
 * required readback. The shader binary will do a color, depth or stencil readback based on the usage parameter. 
 * If HW bugzilla 4849 is present, depth readbacks may require two cycles. 
 */
MALI_STATIC MALI_CHECK_RESULT mali_err_code alloc_fshader_on_pool( mali_mem_pool*       pool,
                                                                   mali_surface*        src, 
                                                                   u32                  usage,
                                                                   mali_addr           *out_shader_addr,
																   u32                 *out_size_of_first_instruction)
{
	/* The shader that will perform the readback, ie. texture the readback quad with the buffer
	 * texture (and its size). The type of shader to use depend on whether HW bugzilla 4849 is present */
	u8  const *readback_shader;
	u32        readback_shader_size;
	void*      pool_mapped_shader_mem;
	mali_addr  pool_mapped_shader_mali_addr;

	/*
	 * Shader used to readback stencil and color:
	 * VAR r1, var2[0]       # read texture coord
	 * TEX #var, sampler[0]  # texture look-up
	 * MOV r0, #tex          # texture to output
	 */
	static const u8 texturing_shader[]           = { 0xE6, 0x05, 0x00, 0x00, 0x20, 0x3C, 0x00, 0xF1, 0x00, 0x00, 0x00, 0x00,
	                                                 0x00, 0x10, 0x00, 0x39, 0x4E, 0x0E, 0x00, 0x00, 0xCF, 0x07, 0x00, 0x00 };
	static const u8 z16_shader[]                 = { 0xE6, 0x05, 0x00, 0x00, 0x20, 0x3C, 0x00, 0xF1, 0x00, 0x00, 0x00, 0x00,
	                                                 0x00, 0x10, 0x00, 0x39, 0x4E, 0x0E, 0x00, 0x00, 0xCF, 0x07, 0x00, 0x00 };
	static const u8 texturing_shader_alpha_one[] = { 0xE9, 0x31, 0x02, 0x00, 0x20, 0x3C, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
	                                                 0x00, 0x10, 0x00, 0x39, 0x4E, 0x0A, 0x00, 0x00, 0xC7, 0x17, 0x03, 0x30,
	                                                 0xE4, 0x03, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#if HARDWARE_ISSUE_4849
	/*
	 * Shader used to readback depth:
	 *
	 * precision mediump float;
	 *
	 * varying vec2 coord;
	 * uniform sampler2D Za;
	 * uniform sampler2D Zb;
	 *
	 * void main()
	 * {
	 *		gl_FragColor = vec4(texture2D(Zb, coord).y, texture2D(Za, coord).yyy);
	 * }
	 */
	static const u8 verbatim32_depth_texturing_shader[] =
	{
		0xC6, 0x11, 0x38, 0x02, 0x20, 0x3C, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x39,
		0x5E, 0x05, 0x00, 0x00, 0xC4, 0x07, 0x00, 0x00, 0xE7, 0x31, 0x00, 0x00, 0x20, 0x3C, 0x00, 0xFF,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x39, 0xA0, 0x0A, 0x00, 0x00, 0xCE, 0x97, 0x03, 0x00,
		0xE4, 0x03, 0x00, 0x00 	};
#endif
	
	MALI_IGNORE(usage); /* in case we won't use the parameter */

	/* select which shader to use */
	if(src->format.pixel_format == MALI_PIXEL_FORMAT_Z16)
	{
		readback_shader = z16_shader;
		readback_shader_size = sizeof(z16_shader);
	} else 
#if HARDWARE_ISSUE_4849
	if ( MALI_OUTPUT_DEPTH & usage )
	{
		readback_shader = verbatim32_depth_texturing_shader;
		readback_shader_size = sizeof(verbatim32_depth_texturing_shader);
	} else
#endif
	{
		if(src->format.alpha_to_one)
		{
			readback_shader = texturing_shader_alpha_one;
			readback_shader_size = sizeof(texturing_shader_alpha_one);
		}
		else
		{
			readback_shader = texturing_shader;
			readback_shader_size = sizeof(texturing_shader);
		}
	}

	/* allocate the shader on the frame pool */
	pool_mapped_shader_mem = _mali_mem_pool_alloc(pool, readback_shader_size, &pool_mapped_shader_mali_addr);
	if(pool_mapped_shader_mem == NULL) return MALI_ERR_OUT_OF_MEMORY;

	_mali_sys_memcpy(pool_mapped_shader_mem, readback_shader, readback_shader_size);
	
	*out_shader_addr = pool_mapped_shader_mali_addr;
	*out_size_of_first_instruction = texturing_shader[0] & 0x1F;


	return MALI_ERR_NO_ERROR;
}

/**
 * Allocate a RSW on the frame pool. 
 * The RSW is allocated and set up using the src, usage and varying parameters.
 * The mali_addr of the RSW is returned in the out_rsw_addr parameter. 
 *
 * The varying parameter contains all the per-varying data for the drawcall. 
 * For the drawcall being set up, this data is a 4-component texture coordinate
 * given in non-normalized texture coordinate values. Essentially it equals the 
 * position data.
 */
MALI_STATIC MALI_CHECK_RESULT mali_err_code alloc_rsw_on_pool( mali_mem_pool*       pool,
                                                               mali_surface*        src, 
                                                               u32                  usage,
															   u32                  mrt_writemask,
                                                               mali_addr            varying_addr,
															   mali_addr           *out_rsw_addr)
{
	mali_addr            shader_mem_mali_addr = 0; 
	void*                pool_mapped_rsw_mem;
	u32                  shader_first_instr_length = 0;
	m200_rsw             readback_rsw;
	mali_err_code        err;

	MALI_DEBUG_ASSERT_POINTER(pool);
	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_POINTER(out_rsw_addr);

	/* allocate the fragment shader */
	err = alloc_fshader_on_pool(pool, src, usage, &shader_mem_mali_addr, &shader_first_instr_length);
	if(err != MALI_ERR_NO_ERROR) return err;

	/* Initialize and encode the RSW to use for the readback draw call in local memory */
	_mali_readback_init_readback_rsw( &readback_rsw,
	                                   usage,
	                                   src->format.texel_format,
	                                   shader_mem_mali_addr,
	                                   shader_first_instr_length,
	                                   varying_addr);

	/* set up the TD matching this MRT count */
	err = add_rendertarget_readback_td_to_rsw( pool, &readback_rsw, src, usage, mrt_writemask );
	if(err != MALI_ERR_NO_ERROR) return err;

	/* copy the RSW onto the pool */ 
	pool_mapped_rsw_mem = _mali_mem_pool_alloc(pool, sizeof(m200_rsw), out_rsw_addr);
	if(pool_mapped_rsw_mem == NULL) return MALI_ERR_OUT_OF_MEMORY;

	_mali_sys_memcpy(pool_mapped_rsw_mem, &readback_rsw, sizeof(m200_rsw));

	return MALI_ERR_NO_ERROR;
}


MALI_STATIC MALI_CHECK_RESULT mali_err_code _mali_internal_frame_readback_specific(
												mali_frame_builder    *frame_builder,
												mali_surface *src, u32 usage,
												u16 offset_x,
												u16 offset_y,
												u16 width,
												u16 height )
{
	mali_err_code        err = MALI_ERR_NO_ERROR;
	u32                  mrt_count = 1;

	mali_mem_pool*       pool;

	mali_addr            vertex_mem_mali_addr;
	mali_addr            varying_mem_mali_addr;
	mali_addr            rsw_mem_mali_addr;
	
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( src );
	pool = _mali_frame_builder_frame_pool_get( frame_builder);

	if ( MALI_OUTPUT_DEPTH & usage )
	{
		/* to ensure the hierarchical Z is set to 0..1 in decision range
		 * we clear the z buffer to 1, and draw the depth quad at z = 0. 
		 * This ensures any z decision is based on depth values written 
		 * by the shaders alone since the hierarchical z buffers
		 * are in the range of 0 to 1. Clearing the Z buffer is only 
		 * legal since we are writing to the depth buffer anyway; and will 
		 * only work if this happens in the beginning of the frame, before 
		 * any depth tests occur. See bugzilla 10547 for reference */
		frame_builder->depth_clear_value = 0xFFFFFFUL; /* 6 * 4 = 24 bit */
	}

	/* allocate vertex and varying buffers, fill with data based on offset and dimension parameters */
	err = alloc_pos_and_texcoords( pool, offset_x, offset_y, width, height, src, &vertex_mem_mali_addr, &varying_mem_mali_addr);
	if(err != MALI_ERR_NO_ERROR) return err;

	/* Check if there are several render targets to read in (MRT is enabled) */
	if( usage & MALI_OUTPUT_MRT) mrt_count = 4; /* 4 targets is the only supported count for MRT mode */
	if ( mrt_count == 1 )
	{
		/* allocate the RSW */
		err = alloc_rsw_on_pool(pool, src, usage, 0xf, varying_mem_mali_addr, &rsw_mem_mali_addr);
		if(err != MALI_ERR_NO_ERROR) return err;
		/* Only one MRT so only one quad needs to be drawn */
		err = _mali200_draw_quad( frame_builder,
		                          vertex_mem_mali_addr,
		                          rsw_mem_mali_addr );
		if(err != MALI_ERR_NO_ERROR) return err;
	}
	else
	{
		/* Draw a quad per MRT render target with the readback RSW (rsw_index) and store the vertices in vertex_mem */
		int i;
		for ( i = 0; i < MALI200_WRITEBACK_UNIT_MRT_COUNT; ++i)
		{
			err = alloc_rsw_on_pool(pool, src, usage, 1<<i, varying_mem_mali_addr, &rsw_mem_mali_addr);
			if(err != MALI_ERR_NO_ERROR) return err;
			err = _mali200_draw_quad( frame_builder,
			                          vertex_mem_mali_addr,
			                          rsw_mem_mali_addr );
			if(err != MALI_ERR_NO_ERROR) return err;
		}
	}

	return err;
}

MALI_STATIC MALI_CHECK_RESULT mali_err_code _mali_internal_frame_readback(
												mali_frame_builder     *frame_builder,
												mali_surface *src, u32 usage,
												u16 offset_x,
												u16 offset_y,
												u16 width,
												u16 height )
{
	mali_err_code err;

	/* Current readback implementation support only depth, stencil OR color. So call the 
	 * underlying code up to three times, with only the proper flags set */
	if(usage & MALI_OUTPUT_DEPTH)
	{
		u32 temp_usage = usage;
		temp_usage &= ~(MALI_OUTPUT_STENCIL | MALI_OUTPUT_COLOR);
		err = _mali_internal_frame_readback_specific(frame_builder, src, temp_usage, offset_x, offset_y, width, height );
		if(err != MALI_ERR_NO_ERROR) return err;
	}


	if(usage & MALI_OUTPUT_STENCIL)
	{
		u32 temp_usage = usage;
		temp_usage &= ~(MALI_OUTPUT_COLOR | MALI_OUTPUT_DEPTH);
		err = _mali_internal_frame_readback_specific(frame_builder, src, temp_usage, offset_x, offset_y, width, height );
		if(err != MALI_ERR_NO_ERROR) return err;
	}


	if(usage & MALI_OUTPUT_COLOR)
	{
		u32 temp_usage = usage;
		temp_usage &= ~(MALI_OUTPUT_STENCIL | MALI_OUTPUT_DEPTH);
		err = _mali_internal_frame_readback_specific(frame_builder, src, temp_usage, offset_x, offset_y, width, height );
		if(err != MALI_ERR_NO_ERROR) return err;
	}

	return MALI_ERR_NO_ERROR;

}

MALI_EXPORT MALI_CHECK_RESULT mali_err_code _mali_frame_builder_readback(
														  struct mali_frame_builder     *frame_builder,
														  struct mali_surface *src,
														  u32 usage, 
														  u16 offset_x,
														  u16 offset_y,
														  u16 width,
														  u16 height )
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	
	return _mali_internal_frame_readback( frame_builder, src, usage, offset_x, offset_y, width, height );
}

