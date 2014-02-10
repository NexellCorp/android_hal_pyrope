/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */ 

#ifdef MALI_TEST_API
	#include <base/mali_macros.h> /* For MALI_IMPORT/EXPORT */
	#include <base/mali_test.h>
#endif
#include "../unit_framework/suite.h"
#include "base_test_pp_util.h"

#include <base/mali_types.h>
#include <base/mali_runtime.h>
#include <base/mali_memory.h>
#include <base/mali_debug.h>
#include <base/mali_byteorder.h>
#include <base/pp/mali_pp_job.h>
#include <mali_render_regs.h>
#include <mali_rsw.h>
#include <mali_pp_cmd.h>

#define SCREENSIZE_X        16
#define SCREENSIZE_Y        16
#define W_VALUE             0x003E0000  /* 24bit floating point == 1 */
#define Z_VALUE             0x4000      /* 16bit linear - somewhere in the middle */

typedef struct pp_test_framework
{
	mali_mem_handle     rsw_list;
	mali_mem_handle     dummy_shader;
	mali_mem_handle     color_shader;
	mali_mem_handle     polygon_list;
	mali_mem_handle     vdb_list[12];
	u16                 *fb_correct[12];
	u32                 number_of_framebuffers;
	u32                 framebuffer_size;
	mali_mem_handle 	framebuffer;
	u16 triangle_color;
 	u16 background_color;
} pp_test_framework;

static u32 test_dummy_shader[] =
{
    0x00020425,
    0x0000000C,
    0x01E007CF,
    0xB0000000,
    0x000005F5
};

static u32 test_blue_color_shader[] =
{
    0x00020425,
    0x0000050C,
    0x000007CF,
    0x000001E0,
    0x00000400,
};

static u32 test_heavy_color_shader[] = /* 50x50 iterations per fragment */ 
{
	0x023a3006, 0x2000000c, 0x600307cf, 0x000003e4, 0x00000000, 0x00000000, 0x021a3807, 0xbe470030, 0x8800039c, 0x202421f3, 0x00000001, 0x00000000, 0x00000000, 0x02501003, 0x00000e72, 0x000007ce, 0x023b3c0a, 0x03ff0002, 0x00418801, 0x211c80d0, 0x40eb9e47, 0x070008e6, 0xa000001c, 0x03ffffff, 0x00000a48, 0x17800780, 0x022b2007, 0x00463106, 0x0000e030, 0xffffec00, 0x0052403f, 0x0000003c, 0x00000000, 0x00021025, 0x0b90850c, 0x03c0000f, 0x00000000, 0x00000000
};


static mali_err_code pp_util_internal__alloate_frame_buffer(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, u32 num_fbs);
static mali_err_code pp_util_internal__alloate_vdb_arrays(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf);
static mali_pp_job_handle   pp_util_internal__allocate_job(mali_base_ctx_handle ctxh, mali_stream_handle stream);
static mali_err_code pp_util_internal__allocate_dummy_shader(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf);
static mali_err_code pp_util_internal__allocate_color_shader(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, pp_test_framework_shader shader);
static mali_err_code pp_util_internal__write_dummy_rsw(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, u32 rsw_index);
static mali_err_code pp_util_internal__write_drawing_rsw(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, u32 rsw_index);
static mali_err_code pp_util_internal__create_reference_frame_buffers(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf);
static mali_err_code pp_util_internal__writing_tile_list(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, u32 num_polys);


