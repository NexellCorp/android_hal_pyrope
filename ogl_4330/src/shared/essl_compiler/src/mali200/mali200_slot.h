/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MALI200_SLOT_H
#define MALI200_SLOT_H

#include "mali200/mali200_scheduler.h"



m200_schedule_classes _essl_mali200_allocate_slots(m200_instruction_word *word, m200_schedule_classes wanted, m200_schedule_classes used, m200_schedule_classes subcycles_unavailable_mask, node *load_op, int same_cycle, essl_bool is_vector);
m200_instruction_word *_essl_mali200_find_free_slots(mali200_scheduler_context *ctx, m200_schedule_classes *maskp, node *n, /*@null@*/ node *constant, node *load_op, essl_bool is_vector);

essl_bool _essl_mali200_is_coalescing_candidate(node *a);

essl_bool _essl_mali200_fit_constants(m200_instruction_word *word, target_descriptor *desc, node *constant, swizzle_pattern *res_swizzle, int *res_reg);

essl_bool _essl_mali200_fit_float_constants(m200_instruction_word *word, float *vals, unsigned n_vals, essl_bool is_shareable, swizzle_pattern *res_swizzle, int *res_reg);


#endif /* MALI200_SLOT_H */
