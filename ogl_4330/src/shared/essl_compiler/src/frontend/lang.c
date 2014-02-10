/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "frontend/preprocessor.h"
#include "frontend/lang.h"
#include "common/essl_dict.h"
#include "common/error_reporting.h"

/** Code to access the error context for this module */
#define ERROR_CONTEXT lang_desc->err_context

/** Code to retrieve the source offset for this module */
#define SOURCE_OFFSET source_offset


static const char *extension_names[EXTENSION_COUNT] = {
	"none",
	"GL_EXT_shadow_samplers",
	"GL_OES_texture_3D",
	"GL_OES_standard_derivatives",
	"GL_ARM_framebuffer_read",
	"GL_ARM_grouped_uniforms",
	"GL_ARM_persistent_globals",
	"GL_OES_EGL_image_external",
	"GL_EXT_shader_texture_lod",
	"debug_preprocessing_directives"
};



static string get_extension_name(extension ext)
{
	const char *name = "";
	if((int)ext < (int)EXTENSION_COUNT)
	{
		name = extension_names[ext];
	}
	return _essl_cstring_to_string_nocopy(name);

}

static string get_extension_macro_name(extension ext)
{
	if(ext == EXTENSION_DEBUG_PREPROCESSING_DIRECTIVES) return _essl_cstring_to_string_nocopy("");
	if(ext == EXTENSION_NONE) return _essl_cstring_to_string_nocopy("");
	return get_extension_name(ext);
}


static essl_bool is_extension_allowed(extension ext, language_descriptor *lang_desc)
{
	if(ext == EXTENSION_GL_ARM_PERSISTENT_GLOBALS)
	{
		mali_core core = lang_desc->target_desc->core;
		return core == CORE_MALI_GP2 || core == CORE_MALI_400_GP;
	}

	if(ext == EXTENSION_GL_EXT_SHADOW_SAMPLERS || ext == EXTENSION_GL_OES_TEXTURE_3D) return ESSL_FALSE; /* these extensions are disabled */
	
	if(ext == EXTENSION_GL_OES_TEXTURE_EXTERNAL)
	{
		mali_core core = lang_desc->target_desc->core;
		return core == CORE_MALI_GP2 || core == CORE_MALI_200 || core == CORE_MALI_400_GP || core == CORE_MALI_400_PP;
	}

	if( ext == EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)
	{
		mali_core core = lang_desc->target_desc->core;
		return core == CORE_MALI_GP2 || core == CORE_MALI_200 || core == CORE_MALI_400_GP || core == CORE_MALI_400_PP;
	}

	return ESSL_TRUE;
}


language_descriptor *_essl_create_language_descriptor(mempool *pool, error_context *e_ctx, target_descriptor *desc)
{
	language_descriptor *lang_desc;
	int i;

	ESSL_CHECK(lang_desc = _essl_mempool_alloc(pool, sizeof(language_descriptor)));

	lang_desc->err_context = e_ctx;
	lang_desc->pool = pool;
	lang_desc->lang_version = VERSION_UNKNOWN;
	lang_desc->target_desc = desc;
	ESSL_CHECK(_essl_dict_init(&lang_desc->pragmas, pool));

	for (i = 0; i < EXTENSION_COUNT; ++i)
	{
		lang_desc->extensions[i] = BEHAVIOR_DISABLE;
	}
	lang_desc->extensions[EXTENSION_NONE] = BEHAVIOR_ENABLE;

	return lang_desc;
}





int _essl_set_language_version(language_descriptor *lang_desc, string version, int source_offset)
{
	if (!_essl_string_cmp(version, _essl_cstring_to_string_nocopy("100")))
	{
		/* react to selected language version */
		lang_desc->lang_version = VERSION_100;
		return 1;
	}
	else if (!_essl_string_cmp(version, _essl_cstring_to_string_nocopy("101")))
	{
		/* react to selected language version */
		lang_desc->lang_version = VERSION_101;
		return 1;
	}
	else
	{
		ERROR_CODE_STRING(ERR_PP_VERSION_UNSUPPORTED, "Language version '%s' unknown, this compiler only supports up to version " HIGHEST_VERSION_STRING "\n", version);
		return 0;
	}
}

string _essl_get_language_version(language_descriptor *lang_desc)
{
	switch (lang_desc->lang_version)
	{
	case VERSION_UNKNOWN:
	case VERSION_100:
		return _essl_cstring_to_string_nocopy("100");
	case VERSION_101:
		return _essl_cstring_to_string_nocopy("101");
	default:
		return _essl_cstring_to_string_nocopy("<unknown_version>");
	}
}

memerr _essl_load_extension_macros(preprocessor_context *ctx)
{
	unsigned i;
	for(i = (int)EXTENSION_NONE + 1; i < (int)EXTENSION_COUNT; ++i)
	{
		extension ext = (extension)i;
		if(is_extension_allowed(ext, ctx->lang_descriptor))
		{
			string macro_string = get_extension_macro_name(ext);
			if(macro_string.len > 0)
			{
				ESSL_CHECK(_essl_preprocessor_extension_macro_add(ctx, macro_string));
			}
		}
	}
	return MEM_OK;
}


static const char *describe_behavior(extension_behavior behavior)
{
	switch (behavior)
	{
		case BEHAVIOR_DISABLE: return "disable";
		case BEHAVIOR_WARN: return "warn";
		case BEHAVIOR_ENABLE: return "enable";
		default: return "<error:unknown behavior>";
	}
}