pp_test_framework_handle pp_test_framework_context_allocate(suite* test_suite, mali_base_ctx_handle ctxh, u32 num_fbs, u32 num_polys, pp_test_framework_shader shader)
{
	pp_test_framework * pptf;
	
	/* Allocating the framework struct - Returning NULL on error */
	pptf = MALLOC(sizeof(*pptf));
	MALI_CHECK_NON_NULL(pptf, NULL);

	/* Memsetting to Zero */
	_mali_sys_memset(pptf,0, sizeof(*pptf));

	/* Allocating frame buffer */
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR==pp_util_internal__alloate_frame_buffer(ctxh, test_suite, pptf, num_fbs), function_failed);
			
	/* Creating 12 VDB arrays */
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR==pp_util_internal__alloate_vdb_arrays(ctxh, test_suite, pptf), function_failed);

	/* Allocating dummy shader */	
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR==pp_util_internal__allocate_dummy_shader(ctxh, test_suite, pptf), function_failed);

	/* Allocating the color shader */
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR==pp_util_internal__allocate_color_shader(ctxh, test_suite, pptf, shader), function_failed);

	/* Allocating RSW list with 2 elements */
	pptf->rsw_list = _mali_mem_alloc(ctxh, sizeof(m200_rsw)*2, 0, MALI_PP_READ );
	MALI_CHECK_GOTO(NULL!=pptf->rsw_list, function_failed);

	/* Allocating and setting dummy rsw */
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR==pp_util_internal__write_dummy_rsw(ctxh, test_suite, pptf, 0), function_failed);

	/* Allocating and setting drawing rsw */ 
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR==pp_util_internal__write_drawing_rsw(ctxh, test_suite, pptf, 1), function_failed);

	/* Allocating Polygon (Tile) list with number of triangles + 2 elements (begin and end tile)*/
	pptf->polygon_list = 	_mali_mem_alloc(ctxh, sizeof(mali_pp_cmd)*(num_polys+2), 0, MALI_PP_READ | MALI_PP_WRITE);
	MALI_CHECK_GOTO(NULL!=pptf->polygon_list, function_failed);

	/* Writing PP Tile list */
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR==pp_util_internal__writing_tile_list(ctxh, test_suite, pptf, num_polys), function_failed);

	/* Creating the 12 refernece images */
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR==pp_util_internal__create_reference_frame_buffers(ctxh, test_suite, pptf), function_failed);

	/* SUCCESS */
	return pptf;

/* ERROR HANDLING */
function_failed:
	pp_test_framework_context_free(pptf);
	return NULL;
}

mali_pp_job_handle pp_test_framework_job_create(mali_base_ctx_handle ctxh, pp_test_framework_handle pptf_h, u32 job_array_index)
{
	return pp_test_framework_stream_job_create(ctxh, pptf_h, job_array_index, NULL);
}

mali_pp_job_handle pp_test_framework_stream_job_create(mali_base_ctx_handle ctxh, pp_test_framework_handle pptf_h, u32 job_array_index, mali_stream_handle stream)
{
	mali_pp_job_handle jobtomake;
	u32	addr;
	pp_test_framework * pptf = MALI_STATIC_CAST(pp_test_framework *)pptf_h;
	MALI_DEBUG_ASSERT_POINTER(pptf);
	MALI_DEBUG_ASSERT(pptf->number_of_framebuffers>job_array_index, ("Error"));

	jobtomake = pp_util_internal__allocate_job(ctxh, stream);

	if ( jobtomake != NULL)
	{
		addr = _mali_mem_mali_addr_get(pptf->polygon_list, 0);
		_mali_pp_job_set_common_render_reg( jobtomake, M200_FRAME_REG_REND_LIST_ADDR,		addr);
		addr = _mali_mem_mali_addr_get( pptf->rsw_list, 0);
		_mali_pp_job_set_common_render_reg( jobtomake, M200_FRAME_REG_REND_RSW_BASE, 		addr);
		addr = _mali_mem_mali_addr_get( pptf->vdb_list[job_array_index%12], 0);
		_mali_pp_job_set_common_render_reg( jobtomake, M200_FRAME_REG_REND_VERTEX_BASE, 	addr);
		addr = _mali_mem_mali_addr_get( pptf->framebuffer, pptf->framebuffer_size*job_array_index);
		_mali_pp_job_set_common_render_reg( jobtomake, M200_WB0_REG_TARGET_ADDR,			addr);
		return jobtomake;
	}

	/* Not thread safe:
	assert_fail((1!=0), "failed to make job");*/
	return MALI_NO_HANDLE;
}

