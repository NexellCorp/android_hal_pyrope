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
#include "mali200/mali200_preschedule.h"
#include "mali200/mali200_scheduler.h"
#include "mali200/mali200_regalloc.h"
#include "mali200/mali200_emit.h"
#include "backend/extra_info.h"
#include "common/output_buffer.h"
#include "backend/serializer.h"
#include "common/essl_profiling.h"
#include "middle/remove_dead_code.h"
#include "common/find_blocks_for_operations.h"



#ifdef DEBUGPRINT

#include "common/middle_printer.h"
#include "mali200/mali200_printer.h"
#include "backend/liveness_printer.h"

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

static memerr _essl_shadergen_mali200_write_shader_binary(error_context* err_ctx, output_buffer *buf, translation_unit *tu)
{
#ifdef DEBUGPRINT
	size_t start_pos = _essl_output_buffer_get_size(buf);
#endif
	ESSL_CHECK_NONFATAL(_essl_mali200_emit_translation_unit(err_ctx, buf, tu));
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


memerr _essl_shadergen_mali200_driver(mempool *pool,  error_context *err,  typestorage_context *ts_ctx, 
							struct _tag_target_descriptor *desc,  translation_unit *tu, 
							output_buffer *out_buf, compiler_options *opts)
{
	symbol_list *sl;
	memerr ret = MEM_OK;
	mali200_relocation_context relocation_context;
	unique_name_context *names = 0;
#ifdef DEBUGPRINT
	FILE *f2 = 0, *f3 = 0;
	f2 = fopen("out-target2.dot", "w");
	if ((f3 = fopen("out-mali200.txt", "w")) != 0) {
		fclose(f3);
	}
#endif
	ESSL_CHECK(_essl_mali200_relocations_init(&relocation_context, pool, tu, err));
	for(sl = tu->functions; sl != 0; sl = sl->next)
	{
		symbol *function = sl->sym;
		assert(function->control_flow_graph != 0);

		TIME_PROFILE_START("remove_dead_code");
		ESSL_CHECK(_essl_remove_dead_code(pool, function, ts_ctx));
		TIME_PROFILE_STOP("remove_dead_code");

		/*
		TIME_PROFILE_START("preschedule");
		ESSL_CHECK(_essl_mali200_preschedule(pool, desc, ts_ctx, function->control_flow_graph));
		TIME_PROFILE_STOP("preschedule");
		*/

		TIME_PROFILE_START("find_best_block");
		ESSL_CHECK(_essl_find_blocks_for_operations_func(pool, function));
		TIME_PROFILE_STOP("find_best_block");

		TIME_PROFILE_START("extra_info");
		ESSL_CHECK(_essl_calculate_extra_info(function->control_flow_graph, desc->get_op_weight_scheduler, pool));
		TIME_PROFILE_STOP("extra_info");
#ifdef DEBUGPRINT
		if (f2) ESSL_CHECK(_essl_debug_function_to_dot(f2, pool, desc, function, 1));
#endif
		TIME_PROFILE_START("schedule");
		ESSL_CHECK(_essl_mali200_schedule_function(pool, tu, function, &relocation_context, err));
		TIME_PROFILE_STOP("schedule");
#ifdef DEBUGPRINT
		if ((f3 = fopen("out-mali200.txt", "a")) != 0) {
			ESSL_CHECK(_essl_m200_print_function(f3, pool, function, &names));
			fclose(f3);
		}
#endif

		ESSL_CHECK(_essl_mali200_allocate_registers(pool, function, desc, opts->n_mali200_registers, &relocation_context, names));
	}
	TIME_PROFILE_START("relocations");
	ESSL_CHECK(_essl_mali200_relocations_resolve(&relocation_context));
	TIME_PROFILE_STOP("relocations");
	for(sl = tu->functions; sl != 0; sl = sl->next) {
		TIME_PROFILE_START("remove_empty");
		_essl_mali200_remove_empty_instructions(sl->sym->control_flow_graph);
		TIME_PROFILE_STOP("remove_empty");

		ESSL_CHECK(_essl_mali200_insert_pad_instruction(pool, sl->sym->control_flow_graph, err));

#ifdef DEBUGPRINT
		if ((f3 = fopen("out-mali200.txt", "a")) != 0) {
			ESSL_CHECK(_essl_m200_print_function(f3, pool, sl->sym, &names));
			fclose(f3);
		}
#endif
	}
#ifdef DEBUGPRINT
	if (f2) fclose(f2);
#endif
	TIME_PROFILE_START("serialize");
	ret = _essl_serialize_translation_unit(pool, err, out_buf, tu, _essl_shadergen_mali200_write_shader_binary, _essl_mali200_emit_function, SERIALIZER_LITTLE_ENDIAN);
	TIME_PROFILE_STOP("serialize");
	return ret;
}