/** Helper function for activating/deactivating an extension
  * @param ctx Preprocessor context
  * @ext Extension to activate or deactivate
  * @behavior The new state of the given extension
  * @return 0 for failure, 1 for success.
  */
static int set_extension_behavior(preprocessor_context *ctx, extension ext, extension_behavior behavior)
{
	ctx->lang_descriptor->extensions[ext] = behavior;

	return 1;
}
/** Enable/disable an extension
  * @param ctx Preprocessor context
  * @param name Name of extension (as it appears in the source file)
  * @param behavior The new behavior for given extension
  * @param source_offset Offset into source code (for error messages)
  * @return 0 for failure, 1 for success
  */
int _essl_set_extension(preprocessor_context *ctx, string name, extension_behavior behavior, int source_offset)
{
	language_descriptor *lang_desc = ctx->lang_descriptor;
	essl_bool all = ESSL_FALSE;
	essl_bool done = ESSL_FALSE;
	int retcode = 0;
	unsigned i;

	/* BEHAVIOR_REQUIRE is handled by caller (preprocessor), we get this as BEHAVIOR_ENABLE */
	assert(behavior == BEHAVIOR_ENABLE || behavior == BEHAVIOR_WARN || behavior == BEHAVIOR_DISABLE);

	if (_essl_string_cmp(name, _essl_cstring_to_string_nocopy("all")) == 0)
	{
		all = ESSL_TRUE;
	}

	for(i = (int)EXTENSION_NONE + 1; i < (int)EXTENSION_COUNT; ++i)
	{
		extension ext = (extension)i;
		string ext_name = get_extension_name(ext);
		if((all || _essl_string_cmp(name, ext_name) == 0) && is_extension_allowed(ext, lang_desc))
		{
			ESSL_CHECK(set_extension_behavior(ctx, ext, behavior));
			done = ESSL_TRUE;
		}
	}

	if (done)
	{
		retcode = 1;
	}

	/* this block (and a couple of tests) should be removed when we get real pragma/extension support running */
	{
		extension_behavior is_debugging = _essl_get_extension_behavior(lang_desc, EXTENSION_DEBUG_PREPROCESSING_DIRECTIVES);
		if (is_debugging == BEHAVIOR_ENABLE || is_debugging == BEHAVIOR_WARN)
		{
			const char *extension_name;
			ESSL_CHECK_OOM(extension_name = _essl_string_to_cstring(lang_desc->pool, name));
			NOTE_CODE_TWOPARAMS(NOTE_MESSAGE, "Set extension '%s': \"%s\"\n", extension_name, describe_behavior(behavior));
			if (is_debugging == BEHAVIOR_WARN)
			{
				WARNING_CODE_MSG(NOTE_MESSAGE, "Extension 'debug_preprocessing_directives' used.\n");
			}
		}
	}

	return retcode;
}

/** Check status of an extension
  * @param lang_desc Language descriptor
  * @param ext Extension to check
  * @return the extension behavior (PS: BEHAVIOR_REQUIRE is never returned, BEHAVIOR_ENABLE returned instead)
  */
extension_behavior _essl_get_extension_behavior(language_descriptor *lang_desc, extension ext)
{
	if (ext < EXTENSION_COUNT)
	{
		return lang_desc->extensions[ext];
	}

	return BEHAVIOR_DISABLE;
}

int _essl_set_pragma(language_descriptor *lang_desc, string pragma, int source_offset)
{
	if(_essl_string_cmp(pragma, _essl_cstring_to_string_nocopy("disable_vertex_shader_output_rewrites")) == 0)
	{
		lang_desc->disable_vertex_shader_output_rewrites = ESSL_TRUE;
	} else 	if(_essl_string_cmp(pragma, _essl_cstring_to_string_nocopy("allow_gl_names")) == 0)
	{
		lang_desc->allow_gl_names = ESSL_TRUE;
	} else if(_essl_string_cmp(pragma, _essl_cstring_to_string_nocopy("ARM_issue_3558_error(on)")) == 0)
	{
		if(lang_desc->target_desc != NULL && lang_desc->target_desc->options != NULL)
		{
			lang_desc->target_desc->options->mali200_unsafe_store_error = ESSL_TRUE;
		}
	} else if(_essl_string_cmp(pragma, _essl_cstring_to_string_nocopy("ARM_issue_3558_error(off)")) == 0)
	{
		if(lang_desc->target_desc != NULL && lang_desc->target_desc->options != NULL)
		{
			lang_desc->target_desc->options->mali200_unsafe_store_error = ESSL_FALSE;
		}
	}

	/* the rest (and a couple of tests) should be removed when we get real pragma/extension support running */
	{
		extension_behavior is_debugging = _essl_get_extension_behavior(lang_desc, EXTENSION_DEBUG_PREPROCESSING_DIRECTIVES);
		if (is_debugging == BEHAVIOR_ENABLE || is_debugging == BEHAVIOR_WARN)
		{
			NOTE_CODE_STRING(NOTE_MESSAGE, "Set pragma '%s'\n", pragma);
			if (is_debugging == BEHAVIOR_WARN)
			{
				WARNING_CODE_MSG(NOTE_MESSAGE, "Extension 'debug_preprocessing_directives' used.\n");
			}
		}
	}
	return 1;
}