void pp_test_framework_context_free(pp_test_framework_handle pptf_h)
{
	pp_test_framework * pptf = MALI_STATIC_CAST(pp_test_framework*) pptf_h;
	u32 i;
	MALI_DEBUG_ASSERT_POINTER(pptf);
	
	if ( NULL!=pptf->polygon_list ) _mali_mem_free( pptf->polygon_list);
	if ( NULL!=pptf->rsw_list )     _mali_mem_free( pptf->rsw_list);
	if ( NULL!=pptf->color_shader ) _mali_mem_free(pptf->color_shader);
	if ( NULL!=pptf->dummy_shader)  _mali_mem_free(pptf->dummy_shader);
	if ( NULL!=pptf->framebuffer)   _mali_mem_free(pptf->framebuffer);
	for(i = 0; i <12; i++)
	{
		if ( NULL!=pptf->vdb_list[i])
		{
			_mali_mem_free(pptf->vdb_list[i]);
		}
	}
	for(i = 0; i <12; i++)
	{
		if ( NULL!=pptf->fb_correct[i])
		{
			FREE(pptf->fb_correct[i]);
		}
	}
	FREE(pptf);
}

void pp_test_framework_check_job_result(suite* test_suite,  pp_test_framework_handle pptf_h, u32 job_array_index)
{
	u16 *fb_mapped;
	pp_test_framework * pptf = MALI_STATIC_CAST(pp_test_framework * ) pptf_h;
	MALI_DEBUG_ASSERT_POINTER(pptf);

	fb_mapped = _mali_mem_ptr_map_area( pptf->framebuffer , job_array_index * pptf->framebuffer_size, pptf->framebuffer_size, pptf->framebuffer_size*job_array_index, MALI_MEM_PTR_READABLE);

	if ( NULL==fb_mapped)
	{
		assert_fail(1==0, "Could not map mali fb memory");
		return;
	}

	/* 
	 * HW cannot write in correct endianess
	 */
	_mali_byteorder_swap_endian(fb_mapped, pptf->framebuffer_size, 2);

	/* Checking if the rendered frame_buffer fb_mapped, is equal to the reference frame_buffer pptf->fb_correct */
#if 0
	assert_fail((0 == _mali_sys_memcmp(fb_mapped, pptf->fb_correct[job_array_index%12], pptf->framebuffer_size)), "Data written to framebuffer is wrong");
#else
	assert_fail((0 == _mali_sys_memcmp((u32*)fb_mapped, (u32*)pptf->fb_correct[job_array_index%12], pptf->framebuffer_size/2)), "Data written to framebuffer is wrong");
#endif
	
	_mali_mem_ptr_unmap_area( pptf->framebuffer );
}

void pp_test_framework_clear_job_result(suite* test_suite,  pp_test_framework_handle pptf_h, u32 job_array_index)
{
	u16 			*fb_mapped;
	pp_test_framework * pptf = MALI_STATIC_CAST(pp_test_framework * ) pptf_h;
	MALI_DEBUG_ASSERT_POINTER(pptf);

	fb_mapped = _mali_mem_ptr_map_area( pptf->framebuffer , job_array_index * pptf->framebuffer_size, pptf->framebuffer_size, pptf->framebuffer_size*job_array_index, MALI_MEM_PTR_READABLE);

	if ( NULL==fb_mapped)
	{
		if (test_suite) assert_fail(1==0, "Could not map mali fb memory");
		return;
	}

	/* Checking if the rendered frame_buffer fb_mapped, is equal to the reference frame_buffer pptf->fb_correct */
	_mali_sys_memset(fb_mapped, 0, pptf->framebuffer_size);
	
	_mali_mem_ptr_unmap_area( pptf->framebuffer );
}

static mali_err_code pp_util_internal__alloate_frame_buffer(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, u32 num_fbs)
{
	u32 framebuffer_size_total;
	u32* mapped_for_memset;
	pptf->number_of_framebuffers = num_fbs;
	pptf->framebuffer_size       = SCREENSIZE_X * SCREENSIZE_Y * sizeof(u16);
	framebuffer_size_total = pptf->number_of_framebuffers * pptf->framebuffer_size;
	pptf->framebuffer = _mali_mem_alloc(ctxh, framebuffer_size_total , 0, MALI_PP_READ | MALI_PP_WRITE);
	MALI_CHECK_NON_NULL(pptf->framebuffer, MALI_ERR_OUT_OF_MEMORY);
	mapped_for_memset = _mali_mem_ptr_map_area( pptf->framebuffer, 0, framebuffer_size_total, 0, MALI_MEM_PTR_WRITABLE);
	MALI_CHECK_NON_NULL(mapped_for_memset, MALI_ERR_OUT_OF_MEMORY);
	_mali_sys_memset(mapped_for_memset, 0x1f ,framebuffer_size_total);
	_mali_mem_ptr_unmap_area(pptf->framebuffer);
	MALI_SUCCESS;
}

