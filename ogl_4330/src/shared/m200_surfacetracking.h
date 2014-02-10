/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _M200_SURFACETRACKING_H_
#define _M200_SURFACETRACKING_H_

#include <mali_system.h>

/* forward declarations. See other headers and the .c file for details */
struct mali_surface; 
struct mali_surfacetracking; 
enum mali_surface_event;

/**
 * The surfacetracking container is a datacontainer holding a bunch of mali_surfaces.
 *
 * Various surfaces may be tracked in different ways depending on surface properties. 
 *
 *  - NO_FLAGS: 
 *    Surface is never tracked. Calling surfacetracking_add is a NOP. 
 *
 *  - MALI_SURFACE_FLAG_TRACK_SURFACE: 
 *    The surface is added to a list on surfacetracking_add. 
 *    On surfacetracking_trigger, the surface is triggered as-is at the time of trigger. 
 *    No special work is taken to ensure the memory is the same at the time of setup and trigger. 
 *    As a consequence, this approach only works well when the memory is never replaced. 
 *    This is good enough for X11, where all surfaces use the DONTMOVE property. 
 *
 *    Surfaces adding in this manner will only be tracked once. This is to reduce 
 *    the amount of callbacks. 
 *
 */

#define MALI_SURFACE_TRACKING_READ 1
#define MALI_SURFACE_TRACKING_WRITE 2


/**
 * Allocates and initializes the mali_surfacetracking structure to default values. 
 * @return A new surfacetracking container
 */
struct mali_surfacetracking* _mali_surfacetracking_alloc( void );

/**
 * Frees a mali_surfacetracking container 
 **/
void _mali_surfacetracking_free( struct mali_surfacetracking* );

/**
 * Empties the list of surfaces stored in the mali_surfacetracking struct
 * @param tracking - The surfacetracking container  
 * @param type - Which surfaces to remove, MALI_SURFACE_TRACKING_READ, MALI_SURFACE_TRACKING_WRITE or a bitmask for both
 *
 * If both READ and WRITE type is set, all tracked surfaces will be removed. 
 * 
 * If READ only is set, all READ only tracked surfaces will be removed, 
 * all surfaces tagged with BOTH will be changed to WRITE, and
 * all surfaces tagged with WRITE will be left as-is.
 *
 * If WRITE only is set, all WRITE only tracked surfaces will be removed, 
 * all surfaces tagged with BOTH will be changed to READ, and
 * all surfaces tagged with READ will be left as-is.
 */
void _mali_surfacetracking_reset( struct mali_surfacetracking* tracking, u32 type );

/**
 * Adds a surface to the list of stored surfaces, using a READ or WRITE tracking. 
 * @param tracking - The surfacetracking container
 * @param surface - The surface to add
 * @param type - MALI_SURFACE_TRACKING_READ or MALI_SURFACE_TRACKING_WRITE or bitwise both. 
 * @return MALI_ERR_NO_ERROR on success, or MALI_ERR_OUT_OF_MEMORY on failure. 
 */
mali_err_code _mali_surfacetracking_add(struct mali_surfacetracking* tracking, struct mali_surface* surface, u32 type);

/**
 * Triggers a the start event on all tracked surfaces within the container 
 * @param tracking - The surfacetracking container
 *
 * Read surfaces will be triggered with the MALI_SURFACE_EVENT_GPU_READ event. 
 * Write surfaces will be triggered with the MALI_SURFACE_EVENT_GPU_WRITE event. 
 * Surfaces tagged with both will be triggered with READ, then WRITE
 *
 **/
void _mali_surfacetracking_start_track(struct mali_surfacetracking* tracking); 

/**
 * Triggers a the stop event on all tracked surfaces within the container 
 * @param tracking - The surfacetracking container
 *
 * Read surfaces will be triggered with the MALI_SURFACE_EVENT_GPU_READ_DONE event. 
 * Write surfaces will be triggered with the MALI_SURFACE_EVENT_GPU_WRITE_DONE event. 
 * Surfaces tagged with both will be triggered with READ, then WRITE
 *
 **/
void _mali_surfacetracking_stop_track(struct mali_surfacetracking* tracking); 




#endif /* _M200_SURFACETRACKING_H_ */

