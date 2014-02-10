/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "m200_regs.h"
#include <mali_system.h>

#include <mali_rsw.h>
#include <shared/mali_pixel_format.h>

u32 _m200_wb_attachment_address( mali_pp_job_handle pp_job, u32 unit )
{
	return _mali_pp_job_get_render_attachment_address(pp_job, unit);
}

void _m200_wb_discard_attachment( mali_pp_job_handle pp_job, u32 unit )
{
	mali_reg_id reg_id;

	reg_id = MALI_STATIC_CAST(mali_reg_id)(M200_WB0_REG_SOURCE_SELECT + (M200_WBx_REG_SOURCE_SELECT >> 2) + (unit * WBx_REG_SPACE ));
	_mali_pp_job_set_common_render_reg(pp_job, reg_id, M200_WBx_SOURCE_NONE);
}

void _m200_wb_reg_write( mali_pp_job_handle pp_job, u32 unit, enum m200_wbx_reg_offset reg_offs, u32 value )
{
	mali_reg_id reg_id;

	/* Convert 8 bit offset to 32 bit offset */
#ifndef MALI_DEBUG_SKIP_ASSERT
	s32 offs32 = (u32)reg_offs>>2;
	MALI_DEBUG_ASSERT( offs32 >= 0 && offs32 < M200_NUM_REGS_WBx, ("Out of range offset for wb unit regs\n") );
#endif

#ifdef DEBUG
	switch( reg_offs )
	{
		case M200_WBx_REG_SOURCE_SELECT :
        	if ( value > 3 ) _mali_sys_printf( "m200_wb_reg_write: SOURCE_SELECT value 0x%x out of range for wb unit\n", value );
			break;
		case M200_WBx_REG_TARGET_ADDR :
			if ( value & 0x7 ) _mali_sys_printf( "m200_wb_reg_write: TARGET_ADDR value 0x%x out of range for wb unit\n", value );
			break;
		case M200_WBx_REG_TARGET_PIXEL_FORMAT:
			if ( value >= MALI_PIXEL_FORMAT_COUNT )
			{
				_mali_sys_printf( "m200_wb_reg_write: TARGET_PIXEL_FORMAT 0x%x invalid\n", value );
			}
			break;
		case M200_WBx_REG_TARGET_AA_FORMAT :
			#if defined(USING_MALI400) || defined(USING_MALI450)
			/* m400 support any of the first 16 bits set */
			MALI_DEBUG_ASSERT( !(value & 0xFFFF0000), ("m200_wb_reg_write: TARGET_AA_FORMAT 0x%x invalid\n", value) );
			#endif
			#if defined(USING_MALI200)
			/* m200 only support up to 3 bits set, directly */
			MALI_DEBUG_ASSERT( value <= M200_WBx_AA_FORMAT_128X, ("m200_wb_reg_write: TARGET_AA_FORMAT 0x%x invalid\n", value) );
			#endif
			break;
		case M200_WBx_REG_TARGET_LAYOUT :
			if ( value > MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS )
			{
            	_mali_sys_printf( "m200_wb_reg_write: TARGET_LAYOUT 0x%x invalid\n", value );
			}
			break;
		case M200_WBx_REG_TARGET_SCANLINE_LENGTH :
			if ( value > 0xffff )
			{
            	_mali_sys_printf( "m200_wb_reg_write: TARGET_SCANLINE_LENGTH 0x%x invalid\n", value );
			}
			if ( value == 0 )
			{
            	_mali_sys_printf( "m200_wb_reg_write: TARGET_SCANLINE_LENGTH is zero. Probably not what you want.\n" );
	        }
			break;
		case M200_WBx_REG_TARGET_FLAGS :
			if ( value > 0x1f )
			{
            	_mali_sys_printf( "m200_wb_reg_write: TARGET_FLAGS 0x%x is invalid\n", value );
    		}
			break;
		case M200_WBx_REG_MRT_ENABLE :
			if ( value > 0xf )
			{
				_mali_sys_printf( "m200_wb_reg_write: MRT_ENABLE 0x%x is invalid\n", value );
			}
			break;
		case M200_WBx_REG_MRT_OFFSET :
			if ( value & 0x7 )
			{
				_mali_sys_printf( "m200_wb_reg_write: MRT_OFFSET is not 8-byte aligned.. address 0x%x\n", value );
			}
			break;
		case M200_WBx_REG_GLOBAL_TEST_ENABLE :
			if ( value > 1 )
			{
				_mali_sys_printf( "m200_wb_reg_write: GLOBAL_TEST_ENABLE value 0x%x is not valid.\n", value );
			}
			break;
		case M200_WBx_REG_GLOBAL_TEST_REF_VALUE :
			if ( value > 0xffff )
			{
				_mali_sys_printf( "m200_wb_reg_write: GLOBAL_TEST_REF_VALUE 0x%x is too large.\n", value );
			}
			break;
		case M200_WBx_REG_GLOBAL_TEST_CMP_FUNC :
			if ( value > M200_TEST_ALWAYS_SUCCEED )
			{
				_mali_sys_printf( "m200_wb_reg_write: GLOBAL_TEST_CMP_FUNC 0x%x is invalid.\n", value );
			}
			break;
	}
#endif
	reg_id = MALI_STATIC_CAST(mali_reg_id)(M200_WB0_REG_SOURCE_SELECT + (reg_offs >> 2) + (unit * WBx_REG_SPACE ));
	_mali_pp_job_set_common_render_reg(pp_job, reg_id, value);
}


