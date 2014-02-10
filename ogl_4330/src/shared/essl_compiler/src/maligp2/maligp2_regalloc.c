/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#define NEEDS_STDIO  /* for snprintf */

#include "common/essl_system.h"
#include "maligp2/maligp2_regalloc.h"
#include "maligp2/maligp2_bypass.h"
#include "maligp2/maligp2_reg_loadstore.h"
#include "maligp2/maligp2_slot.h"
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_constant_register_spilling.h"
#include "common/symbol_table.h"
#include "common/essl_list.h"
#include "backend/eliminate_phi_nodes.h"
#include "backend/extra_info.h"
#include "common/essl_profiling.h"

#ifdef DEBUGPRINT
#include "backend/liveness_printer.h"
#include "maligp2/maligp2_printer.h"
#endif


#define MALIGP2_SCALAR_SIZE SIZE_FP32

typedef struct constant_symbol_list {
	struct constant_symbol_list *next;
	symbol *sym;
	maligp2_instruction_word *word;
} constant_symbol_list;

ASSUME_ALIASING(constant_symbol_list, generic_list);

typedef struct {
	mempool *pool;
	maligp2_relocation_context *relocation_context;
	translation_unit *tu;
	typestorage_context *ts_ctx;
	unsigned n_constant_symbols_so_far;
	constant_symbol_list *constants;
} constant_fixup_context;

/* function to compare to floats even nans (bit-equal) */

static essl_bool float_equals(float a, float b) 
{
	union{
		float f;
		int i; } if1, if2;
	if1.f = a;
	if2.f = b;
	return (essl_bool)(if1.i == if2.i);
} 
/**
   Can the embedded constants in instruction word a fit into the constants in instruction word b
 */
static essl_bool can_fit_into(constant_fixup_context *ctx, maligp2_instruction_word *a, maligp2_instruction_word *b)
{
	int i;
	target_descriptor *desc = ctx->tu->desc;
	assert(a->n_embedded_constants > 0 && b->n_embedded_constants > 0);
	if(a->n_embedded_constants > b->n_embedded_constants) return ESSL_FALSE;
	if(a->embedded_constants_locked && !b->embedded_constants_locked && b->n_embedded_constants != 4) return ESSL_FALSE;
	for(i = 0; i < a->n_embedded_constants; ++i)
	{
		node *an = a->embedded_constants[i];
		node *bn = b->embedded_constants[i];
		node_extra *ane, *bne;
		if(an == NULL) continue;
		if(bn == NULL) return ESSL_FALSE;

		if(an->hdr.kind != bn->hdr.kind) return ESSL_FALSE;
		switch(an->hdr.kind)
		{
		case EXPR_KIND_CONSTANT:
			assert(an->hdr.type && GET_NODE_VEC_SIZE(an) == 1);
			assert(bn->hdr.type && GET_NODE_VEC_SIZE(bn) == 1);
			if(!float_equals(desc->scalar_to_float(an->expr.u.value[0]), desc->scalar_to_float(bn->expr.u.value[0]))) return ESSL_FALSE;
			break;
		case EXPR_KIND_VARIABLE_REFERENCE:
			ESSL_CHECK(ane = EXTRA_INFO(an));
			ESSL_CHECK(bne = EXTRA_INFO(bn));
			if(!_essl_address_symbol_lists_equal(ane->address_symbols, bne->address_symbols)) return ESSL_FALSE;
			if(ane->address_offset != bne->address_offset) return ESSL_FALSE;
			break;
		default:
			return ESSL_FALSE;
		}
	}
	return ESSL_TRUE;
}

