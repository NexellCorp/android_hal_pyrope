/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_test_system.h"

#include "piecegen_mali200/piecegen_mali200_output.h"
#include "common/basic_block.h"

typedef struct _tag_piece {
	struct _tag_piece *next;
	int index;
	symbol *function;
	const char *fun_name;
	const char *name;
	const char *upcased_name;
	struct _tag_phase *phase;

	int code_length;
	int n_instructions;
	int first_instr_length;
	int last_instr_start;
} piece;

typedef struct _tag_phase {
	struct _tag_phase *next;
	const char *name;
	const char *upcased_name;
	int n_pieces;
	struct _tag_piece *first_piece;
	int largest_piece;
} phase;

typedef struct _tag_piecegen_output_context {
	mempool *pool;
	error_context *err;
	output_buffer *out_buf;
	translation_unit *tu;

	piece *pieces;
	phase *phases;
	piece **next_piece;
	phase **next_phase;
	unsigned n_pieces;
	unsigned n_phases;
	dict phase_map;

	FILE *h_file;
	FILE *c_file;
} piecegen_output_context;


static char *upcase(piecegen_output_context *ctx, const char *name) {
	char *upcased;
	int i;
	int len = strlen(name);
	ESSL_CHECK(upcased = _essl_mempool_alloc(ctx->pool, len+1));
	for (i = 0 ; i < len ; i++) {
		char c = name[i];
		if (c >= 'a' && c <= 'z') c += 'A'-'a';
		upcased[i] = c;
	}
	upcased[i] = 0;
	return upcased;
}

static piece *new_piece(piecegen_output_context *ctx, int index, symbol *function) {
	piece *pi;
	ESSL_CHECK(pi = LIST_NEW(ctx->pool, piece));
	pi->index = index;
	pi->function = function;
	ESSL_CHECK(pi->fun_name = _essl_string_to_cstring(ctx->pool, function->name));
	return pi;
}

static phase *new_phase(piecegen_output_context *ctx, string name) {
	phase *ph;
	ESSL_CHECK(ph = LIST_NEW(ctx->pool, phase));
	ESSL_CHECK(ph->name = _essl_string_to_cstring(ctx->pool, name));
	ESSL_CHECK(ph->upcased_name = upcase(ctx, ph->name));
	ph->n_pieces = 0;
	ph->largest_piece = 0;
	return ph;
}

static memerr collect_pieces(piecegen_output_context *ctx)
{
	symbol_list *sl;
	int index = 0;
	for (sl = ctx->tu->functions ; sl != NULL ; sl = sl->next)
	{
		piece *pi;
		phase *ph;
		string phase_name, name;
		int i = 0;
		ESSL_CHECK(pi = new_piece(ctx, index++, sl->sym));

		phase_name.ptr = &pi->fun_name[0];
		while (pi->fun_name[i] != 0 && !(pi->fun_name[i] == '_' && pi->fun_name[i+1] == '_')) i++;
		assert(pi->fun_name[i] != 0);
		phase_name.len = i;
		name.ptr = &pi->fun_name[i += 2];
		while (pi->fun_name[i] != 0) i++;
		name.len = &pi->fun_name[i]-name.ptr;

		if (!_essl_dict_has_key(&ctx->phase_map, phase_name))
		{
			ph = new_phase(ctx, phase_name);
			ph->first_piece = pi;
			*ctx->next_phase = ph;
			ctx->next_phase = &ph->next;
			ctx->n_phases++;
			ESSL_CHECK(_essl_dict_insert(&ctx->phase_map, phase_name, ph));
		} else {
			ph = _essl_dict_lookup(&ctx->phase_map, phase_name);
		}
		pi->phase = ph;
		*ctx->next_piece = pi;
		ctx->next_piece = &pi->next;
		ctx->n_pieces++;
		ph->n_pieces++;

		ESSL_CHECK(pi->name = _essl_string_to_cstring(ctx->pool, name));
		ESSL_CHECK(pi->upcased_name = upcase(ctx, pi->name));
	}

	return MEM_OK;
}

