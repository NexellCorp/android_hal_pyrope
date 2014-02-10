/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 *  Interface for the broadcast unit on VR-450.
 *
 * - Represents up to 8 × (MMU + PP) pairs.
 * - Supports dynamically changing which (MMU + PP) pairs receive the broadcast by
 *   setting a mask.
 */

#include "vr_hw_core.h"
#include "vr_group.h"

struct vr_bcast_unit;

struct vr_bcast_unit *vr_bcast_unit_create(const _vr_osk_resource_t *resource);
void vr_bcast_unit_delete(struct vr_bcast_unit *bcast_unit);

/* Add a group to the list of (MMU + PP) pairs broadcasts go out to. */
void vr_bcast_add_group(struct vr_bcast_unit *bcast_unit, struct vr_group *group);

/* Remove a group to the list of (MMU + PP) pairs broadcasts go out to. */
void vr_bcast_remove_group(struct vr_bcast_unit *bcast_unit, struct vr_group *group);

/* Re-set cached mask. This needs to be called after having been suspended. */
void vr_bcast_reset(struct vr_bcast_unit *bcast_unit);
