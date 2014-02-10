/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_test_system.h"

#include "piecegen_maligp2/piecegen_maligp2_output.h"
#include "common/basic_block.h"


typedef struct _tag_option {
	struct _tag_option *next;
	string name;
	unsigned index;
} option;

typedef struct _tag_feature {
	struct _tag_feature *next;
	string name;
	unsigned n_options;
	option *options;
	dict option_map;
	unsigned shift;
	unsigned n_bits;
} feature;

typedef struct _tag_feature_option {
	struct _tag_feature_option *next;
	string fname;
	string oname;
	essl_bool all;
	feature *fe;
	option *op;
} feature_option;

typedef struct _tag_piece {
	struct _tag_piece *next;
	int index;
	symbol *function;
	const char *fun_name;
} piece;

typedef struct _tag_variant {
	struct _tag_variant *next;
	const char *name;
	const char *upcased_name;
	feature_option *features;
	unsigned n_pieces;
	unsigned piece_count;
	piece *pieces;
	unsigned use_count;
} variant;

typedef struct _tag_phase {
	struct _tag_phase *next;
	const char *name;
	const char *upcased_name;
	unsigned n_variants;
	variant *variants;
	dict variant_map;
	unsigned n_features;
	feature *features;
	dict feature_map;
	unsigned n_feature_bits;
} phase;