static symbol *build_symbol_for_constant(constant_fixup_context *ctx, maligp2_instruction_word *word)
{
	const type_specifier *t;
	symbol *sym;
	node *initializer;
	unsigned i;
	unsigned vec_size = word->embedded_constants_locked ? 4 : (unsigned)word->n_embedded_constants;
	constant_symbol_list *csl;
	assert(word->n_embedded_constants > 0);
	for(csl = ctx->constants; csl != NULL; csl = csl->next)
	{
		if(can_fit_into(ctx, word, csl->word)) return csl->sym;
	}

	ESSL_CHECK(t = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, vec_size, MALIGP2_SCALAR_SIZE));

	ESSL_CHECK(initializer = _essl_new_constant_expression(ctx->pool, vec_size));
	initializer->hdr.type = t;

	for(i = 0; i < vec_size; ++i)
	{
		node *n = word->embedded_constants[i];
		if(n == NULL)
		{
			initializer->expr.u.value[i] = ctx->tu->desc->float_to_scalar(0.0);

		} else {
			switch(n->hdr.kind)
			{
			case EXPR_KIND_CONSTANT:
				assert(n->hdr.type && GET_NODE_VEC_SIZE(n) == 1);
				initializer->expr.u.value[i] = n->expr.u.value[0];
				break;
				
			case EXPR_KIND_VARIABLE_REFERENCE:
			{
				node_extra *ne;
				const symbol_list *syms;
				int subcycle = MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 2);
				ESSL_CHECK(ne = EXTRA_INFO(n));
				assert(ne->address_symbols != 0 && ne->address_symbols->sym == n->expr.u.sym);
				initializer->expr.u.value[i] = ctx->tu->desc->float_to_scalar((float)ne->address_offset);
				for(syms = ne->address_symbols; syms != NULL; syms = syms->next)
				{
					ESSL_CHECK(_essl_maligp2_add_constant_relocation(ctx->relocation_context, syms->sym, initializer, i, subcycle));
				}
				break;
			}
			default:
				assert(0);
				break;
				
			}
		}

	}
	{
		char buf[100];
		string str;
		qualifier_set qual;
		_essl_init_qualifier_set(&qual);
		qual.variable = VAR_QUAL_UNIFORM;
		qual.precision = PREC_HIGH;
		snprintf(buf, 100, "?__maligp2_constant_%03u", ctx->n_constant_symbols_so_far++);
		str = _essl_cstring_to_string(ctx->pool, buf);
		ESSL_CHECK(str.ptr);
		ESSL_CHECK(sym = _essl_new_variable_symbol(ctx->pool, str, t, qual, SCOPE_GLOBAL, ADDRESS_SPACE_UNIFORM, UNKNOWN_SOURCE_OFFSET));
		sym->body = initializer;
	}

	{
		symbol_list *sl;
		ESSL_CHECK(sl = LIST_NEW(ctx->pool, symbol_list));
		sl->sym = sym;
		LIST_INSERT_FRONT(&ctx->tu->uniforms, sl);
	}
	{
		constant_symbol_list *sl;
		ESSL_CHECK(sl = LIST_NEW(ctx->pool, constant_symbol_list));
		sl->sym = sym;
		sl->word = word;
		LIST_INSERT_FRONT(&ctx->constants, sl);
	}
	return sym;
}

static memerr turn_instruction_into_load(constant_fixup_context *ctx, maligp2_instruction *instr, symbol *sym, int offset)
{
	instr->opcode = MALIGP2_LOAD_CONSTANT;
	instr->address_offset = offset;
	instr->args[0].arg = 0;
	instr->args[0].reg_index = -1;
	ESSL_CHECK(_essl_maligp2_add_address_offset_relocation(ctx->relocation_context, sym, instr));

	return MEM_OK;
}

