/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef HWCONFIG_H
#define HWCONFIG_H

/* For Mali400 r0p1 */

/* this has been declared in the hwconfig files up to this revision, so for consistency, it's also declared here */
#define MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT 8

/**
 * Verbatim32 format gives unexpected results.
 */
#define HARDWARE_ISSUE_4849 1

/**
 * Mali400 GP does wait for BRESP
 */
#define HARDWARE_ISSUE_7320 0

/**
 * Points drawn after zero-sized triangles will not be drawn
 * Draw a fullscreen quad on the entire screen
 */
#define HARDWARE_ISSUE_7571_7572 1

/**
 * Normalized numbers cannot cross vertex loader cache lines
 */
#define HARDWARE_ISSUE_8241 1

/**
 * The last used input stream cannot have unaligned data
 */
#define HARDWARE_ISSUE_8733 1

/**
 * Tile write-back can generate write requests outside allocated surface memory.
 */
#define HARDWARE_ISSUE_9427 1
#define HARDWARE_ISSUE_9427_EXTRA_BYTES 128

/**
 * Vertex data might be corrupted between the vertex shader and the PLBU
 * if there are no varyings and the position is the first output
 */
#define HARDWARE_ISSUE_8690

#endif /* HWCONFIG_H */
