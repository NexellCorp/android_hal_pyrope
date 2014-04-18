/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012-2013 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_kernel_descriptor_mapping.h
 */

#ifndef __VR_KERNEL_DESCRIPTOR_MAPPING_H__
#define __VR_KERNEL_DESCRIPTOR_MAPPING_H__

#include "vr_osk.h"

/**
 * The actual descriptor mapping table, never directly accessed by clients
 */
typedef struct vr_descriptor_table {
	u32 * usage; /**< Pointer to bitpattern indicating if a descriptor is valid/used or not */
	void** mappings; /**< Array of the pointers the descriptors map to */
} vr_descriptor_table;

/**
 * The descriptor mapping object
 * Provides a separate namespace where we can map an integer to a pointer
 */
typedef struct vr_descriptor_mapping {
	_vr_osk_mutex_rw_t *lock; /**< Lock protecting access to the mapping object */
	int max_nr_mappings_allowed; /**< Max number of mappings to support in this namespace */
	int current_nr_mappings; /**< Current number of possible mappings */
	vr_descriptor_table * table; /**< Pointer to the current mapping table */
} vr_descriptor_mapping;

/**
 * Create a descriptor mapping object
 * Create a descriptor mapping capable of holding init_entries growable to max_entries
 * @param init_entries Number of entries to preallocate memory for
 * @param max_entries Number of entries to max support
 * @return Pointer to a descriptor mapping object, NULL on failure
 */
vr_descriptor_mapping * vr_descriptor_mapping_create(int init_entries, int max_entries);

/**
 * Destroy a descriptor mapping object
 * @param map The map to free
 */
void vr_descriptor_mapping_destroy(vr_descriptor_mapping * map);

/**
 * Allocate a new mapping entry (descriptor ID)
 * Allocates a new entry in the map.
 * @param map The map to allocate a new entry in
 * @param target The value to map to
 * @return The descriptor allocated, a negative value on error
 */
_vr_osk_errcode_t vr_descriptor_mapping_allocate_mapping(vr_descriptor_mapping * map, void * target, int *descriptor);

/**
 * Get the value mapped to by a descriptor ID
 * @param map The map to lookup the descriptor id in
 * @param descriptor The descriptor ID to lookup
 * @param target Pointer to a pointer which will receive the stored value
 * @return 0 on successful lookup, negative on error
 */
_vr_osk_errcode_t vr_descriptor_mapping_get(vr_descriptor_mapping * map, int descriptor, void** target);

/**
 * Set the value mapped to by a descriptor ID
 * @param map The map to lookup the descriptor id in
 * @param descriptor The descriptor ID to lookup
 * @param target Pointer to replace the current value with
 * @return 0 on successful lookup, negative on error
 */
_vr_osk_errcode_t vr_descriptor_mapping_set(vr_descriptor_mapping * map, int descriptor, void * target);

/**
 * Call the specified callback function for each descriptor in map.
 * Entire function is mutex protected.
 * @param map The map to do callbacks for
 * @param callback A callback function which will be calle for each entry in map
 */
void vr_descriptor_mapping_call_for_each(vr_descriptor_mapping * map, void (*callback)(int, void*));

/**
 * Free the descriptor ID
 * For the descriptor to be reused it has to be freed
 * @param map The map to free the descriptor from
 * @param descriptor The descriptor ID to free
 *
 * @return old value of descriptor mapping
 */
void *vr_descriptor_mapping_free(vr_descriptor_mapping * map, int descriptor);

#endif /* __VR_KERNEL_DESCRIPTOR_MAPPING_H__ */