memerr _essl_maligp2_fixup_constants(mempool *pool, maligp2_relocation_context *rel_ctx, translation_unit *tu, typestorage_context *ts_ctx)
{

	constant_fixup_context context, *ctx = &context;
	ctx->pool = pool;
	ctx->relocation_context = rel_ctx;
	ctx->tu = tu;
	ctx->constants = NULL;
	ctx->ts_ctx = ts_ctx;
	ctx->n_constant_symbols_so_far = 0;
	
	{
		symbol_list *sl;
		for(sl = tu->functions; sl != 0; sl = sl->next)
		{
			control_flow_graph *cfg = sl->sym->control_flow_graph;
			unsigned i; 
			for(i = 0; i < cfg->n_blocks; ++i)
			{
				basic_block *b = cfg->output_sequence[i];
				maligp2_instruction_word *word;
				for(word = b->earliest_instruction_word; word != 0; word = word->successor)
				{
					if(word->u.real_slots.load_const[0] != 0 && word->u.real_slots.load_const[0]->opcode == MALIGP2_CONSTANT)
					{
						int j;
						symbol *sym;
						ESSL_CHECK(sym = build_symbol_for_constant(ctx, word));
						for(j = 0; j < word->n_embedded_constants; ++j)
						{
							maligp2_instruction *instr = word->u.real_slots.load_const[j];
							assert(instr != 0);
							ESSL_CHECK(turn_instruction_into_load(ctx, instr, sym, j));
						}

					}
				}

			}

		}
	}

	return MEM_OK;
}

typedef struct _tag_phielim_context {
	mempool *pool;
	control_flow_graph *cfg;
	target_descriptor *desc;
	liveness_context *liv_ctx;
} phielim_context;

static memerr phielim_insert_move(void *pectx, node *src, node *dst, int earliest, int latest, basic_block *block, essl_bool backward,
								  int *src_position_p, int *dst_position_p, node ***src_ref_p, node ***dst_ref_p)
{
	phielim_context *ctx = pectx;
	maligp2_instruction_word *word;
	maligp2_schedule_classes sc = 0;
	for (word = backward ? block->latest_instruction_word : block->earliest_instruction_word ;
		 word != 0 ;
		 word = backward ? word->predecessor : word->successor)
	{
		if (START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1)) <= earliest &&
			END_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1)) >= latest)
		{
			sc = _essl_maligp2_allocate_move(ctx->cfg, ctx->desc, word, MALIGP2_RESERVATION_FULFILL,
											 MALIGP2_MOV, src, dst);
			if (sc != 0) break;
		}
	}

	if (sc == 0) {
		/* Insert instruction */
		if (backward) {
			/* Insert at end of block */
			maligp2_instruction_word *latest_word = block->latest_instruction_word;
			assert(latest <= END_OF_CYCLE(latest_word->cycle));
			ESSL_CHECK(word = _essl_maligp2_insert_word_after(ctx->pool, ctx->liv_ctx, latest_word, block));
		} else {
			/* Insert at start of block */
			maligp2_instruction_word *earliest_word = block->earliest_instruction_word;
			assert(earliest >= START_OF_CYCLE(earliest_word->cycle));
			ESSL_CHECK(word = _essl_maligp2_insert_word_before(ctx->pool, ctx->liv_ctx, earliest_word, block));
		}
		sc = _essl_maligp2_allocate_move(ctx->cfg, ctx->desc, word, MALIGP2_RESERVATION_FULFILL,
										 MALIGP2_MOV, src, dst);
		assert(sc);
	}

	{
		maligp2_instruction *move;
		int subcycle;
		essl_bool failed_alloc = ESSL_FALSE;
		ESSL_CHECK(move = _essl_maligp2_create_slot_instruction(ctx->pool, word, sc, MALIGP2_MOV, dst, &subcycle, 0, &failed_alloc));
		if(failed_alloc) return MEM_ERROR;
		move->args[0].arg = src;
		move->instr_node = dst;
		*src_position_p = START_OF_SUBCYCLE(subcycle);
		*dst_position_p = END_OF_SUBCYCLE(subcycle);
		*src_ref_p = &move->args[0].arg;
		*dst_ref_p = &move->instr_node;

		if (!backward)
		{
			/* If a forward move is inserted in a word which contains a move reservation
			   of the source variable, the move reservation is unnecessary, so it should
			   be removed in order not to confuse later phases.
			*/
			unsigned i;
			for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++)
			{
				if (word->reserved_moves[i] == src)
				{
					live_range *range;
					live_delimiter **delimp, *delim = 0;

					/* Unreserve move */
					word->reserved_moves[i] = 0;
					word->n_moves_available++;
					word->n_moves_reserved--;
					assert(word->n_moves_available >= 0);
					assert(word->n_moves_reserved >= 0);
					
					/* Remove move reservation delimiter from range */
					range = _essl_ptrdict_lookup(&ctx->liv_ctx->var_to_range, src);
					for (delimp = &range->points ; *delimp ; delimp = &(*delimp)->next)
					{
						delim = *delimp;
						if (delim->var_ref == &word->reserved_moves[i])
						{
							*delimp = delim->next;
							break;
						}
					}
					assert(delim != 0 && delim->var_ref == &word->reserved_moves[i]);
					break;
				}
			}

		}

		return MEM_OK;
	}

}

