/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/basic_block.h"
#include "piecegen_mali200/piecegen_mali200_driver.h"
#include "piecegen_mali200/piecegen_mali200_output.h"
#include "mali200/mali200_driver.h"

static memerr rewrite_loadstores(mempool *pool, storeload_list *ls, dict *alloc_for_name)
{
	for (; ls != NULL ; ls = ls->next)
	{
		symbol *sym = GET_CHILD(ls->n, 0)->expr.u.sym;
		register_allocation *alloc = _essl_dict_lookup(alloc_for_name, sym->name);
		ls->n->expr.u.load_store.address_space = ADDRESS_SPACE_REGISTER;
		ls->n->expr.u.load_store.alloc = alloc;
	}

	return MEM_OK;
}

static memerr allocate_parameters_to_registers(mempool *pool, translation_unit *tu)
{
	symbol_list *sl;
	int next_reg = 0;
	dict alloc_for_name;
	ESSL_CHECK(_essl_dict_init(&alloc_for_name, pool));
	for(sl = tu->functions; sl != 0; sl = sl->next)
	{
		control_flow_graph *cfg = sl->sym->control_flow_graph;
		parameter *param;
		for (param = cfg->parameters ; param != NULL ; param = param->next)
		{
			if (!_essl_dict_has_key(&alloc_for_name, param->sym->name))
			{
				register_allocation *alloc;
				ESSL_CHECK(alloc = _essl_mempool_alloc(pool, sizeof(register_allocation)));
				alloc->reg = next_reg++;
				assert(param->sym->type->basic_type == TYPE_FLOAT);
				alloc->swizzle = _essl_create_identity_swizzle(param->sym->type->u.basic.vec_size);
				ESSL_CHECK(_essl_dict_insert(&alloc_for_name, param->sym->name, alloc));
			}

			ESSL_CHECK(rewrite_loadstores(pool, param->load, &alloc_for_name));
			ESSL_CHECK(rewrite_loadstores(pool, param->store, &alloc_for_name));
		}
	}

	return MEM_OK;
}

#define FILENAME_BUF_SIZE 1000
static char h_filename[FILENAME_BUF_SIZE] = {0};
static char c_filename[FILENAME_BUF_SIZE] = {0};

memerr _essl_piecegen_mali200_set_filename(const char *name)
{
	char tmp[FILENAME_BUF_SIZE];
	int len;
	const char *endptr = 0;
	endptr = strstr(name, ".fragp");
	if(endptr == NULL)
	{
		endptr = name + strlen(name);
	}
	len = endptr - name;
	memcpy(tmp, name, len);
	tmp[len] = '\0';
	
	ESSL_CHECK(endptr - name + 10 < FILENAME_BUF_SIZE); /* enough space */
	snprintf(c_filename, FILENAME_BUF_SIZE, "%s.c", tmp);
	snprintf(h_filename, FILENAME_BUF_SIZE, "%s.h", tmp);

	return MEM_OK;
}

memerr _essl_piecegen_mali200_driver(mempool *pool,  error_context *err,  typestorage_context *ts_ctx, 
									 struct _tag_target_descriptor *desc,  translation_unit *tu, output_buffer *out_buf,
									 compiler_options *opts)
{
	ESSL_CHECK(allocate_parameters_to_registers(pool, tu));
	ESSL_CHECK(_essl_mali200_driver(pool, err, ts_ctx, desc, tu, out_buf, opts));

	ESSL_CHECK(_essl_piecegen_mali200_output_pieces(pool, err, out_buf, tu, 
													h_filename,
													c_filename));

	return MEM_OK;
}
