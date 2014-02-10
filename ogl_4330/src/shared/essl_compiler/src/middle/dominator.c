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
#include "middle/dominator.h"

#ifdef UNIT_TEST 
#include "common/essl_dict.h"
#endif /* UNIT_TEST */


/* this implementation follows the algorithms given in 
A Simple, Fast Dominance Algorithm 
Keith D. Cooper, Timothy J. Harvey, and Ken Kennedy

http://www.hipersoft.rice.edu/grads/publications/dom14.pdf


Caveats: 
These algorithms don't work for irreductible control flow graphs. 
However, since ESSL lacks goto, there is no valid ESSL program
with irreductible control flow, so this should not be a problem.

*/
static memerr postorder_number_helper(basic_block *x, int *number, mempool *pool, control_flow_graph *fb)
{
	unsigned i;
	predecessor_list *lst = 0;
	x->postorder_visit_number = 1; /* use as visited field */
	for(i = 0; i < x->n_successors; ++i)
	{
		basic_block *succ = x->successors[i];
		lst = LIST_NEW(pool, predecessor_list);
		ESSL_CHECK(lst);
		lst->block = x;
		LIST_INSERT_BACK(&succ->predecessors, lst);
	}

	for(i = 0; i < x->n_successors; ++i)
	{
		basic_block *succ = x->successors[i];
		if(succ->postorder_visit_number == -1)
		{
			ESSL_CHECK(postorder_number_helper(succ, number, pool, fb));
		}
	}
	assert(*number < (int)fb->n_blocks);
	x->postorder_visit_number = *number;
	fb->postorder_sequence[x->postorder_visit_number] = x;
	*number += 1;
	return MEM_OK;
}

memerr _essl_basic_block_setup_postorder_sequence(control_flow_graph *fb, mempool *pool)
{
	int i = 0;
	unsigned j;
	basic_block *b = 0;
	unsigned int n_blocks = 0;
	basic_block **seq;
	for(b = fb->entry_block; b != 0; b = b->next)
	{
		b->postorder_visit_number = -1;
		b->predecessors = 0;
		++n_blocks;
	}
	fb->n_blocks = n_blocks;
	ESSL_CHECK(seq = _essl_mempool_alloc(pool, n_blocks*sizeof(basic_block*)));
	fb->postorder_sequence = seq;

	ESSL_CHECK(seq = _essl_mempool_alloc(pool, n_blocks*sizeof(basic_block*)));
	fb->output_sequence = seq;

	for(b = fb->entry_block, j = 0; b != 0; b = b->next, ++j)
	{
		fb->output_sequence[j] = b;
		b->output_visit_number = (int)j;
	}

	ESSL_CHECK(postorder_number_helper(fb->entry_block, &i, pool, fb));
	assert(i == (int)n_blocks);
	return MEM_OK;
}


memerr _essl_reverse_postorder_visit(void *context, control_flow_graph *fb, memerr (*fun)(void *context, basic_block *b))
{
	int i;
	for(i = (int)(fb->n_blocks - 1); i >= 0; --i)
	{

		ESSL_CHECK(fun(context, fb->postorder_sequence[i]));
	}
	return MEM_OK;

}


memerr _essl_postorder_visit(void *context, control_flow_graph *fb, memerr (*fun)(void *context, basic_block *b))
{
	unsigned i;
	for(i = 0; i < fb->n_blocks; ++i)
	{

		ESSL_CHECK(fun(context, fb->postorder_sequence[i]));
	}
	return MEM_OK;

}

static memerr compute_immediate_dominator_helper(void *context, basic_block *b)
{
	basic_block *new_idom;
	predecessor_list *p;
	essl_bool *changed = (essl_bool*)context;

	if(!b->predecessors) return 1;
	new_idom = b->predecessors->block;
	for(p = b->predecessors->next; p != 0; p = p->next)
	{
		assert(p->block != 0);
		if(p->block->immediate_dominator)
		{
			new_idom = _essl_common_dominator(p->block, new_idom);
		}
	}
	
	if(b->immediate_dominator != new_idom)
	{
		b->immediate_dominator = new_idom;
		*changed = ESSL_TRUE;
	}
	
	return MEM_OK;
}



