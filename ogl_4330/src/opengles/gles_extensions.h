/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_extensions.h
 * @brief Defines convenience functions and structures for storing and retrieving
 *        extension function pointers/string pairs. The function pointers are
 *        retrieved based on their string id.
 */

#ifndef _GLES_EXTENSIONS_H_
#define _GLES_EXTENSIONS_H_

#include "gles_config_extension.h"
#include <shared/mali_egl_api_globals.h>

/* Constructing the array of extensions, this can only be used inside a struct/array-initializer. */
#ifndef MALI_BUILD_ANDROID_MONOLITHIC
#define GLES_EXTENSION_EX(name, custom_name) {#name, (gles_funcptr)custom_name }
#define GLES_EXTENSION(name) GLES_EXTENSION_EX(name, name)
#ifdef __SYMBIAN32__
#define MALI_GLES_NAME_WRAP(a) IMPORT_C a
#else
#define MALI_GLES_NAME_WRAP(a) a
#endif
#else
#define GLES_EXTENSION(name) {#name, (gles_funcptr)shim_##name }
#define MALI_GLES_NAME_WRAP(a) shim_##a
#endif

typedef void (*gles_funcptr)(void);

typedef struct gles_extension
{
	char *name; /**< The string matching what the user would supply to us if they want the function-pointer to this extension */
	gles_funcptr function; /**< The function pointer to the extension */
} gles_extension;

/**
 * @brief Retrieves the pointer to a the function with the given name (procname) in the
 *        supplied array of extension functions.
 * @param procname The name of the function to retrieve from the extensions array
 * @param extensions An array containing the gles_extension structures to search through
 *                   to find the entry with the name procname
 * @param array_size The size of the extensions_array
 */
void (* _gles_get_proc_address(const char *procname, const gles_extension extensions[], int array_size))(void);

#endif /* _GLES1_EXTENSIONS_H_ */