static mali_err_code pp_util_internal__alloate_vdb_arrays(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf)
{
	int loop;
	float	x_coords[3];
	float	y_coords[3];
	float	*current_vb_part, *orig;
	u32		vertex;
	for(loop = 0; loop <12; loop++)
	{
		x_coords[0] = 1.f;
		y_coords[0] = 1.f;
		x_coords[1] = (float)(15-(loop%12));
		y_coords[1] = 1.f;
		x_coords[2] = 1.f;
		y_coords[2] = (float)(15-(loop%12));
	
		pptf->vdb_list[loop] = _mali_mem_alloc(ctxh, sizeof(mali_vdb)*64, 0, MALI_PP_READ | MALI_PP_WRITE);
		MALI_CHECK_NON_NULL(pptf->vdb_list[loop], MALI_ERR_OUT_OF_MEMORY);
		orig = (float *)_mali_mem_ptr_map_area( pptf->vdb_list[loop], 0, sizeof(pptf->vdb_list), 0, MALI_MEM_PTR_WRITABLE);
		current_vb_part = orig;
		MALI_CHECK_NON_NULL(current_vb_part , MALI_ERR_OUT_OF_MEMORY);
		for (vertex = 0; vertex < 3; vertex++)
		{
			*current_vb_part++ = x_coords[vertex];
			*current_vb_part++ = y_coords[vertex];
			*current_vb_part++ = 0.5f;
			*current_vb_part++ = 0.5f;
		}
		_mali_byteorder_swap_endian(orig, 3*4*sizeof(float), 8);
		_mali_mem_ptr_unmap_area( pptf->vdb_list[loop]);
	}
	MALI_SUCCESS;
}

static mali_pp_job_handle pp_util_internal__allocate_job(mali_base_ctx_handle ctxh, mali_stream_handle stream)
{
	mali_pp_job_handle job;

	job = _mali_pp_job_new(ctxh, 1, stream);

	MALI_CHECK_NON_NULL(job, NULL);

    _mali_pp_job_set_common_render_reg( job, M200_FRAME_REG_FEATURE_ENABLE, 		M200_FEATURE_EARLYZ_DISABLE1 | M200_FEATURE_EARLYZ_DISABLE2);  /* Turn of EARLYZ */
    _mali_pp_job_set_common_render_reg( job, M200_FRAME_REG_Z_CLEAR_VALUE, 		0xFFFFFFFF);
    _mali_pp_job_set_common_render_reg( job, M200_FRAME_REG_STENCIL_CLEAR_VALUE,	0); /* ? */
	/* All buffers are A8_B8_G8_R8, A=max B=0 G=MAX R=0,  GREEN COLOR Output565: 0x07E0 */
    _mali_pp_job_set_common_render_reg( job, M200_FRAME_REG_ABGR_CLEAR_VALUE_0, 	0xFF00FF00);
    _mali_pp_job_set_common_render_reg( job, M200_FRAME_REG_ABGR_CLEAR_VALUE_1, 	0xFF00FF00);
    _mali_pp_job_set_common_render_reg( job, M200_FRAME_REG_ABGR_CLEAR_VALUE_2, 	0xFF00FF00);
    _mali_pp_job_set_common_render_reg( job, M200_FRAME_REG_ABGR_CLEAR_VALUE_3, 	0xFF00FF00);

	#define RGB_565 0
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_SOURCE_SELECT,				M200_WBx_SOURCE_ARGB);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_TARGET_PIXEL_FORMAT,		RGB_565);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_TARGET_AA_FORMAT,			M200_WBx_AA_FORMAT_0X);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_TARGET_LAYOUT,				0);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_TARGET_SCANLINE_LENGTH,	(SCREENSIZE_X *2) >> 3);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_TARGET_FLAGS,				0);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_MRT_ENABLE,				0);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_MRT_OFFSET,				0);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_GLOBAL_TEST_ENABLE,		0);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_GLOBAL_TEST_REF_VALUE,		0);
    _mali_pp_job_set_common_render_reg( job, M200_WB0_REG_GLOBAL_TEST_CMP_FUNC,		0);

	return job;
}