typedef struct _tag_piecegen_output_context {
	mempool *pool;
	error_context *err;
	output_buffer *out_buf;
	translation_unit *tu;
	symbol_list *uniforms;

	unsigned n_phases;
	phase *phases;
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

static option *new_option(piecegen_output_context *ctx, string name) {
	option *op;
	ESSL_CHECK(op = LIST_NEW(ctx->pool, option));
	op->name = name;
	return op;
}

static feature *new_feature(piecegen_output_context *ctx, string name) {
	feature *fe;
	string off = { "off", 3};
	ESSL_CHECK(fe = LIST_NEW(ctx->pool, feature));
	fe->name = name;
	ESSL_CHECK(_essl_dict_init(&fe->option_map, ctx->pool));
	ESSL_CHECK(fe->options = new_option(ctx, off));
	ESSL_CHECK(_essl_dict_insert(&fe->option_map, off, fe->options));
	fe->options->index = 0;
	fe->n_options = 1;
	return fe;
}

static feature_option *new_feature_option(piecegen_output_context *ctx, string fname, string oname, feature_option *next) {
	feature_option *fo;
	ESSL_CHECK(fo = LIST_NEW(ctx->pool, feature_option));
	fo->fname = fname;
	fo->oname = oname;
	fo->next = next;
	return fo;
}

static piece *new_piece(piecegen_output_context *ctx, int index, symbol *function) {
	piece *pi;
	ESSL_CHECK(pi = LIST_NEW(ctx->pool, piece));
	pi->index = index;
	pi->function = function;
	ESSL_CHECK(pi->fun_name = _essl_string_to_cstring(ctx->pool, function->name));
	return pi;
}

static variant *new_variant(piecegen_output_context *ctx, string name, feature_option *features) {
	variant *va;
	ESSL_CHECK(va = LIST_NEW(ctx->pool, variant));
	ESSL_CHECK(va->name = _essl_string_to_cstring(ctx->pool, name));
	ESSL_CHECK(va->upcased_name = upcase(ctx, va->name));
	va->features = features;
	return va;
}

static phase *new_phase(piecegen_output_context *ctx, string name) {
	phase *ph;
	ESSL_CHECK(ph = LIST_NEW(ctx->pool, phase));
	ESSL_CHECK(ph->name = _essl_string_to_cstring(ctx->pool, name));
	ESSL_CHECK(ph->upcased_name = upcase(ctx, ph->name));
	ESSL_CHECK(_essl_dict_init(&ph->variant_map, ctx->pool));
	ESSL_CHECK(_essl_dict_init(&ph->feature_map, ctx->pool));
	return ph;
}

static memerr function_error(piecegen_output_context *ctx, symbol *function, char *text) {
	const char *name = _essl_string_to_cstring(ctx->pool, function->name);
	ESSL_CHECK(name);
	_essl_error(ctx->err, ERR_SEM_FUNCTION_REDEFINITION, function->body->hdr.source_offset,
				text, name);
	return MEM_ERROR;
}

static memerr add_feature_option(piecegen_output_context *ctx, phase *ph, feature_option *fo) {
	feature *fe;
	option *op;
	fe = _essl_dict_lookup(&ph->feature_map, fo->fname);
	if (fe == 0) {
		ESSL_CHECK(fe = new_feature(ctx, fo->fname));
		LIST_INSERT_BACK(&ph->features, fe);
		ESSL_CHECK(_essl_dict_insert(&ph->feature_map, fo->fname, fe));
		ph->n_features++;
	}
	fo->fe = fe;

	if (fo->oname.len == 3 && strncmp(fo->oname.ptr, "all", 3) == 0) {
		fo->all = ESSL_TRUE;
		fo->op = 0;
	} else {
		op = _essl_dict_lookup(&fe->option_map, fo->oname);
		if (op == 0) {
			ESSL_CHECK(op = new_option(ctx, fo->oname));
			op->index = fe->n_options++;
			LIST_INSERT_BACK(&fe->options, op);
			ESSL_CHECK(_essl_dict_insert(&fe->option_map, fo->oname, op));
		}
		fo->op = op;
	}

	return MEM_OK;
}

static essl_bool variant_supports_feature(variant *va, feature *fe, option *op) {
	feature_option *fo;
	for (fo = va->features ; fo != 0 ; fo = fo->next) {
		if (fo->fe == fe && (fo->all || fo->op == op)) return ESSL_TRUE;
	}
	/* Unmentioned features only support 'off' option */
	return ESSL_FALSE; /* strncmp(op->name.ptr, "off", 3) == 0; */
}

static essl_bool is_feature_mask_valid(phase *ph, unsigned mask) {
	feature *fe;
	for (fe = ph->features ; fe != 0 ; fe = fe->next) {
		unsigned option_index = (mask >> fe->shift) & ((1 << fe->n_bits)-1);
		if (option_index >= fe->n_options) return ESSL_FALSE; /* Illegal mask */
	}
	return ESSL_TRUE;
}

static variant *get_variant_for_feature_mask(phase *ph, unsigned mask) {
	variant *va;
	feature *fe;
	option *op;

	for (va = ph->variants ; va != 0 ; va = va->next) {
		essl_bool all_features = ESSL_TRUE;
		for (fe = ph->features ; fe != 0 ; fe = fe->next) {
			unsigned option_index = (mask >> fe->shift) & ((1 << fe->n_bits)-1);
			for (op = fe->options ; op != 0 ; op = op->next) {
				if (op->index == option_index) break;
			}
			if (op == 0 || !variant_supports_feature(va, fe, op)) {
				all_features = ESSL_FALSE;
				break;
			} 
		}
		if (all_features) {
			return va;
		}
	}
	return 0;
}

static memerr parse_function_name(piecegen_output_context *ctx, symbol *function,
								  string *phase_name, string *variant_name, int *index,
								  feature_option **feature_list) {
	int i = 0;
	const char *name = _essl_string_to_cstring(ctx->pool, function->name);
	ESSL_CHECK(name);
	*index = -1;
	*feature_list = 0;
	phase_name->ptr = name;
	variant_name->ptr = name;
	while (name[i] != 0 && name[i] != '_') i++;
	phase_name->len = i;
	variant_name->len = i;
	while (name[i] == '_' && name[++i] == '_') {
		string fname, oname;
		fname.ptr = &name[++i];
		while (name[i] != 0 && name[i] != '_') i++;
		if (name[i] == 0) {
			*index = atoi(fname.ptr);
			break;
		}
		fname.len = &name[i]-fname.ptr;
		oname.ptr = &name[++i];
		while (name[i] != 0 && name[i] != '_') i++;
		oname.len = &name[i]-oname.ptr;

		ESSL_CHECK(*feature_list = new_feature_option(ctx, fname, oname, *feature_list));

		variant_name->len = i;
	}

	if (name[i] != 0) {
		return function_error(ctx, function, "Syntax error in piece function name %s\n");
	}

	return MEM_OK;
}

static memerr build_piece_structs(piecegen_output_context *ctx) {
	symbol_list *fun_list;
	for (fun_list = ctx->tu->functions ; fun_list != 0 ; fun_list = fun_list->next) {
		symbol *function = fun_list->sym;
		string phase_name;
		string variant_name;
		int index;
		phase *ph;
		variant *va;
		piece *pi;
		feature_option *fo;
		ESSL_CHECK(parse_function_name(ctx, function, &phase_name, &variant_name, &index, &fo));

		/* Find phase */
		ph = _essl_dict_lookup(&ctx->phase_map, phase_name);
		if (ph == 0) {
			/* New phase */
			ESSL_CHECK(ph = new_phase(ctx, phase_name));
			LIST_INSERT_BACK(&ctx->phases, ph);
			ESSL_CHECK(_essl_dict_insert(&ctx->phase_map, phase_name, ph));
			ctx->n_phases++;
		}

		/* Find variant */
		va = _essl_dict_lookup(&ph->variant_map, variant_name);
		if (va == 0) {
			/* New variant */
			ESSL_CHECK(va = new_variant(ctx, variant_name, fo));
			LIST_INSERT_BACK(&ph->variants, va);
			ESSL_CHECK(_essl_dict_insert(&ph->variant_map, variant_name, va));
			ph->n_variants++;
		}

		/* New piece */
		ESSL_CHECK(pi = new_piece(ctx, index, function));
		/* Insert into piece list */
		{
			piece **plist;
			for (plist = &va->pieces ; *plist != 0 ; plist = &(*plist)->next) {
				if ((*plist)->index == pi->index) {
					if (pi->index == -1) {
						return function_error(ctx, function, "Same variant defined more than once: %s\n");
					} else {
						return function_error(ctx, function, "Same index defined more than once: %s\n");
					}
				} else if (pi->index == -1 || (*plist)->index == -1) {
					return function_error(ctx, function, "Both indexed and non-indexed variant defined: %s\n");
				}
				if (pi->index < (*plist)->index) break;
			}
			pi->next = *plist;
			*plist = pi;
			if (pi->index >= 0 && va->n_pieces < (unsigned)pi->index+1)
			{
				va->n_pieces = pi->index+1;
			}
			va->piece_count++;
		}

		/* Add all feature options to phase */
		while (fo != 0) {
			ESSL_CHECK(add_feature_option(ctx, ph, fo));
			fo = fo->next;
		}
	}

	return MEM_OK;
}

static memerr check_piece_structs(piecegen_output_context *ctx) {
	phase *ph;
	variant *va;
	feature *fe;

	/* Check for missing pieces */
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		for (va = ph->variants ; va != 0 ; va = va->next) {
			if (va->n_pieces > 0 && va->piece_count != va->n_pieces) {
				_essl_error(ctx->err, ERR_LINK_NO_ENTRY_POINT, 0,
							"Variant %s has missing pieces\n", va->name);
				return MEM_ERROR;
			}
		}
	}

	/* Calculate bit masks */
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		int shift = 0;
		for (fe = ph->features ; fe != 0 ; fe = fe->next) {
			unsigned bits = fe->n_options-1;
			unsigned n_bits = 0;
			while (bits > 0) {
				n_bits++;
				bits = bits >> 1;
			}
			fe->shift = shift;
			fe->n_bits = n_bits;
			shift += n_bits;

			if (fe->n_options == 1) {
				const char *name;
				ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, fe->name));
				ESSL_CHECK(_essl_warning(ctx->err, ERR_SEM_FUNCTION_REDEFINITION, 0,
									"No options defined for feature %s in phase %s\n", name, ph->name));
			}
		}
		ph->n_feature_bits = shift;
	}

	return MEM_OK;
}

