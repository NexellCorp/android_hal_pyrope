/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FRONTEND_PRECISION_PROPAGATION_H
#define FRONTEND_PRECISION_PROPAGATION_H

#include "common/essl_mem.h"
#include "common/ptrdict.h"
#include "common/error_reporting.h"

typedef struct precision_scope
{
	struct precision_scope *parent_scope;
	precision_qualifier prec_float;
	precision_qualifier prec_int;
	precision_qualifier prec_sampler_2d;
	precision_qualifier prec_sampler_3d;
	precision_qualifier prec_sampler_cube;
	precision_qualifier prec_sampler_2d_shadow;
	precision_qualifier prec_sampler_external;
} precision_scope;

typedef struct precision_context
{
	ptrdict *node_to_precision_dict;
	precision_scope *current_scope;
	precision_scope global_scope;
	mempool *pool;
	typestorage_context *typestor_context;
	error_context *err_context;
	target_descriptor *desc;
	const type_specifier *curr_function_return_type;
} precision_context;


memerr _essl_precision_init_context(precision_context *ctx, mempool *pool, target_descriptor *desc, typestorage_context *typestor_context, error_context *err_context);
memerr _essl_precision_enter_scope(precision_context *ctx);
void _essl_precision_leave_scope(precision_context *ctx);

memerr _essl_precision_propagate_upward(precision_context *ctx, node *n);


memerr _essl_precision_visit_single_node(precision_context *ctx, node *n);

memerr _essl_calculate_precision(precision_context *ctx, node *root);

#endif /* FRONTEND_PRECISION_PROPAGATION_H */