static memerr output_pieces(piecegen_output_context *ctx)
{
	const u32 *data_ptr = _essl_output_buffer_get_raw_pointer(ctx->out_buf);
	piece *pi;
	phase *ph;
	int max_total_piece_size;

	/* Output piece identifiers */
	fprintf(ctx->h_file, "\n/* Piece indentifiers */\n");
	for (pi = ctx->pieces ; pi != NULL ; pi = pi->next)
	{
		fprintf(ctx->h_file, "#define PIECE_%s_%s %d\n", pi->phase->upcased_name, pi->upcased_name, pi->index);
	}
	fprintf(ctx->h_file, "#define NUM_MALI200_PIECES %d\n", ctx->n_pieces);

	/* Output piece struct */
	fprintf(ctx->h_file,
			"\n#if !defined(VG_SINGLE_BUILDFILE)"
			"\nextern struct _mali200_piece {\n"
			"\tconst unsigned int *code;\n"
			"\tint code_length;\n"
			"\tint n_instructions;\n"
			"} _mali200_pieces[NUM_MALI200_PIECES];\n"
			"#endif\n");

	/* Output instructions */
	for (pi = ctx->pieces ; pi != NULL ; pi = pi->next)
	{
		control_flow_graph *cfg = pi->function->control_flow_graph;
		int n_words = cfg->end_address - cfg->start_address;
		const u32 *ptr = &data_ptr[cfg->start_address];
		int i;
		int words_left_in_instr = 0;

		pi->n_instructions = 0;
		fprintf(ctx->c_file, "\nstatic const unsigned int _piece_%s[] = {", pi->fun_name);
		for (i = 0 ; i < n_words ; i++)
		{
			if (words_left_in_instr == 0)
			{
				fprintf(ctx->c_file, "\n\t");
				words_left_in_instr = ptr[i] & 0x1f;
				pi->n_instructions++;
				if (i == 0) pi->first_instr_length = words_left_in_instr;
				pi->last_instr_start = i;
			}
			fprintf(ctx->c_file, "0x%08X,", ptr[i]);
			words_left_in_instr--;
		}
		pi->code_length = n_words;
		if (pi->code_length > pi->phase->largest_piece)
		{
			pi->phase->largest_piece = pi->code_length;
		}
		fprintf(ctx->c_file, "\n};\n");
	}

	/* Output piece data */
	fprintf(ctx->c_file,
			"\nstruct _mali200_piece {\n"
			"\tconst unsigned int *code;\n"
			"\tint code_length;\n"
			"\tint n_instructions;\n"
			"} _mali200_pieces[%d] = {\n",
			ctx->n_pieces);
	for (pi = ctx->pieces ; pi != NULL ; pi = pi->next)
	{
		fprintf(ctx->c_file, "\t{ _piece_%s, %d, %d },\n",
				pi->fun_name, pi->code_length, pi->n_instructions);
	}
	fprintf(ctx->c_file, "};\n");

	/* Largest possible shader (conservative) */
	max_total_piece_size = 0;
	for (ph = ctx->phases ; ph != NULL ; ph = ph->next)
	{
		max_total_piece_size += ph->largest_piece;
	}
	fprintf(ctx->h_file,
			"\n#define MAX_NUM_PIECES %d\n"
			"#define MAX_TOTAL_PIECE_SIZE %d\n",
			ctx->n_phases, max_total_piece_size);
	
	{
		symbol_list *sl;
		int num_uniforms = 0;
		int num_varyings = 0;

		/* Uniform and sampler offsets */
		fprintf(ctx->h_file, "\n");
		for (sl = ctx->tu->uniforms ; sl != NULL ; sl = sl->next)
		{
			const char *kind, *name;
			switch (sl->sym->type->basic_type)
			{
			case TYPE_SAMPLER_2D:
			case TYPE_SAMPLER_3D:
			case TYPE_SAMPLER_CUBE:
			case TYPE_SAMPLER_2D_SHADOW:
			case TYPE_SAMPLER_EXTERNAL:
				kind = "SAMPLER";
				break;
			default:
				kind = "UNIFORM";
				break;
			}
			ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, sl->sym->name));
			if (sl->sym->address != -1)
			{
				int addr_end = sl->sym->address + _essl_get_type_size(sl->sym->type);
				if (addr_end > num_uniforms) num_uniforms = addr_end;
				fprintf(ctx->h_file, "#define %s_OFFSET_%s %d\n",
						kind, name, sl->sym->address);
			}
		}

		/* Varying offsets */
		fprintf(ctx->h_file, "\n");
		for (sl = ctx->tu->fragment_varyings ; sl != NULL ; sl = sl->next)
		{
			const char *name;
			ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, sl->sym->name));
			if (sl->sym->address != -1)
			{
				int addr_end = sl->sym->address + _essl_get_type_size(sl->sym->type);
				if (addr_end > num_varyings) num_varyings = addr_end;
				fprintf(ctx->h_file, "#define VARYING_OFFSET_%s %d /* size %d */\n",
						name, sl->sym->address,_essl_get_type_size(sl->sym->type) );
			}
		}

		fprintf(ctx->h_file,
				"\n#define MAX_NUM_UNIFORMS %d\n"
				"#define MAX_NUM_VARYINGS %d\n",
				num_uniforms, num_varyings);
	}

	return MEM_OK;
}


static void output_copyright_notice(FILE *f)
{
	fprintf(f, 
"/*\n"
" * This confidential and proprietary software may be used only as\n"
" * authorised by a licensing agreement from ARM Limited\n"
" * (C) COPYRIGHT 2006-2011 ARM Limited\n"
" * ALL RIGHTS RESERVED\n"
" * The entire notice above must be reproduced on all authorised\n"
" * copies and copies may only be made to the extent permitted\n"
" * by a licensing agreement from ARM Limited.\n"
" */\n"
		);


}

memerr _essl_piecegen_mali200_output_pieces(mempool *pool, error_context *err, output_buffer *out_buf, translation_unit *tu,
											char *h_file_name, char *c_file_name)
{
	piecegen_output_context pg_ctx, *ctx = &pg_ctx;
	memerr result;

	pg_ctx.pool = pool;
	pg_ctx.err = err;
	pg_ctx.out_buf = out_buf;
	pg_ctx.tu = tu;
	pg_ctx.n_pieces = 0;
	pg_ctx.n_phases = 0;
	pg_ctx.pieces = NULL;
	pg_ctx.phases = NULL;
	pg_ctx.next_piece = &pg_ctx.pieces;
	pg_ctx.next_phase = &pg_ctx.phases;
	ESSL_CHECK(_essl_dict_init(&ctx->phase_map, pool));

	if ((ctx->h_file = fopen(h_file_name, "w")) == 0) {
		(void)REPORT_ERROR(err, ERR_NOTE, 0, "Could not open output file %s\n", h_file_name);
		return MEM_ERROR;
	}
	
	if ((ctx->c_file = fopen(c_file_name, "w")) == 0) {
		(void)REPORT_ERROR(err, ERR_NOTE, 0, "Could not open output file %s\n", c_file_name);
		fclose(ctx->h_file);
		return MEM_ERROR;
	}
	output_copyright_notice(ctx->h_file);
	output_copyright_notice(ctx->c_file);

	result = collect_pieces(ctx);
	if (result) result = output_pieces(ctx);

	fclose(ctx->h_file);
	fclose(ctx->c_file);

	return result;
}
