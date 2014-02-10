/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_target.h"
#include "maligp2/maligp2_constant_register_interference.h"
#include "maligp2/maligp2_preschedule.h"
#include "maligp2/maligp2_scheduler.h"
#include "maligp2/maligp2_driver.h"
#include "maligp2/maligp2_regalloc.h"
#include "maligp2/maligp2_bypass.h"
#include "maligp2/maligp2_emit.h"
#include "maligp2/maligp2_relocation.h"
#include "backend/extra_info.h"
#include "common/output_buffer.h"
#include "backend/serializer.h"
#include "common/essl_profiling.h"
#include "common/compiler_options.h"
#include "middle/remove_dead_code.h"
#include "middle/optimise_constant_fold.h"
#include "common/find_blocks_for_operations.h"


#ifdef DEBUGPRINT

#include "common/middle_printer.h"
#include "maligp2/maligp2_printer.h"
#include "backend/liveness_printer.h"
#include "common/lir_printer.h"

static memerr hex_write(u32 *buf, size_t n_words, FILE *out) {
	size_t i;

	for(i = 0; i < n_words; ++i)
	{
		fprintf(out, "%08x", buf[i]);
		if(i % 4 == 3)
		{
			fprintf(out, "\n");
		} else {
			fprintf(out, " ");
		}
	}
	return MEM_OK;
}
#endif


static memerr _essl_maligp2_write_shader_binary(error_context* err_ctx, output_buffer *buf, translation_unit *tu)
{
#ifdef DEBUGPRINT
	size_t start_pos = _essl_output_buffer_get_size(buf);
#endif
	ESSL_CHECK_NONFATAL(_essl_maligp2_emit_translation_unit(err_ctx, buf, tu, ESSL_FALSE));
#ifdef DEBUGPRINT
	{
		u32 *start_ptr = _essl_output_buffer_get_raw_pointer(buf) + start_pos;
		size_t n_words = _essl_output_buffer_get_size(buf) - start_pos;
		FILE *f;
		if ((f = fopen("output.hex", "w")) != 0) {
			ESSL_CHECK(hex_write(start_ptr, n_words, f));
			fclose(f);
		}
	}
#endif
	return MEM_OK;
}



