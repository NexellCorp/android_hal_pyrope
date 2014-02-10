/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "frontend/parser.h"
#include "frontend/frontend.h"
#include "frontend/callgraph.h"

#ifdef UNIT_TEST
#include "common/essl_stringbuffer.h"


        /** Copy the name of the func of 'elm' to  'buff'.  
		 * Precede by 'count'"x:" if not 1.
		 */
static memerr
append_func_elem(call_graph* elm, string_buffer* buff) {
	ESSL_CHECK(elm->func);
	if (elm->count != 1) {
		ESSL_CHECK(_essl_string_buffer_put_int(buff, elm->count));
		ESSL_CHECK(_essl_string_buffer_put_str(buff, "x:"));
	}
	ESSL_CHECK(_essl_string_buffer_put_str(buff, "'"));
	ESSL_CHECK(_essl_string_buffer_put_string(buff, &(elm->func->name)));
	ESSL_CHECK(_essl_string_buffer_put_str(buff, "'"));
	return MEM_OK;
}



        /** Copy func names and counts in list 'list', 
		 * to the front of char[]  'buf', which has length 'bufZ'.  
		 * Return number of chars that were copied into 'buf'.
		 */
static memerr
append_func_names(call_graph* list, string_buffer* buff) {
	for (; list!=0; list = list->next) {
		ESSL_CHECK(append_func_elem(list, buff));
		ESSL_CHECK(_essl_string_buffer_put_str(buff, ", "));
	}
	return MEM_OK;
}



        /** Copy call- and called-info for the functions in 'list', 
		 * into 'buff'
		 */
static memerr
append_func_info(call_graph* list, string_buffer* buff) {
	for (; list!=0; list = list->next) {
		assert(list->func);
		ESSL_CHECK(_essl_string_buffer_put_str(buff, "\nfunc '"));
		ESSL_CHECK(_essl_string_buffer_put_string(buff, &(list->func->name)));
		ESSL_CHECK(_essl_string_buffer_put_str(buff, "', called from {"));
		ESSL_CHECK(append_func_names(list->func->calls_from, buff));
		ESSL_CHECK(_essl_string_buffer_put_str(buff, "}, calls on {"));
		ESSL_CHECK(append_func_names(list->func->calls_to, buff));
		ESSL_CHECK(_essl_string_buffer_put_str(buff, "}, indirectly calls {"));
		ESSL_CHECK(append_func_names(list->func->calls_to_via, buff));
		ESSL_CHECK(_essl_string_buffer_put_str(buff, "}"));
	}
	ESSL_CHECK(_essl_string_buffer_put_str(buff, "\n"));
	return MEM_OK;
}


#endif /* UNIT_TEST */
/* 4567890123456789012345678901234567890 */


          /** Record a call to or from  fun in list  lis.  (cnt==0)? nearest
		   * neighbor, count occurrences : length of shortest path.  
		   * Return the updated list.
		   */
/*@null@*/ static call_graph*
record_func(mempool* pool, /*@null@*/ call_graph* lis, symbol* fun, int cnt) {
	call_graph* cg;
	assert(fun != 0);
	for (cg = lis;  cg!=0;  cg = cg->next) 
		if (cg->func == fun) {         /* edge is noted already */
			if (cnt==0) {
				++cg->count;
			} else if (cg->count > cnt)
				cg->count = cnt;
			return lis;
		}
	ESSL_CHECK(cg = (call_graph*)_essl_mempool_alloc(pool, sizeof(call_graph)));
	cg->func = fun;
	cg->next = lis;
	cg->count = (cnt==0)? 1 : cnt;
	return cg;
}



        /** Compute transitive closure of the calls_to into calls_to_via.
		 */
