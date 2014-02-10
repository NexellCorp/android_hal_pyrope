/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FRONTEND_CALLGRAPH
#define FRONTEND_CALLGRAPH

#include "frontend/parser.h"
#include "common/essl_list.h"


		/* Linked list of EXPR_KIND_FUNCTION_CALL nodes for the callers/callees
		 */
typedef struct _tag_call_node {
	struct _tag_call_node* next;
	node* caller;
} call_node;

        /* For each function: (elements in) list of functions calling it.
		 */
typedef struct call_graph {
	/*@null@*/ struct call_graph* next;
	symbol* func;                  /**< function calling on or called by owner of list */
	int count;                     /**< distance for 'calls_to_via', else # occurrences */
	call_node* callers;            /**< list of EXPR_KIND_FUNCTION_CALL nodes, only applicable to calls_to and calls_from */
} call_graph;
ASSUME_ALIASING(generic_list, call_graph);


typedef struct {
	/*@null@*/ struct call_graph* all_funcs;  /**< A list of all function symbols */
	/*@null@*/ symbol* curr_func;
	mempool* pool;
} callgraph_context;



/*@null@*/ callgraph_context* _essl_make_callgraph(mempool* pool, node* unit);


#endif /* FRONTEND_CALLGRAPH */