static memerr compute_immediate_dominators(control_flow_graph *fb)
{
	essl_bool changed = ESSL_TRUE;
	basic_block *b = 0;

	/* First clear out any existing immediate dominator pointers */
	for(b = fb->entry_block; b != 0; b = b->next)
		b->immediate_dominator = 0;

	fb->entry_block->immediate_dominator = fb->entry_block;
	while(changed)
	{
		changed = ESSL_FALSE;
		ESSL_CHECK(_essl_reverse_postorder_visit((void*)&changed, fb, compute_immediate_dominator_helper));
	}
	return MEM_OK;
}


static memerr initialize_blocks(control_flow_graph *fb, mempool *pool)
{
	basic_block *b = fb->entry_block;
	for(; b != 0; b = b->next)
	{
		b->immediate_dominator = 0;
		b->postorder_visit_number = 0;
		ESSL_CHECK(_essl_ptrset_init(&b->dominance_frontier, pool));
		
	}
	return MEM_OK;
}

static memerr single_dominance_frontier(/*@unused@*/ void *context, basic_block *b)
{
	predecessor_list *p;
	IGNORE_PARAM(context);
	if(!b->predecessors || !b->predecessors->next) return 1;
	for(p = b->predecessors; p != 0; p = p->next)
	{
		basic_block *runner = p->block;
		while(runner != b->immediate_dominator)
		{
			assert(runner != 0);
			ESSL_CHECK(_essl_ptrset_insert(&runner->dominance_frontier, b));
			runner = runner->immediate_dominator;
		}
	}
	return MEM_OK;
}

memerr _essl_compute_dominance_frontier(control_flow_graph *fb)
{
	return _essl_postorder_visit(0, fb, single_dominance_frontier);
	
}


memerr _essl_compute_dominance_information(mempool *pool, symbol *s)
{
	control_flow_graph *fb;
	
	fb = s->control_flow_graph;
	ESSL_CHECK(fb);
	ESSL_CHECK(initialize_blocks(fb, pool));

	ESSL_CHECK(_essl_basic_block_setup_postorder_sequence(fb, pool));


	ESSL_CHECK(compute_immediate_dominators(fb));

	ESSL_CHECK(_essl_compute_dominance_frontier(fb));


	return MEM_OK;
}



#ifdef UNIT_TEST