static memerr
closure_of_to(mempool* pool, /*@null@*/ call_graph* lis) {
	int changed;
	call_graph* froG;
	for (froG = lis; froG != 0; froG = froG->next) { 
		call_graph* toG;               /* copy list 'calls_to' to 'calls_to_via': */
		symbol* from = froG->func;
		assert(from != 0);
		for (toG = from->calls_to; toG != 0; toG = toG->next) {
			symbol* to = toG->func;
			assert(to != 0);
			ESSL_CHECK(from->calls_to_via = record_func(pool, from->calls_to_via, to, 1));
		}
	}
	do {
		changed = 0;
		for (froG = lis; froG != 0; froG = froG->next) {
			call_graph* viaG;           /* add paths via 'via' to 'calls_to_via': */
			symbol* from = froG->func;
			assert(from != 0);
			for (viaG = from->calls_to_via; viaG != 0; viaG = viaG->next) {
				call_graph* toG;
				symbol* via = viaG->func;
				assert(via != 0);
				for (toG = via->calls_to_via; toG != 0; toG = toG->next) {
					call_graph* old;
					symbol* to = toG->func;
					assert(to != 0);
					old = from->calls_to_via;
					ESSL_CHECK(from->calls_to_via = 
						record_func(pool, from->calls_to_via, to, toG->count+viaG->count));
					changed = changed || from->calls_to_via != old;
					
				}
			}
		}
	} while (changed);
	return MEM_OK;
}



        /** Construct call graph edges, recording that: 
		 * the 'curr_func' indicated by the parser context 'ctx' 
		 * contains a call on the function indicated by the node 'toFn'.
		 */
static memerr
record_call(callgraph_context* ctx, node* toNode) {
	symbol* toFn;
	symbol* fromFn;
	call_node* caller;
	assert(ctx != 0);
	toFn = toNode->expr.u.fun.sym;
	assert(toFn != 0);
	fromFn = ctx->curr_func;
	assert(fromFn != 0);
	ESSL_CHECK(caller = LIST_NEW(ctx->pool, call_node));
	caller->caller = toNode;
	ESSL_CHECK(fromFn->calls_to = record_func(ctx->pool, fromFn->calls_to, toFn, 0));
	LIST_INSERT_BACK(&(fromFn->calls_to->callers), caller);
	ESSL_CHECK(caller = LIST_NEW(ctx->pool, call_node));
	caller->caller = toNode;
	ESSL_CHECK(toFn->calls_from = record_func(ctx->pool, toFn->calls_from, fromFn, 0));
	LIST_INSERT_BACK(&(toFn->calls_from->callers), caller);
	toFn->call_count += 1;
	return MEM_OK;
}



        /** Traverse the semantic tree and build call graphs: 
		 * for each defined function (in 'symbol'): 
		 *    what functions call on it, what functions does it call on.
		 * Also builds a global list of all function  'symbol's, 
		 * in the  'callgraph_context'.
		 */
static memerr
note_calls(node* n, callgraph_context* ctx) {
	unsigned int m;                    /* for-loop upper */
	unsigned int i;                    /* for-loop index */
	essl_bool func_def = ESSL_FALSE;
	if (n==0) 
		return MEM_OK;                 /* empty part contributes nothing */
	ESSL_CHECK(ctx);
	switch (n->hdr.kind) {
	case DECL_KIND_FUNCTION:           /* n is func decl */
		if (n->decl.sym->body != 0)
		{
			ESSL_CHECK(ctx->curr_func == 0);
			func_def = ESSL_TRUE;
			ctx->curr_func = n->decl.sym;
			ESSL_CHECK(ctx->all_funcs = record_func(ctx->pool, ctx->all_funcs, n->decl.sym, 0));
		}
		break;
	case EXPR_KIND_FUNCTION_CALL:      /* n is call from  ctx->curr_func */
		ESSL_CHECK(record_call(ctx, n));
		break;
	default:
		break;
	}
	m = GET_N_CHILDREN(n);
	for (i = 0; i < m; ++i) {
		node* child = GET_CHILD(n, i);
		if (child!=0) {
			ESSL_CHECK(note_calls(child, ctx));
		} 
	}
	if (func_def) {
		ctx->curr_func = 0;
	}
	return MEM_OK;
}



        /** Set up a context for, and build the call graph of the compilation unit.
		 */
