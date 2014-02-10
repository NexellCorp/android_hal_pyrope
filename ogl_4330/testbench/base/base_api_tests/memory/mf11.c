/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_types.h>
#include <base/mali_runtime.h>
#include <base/mali_context.h>
#include <base/mali_debug.h>
#include "../unit_framework/suite.h"

#include <base/mali_memory.h>

/**
 *	The test fixture.
 *	Note that this is not visible outside this file, so we need not change its name every time.
 */
typedef struct
{
	int dummy;
} fixture;

/**
 *	Description:
 *	Create the test fixture.
 *	Note that this is a static declaration, so that we do not have to change its name every time.
 */
static void* create_fixture(suite* test_suite)
{
	mempool* pool = &test_suite->fixture_pool;
	fixture* fix = (fixture*)_test_mempool_alloc(pool,sizeof(fixture));
	return fix;
}

static void test_mf11(suite* test_suite)
{
	mali_mem_handle mem_handle;
	mali_mem_handle tmp_mem_handle;
	mali_base_ctx_handle ctxh;
	u32 alloc_nr;
	mali_err_code err;
	u32 * memptr;
	u32 i, j;
    u32 * buf;
    u32 check_count;

	u32 mem_size = 512*1024;

	ctxh = _mali_base_context_create();
	assert_fail((NULL != ctxh), "ctx create failed");
	if(ctxh != NULL)
	{
		err = _mali_mem_open(ctxh);
		assert_fail((MALI_ERR_NO_ERROR == err), "Memory system could not be opened");
		if(MALI_ERR_NO_ERROR == err)
		{
			mem_handle = _mali_mem_alloc(ctxh, mem_size, 0, MALI_CPU_READ | MALI_CPU_WRITE);
			assert_fail((NULL != mem_handle), "nullpointer");
			if(NULL != mem_handle)
			{
				mali_mem_handle add_after = mem_handle;

				/* allocate cpu-pointer and set data */
				memptr = MALLOC(mem_size);
				assert_fail((NULL != memptr), "nullpointer");
				if(NULL == memptr) return;

				buf = MALLOC(mem_size);
				assert_fail((NULL != buf), "nullpointer");
				if(NULL == buf) {
					FREE(memptr);
					return;
				}

				alloc_nr=0;
		    	for ( i=0; i< (mem_size>>2) ; i++)
		    	{
		        	memptr[i] = i*4 + _mali_mem_mali_addr_get(mem_handle, 0);
		    	}
				_mali_mem_write(mem_handle, 0, memptr, mem_size);

				do
				{
					alloc_nr++;
					tmp_mem_handle = _mali_mem_alloc(ctxh, mem_size, 0, MALI_CPU_READ | MALI_CPU_WRITE);
					if(NULL == tmp_mem_handle) break;
					_mali_mem_list_insert_after(add_after, tmp_mem_handle);
					add_after = tmp_mem_handle;

					/* fill memory with data */
					for ( j=0; j< (mem_size>>2) ; j++, i++)
					{
						memptr[j] = j*4 + _mali_mem_mali_addr_get(tmp_mem_handle, 0);
					}
					_mali_mem_write(tmp_mem_handle, 0, memptr, mem_size);
				}
				while(MALI_TRUE);

				assert_fail((NULL == tmp_mem_handle), "nullpointer");

				/* reset i befor compare */
				i = 0;

				check_count = 0;
				for (tmp_mem_handle = mem_handle; MALI_NO_HANDLE != tmp_mem_handle; tmp_mem_handle = _mali_mem_list_get_next(tmp_mem_handle))
		   		{
					int num_errors = 0;

		   			_mali_mem_read(buf, tmp_mem_handle, 0, mem_size);

					for ( j=0; j< (mem_size>>2) ; j++, i++)
					{
						if (buf[j] != _mali_mem_mali_addr_get(tmp_mem_handle, 0) + j*4)
						{
						u32 helpaddr;
						helpaddr = _mali_mem_mali_addr_get(tmp_mem_handle, 0) + j*4;
						printf("address %08x ( %08x  ) has the wrong value: %08x \n", helpaddr, helpaddr - 0xc0200000 + 0x78200000 , buf[j]);
						num_errors++;
						}
					}

					assert_ints_equal(num_errors, 0, "Number of errors found not 0");
					check_count++;
		   		}

				_mali_mem_list_free(mem_handle);
				FREE(memptr);
				FREE(buf);
			}
			_mali_mem_close(ctxh);
		}
		_mali_base_context_destroy(ctxh);
	}

}

/**
 *	Description:
 *	Remove the test fixture.
 *	Note that this is a static declaration, so that we do not have to change its name every time.
 */
static void remove_fixture(suite* test_suite) { }

suite* create_suite_mf11(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "mf11", create_fixture, remove_fixture, results );

	add_test(new_suite, "mf11", test_mf11);

	return new_suite;
}
