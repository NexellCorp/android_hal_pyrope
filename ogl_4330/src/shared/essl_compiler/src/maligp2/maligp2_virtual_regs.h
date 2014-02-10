/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_VIRTUAL_REGS_H
#define MALIGP2_VIRTUAL_REGS_H

#include "common/essl_mem.h"
#include "common/ptrdict.h"
#include "common/essl_node.h"
#include "common/basic_block.h"
#include "common/essl_target.h"
#include "common/unique_names.h"
#include "common/error_reporting.h"
#include "common/compiler_options.h"
#include "backend/interference_graph.h"

typedef struct _tag_virtual_reg {
	unsigned int index;
	int alloc_reg;
	essl_bool coalesced;
	node *virtual_var;
	node *vars[4];
} virtual_reg;

typedef struct _tag_virtual_reg_context {
	mempool *pool;
	int n_regs;
	int n_regs_used;

	int n_virtual_regs;
	int regs_capacity;
	virtual_reg **regs;
	ptrdict var_to_reg;
	ptrdict virtual_var_to_reg;
	type_specifier *vr_type;

	interference_graph_context *conflict_graph;
} virtual_reg_context;

virtual_reg_context *_essl_maligp2_virtual_reg_init(mempool *pool, compiler_options *opts);
essl_bool _essl_maligp2_virtual_reg_allocated(virtual_reg_context *ctx, node *var);
void _essl_maligp2_virtual_reg_get_allocation(virtual_reg_context *ctx, node *var, int *reg, int *comp);
virtual_reg *_essl_maligp2_virtual_reg_get(virtual_reg_context *ctx, int reg);
virtual_reg *_essl_maligp2_virtual_reg_allocate(virtual_reg_context *ctx);
memerr _essl_maligp2_virtual_reg_assign(virtual_reg_context *ctx, node *var, int reg, int comp);
essl_bool _essl_maligp2_virtual_reg_coalesce(virtual_reg_context *ctx, int reg1, int reg2);

void _essl_maligp2_virtual_reg_set_conflict_graph(virtual_reg_context *ctx, interference_graph_context *conflict_graph);

/*memerr _essl_maligp2_allocate_work_registers(virtual_reg_context *ctx, control_flow_graph *cfg, target_descriptor *desc);*/
memerr _essl_maligp2_allocate_work_registers(virtual_reg_context *ctx, control_flow_graph *cfg, target_descriptor *desc, error_context *err, unique_name_context *names);

#endif /* MALIGP2_VIRTUAL_REGS_H */
