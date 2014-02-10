/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_instrumented_main.h"
#include "mali_system.h"

#include "mali_instrumented_context.h"

#ifdef EGL_MALI_VG
/* #include "egl_vg.h" */
#include "vg_instrumented.h"
#endif

#ifdef EGL_MALI_GLES
/* #include "egl_gles.h" */
#include "gles_instrumented.h"
#endif

#include "egl_instrumented.h"
#include "mali_pp_instrumented.h"
#include "mali_gp_instrumented.h"
#include "mali_memory_instrumented.h"

MALI_EXPORT mali_bool _mali_instrumented_init(void)
{
	mali_instrumented_context *instrumented_ctx = _instrumented_create_context();
	if ( NULL == instrumented_ctx )
	{
		return MALI_FALSE;
	}

	if ( MALI_ERR_NO_ERROR != _mali_pp_instrumented_init( instrumented_ctx ) )
	{
		_instrumented_destroy_context();
		return MALI_FALSE;
	}

	if ( MALI_ERR_NO_ERROR != _mali_gp_instrumented_init( instrumented_ctx ) )
	{
		_instrumented_destroy_context();
		return MALI_FALSE;
	}

	if ( MALI_ERR_NO_ERROR != _mali_memory_instrumented_init( instrumented_ctx ) )
	{
		_instrumented_destroy_context();
		return MALI_FALSE;
	}

#if defined(EGL_MALI_GLES)
	if ( MALI_ERR_NO_ERROR != _gles_instrumentation_init( instrumented_ctx ) )
	{
		_instrumented_destroy_context();
		return MALI_FALSE;
	}
#endif

#if defined(EGL_MALI_VG)
	if ( MALI_ERR_NO_ERROR != _vg_instrumentation_init( instrumented_ctx ) )
	{
		_instrumented_destroy_context();
		return MALI_FALSE;
	}
#endif

	if (MALI_ERR_NO_ERROR != _egl_instrumentation_init( instrumented_ctx ))
	{
		_instrumented_destroy_context();
		return MALI_FALSE;
	}

	_instrumented_finish_init();

	/* Init the first frame */
	if (MALI_ERR_NO_ERROR != _instrumented_begin_new_frame(NULL))
	{
		_instrumented_destroy_context();
		return MALI_FALSE;
	}

	return MALI_TRUE;
}

MALI_EXPORT void _mali_instrumented_term(void)
{
	mali_instrumented_frame* frame = _instrumented_get_current_frame();
	if (NULL != frame)
	{
		/* This current frame should belong to EGL, and this should be the last reference to it */
		_instrumented_release_frame(frame);
	}

	_instrumented_destroy_context();
}
