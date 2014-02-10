/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_share_lists_backend.h"

#include "shared/mali_named_list.h"
#include "gles_share_lists.h"
#include "gles2_state/gles2_program_object.h"

/* empties a named list */
MALI_STATIC void _gles_share_list_clear_list( mali_named_list* namelist, void (*freefunc)() )
{
	u32 index = 0;
	void* data = NULL;
	data = __mali_named_list_iterate_begin( namelist, &index );
	while(data != NULL )
	{
		__mali_named_list_remove(namelist, index);
		freefunc(data);
		/* iterator destroyed, get a new one */
		data = __mali_named_list_iterate_begin( namelist, &index );
	}
	/* list is now empty */

	MALI_DEBUG_ASSERT( 0 == __mali_named_list_size(namelist), ("List should now be empty"));
}


void _gles_share_lists_clear_v2_content(struct gles_share_lists *share_lists)
{

	/* program object list */
	_gles_share_list_clear_list( share_lists->program_object_list, _gles2_program_object_list_entry_delete );
}
