/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_geometry_backend.h"

#include "mali_system.h"

#include "base/mali_debug.h"
#include "base/mali_memory.h"

#include "regs/MALIGP2/mali_gp_vs_config.h" /* needed for stream-config */
#include "regs/MALIGP2/mali_gp_plbu_config.h" /* needed for stream-config */
#include "shared/binary_shader/bs_object.h"
#include "shared/binary_shader/bs_symbol.h"
#include "shared/m200_gp_frame_builder_inlines.h"

#include "../gles_config.h"
#include "../gles_context.h"
#include "../m200_backend/gles_fb_context.h"
#include "gles_geometry_backend_context.h"
#include "../gles_gb_interface.h"

void _gles_gb_free( gles_context *ctx )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	if ( ctx->gb_ctx == NULL ) return;

	_mali_sys_free( ctx->gb_ctx );
	ctx->gb_ctx = NULL;
}


mali_err_code _gles_gb_init( gles_context *ctx )
{
	gles_gb_context *gb_ctx;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	/* if there already exists a backend context it is deleted before we init the new one */
	if ( ctx->gb_ctx != NULL ) _gles_gb_free( ctx );

	gb_ctx = (gles_gb_context *) _mali_sys_malloc( sizeof( gles_gb_context ));
	if( NULL == gb_ctx ) return MALI_ERR_OUT_OF_MEMORY;

	_mali_sys_memset( (void *) gb_ctx, 0x0, sizeof( gles_gb_context ));

	gb_ctx->base_ctx = ctx->base_ctx;
	gb_ctx->api_version = ctx->api_version;

	gb_ctx->const_reg_addr = 0;
	gb_ctx->uniforms_reg_addr= 0;

	ctx->gb_ctx = gb_ctx;
	MALI_SUCCESS;
}

mali_err_code MALI_CHECK_RESULT _gles_gb_alloc_position( gles_context *ctx,
							    mali_mem_pool* pool,
							    mali_addr *out_position_mem_addr)
{
	float* position;
	float z;

	z = CLAMP( FTYPE_TO_FLOAT( ctx->state.common.framebuffer_control.depth_clear_value ), 0.f, 1.f );

	position = _mali_mem_pool_alloc(pool, (3 * 4 * sizeof(float)), out_position_mem_addr);
	if( NULL == position) return MALI_ERR_OUT_OF_MEMORY;

	/* set quad position */
	position[0] = 0;
	position[1] = 0;
	position[2] = z;
	position[3] = 1.0;

	position[4] = GLES_MAX_RENDERBUFFER_SIZE;
	position[5] = 0;
	position[6] = z;
	position[7] = 1.0;

	position[8] = 0;
	position[9] = GLES_MAX_RENDERBUFFER_SIZE;
	position[10] = z;
	position[11] = 1.0;


	return MALI_ERR_NO_ERROR;
}


/**
 *  Interface function for the geometry backend
 * Currently does not do anything
 */
void _gles_gb_finish( struct gles_context *ctx )
{
	MALI_IGNORE(ctx);
}