static mali_err_code pp_util_internal__allocate_dummy_shader(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf)
{
	void * shader_bin = test_dummy_shader;
	u32 size = sizeof(test_dummy_shader);
	pptf->dummy_shader = _mali_mem_alloc(ctxh, size, 0 , MALI_PP_READ | MALI_PP_WRITE);
	MALI_CHECK_NON_NULL(pptf->dummy_shader, MALI_ERR_OUT_OF_MEMORY);
	_mali_mem_write(pptf->dummy_shader, 0, shader_bin, size);
	MALI_SUCCESS;
}

static mali_err_code pp_util_internal__allocate_color_shader(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, pp_test_framework_shader shader)
{
	void * shader_bin;
	u32 size;
	
	MALI_DEBUG_ASSERT_POINTER(pptf);
	MALI_DEBUG_ASSERT(pptf->color_shader==NULL, ("Error"));
	
	switch (shader)
	{
		case PPTF_SHADER_SIMPLE:
			shader_bin = test_blue_color_shader;
			size = sizeof(test_blue_color_shader);
			pptf->triangle_color   = 0x001f;
			pptf->background_color = 0x07e0;
			break;
		case PPTF_SHADER_HEAVY:
			shader_bin = test_heavy_color_shader;
			size = sizeof(test_heavy_color_shader);
			pptf->triangle_color   = 0xFFE0;
			pptf->background_color = 0x07e0;
			break;
		default:
			MALI_DEBUG_ASSERT(0==1, ("Error"));
			return MALI_ERR_FUNCTION_FAILED;
	}
	
	pptf->color_shader = _mali_mem_alloc(ctxh, size, 0 , MALI_PP_READ | MALI_PP_WRITE);
	if(MALI_NO_HANDLE == pptf->color_shader) return MALI_ERR_OUT_OF_MEMORY;

	_mali_mem_write(pptf->color_shader, 0, shader_bin, size);
	return MALI_ERR_NO_ERROR;
}

/* setting up dummy rsw */
static mali_err_code pp_util_internal__write_dummy_rsw(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, u32 rsw_index)
{
	u32 len_first_instr;
	u32	addr;
	u32 shader_first_word;
	m200_rsw rsw_tmp;

	_mali_sys_memset(&rsw_tmp,0, sizeof(m200_rsw));

	MALI_DEBUG_ASSERT_POINTER(pptf->dummy_shader);

	/* Getting the instruction length of the first shader instruction.
	Is set in the RSW at the end of the function. */
	_mali_mem_read(&shader_first_word, pptf->dummy_shader, 0, 4);
	len_first_instr = shader_first_word & 0x1f;
	addr = _mali_mem_mali_addr_get(pptf->dummy_shader, 0);

	/* Writing to the rsw_tmp */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_MULTISAMPLE_WRITE_MASK, 0xf );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_MULTISAMPLE_GRID_ENABLE, 0 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_SHADER_ADDRESS, addr);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_HINT_NO_SHADER_PRESENT, 0 );
    /* See P00086 5.1 The header which says 4:0 == 1st instruction word length, this needs
	to be encoded into the dummy program */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH, (u32)len_first_instr );

	/* Writing the generated tmp_rsw on stack to the array of rsws in mali memory at position rsw_index */
	_mali_mem_write(pptf->rsw_list, rsw_index*sizeof(m200_rsw), rsw_tmp, sizeof(m200_rsw));

	return MALI_ERR_NO_ERROR;
}

