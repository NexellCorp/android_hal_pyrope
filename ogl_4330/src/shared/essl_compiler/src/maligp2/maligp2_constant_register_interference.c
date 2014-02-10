/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "maligp2/maligp2_constant_register_interference.h"
#include "backend/interference_graph.h"

/* Code to work around maligp2_constant_reg_readwrite_workaround - 
   Bug in some memory implementations which causes the result of a constant register read
   at the same time as a write to the same register to be undefined.
*/

typedef enum {
	INTERFERENCE_KIND_LOAD,
	INTERFERENCE_KIND_STORE
} interference_kind;

typedef struct symbol_interference_list {
	struct symbol_interference_list *next;
	interference_kind kind;
	const symbol *sym;
	int subcycle;
} symbol_interference_list;

ASSUME_ALIASING(generic_list, symbol_interference_list);


static int descending_subcycle_order(generic_list *e1, generic_list *e2)
{
	symbol_interference_list *a = (symbol_interference_list*)e1;
	symbol_interference_list *b = (symbol_interference_list*)e2;

	return b->subcycle - a->subcycle;
}


static symbol *find_symbol_for_maligp2_address_node(node *n)
{
	if(n == NULL) return NULL;
	if(n->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE &&
	   (n->hdr.kind != EXPR_KIND_BINARY || n->expr.operation != EXPR_OP_ADD)
		   )
	{
		return NULL;
	}

	while(n!=0 && n->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE 
	      && GET_N_CHILDREN(n) > 0)
	{
		n = GET_CHILD(n, 0);
		if(n != 0)
		{
			/* if the child isn't a reasonable node, abort */
			if( n->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE &&
				(n->hdr.kind != EXPR_KIND_BINARY || n->expr.operation != EXPR_OP_ADD)
				)
			{
				
				return NULL;
			}
		}
	}


	if(n == NULL) return NULL;
	if(n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		assert(n->expr.u.sym != NULL);
		return n->expr.u.sym;
	}
	return NULL;

}


memerr extract_store_list_for_block(mempool *pool, symbol_interference_list **lst, basic_block *b)
{
	maligp2_instruction_word *word;
	for(word = b->latest_instruction_word; word != NULL; word = word->predecessor)
	{
		if((word->u.real_slots.store_xy != NULL && word->u.real_slots.store_xy->opcode == MALIGP2_STORE_CONSTANT) 
			|| (word->u.real_slots.store_zw != NULL && word->u.real_slots.store_zw->opcode == MALIGP2_STORE_CONSTANT))
		{
			symbol_interference_list *tmp;
			ESSL_CHECK(tmp = _essl_mempool_alloc(pool, sizeof(*tmp)));
			tmp->sym = NULL;
			tmp->subcycle = MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 2);
			tmp->kind = INTERFERENCE_KIND_STORE;
			if(word->u.real_slots.lut != NULL && word->u.real_slots.lut->opcode == MALIGP2_SET_ADDRESS)
			{
				/* If we can find the address, use it. Otherwise, use a wildcard node */
				tmp->sym = find_symbol_for_maligp2_address_node(word->u.real_slots.lut->args[0].arg);
			}
			LIST_INSERT_FRONT(lst, tmp);

		}
		
	}

	return MEM_OK;
}

memerr extract_store_list_for_function(mempool *pool, symbol_interference_list **lst, control_flow_graph *cfg)
{
	int i;
	for(i = cfg->n_blocks-1; i >= 0; --i)
	{
		basic_block *b = cfg->output_sequence[i];

		ESSL_CHECK(extract_store_list_for_block(pool, lst, b));
	}

	return MEM_OK;
}


memerr extract_store_list(mempool *pool, symbol_interference_list **lst, translation_unit *tu)
{
	symbol_list *sl;
	for(sl = tu->functions; sl != NULL; sl = sl->next)
	{
		symbol *fun = sl->sym;
		ESSL_CHECK(extract_store_list_for_function(pool, lst, fun->control_flow_graph));
	}
	return MEM_OK;
}


memerr extract_load_list(mempool *pool, symbol_interference_list **lst, maligp2_relocation *relocations)
{
	maligp2_relocation *reloc;

	for(reloc = relocations; reloc != 0; reloc = reloc->next)
	{

		if(reloc->source_symbol->kind == SYM_KIND_VARIABLE && reloc->target == MALIGP2_RELOC_ADDRESS_OFFSET)
		{
			symbol_interference_list *tmp;
			switch(reloc->source_symbol->address_space)
			{
			case ADDRESS_SPACE_THREAD_LOCAL:
			case ADDRESS_SPACE_GLOBAL:
			case ADDRESS_SPACE_UNIFORM:
				ESSL_CHECK(tmp = _essl_mempool_alloc(pool, sizeof(*tmp)));
				tmp->sym = reloc->source_symbol;
				tmp->subcycle = reloc->u.instr->subcycle;
				tmp->kind = INTERFERENCE_KIND_LOAD;
				LIST_INSERT_FRONT(lst, tmp);
				break;
			
			case ADDRESS_SPACE_VERTEX_VARYING:
			case ADDRESS_SPACE_ATTRIBUTE:
			case ADDRESS_SPACE_UNKNOWN:
			case ADDRESS_SPACE_FRAGMENT_VARYING:
			case ADDRESS_SPACE_FRAGMENT_SPECIAL:
			case ADDRESS_SPACE_FRAGMENT_OUT:
				break;
			}
		}
	}

	return MEM_OK;
}

memerr build_interference_graph(interference_graph_context *ctx, symbol_interference_list *lst)
{

	/* now scan through the list looking for stores, and when we find a store, look for a corresponding load the exact number of pipeline steps after that will conflict */

	{
		symbol_interference_list *s;
		for(s = lst; s != NULL; s = s->next)
		{
			if(s->kind == INTERFERENCE_KIND_STORE)
			{
				int target_subcycle = s->subcycle - MALIGP2_CYCLE_TO_SUBCYCLE(3, 0);
				symbol_interference_list *l;
				for(l = s; l != NULL && l->subcycle >= target_subcycle; l = l->next)
				{
					if(l->subcycle == target_subcycle)
					{
						if(s->sym != NULL && l->sym != NULL)
						{
							ESSL_CHECK(_essl_interference_graph_register_edge(ctx, (void*)s->sym, (void*)l->sym));
						} else if(s->sym != NULL)
						{
							ESSL_CHECK(_essl_interference_graph_register_wildcard_edge(ctx, (void*)s->sym));
						} else if(l->sym != NULL)
						{
							ESSL_CHECK(_essl_interference_graph_register_wildcard_edge(ctx, (void*)l->sym));
						}
					}
				}

			}
		}
	}

	return MEM_OK;
}



struct interference_graph_context *_essl_maligp2_calc_constant_register_interference(mempool *pool, translation_unit *tu, maligp2_relocation_context *rel_ctx)
{
	interference_graph_context *ctx;
	symbol_interference_list *lst = NULL;
	ESSL_CHECK(ctx = _essl_mempool_alloc(pool, sizeof(*ctx)));
	ESSL_CHECK(_essl_interference_graph_init(ctx, pool));

	ESSL_CHECK(extract_store_list(pool, &lst, tu));
	
	ESSL_CHECK(extract_load_list(pool, &lst, _essl_maligp2_get_normal_relocations(rel_ctx)));

	lst = LIST_SORT(lst, descending_subcycle_order, symbol_interference_list);

	ESSL_CHECK(build_interference_graph(ctx, lst));



	return ctx;
}