static memerr output_piece_structs(piecegen_output_context *ctx) {
	const u32 *data_ptr = _essl_output_buffer_get_raw_pointer(ctx->out_buf);
	phase *ph;
	variant *va;
	piece *pi;
	int si;      /* signed counter */
	unsigned ui; /* unsigned counter */
	int min_code_address = 1000000;
	unsigned next_piece_id = 1;
	unsigned total_bits;

	/* Output header file */
	fprintf(ctx->h_file,
			"/* This file is generated by the shader piece generator. */\n"
			"\n"
			"#ifndef MALIGP2_SHADERGEN_PIECES_H\n"
			"#define MALIGP2_SHADERGEN_PIECES_H\n"
			"\n"
			"typedef struct {\n"
			"    unsigned int data[4];\n"
			"} instruction;\n"
			"\n"
			"typedef struct {\n"
			"    unsigned id;\n"
			"    unsigned len;\n"
			"    const instruction *ptr;\n"
			"} piece;\n"
			"\n"
			"typedef struct {\n"
			"    unsigned address;\n"
			"    float value;\n"
			"} uniform_initializer;\n"
			"\n"
			"#define VERTEX_UNIFORM_POS(name) (VERTEX_UNIFORM_POS_##name)\n"
			"#define VERTEX_VARYING_POS(name) (VERTEX_VARYING_POS_##name)\n"
			"#define VERTEX_ATTRIBUTE_POS(name) (VERTEX_ATTRIBUTE_POS_##name)\n"
			"\n"
		);

	/* Output phase definitions */
	ui = 0;
	fprintf(ctx->h_file, "/* Phases */\n");
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		fprintf(ctx->h_file, "#define VERTEX_PHASE_%s %d\n", ph->upcased_name, ui++);
	}
	fprintf(ctx->h_file, "\n");

	/* Output feature bitfield definitions */
	total_bits = 0;

	fprintf(ctx->h_file,
			"/* Definitions for the bitfield of features. Current scheme assumes < 32 bits worth of features. */\n"
			"/*  - PHASE masks is the collection of all feature bits within that group. Always in word 0, offset 0 */\n"
			"/*  - FEATURE masks cover the bits used by a single feature. Always in word 0, at a given mask */\n"
			"/* These bits are used for setting up the VERTEX_SHADERGEN_FEATURE_* bits, as well as inside the shadergenerator */\n"
			"\n");

	for (ph = ctx->phases ; ph != 0; ph = ph->next) {
		feature *fe;
		if (ph->n_feature_bits != 0) {
			fprintf(ctx->h_file, 
					"#define _VERTEX_PHASE_%s_MASK   0x%xU\n"
					"#define _VERTEX_PHASE_%s_OFFSET 0x%xU\n",
					ph->upcased_name, ((1U << ph->n_feature_bits) - 1), 
					ph->upcased_name, total_bits);

		}
		for (fe = ph->features ; fe != 0 ; fe = fe->next) {
			const char *fn, *ucfn;
			ESSL_CHECK(fn = _essl_string_to_cstring(ctx->pool, fe->name));
			ESSL_CHECK(ucfn = upcase(ctx, fn));
			fprintf(ctx->h_file, 
					"#define _VERTEX_FEATURE_%s_%s_MASK 0x%xU\n"
					"#define _VERTEX_FEATURE_%s_%s_OFFSET %d\n",
					ph->upcased_name, ucfn, (1U << fe->n_bits) - 1,
					ph->upcased_name, ucfn, total_bits);
			total_bits += fe->n_bits;
		}
	}
	fprintf(ctx->h_file, "\n");

	fprintf(ctx->h_file, "/* Enum values that can be placed into vertex_shadergen_encode/decode. */\n"
	                     "/* They complement the ones found inside shadergen_state.h . The only */\n"
						 "/* reason they are defined here is that subword 0 depend on the autogenerated locations */\n"
						 "/* provided by the piecegen, and the featurebits are part of that. */\n");

	for (ph = ctx->phases ; ph != 0; ph = ph->next) {
		feature *fe;
		if (ph->n_feature_bits != 0) {
			fprintf(ctx->h_file,
					"#define VERTEX_SHADERGEN_FEATURES_%s %*s 0, _VERTEX_PHASE_%s_MASK, _VERTEX_PHASE_%s_OFFSET\n",
					ph->upcased_name, (int)(30-strlen(ph->upcased_name)), "", 
					ph->upcased_name, ph->upcased_name);
			/* Currently no need for VERTEX_SET_PHASE_*, but it can be added if needed */
		}
		for (fe = ph->features ; fe != 0 ; fe = fe->next) {
			const char *fn, *ucfn;
			ESSL_CHECK(fn = _essl_string_to_cstring(ctx->pool, fe->name));
			ESSL_CHECK(ucfn = upcase(ctx, fn));
			fprintf(ctx->h_file,
					"#define VERTEX_SHADERGEN_FEATURE_%s_%s %*s 0, _VERTEX_FEATURE_%s_%s_MASK, _VERTEX_FEATURE_%s_%s_OFFSET\n",
					ph->upcased_name, ucfn, (int)(30-strlen(ph->upcased_name)-strlen(ucfn)), "", 
					ph->upcased_name, ucfn, ph->upcased_name, ucfn);
			total_bits += fe->n_bits;
		}
	}
	fprintf(ctx->h_file, "\n");

	/* Output types for features */
	for (ph = ctx->phases ; ph != 0; ph = ph->next) {
		feature *fe;
		for (fe = ph->features ; fe != 0 ; fe = fe->next) {
			option *op;
			essl_bool need_enum = ESSL_FALSE;
			/* Only output if we have a non-boolean option i.e. something other than on/off */
			for (op = fe->options ; op != 0 ; op = op->next) {
				const char *on;
				ESSL_CHECK(on =_essl_string_to_cstring(ctx->pool, op->name));
				if (strcmp(on, "on") != 0 && strcmp(on, "off") != 0) {
					need_enum = ESSL_TRUE;
					break;
				}
			}
			if (need_enum) {
				const char *fn, *ucfn;
				ESSL_CHECK(fn = _essl_string_to_cstring(ctx->pool, fe->name));
				ESSL_CHECK(ucfn = upcase(ctx, fn));
				fprintf(ctx->h_file, "typedef enum {\n");
				/* Output the enum */
				for (op = fe->options ; op != 0 ; op = op->next) {
					const char *on, *ucon;
					ESSL_CHECK(on =_essl_string_to_cstring(ctx->pool, op->name));
					ESSL_CHECK(ucon = upcase(ctx, on));
					fprintf(ctx->h_file, "\t%s_%s", ucfn, ucon);
					if (op->next != 0) {
						fprintf(ctx->h_file, ",");
					}
					fprintf(ctx->h_file, "\n");
				}
				fprintf(ctx->h_file, "} %s_%s;\n\n", ph->name, fn);
			}
		}
	}

	/* Output variable positions definitions */
	fprintf(ctx->h_file, "/* Uniforms */\n");
	{
		dict group_min;
		dict group_max;
		symbol_list *sl;
		ESSL_CHECK(_essl_dict_init(&group_min, ctx->pool));
		ESSL_CHECK(_essl_dict_init(&group_max, ctx->pool));
		for (sl = ctx->uniforms ; sl != 0 ; sl = sl->next) {
			symbol *u = sl->sym;
			if (u->address != -1) {
				const char *name;
				string group = sl->sym->qualifier.group;
				int min_addr = 99999;
				int max_addr = 0;
				int end_address = u->address + ctx->tu->desc->get_type_size(ctx->tu->desc, u->type, ADDRESS_SPACE_UNIFORM);

				if (_essl_dict_has_key(&group_min, group))
				{
					min_addr = (int)(long)_essl_dict_lookup(&group_min, group);
				}
				if (_essl_dict_has_key(&group_max, group))
				{
					max_addr = (int)(long)_essl_dict_lookup(&group_max, group);
				}
				if (u->address < min_addr) min_addr = u->address;
				if (end_address > max_addr) max_addr = end_address;

				ESSL_CHECK(_essl_dict_insert(&group_min, group, (void *)(long)min_addr));
				ESSL_CHECK(_essl_dict_insert(&group_max, group, (void *)(long)max_addr));

				ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, u->name));
				if (name[0] != '?') {
					fprintf(ctx->h_file, "#define VERTEX_UNIFORM_POS_%s %d\n", name, u->address);
				}
			}
		}
		fprintf(ctx->h_file, "\n");
		fprintf(ctx->h_file, "/* Uniform groups */\n");
		{
			string groups[5] = {
				{ "", 0 },
				{ "basic", 5 },
				{ "lighting", 8 },
				{ "textrans", 8 },
				{ "zkinning", 8 }
			};
			int i;
			for (i = 0 ; i < 5 ; i++)
			{
				int min_addr = (int)(long)_essl_dict_lookup(&group_min, groups[i]);
				int max_addr = (int)(long)_essl_dict_lookup(&group_max, groups[i]);
				const char *gname = i == 0 ? "constant" : groups[i].ptr;
				fprintf(ctx->h_file, "#define VERTEX_UNIFORM_GROUP_POS_%s %d\n", gname, min_addr);
				fprintf(ctx->h_file, "#define VERTEX_UNIFORM_GROUP_SIZE_%s %d\n", gname, max_addr-min_addr);
			}
		}
	}
	fprintf(ctx->h_file, "\n");
	fprintf(ctx->h_file, "/* Varyings */\n");
	{
		symbol_list *sl;
		for (sl = ctx->tu->vertex_varyings ; sl != 0 ; sl = sl->next) {
			symbol *u = sl->sym;
			if (u->address != -1) {
				const char *name;
				ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, u->name));
				if (name[0] != '?') {
					fprintf(ctx->h_file, "#define VERTEX_VARYING_POS_%s %d\n", name, u->address);
				}
			}
		}
	}
	fprintf(ctx->h_file, "\n");
	fprintf(ctx->h_file, "/* Attributes */\n");
	{
		symbol_list *sl;
		for (sl = ctx->tu->vertex_attributes ; sl != 0 ; sl = sl->next) {
			symbol *u = sl->sym;
			if (u->address != -1) {
				const char *name;
				ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, u->name));
				if (name[0] != '?') {
					fprintf(ctx->h_file, "#define VERTEX_ATTRIBUTE_POS_%s %d\n", name, u->address);
				}
			}
		}
	}
	fprintf(ctx->h_file, "\n");


	/* Output getter function prototypes */
	fprintf(ctx->h_file,
			"const piece *_piecegen_get_piece(unsigned phase, unsigned features);\n"
			"const piece *_piecegen_get_indexed_piece(unsigned phase, unsigned features, unsigned index);\n"
			"const uniform_initializer *_piecegen_get_uniform_initializers(unsigned *n_inits);\n"
			"const unsigned *_piecegen_get_serialized_data(unsigned *n_words);\n"
			"\n"
		);

	/* End of header file */
	fprintf(ctx->h_file,
			"#endif\n"
		);

	/* Output data file */
	fprintf(ctx->c_file,
			"/* This file is generated by the shader piece generator. */\n"
			"\n"
			"typedef struct {\n"
			"    unsigned int data[4];\n"
			"} instruction;\n"
			"\n"
			"typedef struct {\n"
			"    unsigned id;\n"
			"    unsigned len;\n"
			"    const instruction *ptr;\n"
			"} piece;\n"
			"\n"
			"typedef struct {\n"
			"    unsigned address;\n"
			"    float value;\n"
			"} uniform_initializer;\n"
			"\n"
			"typedef struct {\n"
			"    const piece *p;\n"
			"    unsigned n_pieces;\n"
			"} variant;\n"
			"\n"
		);

	/* Output pieces */
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		for (va = ph->variants ; va != 0 ; va = va->next) {
			for (pi = va->pieces ; pi != 0 ; pi = pi->next) {
				control_flow_graph *cfg = pi->function->control_flow_graph;
				int n_instructions = (cfg->end_address - cfg->start_address)/4;
				const u32 *ptr = &data_ptr[cfg->start_address];
				int i;

				if (cfg->start_address < min_code_address) min_code_address = cfg->start_address;
				if(n_instructions > 0)
				{
					fprintf(ctx->c_file, "static const instruction _piece_%s[] = {", pi->fun_name);
					for (i = 0 ; i < n_instructions ; i++) {
						fprintf(ctx->c_file, "%s\n    { { 0x%08X, 0x%08X, 0x%08X, 0x%08X } }", i == 0 ? "" : ",", ptr[i*4+0], ptr[i*4+1], ptr[i*4+2], ptr[i*4+3]);
					}
					fprintf(ctx->c_file, "\n};\n\n");
				}
			}
		}
	}

	/* Output piece lists */
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		for (va = ph->variants ; va != 0 ; va = va->next) {
			fprintf(ctx->c_file, "static const piece _pieces_%s[] = {\n", va->name);
			for (pi = va->pieces ; pi != 0 ; pi = pi->next) {
				control_flow_graph *cfg = pi->function->control_flow_graph;
				int n_instructions = (cfg->end_address - cfg->start_address)/4;
				fprintf(ctx->c_file, "    { %3d, %3d, %s%s }%s\n", next_piece_id++, n_instructions, n_instructions > 0 ? "_piece_" : "", n_instructions > 0 ? pi->fun_name : "0", pi->next ? "," : "");
			}
			fprintf(ctx->c_file, "};\n");

			fprintf(ctx->c_file, "static const variant _variant_%s = { _pieces_%s, %d };\n\n", va->name, va->name, va->n_pieces);
		}
	}

	/* Output phase feature tables */
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		unsigned mask;
		fprintf(ctx->c_file, "static const variant *_phase_%s[] = {", ph->name);
		for (mask = 0 ; mask < (1U << (unsigned)ph->n_feature_bits) ; mask++) {
			if (is_feature_mask_valid(ph, mask)) {
				va = get_variant_for_feature_mask(ph, mask);
				if (va == 0) {
					/* Missing piece */
					char feature_string[500];
					int i = 0;
					feature *fe;
					option *op;
					for (fe = ph->features ; fe != 0 ; fe = fe->next) {
						unsigned option_index = (mask >> fe->shift) & ((1 << fe->n_bits)-1);
						const char *fe_name, *op_name;
						for (op = fe->options ; op != 0 ; op = op->next) {
							if (op->index == option_index) break;
						}
						assert(op != 0);
						ESSL_CHECK(fe_name = _essl_string_to_cstring(ctx->pool, fe->name));
						ESSL_CHECK(op_name = _essl_string_to_cstring(ctx->pool, op->name));
						i += snprintf(&feature_string[i], 500-i, "%s=%s%s", fe_name, op_name, fe->next ? ", " : "");
					}
					ESSL_CHECK(_essl_warning(ctx->err, ERR_SEM_FUNCTION_REDEFINITION, 0,
										"No shader defined for phase %s with features %s\n", ph->name, feature_string));
				}
			} else {
				va = 0;
			}
			if (va == 0) {
				fprintf(ctx->c_file, "%s\n    0", mask == 0 ? "" : ",");
			} else {
				fprintf(ctx->c_file, "%s\n    &_variant_%s", mask == 0 ? "" : ",", va->name);
				va->use_count++;
			}
		}
		fprintf(ctx->c_file, "\n};\n\n");
	}

	/* Output phase list */
	fprintf(ctx->c_file, "static const variant **_phases[] = {\n");
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		fprintf(ctx->c_file, "    _phase_%s%s\n", ph->name, ph->next ? "," : "");
	}
	fprintf(ctx->c_file, "};\n\n");

	/* Phase feature table size */
	fprintf(ctx->c_file, "static const unsigned _phase_size[] = {\n    ");
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		fprintf(ctx->c_file, "%d%s", 1 << ph->n_feature_bits, ph->next ? ", " : "\n");
	}
	fprintf(ctx->c_file, "};\n\n");

	/* Number of phases */
	fprintf(ctx->c_file, "static const unsigned _n_phases = %d;\n\n", ctx->n_phases);

	/* Uniform initializers */
	{
		int n_uniform_inits = 0;
		symbol_list *sl;

		fprintf(ctx->c_file, "/* Uniform initializers */\n");
		fprintf(ctx->c_file, "static const uniform_initializer _uniform_initializers[] = {");
		for (sl = ctx->uniforms ; sl != 0 ; sl = sl->next) {
			symbol *u = sl->sym;
			if (u->address_space == ADDRESS_SPACE_UNIFORM && u->address != -1) {
				if (u->body != 0 && u->body->hdr.kind == EXPR_KIND_CONSTANT) {
					unsigned i;
					unsigned size = _essl_get_type_size(u->body->hdr.type);
					for(i = 0; i < size; ++i)
					{
						float value = ctx->tu->desc->scalar_to_float(u->body->expr.u.value[i]);
						fprintf(ctx->c_file, "%s\n    { %d, %f }", n_uniform_inits == 0 ? "" : ",", u->address+i, value);
						n_uniform_inits++;
					}
				}
			}
		}
		fprintf(ctx->c_file, "\n};\n\n");

		/* Number of uniform initializers */
		fprintf(ctx->c_file, "static const unsigned _n_uniform_initializers = %d;\n\n", n_uniform_inits);
	}

	/* Serialized attribute and varying data */
	fprintf(ctx->c_file, "/* Serialized data */\n");
	fprintf(ctx->c_file, "static const unsigned _serialized[] = {");
	for (si = 0 ; si < min_code_address ; si++) {
		fprintf(ctx->c_file, "%s0x%08X", si == 0 ? "\n    " : (si % 4) == 0 ? ",\n    " : ",", data_ptr[si]);
	}
	fprintf(ctx->c_file, "\n};\n\n");

	/* Amount of serialized data */
	fprintf(ctx->c_file, "static const unsigned _n_serialized_words = %d;\n\n", min_code_address);

	/* Output getter functions */
	fprintf(ctx->c_file,
			"const piece *_piecegen_get_piece(unsigned phase, unsigned features)\n"
			"{\n"
			"    if (phase >= _n_phases) return 0;\n"
			"    if (features >= _phase_size[phase]) return 0;\n"
			"    if (_phases[phase][features]->n_pieces != 0) return 0;\n"
			"    return _phases[phase][features]->p;\n"
			"}\n"
			"\n"
			"const piece *_piecegen_get_indexed_piece(unsigned phase, unsigned features, unsigned index)\n"
			"{\n"
			"    if (phase >= _n_phases) return 0;\n"
			"    if (features >= _phase_size[phase]) return 0;\n"
			"    if (index >= _phases[phase][features]->n_pieces) return 0;\n"
			"    return &_phases[phase][features]->p[index];\n"
			"}\n"
			"\n"
			"const uniform_initializer *_piecegen_get_uniform_initializers(unsigned *n_inits)\n"
			"{\n"
			"    *n_inits = _n_uniform_initializers;\n"
			"    return _uniform_initializers;\n"
			"}\n"
			"\n"
			"const unsigned *_piecegen_get_serialized_data(unsigned *n_words)\n"
			"{\n"
			"    *n_words = _n_serialized_words;\n"
			"    return _serialized;\n"
			"}\n"
			"\n"
		);

	/* Check for unused variants */
	for (ph = ctx->phases ; ph != 0 ; ph = ph->next) {
		for (va = ph->variants ; va != 0 ; va = va->next) {
			if (va->use_count == 0) {
				ESSL_CHECK(_essl_warning(ctx->err, ERR_SEM_FUNCTION_REDEFINITION, 0,
									"Shader %s not used for any feature combination\n", va->name));
			}
		}
	}

	return MEM_OK;
}