static mali_err_code pp_util_internal__write_drawing_rsw(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, u32 rsw_index)
{
	u32 len_first_instr;
	u32	addr;
	u32 shader_first_word;
	m200_rsw rsw_tmp;

	/* Shader must be set up prior to calling this function */
	MALI_DEBUG_ASSERT(pptf->color_shader!=NULL, ("Error"));

	/* Getting the instruction length of the first shader instruction.
	Is set in the RSW at the end of the function. */
	_mali_mem_read(&shader_first_word, pptf->color_shader, 0, 4);
	len_first_instr = shader_first_word & 0x1f;
	addr = _mali_mem_mali_addr_get(pptf->color_shader, 0);

	_mali_sys_memset(&rsw_tmp,0, sizeof(m200_rsw));
	
	/* Writing to the rsw_tmp */
	__m200_rsw_encode(&rsw_tmp,  M200_RSW_ABGR_WRITE_MASK,          0xf);
	__m200_rsw_encode(&rsw_tmp,  M200_RSW_MULTISAMPLE_GRID_ENABLE,  0x01);
	__m200_rsw_encode(&rsw_tmp,  M200_RSW_SAMPLE_0_REGISTER_SELECT, 0x0 );
	__m200_rsw_encode(&rsw_tmp,  M200_RSW_SAMPLE_1_REGISTER_SELECT, 0x0 );
	__m200_rsw_encode(&rsw_tmp,  M200_RSW_SAMPLE_2_REGISTER_SELECT, 0x0 );
	__m200_rsw_encode(&rsw_tmp,  M200_RSW_SAMPLE_3_REGISTER_SELECT, 0x0 );

	/* set some sensible RSW defaults */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_MULTISAMPLE_WRITE_MASK,        0xf );

	/* stencil test settings, front & back */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_FRONT_WRITE_MASK,      0xff);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_FRONT_REF_VALUE,       0x0);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_FRONT_OP_MASK,         0xff);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_FRONT_COMPARE_FUNC,    M200_TEST_ALWAYS_SUCCEED);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_FRONT_OP_IF_SFAIL,     M200_STENCIL_OP_KEEP_CURRENT);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_FRONT_OP_IF_ZFAIL,     M200_STENCIL_OP_KEEP_CURRENT);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS,     M200_STENCIL_OP_KEEP_CURRENT);
	/*  __mt200_rsw_encode( ctx, MT200_RSW_STENCIL_FRONT_VALUE_REGISTER,    0xf); *//* get ref value from RSW */

	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_BACK_WRITE_MASK,       0xff);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_BACK_REF_VALUE,        0x0);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_BACK_OP_MASK,          0xff);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_BACK_COMPARE_FUNC,     M200_TEST_ALWAYS_SUCCEED);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_BACK_OP_IF_SFAIL,      M200_STENCIL_OP_KEEP_CURRENT);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_BACK_OP_IF_ZFAIL,      M200_STENCIL_OP_KEEP_CURRENT);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_STENCIL_BACK_OP_IF_ZPASS,      M200_STENCIL_OP_KEEP_CURRENT);
	/*  __mt200_rsw_encode( ctx, MT200_RSW_STENCIL_BACK_VALUE_REGISTER, 0xf); *//* get ref value from RSW */

	/* alpha test */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_ALPHA_TEST_FUNC,               M200_TEST_ALWAYS_SUCCEED);

	/* framebuffer blending: S * 1 + D * 0 */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_RGB_BLEND_FUNC,                M200_BLEND_CsS_pCdD );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_RGB_S_SOURCE_SELECT,           M200_SOURCE_0 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_RGB_S_SOURCE_1_M_X,            1);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_RGB_S_SOURCE_ALPHA_EXPAND,     0);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_RGB_D_SOURCE_SELECT,           M200_SOURCE_0 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_RGB_D_SOURCE_1_M_X,            0);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_RGB_D_SOURCE_ALPHA_EXPAND,     0);

	__m200_rsw_encode(&rsw_tmp, M200_RSW_ALPHA_BLEND_FUNC,              M200_BLEND_CsS_pCdD );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_ALPHA_S_SOURCE_SELECT,         M200_SOURCE_0 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_ALPHA_S_SOURCE_1_M_X,          1);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_ALPHA_D_SOURCE_SELECT,         M200_SOURCE_0 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_ALPHA_D_SOURCE_1_M_X,          0);

	/* depth test */
	/*  __mt200_rsw_encode( ctx, MT200_RSW_Z_VALUE_REGISTER,        0xf );  */      /* use fixed-function z */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_Z_COMPARE_FUNC,                M200_TEST_ALWAYS_SUCCEED);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_Z_WRITE_MASK,                  0x0);
	__m200_rsw_encode(&rsw_tmp, M200_RSW_Z_NEAR_BOUND,                  0x0 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_Z_FAR_BOUND,                   0xffff );

	/* set all writemasks to allow RGBA writing */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_A_WRITE_MASK,                  1 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_R_WRITE_MASK,                  1 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_G_WRITE_MASK,                  1 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_B_WRITE_MASK,                  1 );
	
	__m200_rsw_encode(&rsw_tmp, M200_RSW_MULTISAMPLE_WRITE_MASK, 0xf );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_MULTISAMPLE_GRID_ENABLE, 0 );
	__m200_rsw_encode(&rsw_tmp, M200_RSW_SHADER_ADDRESS, addr);
	/*__m200_rsw_encode(rsw_tmp, M200_RSW_HINT_NO_SHADER_PRESENT, 0 );*/
    /* See P00086 5.1 The header which says 4:0 == 1st instruction word length, this needs
	to be encoded into the dummy program */
	__m200_rsw_encode(&rsw_tmp, M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH, (u32)len_first_instr );

	/* Writing the generated tmp_rsw on stack to the array of rsws in mali memory at position rsw_index */
	_mali_mem_write(pptf->rsw_list, rsw_index*sizeof(m200_rsw), rsw_tmp, sizeof(m200_rsw));

	return MALI_ERR_NO_ERROR;

}