int main()
{
	char a[100] = {0}, b[100] = {0}, c[100] = {0};
	int i;
	struct {
		const char *data;
		const char *result;
		
	} test_cases[] = {
		{
			"entry -> a;\n"
			"a -> b;\n"
			"a -> c;\n"
			"b -> exit;\n"
			"c -> exit;\n",

			"entry : entry\n" /* immediate dominator */
			";\n"  /* dominance frontier */

			"a : entry\n"
			";\n"

			"b : a\n"
			"exit;\n"

			"c : a\n"
			"exit;\n"

			"exit : a\n"
			";\n"
		},
		


		{ /* simple if statement */
			"entry -> a;\n"
			"a -> b;\n"
			"a -> c;\n"
			"b -> d;\n"
			"c -> d;\n"
			"d -> exit;\n",

			"entry : entry\n" /* immediate dominator */
			";\n"  /* dominance frontier */

			"a : entry\n"
			";\n"

			"b : a\n"
			"d;\n"

			"c : a\n"
			"d;\n"

			"d : a\n"
			";\n"

			"exit : d\n"
			";\n"

		},
		{
			"entry -> one;\n"
			"one -> two;\n"
			"two -> one;\n"
			"two -> exit;\n",

			"entry : entry\n"
			";\n"

			"one : entry\n"
			"one;\n"
			"two : one\n"
			"one;\n"

			"exit : two\n"
			";\n"
		},
		{
			"entry -> 9;\n"
			"9 -> 10;\n"
			"9 -> 11;\n"
			"10 -> 11;\n"
			"11 -> 9;\n"
			"11 -> 12;\n"
			"12 -> exit;\n",

			"entry : entry\n"
			";\n"

			"9 : entry\n"
			"9;\n"
			"10 : 9\n"
			"11;\n"

			"11 : 9\n"
			"9;\n"

			"12 : 11\n"
			";\n"

			"exit : 12\n"
			";\n"
		},
		{
			"entry -> 1;\n"
			"entry -> exit;\n"
			"1 -> 2;\n"
			"2 -> 3;\n"
			"2 -> 7;\n"
			"3 -> 4;\n"
			"3 -> 5;\n"
			"4 -> 6;\n"
			"5 -> 6;\n"
			"6 -> 8;\n"
			"7 -> 8;\n"
			"8 -> 9;\n"
			"9 -> 10;\n"
			"9 -> 11;\n"
			"10 -> 11;\n"
			"11 -> 9;\n"
			"11 -> 12;\n"
			"12 -> 2;\n"
			"12 -> exit;\n",

			"entry : entry\n"
			";\n"

			"1 : entry\n"
			"exit;\n"

			"2 : 1\n"
			"2 exit;\n"

			"3 : 2\n"
			"8;\n"

			"4 : 3\n"
			"6;\n"

			"5 : 3\n"
			"6;\n"

			"6 : 3\n"
			"8;\n"

			"7 : 2\n"
			"8;\n"

			"8 : 2\n"
			"exit 2;\n"

			"9 : 8\n"
			"exit 2 9;\n"

			"10 : 9\n"
			"11;\n"

			"11 : 9\n"
			"exit 2 9;\n"

			"12 : 11\n"
			"exit 2;\n"

			"exit : entry\n"
			";\n"
		}, 
		{
			"entry -> 1;\n"
			"1 -> 3;\n"
			"2 -> 3;\n"
			"3 -> 4;\n"
			"3 -> 2;\n"
			"4 -> exit;\n",

			"entry : entry\n"
			";\n"

			"1 : entry\n"
			";\n"

			"2 : 3\n"
			"3;\n"
			
			"3 : 1\n"
			"3;\n"

			"4 : 3\n"
			";\n"

			"exit : 4\n"
			";\n"

		},
		{
			"entry -> 1;\n"
			"1 -> 4;\n"
			"2 -> 3;\n"
			"3 -> 4;\n"
			"4 -> 5;\n"
			"4 -> 2;\n"
			"5 -> exit;\n",

			"entry : entry\n"
			";\n"

			"1 : entry\n"
			";\n"

			"2 : 4\n"
			"4;\n"
			
			"3 : 2\n"
			"4;\n"

			"4 : 1\n"
			"4;\n"

			"5 : 4\n"
			";\n"

			"exit : 5\n"
			";\n"
		},
	};
	for(i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); ++i)
	{
		mempool_tracker tracker;
		mempool pool;
		symbol fb;
		control_flow_graph g;
		ptrset set;
		dict blocks;
		basic_block *lst = 0;
		int n, ret;
		const char *test = test_cases[i].data;
		_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
		_essl_mempool_init(&pool, 0, &tracker);
		_essl_dict_init(&blocks, &pool);
		_essl_ptrset_init(&set, &pool);
		fb.control_flow_graph = &g;
#ifdef VERBOSE_TEST
		fprintf(stderr, "Running test case %d...\n", i);
#endif /* VERBOSE_TEST */
		while(1)
		{
			basic_block *toblock, *fromblock;
			string froms, tos;


			n = 0;
			ret = sscanf(test, "%s -> %[^; \n];%n", a, b, &n);
			if(ret < 2) break;
			test += n;
			froms = _essl_cstring_to_string(&pool, a);
			tos = _essl_cstring_to_string(&pool, b);

			if(!(fromblock = _essl_dict_lookup(&blocks, froms)))
			{
				fromblock = _essl_new_basic_block(&pool);
				assert(fromblock);
				_essl_dict_insert(&blocks, froms, fromblock);
				LIST_INSERT_BACK(&lst, fromblock);

			}

			if(!(toblock = _essl_dict_lookup(&blocks, tos)))
			{
				toblock = _essl_new_basic_block(&pool);
				assert(toblock);
				_essl_dict_insert(&blocks, tos, toblock);
				LIST_INSERT_BACK(&lst, toblock);

			}
			if(fromblock->n_successors < 2)
			{
				fromblock->successors[fromblock->n_successors++] = toblock;
			} else {
				assert(!"more than two children of a block\n");
			}
			

		}
		g.entry_block = _essl_dict_lookup(&blocks, _essl_cstring_to_string_nocopy("entry"));
		assert(g.entry_block);
		g.exit_block = _essl_dict_lookup(&blocks, _essl_cstring_to_string_nocopy("exit"));
		assert(g.exit_block);
		ret = _essl_compute_dominance_information(&pool, &fb);
		assert(ret);




		test = test_cases[i].result;
		while(1)
		{
			basic_block *currblock, *idomblock, *tmpblock;
			string curr, idom;


			n = 0;
			ret = sscanf(test, "%s : %s\n%n", a, b, &n);
			if(ret < 2) break;
			test += n;
			curr = _essl_cstring_to_string_nocopy(a);
			idom = _essl_cstring_to_string_nocopy(b);

			currblock = _essl_dict_lookup(&blocks, curr);
			assert(currblock);

			idomblock = _essl_dict_lookup(&blocks, idom);
#ifdef VERBOSE_TEST
			fprintf(stderr, "%s dominates %s, %p %p %p\n", b, a, idomblock, currblock->immediate_dominator, currblock);
#endif /* VERBOSE_TEST */
			assert(idomblock);
			if(currblock->immediate_dominator != idomblock) fprintf(stderr, "immediate dominator problem\n");
			assert(currblock->immediate_dominator == idomblock);
			
			_essl_ptrset_clear(&set);
			while(*test && *test != ';')
			{
				while(*test == ' ') ++test; 
				n = 0;
				ret = sscanf(test, "%[^; \n]%n", c, &n);
				if(ret < 1) break;
				test += n;
				tmpblock = _essl_dict_lookup(&blocks, _essl_cstring_to_string_nocopy(c));
				if(!tmpblock) 
				{
					fprintf(stderr, "Could not look up \"%s\", input string is '%s'\n", c, test);
				}
				assert(tmpblock);
				_essl_ptrset_insert(&set, tmpblock);

			}
			if(*test) ++test;
			
			if(!_essl_ptrset_equal(&set, &currblock->dominance_frontier))
			{
				ptrset_iter it;
				fprintf(stderr, "\nExpected dominance frontier set for %s:\n", a);
				_essl_ptrset_iter_init(&it, &set);
				while((tmpblock = _essl_ptrset_next(&it)) != 0)
				{
					fprintf(stderr, "%p ", tmpblock);
				}
				fprintf(stderr, "\nActual dominance frontier set for %s:\n", a);
				_essl_ptrset_iter_init(&it, &currblock->dominance_frontier);
				while((tmpblock = _essl_ptrset_next(&it)) != 0)
				{
					fprintf(stderr, "%p ", tmpblock);
				}
				fprintf(stderr, "\n\n");
				



			}
			assert(_essl_ptrset_equal(&set, &currblock->dominance_frontier));

			

		}
		{
			const char *rstr = test;
			while(*rstr) {
				char c = *rstr++;
				if(c != ' ' && c != '\n')
				{
					fprintf(stderr, "Parsing problem, remaining test string: \"%s\"\n", test);
					assert(0);
					
				}
			}
		}







		_essl_mempool_destroy(&pool);
	}

	fprintf(stderr, "All tests OK!\n");
	return 0;
}

#endif /* UNIT_TEST */
