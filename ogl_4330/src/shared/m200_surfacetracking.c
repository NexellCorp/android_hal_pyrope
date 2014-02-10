/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "m200_surfacetracking.h"
#include <shared/mali_surface.h>
#include <base/mali_memory.h>

#define DEFAULT_ARRAYSIZE 10

struct node
{
	u32 trackingtype; /* READ, WRITE or bitwise both */
	mali_surface* surface; /* the mali surface structure we are tracking */
	struct mali_shared_mem_ref* mem; /* the memory at the time of tracking */
};

struct mali_surfacetracking
{
	unsigned int num_surfaces_added;  /* number of surfaces actually added */
	unsigned int arraysize;           /* actual size of the array */ 
	struct node* array;	     /* the array of nodes */
};


struct mali_surfacetracking* _mali_surfacetracking_alloc(void)
{
	struct mali_surfacetracking* retval;
	
	retval = _mali_sys_calloc(1, sizeof(struct mali_surfacetracking));
	if(!retval) return NULL;

	retval->array = _mali_sys_calloc(1, sizeof(struct node) * DEFAULT_ARRAYSIZE);
	if(!retval->array)
	{
		_mali_sys_free(retval); 
		return NULL;
	}
	retval->arraysize = DEFAULT_ARRAYSIZE;

	return retval;
}

void _mali_surfacetracking_free( struct mali_surfacetracking* tracking)
{
	MALI_DEBUG_ASSERT_POINTER(tracking);
	_mali_surfacetracking_reset(tracking, MALI_SURFACE_TRACKING_READ | MALI_SURFACE_TRACKING_WRITE); 
	MALI_DEBUG_ASSERT(tracking->num_surfaces_added == 0, ("Nothing should remain tracked at this point"));

	/* free the structure itself, undoing the alloc */
	_mali_sys_free(tracking->array);
	_mali_sys_free(tracking); 
}


MALI_STATIC int _mali_surfacetracking_comparefunc(const void* a, const void* b)
{
	const struct node* na = (const struct node*) a;
	const struct node* nb = (const struct node*) b;

	if(na->trackingtype == 0) return 1; /* NULL elements should be at the end, will be killed */
	if(nb->trackingtype == 0) return -1; /* NULL elements should be at the end, will be killed */

	MALI_DEBUG_ASSERT_POINTER(na->surface);
	MALI_DEBUG_ASSERT_POINTER(nb->surface);
	MALI_DEBUG_ASSERT_POINTER(na->mem);
	MALI_DEBUG_ASSERT_POINTER(nb->mem);

	return _mali_mem_relative_compare(na->mem->mali_memory, nb->mem->mali_memory );
}

MALI_STATIC void _mali_surfacetracking_sort( struct mali_surfacetracking* tracking )
{
	int i;
	
	/* sort all nodes */
	_mali_sys_qsort(tracking->array, tracking->num_surfaces_added, sizeof(struct node), _mali_surfacetracking_comparefunc);
	

	/* remove all empty nodes at the end */
	for(i = tracking->num_surfaces_added-1; i >= 0; i--)
	{
		struct node* n = &tracking->array[i];
		if(n->trackingtype == 0) 
		{
			/* we found an empty node, just cut it away */
			tracking->num_surfaces_added--;
		} else {
			break; /* we found a non-empty node */
		}
	}

	MALI_DEBUG_ASSERT(tracking->num_surfaces_added <= tracking->arraysize, ("something went horribly wrong!"));

}

void _mali_surfacetracking_reset( struct mali_surfacetracking* tracking, u32 type)
{
	int i;
	MALI_DEBUG_ASSERT_POINTER(tracking);

	/* deref all surfaces in the tracking list */
	for(i = tracking->num_surfaces_added-1; i >= 0; i--)
	{
		struct node* n = &tracking->array[i];
		
		/* Is this node something we remotely want to remove?
		 * If input type is WRITE only and node type is READ only, then no. */
		if( ! (n->trackingtype & type) ) continue; 

		/* clear the types we want to remove... */
		n->trackingtype &= ~type; 

		/* ... and if there is anything left, keep the node */
		if(n->trackingtype) continue;

		/* Finally, clear the node! */ 
		if(n->surface) _mali_surface_deref(n->surface);
		n->surface = NULL;
		if(n->mem) _mali_shared_mem_ref_owner_deref(n->mem);
		n->mem = NULL;
		
		/* and we will let the resort function move the empty node to the end later 
		 * That will also shrink the number of surfaces added! */
	}

	/* do a sort to get rid of the nodes we killed */
	_mali_surfacetracking_sort(tracking);

}

