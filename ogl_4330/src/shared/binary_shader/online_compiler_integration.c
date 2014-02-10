/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>

#include <shared/binary_shader/online_compiler_integration.h>
#include <shared/mali_linked_list.h>
#include <shared/binary_shader/bs_object.h>
#include <shared/binary_shader/bs_loader.h>
#include <shared/binary_shader/bs_error.h>

#ifndef NO_ESSL_SUPPORT

#include "compiler.h"

#if defined USING_MALI200
#define MALI_HWVER (HW_REV_CORE_MALI200 << 16) | MALI200_HWVER
#elif defined USING_MALI400
#define MALI_HWVER (HW_REV_CORE_MALI400 << 16) | MALI400_HWVER
#elif defined(USING_MALI450)
/* TODO: Mali-450: Change this to something 450 specific?? */
#define MALI_HWVER (HW_REV_CORE_MALI400 << 16) | MALI450_HWVER
#else
#error "No supported mali core defined"
#endif

#endif

#ifdef NO_ESSL_SUPPORT
/* this function version is a stub that automatically fails all ESSL compilations where the ESSL compiler doesn't exist */
MALI_EXPORT mali_err_code __mali_compile_essl_shader( struct bs_shader* so, u32 shadertype, char* concatenated_strings, s32* string_lengths, int source_string_count )
{
	MALI_DEBUG_ASSERT_POINTER( so );
	so->compiled = MALI_FALSE;
	bs_set_error(&so->log, BS_ERR_NOTE_MESSAGE, "Support for source shaders disabled in this build. Use binary shaders instead");
	if(bs_is_error_log_set_to_out_of_memory(&so->log)) return MALI_ERR_OUT_OF_MEMORY;
	return MALI_ERR_FUNCTION_FAILED;
}

#else
/* this function version calls the ESSL compiler */
MALI_EXPORT mali_err_code __mali_compile_essl_shader( struct bs_shader* so, u32 shadertype, char* concatenated_strings, s32* string_lengths, int source_string_count )
{
	compiler_context * cctx;
	shader_kind kind;
	mali_err_code error;
	essl_size_t errorlogsize, binarysize;
	void* binarydata = NULL;

	MALI_DEBUG_ASSERT(shadertype == BS_FRAGMENT_SHADER || shadertype == BS_VERTEX_SHADER, ("Shader type is not legal"));
	MALI_DEBUG_ASSERT(source_string_count == 0 ||(source_string_count > 0 && concatenated_strings && string_lengths),
					("Either there must be no source strings, or there must be a length array and a concatenated strings string"));
	MALI_DEBUG_ASSERT_POINTER( so );

	kind = (shadertype == BS_FRAGMENT_SHADER)?SHADER_KIND_FRAGMENT_SHADER:SHADER_KIND_VERTEX_SHADER;

	/* create compiler context */
	cctx = _essl_new_compiler(kind, concatenated_strings, string_lengths, source_string_count, MALI_HWVER, _mali_sys_malloc, _mali_sys_free);
	if(!cctx)
	{
		return MALI_ERR_OUT_OF_MEMORY;
	}

	error = _essl_run_compiler(cctx);

	/* handle error log regardless of outcome */
	errorlogsize = _essl_get_error_log_size(cctx);
	if(errorlogsize > 0)
	{
		MALI_DEBUG_ASSERT_POINTER( !so->log.log ); /* log should not be allocated yet! */
		so->log.log = _mali_sys_malloc(errorlogsize+1); /* +1 for null terminator */
		if(!so->log.log)
		{
			error = MALI_ERR_OUT_OF_MEMORY;
		} else {
			_essl_get_error_log(cctx, so->log.log, errorlogsize);
		}
	}

	/* compiling done, if result not good, we stop here */
	if(error != MALI_ERR_NO_ERROR)
	{
		_essl_destroy_compiler(cctx);
		return error;
	}

	/* acquire compiler output data */
	binarysize =  _essl_get_binary_shader_size(cctx);
	if(binarysize > 0)
	{
		binarydata = _mali_sys_malloc(binarysize);
		if(!binarydata)
		{
			_essl_destroy_compiler(cctx);
			return MALI_ERR_OUT_OF_MEMORY;
		} else {
			_essl_get_binary_shader(cctx, binarydata, binarysize);
		}
	}

	_essl_destroy_compiler(cctx);


	/* process binary data */
	error = __mali_binary_shader_load(so, shadertype, binarydata, binarysize);
	
	if(so->binary_block != NULL)
	{
		_mali_sys_free(so->binary_block);
	}

	so->binary_block = binarydata;
	so->binary_size = binarysize;	
	return error;
}
#endif

