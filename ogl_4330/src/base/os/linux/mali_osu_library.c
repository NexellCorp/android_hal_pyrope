/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_osu_library.c
 * File implements the user side of the OS interface
 */

#include <mali_system.h>
#include "mali_osu.h"

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdlib.h>
#include <dlfcn.h>


/* Private declaration of the OSU library type */
struct _mali_library_t_struct
{
    void* handle;
};

_mali_library_t * _mali_osu_library_load(const char * name)
{
	if (NULL != name)
	{
		u32 name_size = _mali_sys_strlen(name);
		char* name_with_ext = (char*)_mali_sys_malloc(name_size + 4); /* + ".so\n" */
		if (NULL != name_with_ext)
		{
			_mali_library_t *mali_lib = _mali_sys_malloc(sizeof(_mali_library_t));
			if (NULL != mali_lib)
			{
				_mali_sys_memcpy(name_with_ext, name, name_size);
				_mali_sys_memcpy(name_with_ext + name_size, ".so", 4);
				mali_lib->handle = dlopen(name_with_ext, RTLD_LAZY);
				if (NULL != mali_lib->handle)
				{
					_mali_sys_free(name_with_ext);
					return mali_lib;
				}

				_mali_sys_free(mali_lib);
			}

			_mali_sys_free(name_with_ext);
		}
	}

	return NULL;
}

void _mali_osu_library_unload(_mali_library_t * handle)
{
	if (NULL != handle)
	{
		dlclose(handle->handle);
		_mali_sys_free(handle);
	}
}

_mali_osu_errcode_t _mali_osu_library_init(_mali_library_t * handle, void* param, u32* retval)
{
	typedef u32 (*mali_library_init_function)(void*);

	mali_library_init_function init_func = (mali_library_init_function)dlsym(handle->handle, "mali_library_init");
	if (NULL != init_func)
	{
		*retval = init_func(param);
		return _MALI_OSU_ERR_OK;
	}

	return _MALI_OSU_ERR_FAULT;
}