mali_err_code _mali_surfacetracking_add(struct mali_surfacetracking* tracking, mali_surface* surface, u32 type)
{
	int i;
	struct node* new_node;

	MALI_DEBUG_ASSERT_POINTER(tracking);
	MALI_DEBUG_ASSERT_POINTER(surface);
	MALI_DEBUG_ASSERT_POINTER(surface->mem_ref);

	/* currently: only surfaces with property MALI_SURFACE_FLAG_TRACK_SURFACE is supported */
	if(! (surface->flags & MALI_SURFACE_FLAG_TRACK_SURFACE)) return MALI_ERR_NO_ERROR; 

	_mali_surface_access_lock( surface );
	/* check if in array already */
	for(i = tracking->num_surfaces_added-1; i >= 0; i--)
	{
		struct node* n = &tracking->array[i];
		if(surface == n->surface && surface->mem_ref == n->mem)
		{
			/* already in list! 
			 * Update the tracking type and just return */
			n->trackingtype |= type;
			_mali_surface_access_unlock( surface );
			return MALI_ERR_NO_ERROR;
		}
	}

	/* guaranteed not in the list! */

	MALI_DEBUG_ASSERT(tracking->num_surfaces_added <= tracking->arraysize, ("Illegal state"));

	/* if no more room, make some room */
	if(tracking->arraysize == tracking->num_surfaces_added)
	{
		struct node* new_array = _mali_sys_realloc(tracking->array, tracking->arraysize * 2 * sizeof(struct node));
		if(new_array == NULL) 
		{
			_mali_surface_access_unlock( surface );
			return MALI_ERR_OUT_OF_MEMORY;
		}
		
		tracking->arraysize *= 2;
		tracking->array = new_array;

	}
	
	/* there is room, just add it at the end */
	
	new_node = &tracking->array[tracking->num_surfaces_added];
	new_node->trackingtype = type;
	new_node->surface = surface;
	_mali_surface_addref(surface);
	new_node->mem = surface->mem_ref;
	_mali_shared_mem_ref_owner_addref(new_node->mem);

	_mali_surface_access_unlock( surface );

	tracking->num_surfaces_added++;

	/* and re-sort the list */
	_mali_surfacetracking_sort( tracking );

	return MALI_ERR_NO_ERROR;
}

void _mali_surfacetracking_start_track(struct mali_surfacetracking* tracking)
{
	unsigned int i;
	MALI_DEBUG_ASSERT_POINTER(tracking);

	/* trigger all surfaces in the tracking list */
	for(i = 0; i < tracking->num_surfaces_added; i++)
	{
		struct node* n = &tracking->array[i];
		if(n->trackingtype & MALI_SURFACE_TRACKING_READ) _mali_surface_trigger_event(n->surface, n->mem, MALI_SURFACE_EVENT_GPU_READ );
		if(n->trackingtype & MALI_SURFACE_TRACKING_WRITE) _mali_surface_trigger_event(n->surface, n->mem, MALI_SURFACE_EVENT_GPU_WRITE );
	}
}

void _mali_surfacetracking_stop_track(struct mali_surfacetracking* tracking)
{
	unsigned int i;
	MALI_DEBUG_ASSERT_POINTER(tracking);

	/* trigger all surfaces in the tracking list */
	for(i = 0; i < tracking->num_surfaces_added; i++)
	{
		struct node* n = &tracking->array[i];
		if(n->trackingtype & MALI_SURFACE_TRACKING_READ) _mali_surface_trigger_event(n->surface, n->mem, MALI_SURFACE_EVENT_GPU_READ_DONE );
		if(n->trackingtype & MALI_SURFACE_TRACKING_WRITE) _mali_surface_trigger_event(n->surface, n->mem, MALI_SURFACE_EVENT_GPU_WRITE_DONE );
	}
}