static void prioritize_ranges(liveness_context *liv_ctx)
{
	/* Move fixed-point vars and spill vars to the front */
	live_range *p1r, **p1rp = &p1r;
	live_range *p2r, **p2rp = &p2r;
	live_range *ur, **urp = &ur;
	live_range *r;
	for (r = liv_ctx->var_ranges ; r != 0 ; r = r->next) {
		if (_essl_maligp2_is_fixedpoint_range(r)) {
			*p1rp = r;
			p1rp = &r->next;
		} else if (r->spill_range) {
			*p2rp = r;
			p2rp = &r->next;
		} else {
			*urp = r;
			urp = &r->next;
		}
	}
	*urp = 0;
	*p2rp = ur;
	*p1rp = p2r;
	liv_ctx->var_ranges = p1r;
}

static memerr check_for_illegal_unallocated_ranges(live_range *unallocated_ranges, error_context *err, target_descriptor *desc)
{
	live_range *r;
	for (r = unallocated_ranges ; r != 0 ; r = r->next)
	{
		if (_essl_maligp2_is_fixedpoint_range(r))
		{
			(void)REPORT_ERROR(err, ERR_RESOURCES_EXHAUSTED, 0, "Unable to allocate %s fixed-point ranges.\n",
				_essl_mali_core_name(desc->core));
			return MEM_ERROR;
		}
	}
	return MEM_OK;
}

#ifdef DEBUGPRINT
#define DEBUGP(exp) \
	if (out) { \
		ESSL_CHECK(exp); \
		fflush(out); \
	}
#else
#define DEBUGP(exp)
#endif

static void demote_misc_moves(translation_unit *tu, symbol *function)
{
	control_flow_graph *cfg = function->control_flow_graph;
	unsigned i; 
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *b = cfg->output_sequence[i];
		maligp2_instruction_word *word;
		for(word = b->earliest_instruction_word; word != 0; word = word->successor)
		{
			if(word->u.real_slots.misc != 0 && word->u.real_slots.misc->opcode == MALIGP2_RESERVE_MOV_MISC)
			{
				_essl_maligp2_demote_misc_move_reservation(cfg, tu->desc, word);
			}
		}
		
	}
}


