/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_LANG_H
#define COMMON_LANG_H

#include "common/essl_target.h"
#include "common/essl_dict.h"
#include "common/error_reporting.h"
#include "common/essl_mem.h"

/** List of the known shader language versions. */
typedef enum {
	VERSION_UNKNOWN,
	VERSION_100 = 100,
	VERSION_101 = 101,
	VERSION_DEFAULT = VERSION_100
} language_version;
#define HIGHEST_VERSION_STRING "101"

/**
   Supported extensions.
*/
typedef enum {
	EXTENSION_NONE = 0,
	EXTENSION_GL_EXT_SHADOW_SAMPLERS,
	EXTENSION_GL_OES_TEXTURE_3D,
	EXTENSION_GL_OES_STANDARD_DERIVATIVES,
	EXTENSION_GL_ARM_FRAMEBUFFER_READ,
	EXTENSION_GL_ARM_GROUPED_UNIFORMS,
	EXTENSION_GL_ARM_PERSISTENT_GLOBALS,
	EXTENSION_GL_OES_TEXTURE_EXTERNAL,
	EXTENSION_GL_EXT_SHADER_TEXTURE_LOD,
	EXTENSION_DEBUG_PREPROCESSING_DIRECTIVES, /* just temporary */
	EXTENSION_COUNT /* Must be last! */
} extension;
/**
   Extension behavior setting.
   'Require' is handled in the same way as 'enable', except the
   kind of diagnostic message issued by the preprocessor.
*/
typedef enum {
	BEHAVIOR_DISABLE, /**< Disable extension */
	BEHAVIOR_WARN, /**< Warn when extension is used */
	BEHAVIOR_ENABLE  /**< Enable/require extension */
} extension_behavior;

/**
   The language descriptor is the mechanism used for controlling
   compiler behavior. This includes the language version,
   enabled pragmas and enabled extensions.
*/
typedef struct _tag_language_descriptor {
	mempool *pool; /**< Memory pool */
	error_context *err_context;  /**< Error reporting context */
	int lang_version; /**< Language version */
	extension_behavior extensions[EXTENSION_COUNT]; /**< Extension settings (enabled/warn/disabled) */
	dict pragmas; /**< Pragma directives */
	target_descriptor *target_desc; /**< Target processor descriptor */

	essl_bool disable_vertex_shader_output_rewrites;
	essl_bool allow_gl_names;
} language_descriptor;

/* forward decl, from preprocessor.h */
struct _tag_preprocessor_context;


/**
   Creates and initializes a new language descriptor
*/
language_descriptor *_essl_create_language_descriptor(mempool *pool, error_context *e_ctx, target_descriptor *desc);

/**
   Loads extension macros corresponding to supported extensions
*/

memerr _essl_load_extension_macros(struct _tag_preprocessor_context *ctx);

/**
   Sets the language version used, if supported.
   @return 1 if the version is supported, 0 if not.
*/
int _essl_set_language_version(language_descriptor *lang_desc, string version, int source_offset);

/**
   Returns the language version used.
   @return A string representation suitable for use as __VERSION__.
*/
string _essl_get_language_version(language_descriptor *lang_desc);

/**
   Sets a specific extension's behavior, or for all extensions if the special name 'all' is used.
   @return 1 if the extension is supported, 0 if not.
*/
int _essl_set_extension(struct _tag_preprocessor_context *ctx, string name, extension_behavior behavior, int source_offset);

/** Returns a specific extension's behavior setting. */
extension_behavior _essl_get_extension_behavior(language_descriptor *lang_desc, extension ext);

/**
   Sets a specific compiler pragma.
   @return 1 if the pragma is recognized, 0 if not.
*/
int _essl_set_pragma(language_descriptor *lang_desc, string pragma, int source_offset);

#endif /* COMMON_LANG_H */
