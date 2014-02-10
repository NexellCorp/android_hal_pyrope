/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_MALIGP2_H
#define MALIGP2_MALIGP2_H

#include "common/essl_target.h"
#include "common/essl_mem.h"

unsigned int _essl_maligp2_get_type_alignment(target_descriptor *desc, const type_specifier *t, address_space_kind kind);
unsigned int _essl_maligp2_get_type_size(target_descriptor *desc, const type_specifier *t, address_space_kind kind);
unsigned int _essl_maligp2_get_address_multiplier(target_descriptor *desc, const type_specifier *t, address_space_kind kind);
unsigned int _essl_maligp2_get_array_stride(target_descriptor *desc, const type_specifier *t, address_space_kind kind);
int _essl_maligp2_get_type_member_offset(target_descriptor *desc, const single_declarator *sd, address_space_kind kind);


/*@null@*/ target_descriptor *_essl_maligp2_new_target_descriptor(mempool *pool, target_kind kind, compiler_options *options);


#endif /* MALIGP2_MALIGP2_H */
