/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_base.h"
#include "gles2_extensions.h"
#include "gles_extensions.h"
#include "gles_config_extension.h"

#if MALI_BUILD_ANDROID_MONOLITHIC
#include "gles_entrypoints_shim.h"
#include "gles2_entrypoints_shim.h"
#endif


static const gles_extension _gles2_extensions[] = {

	/* dummy-entry, in case of empty list (empty initializers not allowed in ANSI C) */
	{NULL, NULL},

	/*** begin extension-list ***/
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	GLES_EXTENSION(glFramebufferTexture2DMultisampleEXT),
	GLES_EXTENSION(glRenderbufferStorageMultisampleEXT),
#endif

#if EXTENSION_DISCARD_FRAMEBUFFER
	GLES_EXTENSION(glDiscardFramebufferEXT),
#endif

#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
	GLES_EXTENSION(glGetProgramBinaryOES),
	GLES_EXTENSION(glProgramBinaryOES),	
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
	GLES_EXTENSION(glGetGraphicsResetStatusEXT),
	GLES_EXTENSION(glGetnUniformfvEXT),
	GLES_EXTENSION(glGetnUniformivEXT),
	GLES_EXTENSION(glReadnPixelsEXT)
#endif
	/*** end extension-list ***/
};

MALI_INTER_MODULE_API void (* _gles2_get_proc_address(const char *procname))(void)
{
	return _gles_get_proc_address(procname, _gles2_extensions, MALI_ARRAY_SIZE(_gles2_extensions));
}
