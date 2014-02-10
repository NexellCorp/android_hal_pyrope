/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <regs/MALI200/mali200_core.h>
#include "mali_frame_dump.h"
#include "shared/mali_surface.h"
#include "base/mali_memory.h"

#include "mali_instrumented_frame.h"

MALI_EXPORT
void _instrumented_write_frame_dump(char *filename, mali_surface *surface)
{
	_mali_surface_fprintf( surface, filename );
}

MALI_EXPORT
void _instrumented_write_frame_dump_hex(char *filename_prefix, mali_surface *surface)
{
	_mali_surface_dump( surface, filename_prefix );
}
