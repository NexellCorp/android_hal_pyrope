/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MRI_INTERFACE_H_
#define _MRI_INTERFACE_H_

#include "mali_instrumented_context_types.h"


/** Set up the data object for MRI communications,
 *	with reciprocal pointers to the instrumented context.
 */
void _init_MRPServer(mali_instrumented_context* ctx, int inport);


/** Free the MRI data object.
 */
void _free_MRPServer(mali_instrumented_context* ctx);


/** Wait for MRI commands concerning this frame, exit on command.
 * The  frame, instrumented_ctx and mrp_context pointers must be guaranteed OK.
 * Bits may be set in options_mask to control exit from this function, and later re-entry.
 */
void _loop_receive_MRI(mali_instrumented_frame* fram);


/** See if an MRI command is waiting, if so start normal receive behavior.
 * The frame, instrumented_ctx and mrp_context pointers must be guaranteed OK.
 */
void _look_for_MRI(mali_instrumented_frame* fram);


#endif /* _MRI_INTERFACE_H_ */
