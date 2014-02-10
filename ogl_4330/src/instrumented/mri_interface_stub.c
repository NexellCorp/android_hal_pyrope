/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mri_interface.h"

/* Dummy implementation of the MRI functions for use when compiling without MRI */

void _init_MRPServer(mali_instrumented_context* ctx, int inport)
{
	MALI_IGNORE(ctx);
	MALI_IGNORE(inport);
}

void _free_MRPServer(mali_instrumented_context* ctx)
{
	MALI_IGNORE(ctx);
}

void _loop_receive_MRI(mali_instrumented_frame* frame)
{
	MALI_IGNORE(frame);
}

void _look_for_MRI(mali_instrumented_frame* frame)
{
	MALI_IGNORE(frame);
}