static memerr allocate_registers_helper(mempool *pool, symbol *function, maligp2_relocation_context *rel_ctx, translation_unit *tu, typestorage_context *ts_ctx, error_context *err, compiler_options *opts, mempool *liveness_pool, unique_name_context *names)
{
	live_range *allocated_ranges = 0, *unallocated_ranges = 0;
	liveness_context *liv_ctx;
	virtual_reg_context *vr_ctx;
	loadstore_context *ls_ctx;
	mempool loadstore_pool;
	int n_iterations;
	memerr ret;
	int prev_n_cycles = -1;
	unsigned n_iterations_with_constant_n_cycles = 0;
#ifdef DEBUGPRINT
	FILE *out;
	out = fopen("out-maligp2.txt", "a");
#endif
	IGNORE_PARAM(names);

	demote_misc_moves(tu, function);

	TIME_PROFILE_START("live_ranges");
	ESSL_CHECK(liv_ctx = _essl_maligp2_calculate_live_ranges(liveness_pool, function->control_flow_graph, tu->desc, err));
	TIME_PROFILE_STOP("live_ranges");
	DEBUGP(_essl_liveness_printer(out, liv_ctx->var_ranges, names, 1));

	{
		phielim_context pe_ctx;
		pe_ctx.pool = pool;
		pe_ctx.cfg = function->control_flow_graph;
		pe_ctx.desc = tu->desc;
		pe_ctx.liv_ctx = liv_ctx;
		TIME_PROFILE_START("eliminate_phi");
		ESSL_CHECK(_essl_eliminate_phi_nodes(pool, liv_ctx, phielim_insert_move, &pe_ctx));
		TIME_PROFILE_STOP("eliminate_phi");
	}

	ESSL_CHECK(vr_ctx = _essl_maligp2_virtual_reg_init(pool, opts));
	ESSL_CHECK(ls_ctx = _essl_maligp2_create_loadstore_context(pool, function->control_flow_graph, vr_ctx, opts));
	n_iterations = 0;
	do {
		DEBUGP(_essl_maligp2_print_function(out, pool, function, tu->desc, names));

		TIME_PROFILE_START("sort_live_ranges");
		liv_ctx->var_ranges = _essl_liveness_sort_live_ranges(liv_ctx->var_ranges);
		prioritize_ranges(liv_ctx);
		TIME_PROFILE_STOP("sort_live_ranges");

		DEBUGP(_essl_liveness_printer(out, liv_ctx->var_ranges, names, 1));

		if (n_iterations > opts->max_maligp2_regalloc_iterations)
		{
			/* Give up */
			if(n_iterations_with_constant_n_cycles > 3)
			{
				(void)REPORT_ERROR(err, ERR_INTERNAL_COMPILER_ERROR, 0, "%s register allocation failed with convergence for vertex shader.\n", _essl_mali_core_name(tu->desc->core));
			} else {
				(void)REPORT_ERROR(err, ERR_INTERNAL_COMPILER_ERROR, 0, "%s register allocation failed for vertex shader.\n", _essl_mali_core_name(tu->desc->core));
			}
			return MEM_ERROR;
		}

		TIME_PROFILE_START("bypass");
		ESSL_CHECK(_essl_maligp2_allocate_bypass_network(pool, function, tu->desc, liv_ctx->var_ranges,
													&allocated_ranges, &unallocated_ranges));
		/* Join allocated and unallocated ranges back into the liveness context */
		{
			live_range **arange;
			for (arange = &allocated_ranges ; *arange != 0 ; arange = &(*arange)->next);
			*arange = unallocated_ranges;
			liv_ctx->var_ranges = allocated_ranges;
		}
		TIME_PROFILE_STOP("bypass");

		if (unallocated_ranges == 0)
		{
			/* Bypass allocation succeeded */
			TIME_PROFILE_START("workreg_alloc");
			ESSL_CHECK(_essl_maligp2_allocate_work_registers(vr_ctx, function->control_flow_graph, tu->desc, err, names));
			TIME_PROFILE_STOP("workreg_alloc");

			if (vr_ctx->n_regs_used > vr_ctx->n_regs) {

				if (tu->desc->options->maligp2_constant_store_workaround) /* constant register spilling does not function when hardware bugzilla 3688 is in effect. just stop and report the error. */
				{
					(void)REPORT_ERROR(err, ERR_RESOURCES_EXHAUSTED, 0, 
									   "Insufficient MaliGP2 work register space for vertex shader.\n"
									   "            %d registers needed, but only %d registers are available.\n"
									   "            Note that MaliGP2 hardware revisions r0p3 and newer do not have\n"
									   "            this limitation.\n", vr_ctx->n_regs_used, tu->desc->options->n_maligp2_work_registers);
					return MEM_ERROR;
				}

				/* Work register allocation failed - spill to constant registers */
				TIME_PROFILE_START("bypass_rollback");
				_essl_maligp2_rollback_bypass_network(function);
				TIME_PROFILE_STOP("bypass_rollback");
				ESSL_CHECK(_essl_maligp2_constant_register_spilling(pool, vr_ctx, function->control_flow_graph, ts_ctx, rel_ctx, liv_ctx));
				/*printf("constant register spilling\n");*/

				/* Calculate new liveness information but move all spilled ranges over from the old one */
				{
					live_range *old_var_ranges = liv_ctx->var_ranges;
					live_range *range, *range_next;
					TIME_PROFILE_START("live_ranges");
					ESSL_CHECK(liv_ctx = _essl_maligp2_calculate_live_ranges(liveness_pool, function->control_flow_graph, tu->desc, err));
					TIME_PROFILE_STOP("live_ranges");
					for (range = old_var_ranges ; range != 0 ; range = range_next)
					{
						range_next = range->next;
						if (range->spilled)
						{
							LIST_INSERT_FRONT(&liv_ctx->var_ranges, range);
							ESSL_CHECK(_essl_ptrdict_insert(&liv_ctx->var_to_range, range->var, range));
						}
					}
				}

				/* Regenerate conflict graph */
				ESSL_CHECK(_essl_maligp2_produce_conflict_graph(ls_ctx, liv_ctx));

				n_iterations++;
				{
					int curr_n_cycles = function->control_flow_graph->entry_block->top_cycle;
					if(curr_n_cycles == prev_n_cycles)
					{
						++n_iterations_with_constant_n_cycles;
					} else {
						n_iterations_with_constant_n_cycles = 0;
					}
					prev_n_cycles = curr_n_cycles;
				}
				continue;
			} else {
				break;
			}
		}

		TIME_PROFILE_START("bypass_rollback");
		_essl_maligp2_rollback_bypass_network(function);
		TIME_PROFILE_STOP("bypass_rollback");

		/* Check that all fixed-point ranges were allocated */
		ESSL_CHECK_NONFATAL(check_for_illegal_unallocated_ranges(unallocated_ranges, err, tu->desc));

		ESSL_CHECK(_essl_mempool_init(&loadstore_pool, 1024, _essl_mempool_get_tracker(pool)));
		TIME_PROFILE_START("loadstore");
		ret = _essl_maligp2_allocate_register_loadstores(&loadstore_pool, ls_ctx, liv_ctx, unallocated_ranges);
		TIME_PROFILE_STOP("loadstore");
		_essl_mempool_destroy(&loadstore_pool);
		if(ret == MEM_ERROR) return MEM_ERROR;

		n_iterations++;
	} while (ESSL_TRUE);
	DEBUGP(_essl_maligp2_print_function(out, pool, function, tu->desc, names));	

#ifdef DEBUGPRINT
	if (out) fclose(out);
#endif

	return MEM_OK;
}


memerr _essl_maligp2_allocate_registers(mempool *pool, symbol *function, maligp2_relocation_context *rel_ctx, translation_unit *tu, typestorage_context *ts_ctx, error_context *err, compiler_options *opts, unique_name_context *names)
{
	mempool liveness_pool;
	memerr ret;
	IGNORE_PARAM(names);
	ESSL_CHECK(_essl_mempool_init(&liveness_pool, 1024, _essl_mempool_get_tracker(pool)));
	ret = allocate_registers_helper(pool, function, rel_ctx, tu, ts_ctx, err, opts, &liveness_pool, names);
	_essl_mempool_destroy(&liveness_pool);
	return ret;
}
