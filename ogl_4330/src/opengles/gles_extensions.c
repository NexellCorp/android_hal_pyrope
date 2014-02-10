/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_extensions.h"
#include "gles_util.h"

void (* _gles_get_proc_address(const char *procname, const gles_extension extensions[], int array_size))(void)
{
	int i = 0;

	if (NULL == procname) return NULL;

	/* search for function name */
	for (i = 1; i < array_size; i++)
	{
		if (0 == _mali_sys_strcmp(extensions[i].name, procname))
		{
			return extensions[i].function;
		}
	}

	/* function not found */
	return NULL;
}
