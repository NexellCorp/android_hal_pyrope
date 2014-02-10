/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_program_rendering_state.h"
#include "../gles_gb_interface.h"
#include "../gles_fb_interface.h"

gles_program_rendering_state* _gles_program_rendering_state_alloc(void)
{
	gles_program_rendering_state* retval;
	int i;
	retval = _mali_sys_malloc(sizeof(struct gles_program_rendering_state));
	if( NULL == retval ) return NULL;

	/* set everything to 0, just to be safe. This is pretty redundant. */
	_mali_sys_memset(retval, 0, sizeof(struct gles_program_rendering_state));

	/* initialize program binary struct */
	__mali_program_binary_state_init(&retval->binary);

	/* set uniform location size */
	retval->gl_uniform_locations = NULL;
	retval->gl_uniform_location_size = 0;

	/* set locations */
	retval->viewport_uniform_vs_location = -1;
	retval->pointsize_parameters_uniform_vs_location = -1;
	retval->pointcoordscalebias_uniform_fs_location = -1;
	retval->derivativescale_uniform_fs_location = -1;
	retval->depthrange_uniform_vs_location_near = -1;
	retval->depthrange_uniform_vs_location_far = -1;
	retval->depthrange_uniform_vs_location_diff = -1;
	retval->depthrange_uniform_fs_location_near = -1;
	retval->depthrange_uniform_fs_location_far = -1;
	retval->depthrange_uniform_fs_location_diff = -1;
	retval->fragcoordscale_uniform_fs_location = -1;

	retval->depthrange_locations_fs_in_use      = MALI_FALSE;
	retval->flip_scale_bias_locations_fs_in_use = MALI_FALSE;

	retval->fb_prs = NULL;
	retval->gb_prs = NULL;

	_mali_sys_atomic_initialize(&retval->ref_count, 1);/* we have one reference to this object - its creator. */

	for(i = 0; i < GLES_VERTEX_ATTRIB_COUNT; i++) retval->attribute_remap_table[i] = -1;
	for(i = 0; i < MALIGP2_MAX_VS_INPUT_REGISTERS; i++) retval->reverse_attribute_remap_table[i] = -1;

	return retval;
}


/**
 * @brief Frees and deletes a program rendering state, deallocating all attached resources.
 * @param prs The program rendering state to delete
 * @note Refcount must be zero before calling this function. Call deref instead
 */
MALI_STATIC void _gles_program_rendering_state_free( gles_program_rendering_state *prs )
{
	MALI_DEBUG_ASSERT_POINTER(prs);

	/* free the binary state */
	__mali_program_binary_state_reset(&prs->binary);

	if( prs->fb_prs != NULL)
	{
		_gles_fb_free_program_rendering_state(prs->fb_prs);
		prs->fb_prs = NULL;
	}
	if( prs->gb_prs != NULL)
	{
		_gles_gb_free_program_rendering_state(prs->gb_prs);
		prs->gb_prs = NULL;
	}

	if( NULL != prs->gl_uniform_locations )
	{
		_mali_sys_free( prs->gl_uniform_locations );
		prs->gl_uniform_locations = NULL;
	}
	if( NULL != prs->fp16_cached_fs_uniforms )
	{
		_mali_sys_free( prs->fp16_cached_fs_uniforms );
		prs->fp16_cached_fs_uniforms = NULL;
	}

	if( prs->get_program_binary_data )
	{
		_mali_sys_free(prs->get_program_binary_data);
		prs->get_program_binary_data = NULL;
	}

	_mali_sys_free(prs);
}


void _gles_program_rendering_state_deref( gles_program_rendering_state *prs)
{
	u32 ref_count = 0;
	MALI_DEBUG_ASSERT_POINTER( prs );
	MALI_DEBUG_ASSERT((_mali_sys_atomic_get( &prs->ref_count ) > 0 ), ("ref_count is already 0,  should have been deleted by now!\n"));

	/* dereference */
	ref_count = _mali_sys_atomic_dec_and_return( &prs->ref_count );
	if ( ref_count > 0) return;

	_gles_program_rendering_state_free( prs );
	prs = NULL;


}

