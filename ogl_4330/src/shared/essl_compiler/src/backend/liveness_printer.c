/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "backend/liveness_printer.h"
#include "backend/liveness.h"
#include "common/unique_names.h"
#include "backend/extra_info.h"


static void print_swizzle(FILE *f, swizzle_pattern *swz, unsigned register_width) {
	char swzid[] = "?xyzw456789abcdef";
	unsigned i;
	for (i = 0 ; i < register_width ; i++) {
		fprintf(f, "%c", swzid[swz->indices[i]+1]);
	}
}

memerr _essl_liveness_printer(FILE *f, live_range *range, unique_name_context *names, unsigned register_width) {
	/* No file, no output */
	if (f == 0) return MEM_OK;

	for (; range != 0 ; range = range->next) {
		live_delimiter *delim;
		const char *name;
		ESSL_CHECK(name = _essl_unique_name_get_or_create(names, range->var));
		if (HAS_EXTRA_INFO(range->var)) {
			node_extra *info = EXTRA_INFO(range->var);
			char spill_status = range->spilled ? '-' : range->spill_range ? '+' : range->potential_spill ? '?' : ' ';
			fprintf(f, "%c%8s (%p): r%d.", spill_status, name, range->var, info->out_reg);
			print_swizzle(f, &info->reg_swizzle, register_width);
		} else {
			fprintf(f, "%8s:        ", name);
		}
		for (delim = range->points ; delim != 0 ; delim = delim->next) {
			char mask[20];
			int i;
			switch (delim->kind) {
			case LIVE_UNKNOWN:
				fprintf(f, "?");
				break;
			case LIVE_DEF:
				fprintf(f, "def");
				break;
			case LIVE_USE:
				fprintf(f, "use");
				break;
			case LIVE_STOP:
				fprintf(f, "--]");
				break;
			case LIVE_RESTART:
				fprintf(f, "[--");
				break;
			}

			i = 0;
			if (delim->next) {
				char swzs[] = "xyzw456789abcdef";
				unsigned j;
				int lm = delim->next->live_mask;
				mask[i++] = '<';
				for(j = 0; j < register_width; ++j)
				{
					mask[i++] = lm&(1<<j) ? swzs[j] : ' ';
				}
				mask[i++] = '>';
			}
			mask[i++] = 0;

			fprintf(f, " %3d%s %s  ", delim->position, delim->locked ? "*" : " ", mask);
		}
		if (range->var->hdr.kind == EXPR_KIND_UNARY && range->var->expr.operation == EXPR_OP_SPILL)
		{
			const char *spilled_name;
			ESSL_CHECK(spilled_name = _essl_unique_name_get_or_create(names, GET_CHILD(range->var, 0)));
			fprintf(f, " (spilled %s)", spilled_name);
		}
		fprintf(f, "\n");
	}
	fprintf(f, "\n\n");

	return MEM_OK;
}