callgraph_context* 
_essl_make_callgraph(mempool* pool, node* unit) {
	callgraph_context* ctx;
	ESSL_CHECK(ctx = _essl_mempool_alloc(pool, sizeof(callgraph_context)));
	ctx->all_funcs = 0;
	ctx->curr_func = 0;
	ctx->pool = pool;

	ESSL_CHECK(note_calls(unit, ctx));      /* - set up the call graph */
	ESSL_CHECK(closure_of_to(pool, ctx->all_funcs)); /* annotate with transitive closure */
	return ctx;
}



/* 4567890123456789012345678901234567890 */
#ifdef UNIT_TEST


static int
simple_test(mempool* pool, target_descriptor* desc) {
	int i;
	int lengths[1];
	int tests_failed = 0;
	char* inn[] = {
		"int f5(int p);\n"
		"int f1(int p1, int p2) { return f5(p1+p2); }\n"
		"int f2(int p1, int p2) { return f1(p1, f5(p2)); }\n"
		"int f3(int p1, int p2) { return f5(p1-p2); }\n"
		"int f4(int p)          { return f3(p, p+1); }\n"
		"int f5(int p)          { return f4(p)+1; }", 
		" "};
	char* res[] = {"\n"
		"func 'f5', called from {'f3', 'f2', 'f1', }, calls on {'f4', }, "
			"indirectly calls {3x:'f5', 2x:'f3', 'f4', }\n"
		"func 'f4', called from {'f5', }, calls on {'f3', }, "
			"indirectly calls {3x:'f4', 2x:'f5', 'f3', }\n"
		"func 'f3', called from {'f4', }, calls on {'f5', }, "
			"indirectly calls {2x:'f4', 3x:'f3', 'f5', }\n"
		"func 'f2', called from {}, calls on {'f5', 'f1', }, "
			"indirectly calls {2x:'f4', 3x:'f3', 'f1', 'f5', }\n"
		"func 'f1', called from {'f2', }, calls on {'f5', }, "
			"indirectly calls {2x:'f4', 3x:'f3', 'f5', }\n"
	};
	int innN = sizeof(inn)/sizeof(inn[0]);
	innN = 1;                          /* - for the time being */
	for (i = 0; i<innN; ++i) {
		node* unit;
		frontend_context* fctx;
		callgraph_context* cctx;
		char* cbuf;
		string_buffer* buff = _essl_new_string_buffer(pool);
		lengths[0] = strlen(inn[i]);   /* - hva er dette? */
		fctx = _essl_new_frontend(pool, desc, inn[i], lengths, 1);
		if (fctx==0)
		{
			return 1;
		}
		unit = _essl_parse_translation_unit(&fctx->parse_context);
		unit = _essl_typecheck(&fctx->typecheck_context, unit, NULL);
		cctx = _essl_make_callgraph(pool, unit);
		ESSL_CHECK(append_func_info(cctx->all_funcs, buff));
		cbuf = _essl_string_buffer_to_string(buff, pool);
		ESSL_CHECK(cbuf);
		if (strncmp(cbuf, res[i], 10000) !=0) {
			fprintf(stderr, "\nTest case %d, '%s':\n", i, inn[i]);
			fprintf(stderr, "Expected code:\n{%s}\n\n", res[i]);
			fprintf(stderr, "Actual code:\n{%s}\n\n", cbuf);
			++tests_failed;
		}
	}
	return tests_failed;
}



int main(void) {
	mempool_tracker tracker;
	mempool pool;
	compiler_options* opts;
	target_descriptor *desc;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	opts = _essl_new_compiler_options(&pool);
	desc = _essl_new_target_descriptor(&pool, TARGET_FRAGMENT_SHADER, opts);
	if (simple_test(&pool, desc) !=0) {
		_essl_mempool_destroy(&pool);
		return 1;
	}
	fprintf(stderr, "All tests OK!\n");
	_essl_mempool_destroy(&pool);
	return 0;
}


#endif /* UNIT_TEST */

