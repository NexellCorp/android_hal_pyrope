/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_share_lists_backends.h
 * @brief Defines interfaces for initializing and de-initializing the API-specific
 *        parts of gles_share_lists. These interfaces are used by the functionality
 *        of gles_share_lists.c and must be implemented in a backend that must be
 *        included with the build
 */

#ifndef _GLES_SHARE_LISTS_BACKEND_H_
#define _GLES_SHARE_LISTS_BACKEND_H_

#include <base/mali_types.h>

struct gles_share_lists;

/**
 * @brief Empties the GLES2 specific share lists. GLES1 only contexts haven't got any content in
 *        these lists and don't know how to delete any content in them anyway; so this
 *        functionality is wrapped up in a API-specific function.
 * @param share_lists The set of share lists to empty the API specific
 *                    lists of.
 */
void _gles_share_lists_clear_v2_content(struct gles_share_lists *share_lists);

#endif /* _GLES_SHARE_LISTS_BACKEND_H_ */