static void output_copyright_notice(FILE *f)
{
	fprintf(f, 
"/*\n"
" * This confidential and proprietary software may be used only as\n"
" * authorised by a licensing agreement from ARM Limited\n"
" * (C) COPYRIGHT 2007-2011 ARM Limited\n"
" * ALL RIGHTS RESERVED\n"
" * The entire notice above must be reproduced on all authorised\n"
" * copies and copies may only be made to the extent permitted\n"
" * by a licensing agreement from ARM Limited.\n"
" */\n"
		);


}

memerr _essl_piecegen_maligp2_output_pieces(mempool *pool, error_context *err, output_buffer *out_buf, translation_unit *tu,
											symbol_list *uniforms,
											char *h_file_name, char *c_file_name)
{
	piecegen_output_context pg_ctx, *ctx = &pg_ctx;
	memerr result;

	pg_ctx.pool = pool;
	pg_ctx.err = err;
	pg_ctx.out_buf = out_buf;
	pg_ctx.tu = tu;
	pg_ctx.uniforms = uniforms;
	pg_ctx.n_phases = 0;
	pg_ctx.phases = 0;
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
	result = build_piece_structs(ctx);
	if (result) result = check_piece_structs(ctx);
	if (result) result = output_piece_structs(ctx);

	fclose(ctx->h_file);
	fclose(ctx->c_file);

	return result;
}