static mali_err_code pp_util_internal__writing_tile_list(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf, u32 num_polys)
{
	mali_pp_cmd * cur_cmd;
	u32 triangle_counter;
	cur_cmd = (mali_pp_cmd *)_mali_mem_ptr_map_area( pptf->polygon_list, 0, (num_polys + 2)*sizeof(mali_pp_cmd), 0, MALI_MEM_PTR_READABLE);
	MALI_CHECK_NON_NULL(cur_cmd , MALI_ERR_OUT_OF_MEMORY);
										
	MALI_PL_CMD_BEGIN_NEW_TILE(*cur_cmd, 0, 0);
	cur_cmd++;

	/* Drawing num_polys identical triangles on top of eachother */
	triangle_counter = num_polys;
	while(0!=triangle_counter--)
	{
		MALI_PL_PRIMITIVE(*cur_cmd,
							MALI_PRIMITIVE_TYPE_TRIANGLE,
							1,  /* rsw-index */
							0,          /* vertex 0 */
							1,          /* vertex 1 */
							2);         /* vertex 2 */
		cur_cmd++;
	}

	MALI_PL_CMD_END_OF_LIST(*cur_cmd);
	cur_cmd++;
									
	_mali_mem_ptr_unmap_area(pptf->polygon_list);
	MALI_SUCCESS;
}

static mali_err_code pp_util_internal__create_reference_frame_buffers(mali_base_ctx_handle ctxh, suite * test_suite, pp_test_framework * pptf)
{
	int i, j, vdb_array_index;
	u16 triangle_color;
	u16 background_color;

	triangle_color   = pptf->triangle_color;
	background_color = pptf->background_color;
	
	for(vdb_array_index=0;vdb_array_index<12;vdb_array_index++)
	{
		u16 * fb_current;
		fb_current = MALLOC(pptf->framebuffer_size);
		MALI_CHECK_NON_NULL(fb_current, MALI_ERR_OUT_OF_MEMORY);
		pptf->fb_correct[vdb_array_index] = fb_current;

		for(i = 0; i < SCREENSIZE_Y; i++)
		{
			for(j = 0;j<SCREENSIZE_X; j++)
			{
				if(i<1 || j < 1 || j >((15-(vdb_array_index))-i-1) || j>(15-(vdb_array_index)-1) || i>(15-(vdb_array_index)-1)) fb_current[j + SCREENSIZE_X*i] = background_color;
				else fb_current[j + SCREENSIZE_X*i] = triangle_color ;
			}
		}
	}
	MALI_SUCCESS;
}