memerr _essl_maligp2_driver(mempool *pool,  error_context *err,  typestorage_context *ts_ctx, 
							struct _tag_target_descriptor *desc,  translation_unit *tu, output_buffer *out_buf,
							compiler_options *opts)
{
	symbol_list *sl;
	maligp2_relocation_context relocation_context;
 	unique_name_context *names;
#ifdef DEBUGPRINT
	FILE *f2 = 0, *f3 = 0;
	FILE *f0 = 0;
	f2 = fopen("out-target2.dot", "w");
	if ((f3 = fopen("out-maligp2.txt", "w")) != 0) {
		fclose(f3);
	}
	f0 = fopen("out-maligp2-lir.txt", "w");
#endif
	ESSL_CHECK(_essl_maligp2_relocations_init(&relocation_context, pool, tu, err, opts));
	ESSL_CHECK(names = _essl_mempool_alloc(pool, sizeof(unique_name_context)));
	ESSL_CHECK(_essl_unique_name_init(names, pool, "temp"));

	for(sl = tu->functions; sl != 0; sl = sl->next)
	{
		symbol *function = sl->sym;
		/*liveness_context *liv_ctx;*/
		assert(function->body != 0);
		assert(function->control_flow_graph != 0);

		TIME_PROFILE_START("preschedule");
		ESSL_CHECK(_essl_maligp2_preschedule(pool, desc, ts_ctx, function->control_flow_graph, opts, err));
		TIME_PROFILE_STOP("preschedule");
#ifdef DEBUGPRINT
		if(f0) 
		{
			ESSL_CHECK(lir_function_to_txt(f0, pool, function, desc));
			fflush(f0);
		}
#endif
		TIME_PROFILE_START("late_constant_fold");
		ESSL_CHECK(_essl_optimise_constant_fold_nodes(pool, function, desc, err));
		TIME_PROFILE_STOP("late_constant_fold");

		TIME_PROFILE_START("remove_dead_code");
		ESSL_CHECK(_essl_remove_dead_code(pool, function, ts_ctx));
		TIME_PROFILE_STOP("remove_dead_code");

		TIME_PROFILE_START("find_best_block");
		ESSL_CHECK(_essl_find_blocks_for_operations_func(pool, function));
		TIME_PROFILE_STOP("find_best_block");

		TIME_PROFILE_START("extra_info");
		ESSL_CHECK(_essl_calculate_extra_info(function->control_flow_graph, desc->get_op_weight_scheduler, pool));
		TIME_PROFILE_STOP("extra_info");

#ifdef DEBUGPRINT
		ESSL_CHECK(_essl_debug_function_to_dot(f2, pool, desc, function, 1));
#endif
		TIME_PROFILE_START("schedule");
		ESSL_CHECK_NONFATAL(_essl_maligp2_schedule_function(pool, tu, function, &relocation_context, err));
		TIME_PROFILE_STOP("schedule");

#ifdef DEBUGPRINT
		/*ESSL_CHECK(_essl_maligp2_print_function(stdout, pool, function, desc, names));*/
		if ((f3 = fopen("out-maligp2.txt", "a")) != 0) {
			ESSL_CHECK(_essl_maligp2_print_function(f3, pool, function, desc, names));
			fclose(f3);
		}
#endif
		TIME_PROFILE_START("_regalloc");
		ESSL_CHECK_NONFATAL(_essl_maligp2_allocate_registers(pool, function, &relocation_context, tu, ts_ctx, err, opts, names));
		TIME_PROFILE_STOP("_regalloc");

	}
#ifdef DEBUGPRINT
	if (f2) fclose(f2);
	if (f0) fclose(f0);
#endif

	TIME_PROFILE_START("fixup_constants");
	ESSL_CHECK(_essl_maligp2_fixup_constants(pool, &relocation_context, tu, ts_ctx));
	TIME_PROFILE_STOP("fixup_constants");

	TIME_PROFILE_START("relocations");
	{
		struct interference_graph_context *constant_reg_interference = NULL;
		if(opts->maligp2_constant_reg_readwrite_workaround)
		{
			ESSL_CHECK(constant_reg_interference = _essl_maligp2_calc_constant_register_interference(pool, tu, &relocation_context));
		}

		ESSL_CHECK_NONFATAL(_essl_maligp2_relocations_resolve(&relocation_context, constant_reg_interference));
	}
	TIME_PROFILE_STOP("relocations");
	TIME_PROFILE_START("integrate_bypass");
	ESSL_CHECK(_essl_maligp2_integrate_bypass_allocations(pool, tu));
	TIME_PROFILE_STOP("integrate_bypass");

#ifdef DEBUGPRINT
	if ((f3 = fopen("out-maligp2.txt", "a")) != 0) {
		for(sl = tu->functions; sl != 0; sl = sl->next) {
			ESSL_CHECK(_essl_maligp2_print_function(f3, pool, sl->sym, desc, names));
		}
		fclose(f3);
	}
#endif

	/* Calculate number of instructions */
	{
		int n_instruction_words = 0;
		symbol_list *fl;
		basic_block *exit_block = tu->entry_point->control_flow_graph->exit_block;
		maligp2_instruction_word *word;
		for (fl = tu->functions ; fl != 0 ; fl = fl->next)
		{
			if(fl->sym->opt.pilot.is_proactive_func)
			{
				fl->sym->opt.pilot.one_past_program_end_address = fl->sym->control_flow_graph->entry_block->top_cycle;
			}else
			{
				n_instruction_words += fl->sym->control_flow_graph->entry_block->top_cycle;
			}
		}

		/*fprintf(stderr, "Emitted %d instructions.\n", n_instruction_words);*/
		if(n_instruction_words > opts->n_maligp2_instruction_words)
		{
			if(opts->n_maligp2_instruction_words != 256)
			{
				(void)REPORT_ERROR(err, ERR_RESOURCES_EXHAUSTED, 0, 
								   "Shader too long. This shader is %d instruction words long, but\n"
								   "            %s only supports up to %d instruction words.\n"
								   , n_instruction_words, _essl_mali_core_name(desc->core), opts->n_maligp2_instruction_words);
			} else {
				(void)REPORT_ERROR(err, ERR_RESOURCES_EXHAUSTED, 0, 
								   "Shader too long. This shader is %d instruction words long, but\n"
								   "            MaliGP2 hardware revision r0p1 only supports up to %d instruction\n"
								   "            words. Note that hardware revisions r0p2 and newer support up to\n"
								   "            512 instruction words.\n"
								   , n_instruction_words, opts->n_maligp2_instruction_words);

			}
			return MEM_ERROR;
		}
		tu->program_address.program_start_address = 0;
		tu->program_address.one_past_program_end_address = n_instruction_words;
		tu->program_address.one_past_last_input_read_address = tu->program_address.one_past_program_end_address;
		for(word = (maligp2_instruction_word *)exit_block->latest_instruction_word; word != NULL && word->predecessor != NULL; word = word->predecessor)
		{
			unsigned i;
			essl_bool input_load_encountered = ESSL_FALSE;
			for(i = 0; i < 4; ++i)
			{
				if(word->u.real_slots.load0[i] != NULL && word->u.real_slots.load0[i]->opcode == MALIGP2_LOAD_INPUT_REG)
				{
					input_load_encountered = ESSL_TRUE;
				}
			}
			if(input_load_encountered)
			{
				break;
			}
			--tu->program_address.one_past_last_input_read_address;

		}
	}

	{
		memerr ret;
		TIME_PROFILE_START("serialize");
		ret = _essl_serialize_translation_unit(pool, err, out_buf, tu, _essl_maligp2_write_shader_binary, _essl_maligp2_emit_function, SERIALIZER_LITTLE_ENDIAN);
		TIME_PROFILE_STOP("serialize");

		return ret;
	}
}