#ifdef DEBUG
static void _mali_error_check_frame_reg(enum mali_reg_id reg, u32 value)
{
	switch ( reg )
	{
		case M200_FRAME_REG_REND_LIST_ADDR:
			if ( value & 0x1f )
			{
				_mali_sys_printf( "m200_frame_reg_write : REND_LIST_ADDR is not 32 byte aligned\n" );
			}
			break;
		case M200_FRAME_REG_REND_RSW_BASE:
			if ( value & 0x1f )
			{
				_mali_sys_printf( "m200_frame_reg_write : REND_RSW_BASE is not 32 byte aligned\n" );
			}
			break;
		case M200_FRAME_REG_REND_VERTEX_BASE:
			if ( value & 0x3f )
			{
				_mali_sys_printf( "m200_frame_reg_write : REND_VERTEX_BASE is not 64 byte aligned\n" );
			}
			break;
		case M200_FRAME_REG_FEATURE_ENABLE:
			if ( value & ~0x3f )
			{
				_mali_sys_printf( "m200_frame_reg_write : FEATURE_ENABLE bits 31:6 are not 0. got 0x%x\n", value );
			}
			break;
		case M200_FRAME_REG_Z_CLEAR_VALUE:
			if ( value > 0xffffff )
			{
				_mali_sys_printf( "m200_frame_reg_write : Z_CLEAR_VALUE > 24 bit. got 0x%x\n", value );
			}
			break;
		case M200_FRAME_REG_STENCIL_CLEAR_VALUE:
			if ( value > 0xff )
			{
				_mali_sys_printf( "m200_frame_reg_write : STENCIL_CLEAR_VALUE > 8 bit. got 0x%x\n", value );
			}
			break;
		case M200_FRAME_REG_BOUNDING_BOX_LEFT_RIGHT:
			if ( (value & 0xc000) || value & (0xfff00000) )
			{
				_mali_sys_printf( "m200_frame_reg_write : BOUNDING_BOX_LEFT_RIGHT bits 31:20 or 15:14 are not 0. got 0x%x\n", value );
			}
			break;
		case M200_FRAME_REG_BOUNDING_BOX_BOTTOM:
			if ( value & ~0x3fff )
			{
				_mali_sys_printf( "m200_frame_reg_write : BOUNDING_BOX_BOTTOM bits 31:14 are not 0. got 0x%x\n", value );
			}
			break;
		case M200_FRAME_REG_FS_STACK_ADDR:
			if ( value & 0x7 )
			{
				_mali_sys_printf( "m200_frame_reg_write : FS_STACK_ADDR bits 2:0 are not 0. got 0x%x\n", value );
			}
			break;
		case M200_FRAME_REG_ORIGIN_OFFSET_X :
			if ( value > 0xffff )
			{
				_mali_sys_printf( "m200_frame_reg_write : ORIGIN_OFFSET_X >  0xffff. got 0x%x\n", value );
			}
			break;
		case M200_FRAME_REG_ORIGIN_OFFSET_Y :
			if ( value > 0xffff )
			{
				_mali_sys_printf( "m200_frame_reg_write : ORIGIN_OFFSET_Y >  0xffff. got 0x%x\n", value );
			}
			break;
		case M200_FRAME_REG_ABGR_CLEAR_VALUE_0 :
		case M200_FRAME_REG_ABGR_CLEAR_VALUE_1 :
		case M200_FRAME_REG_ABGR_CLEAR_VALUE_2 :
		case M200_FRAME_REG_ABGR_CLEAR_VALUE_3 :
		case M200_FRAME_REG_FS_STACK_SIZE_AND_INIT_VAL :
		case M200_FRAME_REG_SUBPIXEL_SPECIFIER :
		case M200_FRAME_REG_TIEBREAK_MODE:
			/* Silence warnings */
			break;
#if defined(USING_MALI400) || defined(USING_MALI450)
		case M400_FRAME_REG_PLIST_CONFIG :
			if (value & 0xC0000000)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_PLIST_CONFIG bits 31:30 are not 0. got 0x%x\n", value );
			}
			if (value & 0x0FC00000)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_PLIST_CONFIG bits 27:22 are not 0. got 0x%x\n", value );
			}
			if (value & 0x0000FFC0)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_PLIST_CONFIG bits 15:6 are not 0. got 0x%x\n", value );
			}
			break;
		case M400_FRAME_REG_SCALING_CONFIG :
			if (value & 0xFF800000)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_SCALING_CONFIG bits 31:23 are not 0. got 0x%x\n", value );
			}
			if (value & 0x00080000)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_SCALING_CONFIG bit 19 is not 0. got 0x%x\n", value );
			}
			if (value & 0x0000F000)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_SCALING_CONFIG bits 15:12 are not 0. got 0x%x\n", value );
			}
			if (value & 0x000000F0)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_SCALING_CONFIG bits 7:4 are not 0. got 0x%x\n", value );
			}
			break;
		case M400_FRAME_REG_TILEBUFFER_BITS :
			if ((value & 0xFFFF0000) != 0)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_TILEBUFFER_BITS bits 31:16 are not 0. got 0x%x\n", value );
			}
			if (((value & 0x0000F000) >> 12) > 8)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_TILEBUFFER_BITS bits 15:12 are not <= 8. got 0x%x\n", value );
			}
			if (((value & 0x00000F00) >> 8) > 8)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_TILEBUFFER_BITS bits 11:8 are not <= 8. got 0x%x\n", value );
			}
			if (((value & 0x000000F0) >> 4) > 8)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_TILEBUFFER_BITS bits 7:4 are not <= 8. got 0x%x\n", value );
			}
			if ((value & 0x0000000F) > 8)
			{
				_mali_sys_printf( "m200_frame_reg_write : M400_FRAME_REG_TILEBUFFER_BITS bits 0:3 are not <= 8. got 0x%x\n", value );
			}
			break;
#endif
		default:
			_mali_sys_printf( "m200_frame_reg_write : invalid frame register: 0x%x\n", reg );
	}
}
#endif

void _m200_frame_reg_write_specific( mali_pp_job_handle pp_job_handle, int core_no, enum mali_reg_id reg, u32 value )
{
#ifdef DEBUG
	_mali_error_check_frame_reg(reg, value);
#endif
	_mali_pp_job_set_specific_render_reg( pp_job_handle, core_no, reg, value );
}

void _m200_frame_reg_write_common( mali_pp_job_handle pp_job, enum mali_reg_id reg, u32 value )
{
#ifdef DEBUG
	_mali_error_check_frame_reg(reg, value);
#endif
	_mali_pp_job_set_common_render_reg( pp_job, reg, value );
}
