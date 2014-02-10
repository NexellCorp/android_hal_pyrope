/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_types.h>
#include <base/mali_runtime.h>
#include <base/mali_context.h>
#include <base/mali_debug.h>
#include <base/mali_byteorder.h>
#ifdef MALI_TEST_API
	#include <base/mali_test.h>
#endif
#include "../unit_framework/suite.h"
#include "base_test_gp_util.h"
#include "../util/base_test_util.h"

#include <base/mali_memory.h>
#include <base/gp/mali_gp_job.h>
#include <mali_gp_vs_cmd.h>
#include <mali_gp_vs_config.h>
#include <mali_gp_plbu_cmd.h>
#include <mali_gp_plbu_config.h>
#include <base/mali_debug.h>
#define MAX_NUM_VS_JOBS 100
#define NUM_RUNS_BGFVS09 100

u32 *work_buffer32;
u16 *work_buffer16;
u8 *work_buffer8;
mali_mem_handle mp_data_input, mp_data_output[MAX_NUM_VS_JOBS], ioconf_handle[MAX_NUM_VS_JOBS];
mali_mem_handle mp_vs_instr_copy_prog;

#define MALI_SET_AS_FLOAT(var, value) do { float * _flt_tmp = (float*)&var; *_flt_tmp = (value); } while (0)

u32 vs_instr_copy_prog[]=
{
    0xb00b0ec2, 0x400002c1, 0x40034400, 0x00001c08,
    0xb00b0ec2, 0x440002c1, 0xc4034400, 0x00001c08,
    0xb00b0ec2, 0x480002c1, 0x48034400, 0x00001c09,
    0xb00b0ec2, 0x4c0002c1, 0xcc034400, 0x00001c09,
    0xb00b0ec2, 0x500002c1, 0x50034400, 0x00001c0a,
    0xb00b0ec2, 0x540002c1, 0xd4034400, 0x00001c0a,
    0xb00b0ec2, 0x580002c1, 0x58034400, 0x00001c0b,
    0xb00b0ec2, 0x5c0002c1, 0xdc034400, 0x00001c0b,
    0xb00b0ec2, 0x600002c1, 0x60034400, 0x00001c0c,
    0xb00b0ec2, 0x640002c1, 0xe4034400, 0x00001c0c,
    0xb00b0ec2, 0x680002c1, 0x68034400, 0x00001c0d,
    0xb00b0ec2, 0x6c0002c1, 0xec034400, 0x00001c0d,
    0xb00b0ec2, 0x700002c1, 0x70034400, 0x00001c0e,
    0xb00b0ec2, 0x740002c1, 0xf4034400, 0x00001c0e,
    0xb00b0ec2, 0x780002c1, 0x78034400, 0x00001c0f,
    0xb00b0ec2, 0x7c0002c1, 0xfc034400, 0x00001c0f
};

mali_gp_job_handle make_dummy_job_gp(suite* test_suite, mali_base_ctx_handle ctxh, u32 num_vs_cmds, u32 num_plbu_cmds)
{
	mali_gp_job_handle jobtomake;
	u32 loop;
	mali_err_code err = MALI_ERR_NO_ERROR;

	jobtomake =	_mali_gp_job_new(ctxh);
	if (test_suite)	assert_fail((MALI_NO_HANDLE != jobtomake), "gp_job_new failed");
	if(MALI_NO_HANDLE != jobtomake)
	{
		for(loop = 0; loop < num_vs_cmds; loop++)
		{
			err = _mali_gp_job_add_vs_cmd(jobtomake, GP_VS_COMMAND_WRITE_CONF_REG(0x0000000, 0x40));
			if (test_suite)	assert_fail((MALI_ERR_NO_ERROR == err), "Adding vs commands - Errorcode returned");
			if(err != MALI_ERR_NO_ERROR) break;
		}
		if(err != MALI_ERR_NO_ERROR)
		{
			_mali_gp_job_free(jobtomake);
			return MALI_NO_HANDLE;
		}
		for(loop = 0; loop < num_plbu_cmds; loop++)
		{
			err = _mali_gp_job_add_plbu_cmd(jobtomake, GP_PLBU_COMMAND_WRITE_CONF_REG(0x0000000, 0x2));
			if (test_suite)	assert_fail((MALI_ERR_NO_ERROR == err), "Adding plbu commands - Errorcode returned");
			if(err != MALI_ERR_NO_ERROR) break;
		}
		if(err != MALI_ERR_NO_ERROR)
		{
			_mali_gp_job_free(jobtomake);
			return MALI_NO_HANDLE;
		}
	}
	return jobtomake;
}

mali_err_code allocate_vs_stuff(mali_base_ctx_handle ctxh, u32 num_jobs, suite* test_suite)
{
	s16 input_counter;
	u32 i,loop;
	mali_err_code err = MALI_ERR_NO_ERROR;

	work_buffer32 = (u32*) MALLOC(4096);
	assert_fail((NULL != work_buffer32), "sys_malloc failed work buffer");
    if(NULL == work_buffer32) return MALI_ERR_OUT_OF_MEMORY;

    work_buffer16 = (u16*)work_buffer32;
    work_buffer8  =  (u8*)work_buffer16;
    {
        mp_data_input = _mali_mem_alloc(ctxh, (16*16*2), 0, MALI_GP_READ);
        assert_fail((MALI_NO_HANDLE != mp_data_input), "mem_alloc failed input buffer");
        if(MALI_NO_HANDLE == mp_data_input)
        {
        	FREE(work_buffer32);
        	return MALI_ERR_OUT_OF_MEMORY;
        }
		PRINT_MAGIC(mp_data_input);
        input_counter = 0;
        /*data_input = (u16*)mp_data_input->mali_mem;*/
        /* 4 w * 4 h = 16, 16 val * 4 blocks = 64*/
        for(i=0; i<16*16 ; i++)
        {
            *(work_buffer16+i) = input_counter++;
            /*input_counter = -input_counter;
            input_counter += input_counter>0 ? 1 : -1;*/
        }
        _mali_mem_write_cpu_to_mali(mp_data_input, 0, work_buffer16, (16*16*2), sizeof(*work_buffer16));
    }

    /* Output "image" 16x16x1B = 256B + Add "image" 16x16x1B = 256B*/
    for(loop = 0; loop <num_jobs; loop++)
    {
        mp_data_output[loop] = _mali_mem_alloc(ctxh, (16*16), 0, MALI_GP_READ);
        assert_fail((MALI_NO_HANDLE != mp_data_output[loop]), "mem_alloc failed output buffer");
        if(MALI_NO_HANDLE == mp_data_output[loop])
        {
        	err = MALI_ERR_OUT_OF_MEMORY;
         	break;
        }
		PRINT_MAGIC(mp_data_output[loop]);
        for(i=0; i<16*16 ; i++)
        {
            *(work_buffer8+i) = 2;
        }
        /*_mali_sys_memcpy(data_output, work_buffer8, 16*16);*/
        _mali_mem_write(mp_data_output[loop], 0, work_buffer8, (16*16));
    }

    if(MALI_ERR_OUT_OF_MEMORY == err)
    {
    	FREE(work_buffer32);
    	_mali_mem_free(mp_data_input);
    	for(i = 0; i < loop; i++)
    	{
    		_mali_mem_free(mp_data_output[i]);
    	}
    	return MALI_ERR_OUT_OF_MEMORY;
    }

    /* Load shader into memory
    instr = mali_load_hex_file_to_mem("instr.hex", 16*4*4);*/
    mp_vs_instr_copy_prog = _mali_mem_alloc(ctxh, (16*4*4), 0, MALI_GP_READ);
    assert_fail((MALI_NO_HANDLE != mp_vs_instr_copy_prog), "mem_alloc failed instr prog");
	if(MALI_NO_HANDLE == mp_vs_instr_copy_prog)
	{
		FREE(work_buffer32);
		_mali_mem_free(mp_data_input);
    	for(i = 0; i < num_jobs; i++)
    	{
    		_mali_mem_free(mp_data_output[i]);
    	}
    	return MALI_ERR_OUT_OF_MEMORY;
	}
	PRINT_MAGIC(mp_vs_instr_copy_prog);
    _mali_mem_write(mp_vs_instr_copy_prog, 0, vs_instr_copy_prog, (16*4*4));

    for(loop = 0; loop<num_jobs; loop++)
    {
	    ioconf_handle[loop] = _mali_mem_alloc(ctxh, 256, 0, MALI_PP_READ);
	    assert_fail((MALI_NO_HANDLE != ioconf_handle[loop]), "mem_alloc failed ioconf");
        if(MALI_NO_HANDLE == ioconf_handle[loop])
        {
        	err = MALI_ERR_OUT_OF_MEMORY;
         	break;
        }
		PRINT_MAGIC(ioconf_handle[loop]);
    }

    if(err == MALI_ERR_OUT_OF_MEMORY)
	{
		FREE(work_buffer32);
		_mali_mem_free(mp_data_input);
    	for(i = 0; i < num_jobs; i++)
    	{
    		_mali_mem_free(mp_data_output[i]);
    	}
    	_mali_mem_free(mp_vs_instr_copy_prog);
    	for(i = 0; i < loop; i++)
    	{
    		_mali_mem_free(ioconf_handle[i]);
    	}
    	return MALI_ERR_OUT_OF_MEMORY;
	}

	return MALI_ERR_NO_ERROR;
}


mali_gp_job_handle make_vs_job(suite* test_suite, mali_base_ctx_handle ctxh, u32 jobnum)
{

	mali_gp_job_handle jobtomake;
	mali_err_code err;
	u32 *vs_conf_input,*vs_conf_output;
	u32 i;

	jobtomake =	_mali_gp_job_new(ctxh);
	assert_fail((MALI_NO_HANDLE != jobtomake), "nullpointer");
	if(MALI_NO_HANDLE == jobtomake) return jobtomake;

	vs_conf_input = _mali_mem_ptr_map_area(ioconf_handle[jobnum], 0 , 256, 0, MALI_MEM_PTR_WRITABLE | MALI_MEM_PTR_READABLE);
	if(NULL == vs_conf_input)
	{
		assert_fail((NULL != vs_conf_input), "mem mapping failed");
		_mali_gp_job_free(jobtomake);
		return MALI_NO_HANDLE;
	}
    vs_conf_output = vs_conf_input + 32;

    /* Configurationregs: Input */
    for(i=0; i<31 ; i+=2)
    {
        int type, commaloc, stride;
        type        = GP_VS_VSTREAM_FORMAT_4_FIX_S16;
        commaloc    = 0;
        stride      = 16*4*2; /* 16Regs * 4Elmt * 2B = 128*/
        /* Address load 16 bit val from */
		*(vs_conf_input+i) = (u32)_mali_mem_mali_addr_get(mp_data_input, i*2*2);
        /**(vs_conf_input+i) = MALI_MEM_V2P(data_input+i*2);*/
        /* Input specifier */
        *(vs_conf_input+i+1) = GP_VS_CONF_REG_INP_SPEC_CREATE(type, commaloc, stride);
    }

    /* Configurationregs: Output */
    for(i=0; i<31 ; i+=2)
    {
        int type, commaloc, stride;
        type        = GP_VS_VSTREAM_FORMAT_4_FIX_U8;
        commaloc    = 0;
        stride      = 16*4*1; /* 16Regs * 4Elmt * 1B = 64 */
        /* Address load 16 bit val from */
        /**(vs_conf_output+i) = MALI_MEM_V2P(data_output+i*2);*/
        *(vs_conf_output+i) = (u32)_mali_mem_mali_addr_get(mp_data_output[jobnum], i*2);
        /* Output specifier */
        *(vs_conf_output+i+1) = GP_VS_CONF_REG_OUTP_SPEC_CREATE(type, commaloc, stride);
    }

    _mali_mem_ptr_unmap_area(ioconf_handle[jobnum]);

    /* ADDING THE DIFFERENT CONFIGURATION REGISTERS TO THE VS */
    err = _mali_gp_job_add_vs_cmd(jobtomake,
    			GP_VS_COMMAND_WRITE_INPUT_OUTPUT_CONF_REGS(
                           (u32)_mali_mem_mali_addr_get(ioconf_handle[jobnum], 0),
                           0,
                           32 ));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;



    { /* GP_VS_CONF_REG_PROG_PARAM  */
        int start    = 0;
        int end      = 15;
        int idx_last = 15;

        u32 idx  = GP_VS_CONF_REG_PROG_PARAM;
        u32 data = GP_VS_CONF_REG_PROG_PARAM_CREATE(start,end,idx_last);;

        /*mali_gp_vs_job_add_cmd(job, GP_VS_COMMAND_WRITE_CONF_REG(data,idx));*/
		err = _mali_gp_job_add_vs_cmd(jobtomake, GP_VS_COMMAND_WRITE_CONF_REG(data,idx));
    	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
    }



    { /* GP_VS_CONF_REG_OPMOD  */
        /* input and output mode not used in GP2
        int input_operation_mode    = 0;
        int output_operation_mode   = 0;*/

        int num_input_regs                = 16;
        int num_output_regs               = 16;

        unsigned int idx  = GP_VS_CONF_REG_OPMOD;
        unsigned int data = GP_VS_CONF_REG_OPMOD_CREATE(num_input_regs, num_output_regs);

        err = _mali_gp_job_add_vs_cmd(jobtomake, GP_VS_COMMAND_WRITE_CONF_REG(data,idx));
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
    }

    { /* GP_VS_CONF_REG_VERTICES_ALT_STRIDE */
        int input_vertices_pr_alt_stride  = 2; /* we use alt_stride at inp-line: 8*x=16 */
        int output_vertices_pr_alt_stride = 2; /* we use alt_stride at out-line: 4*x=8 */

        unsigned int idx  = GP_VS_CONF_REG_VERTICES_ALT_STRIDE ;
        unsigned int data = GP_VS_CONF_REG_VERTICES_ALT_STRIDE_CREATE(\
                input_vertices_pr_alt_stride,\
                output_vertices_pr_alt_stride) ;;

        err = _mali_gp_job_add_vs_cmd(jobtomake, GP_VS_COMMAND_WRITE_CONF_REG(data,idx));
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
    }


    /* Makes no difference unless we lower the xx_alt_stride above */
    {
        int alt_stride_input  = 16*4*2 ;
        int alt_stride_output = 16*4*1 ;

         err = _mali_gp_job_add_vs_cmd(jobtomake, \
                GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_input,  GP_VS_CONF_REG_INPUT_ALT_STRIDE_0) );
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

         err = _mali_gp_job_add_vs_cmd(jobtomake, \
                GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_input,  GP_VS_CONF_REG_INPUT_ALT_STRIDE_4) );
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

         err = _mali_gp_job_add_vs_cmd(jobtomake, \
                GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_input,  GP_VS_CONF_REG_INPUT_ALT_STRIDE_8) );
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

         err = _mali_gp_job_add_vs_cmd(jobtomake, \
                GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_input,  GP_VS_CONF_REG_INPUT_ALT_STRIDE_12) );
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

         err = _mali_gp_job_add_vs_cmd(jobtomake, \
                GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_output,  GP_VS_CONF_REG_OUTP_ALT_STRIDE_0) );
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

         err = _mali_gp_job_add_vs_cmd(jobtomake, \
                GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_output,  GP_VS_CONF_REG_OUTP_ALT_STRIDE_4) );
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

         err = _mali_gp_job_add_vs_cmd(jobtomake, \
                GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_output,  GP_VS_CONF_REG_OUTP_ALT_STRIDE_8) );
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

         err = _mali_gp_job_add_vs_cmd(jobtomake, \
                GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_output,  GP_VS_CONF_REG_OUTP_ALT_STRIDE_12) );
        if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
    }

    /* ************* END CONFIGURATION REGISTERS *********************/

    /*      ************* ADDING JOB COMMANDS *********************/


    err = _mali_gp_job_add_vs_cmd(jobtomake, GP_VS_COMMAND_LOAD_SHADER(
            (u32)_mali_mem_mali_addr_get(mp_vs_instr_copy_prog, 0),
            0,
            16));
	MALI_DEBUG_PRINT(5, ("Shader addr: 0x%08x @ %d\n",
					 (u32)_mali_mem_mali_addr_get(mp_vs_instr_copy_prog, 0),
					 (u32)_mali_mem_size_get(mp_vs_instr_copy_prog)));
    if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

    err = _mali_gp_job_add_vs_cmd(jobtomake, GP_VS_COMMAND_SHADE_VERTICES(
             1,
             4));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

    err = _mali_gp_job_add_vs_cmd(jobtomake, GP_VS_COMMAND_FLUSH_WRITEBACK_BUF() );
    if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	return jobtomake;

err_cleanup:
	assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned from add cmd");
	_mali_gp_job_free(jobtomake);
	return MALI_NO_HANDLE;



}

mali_gp_job_handle make_vs_job_one_add(suite* test_suite, mali_base_ctx_handle ctxh, u32 jobnum)
{
	mali_gp_job_handle jobtomake;
	mali_err_code err;
	u32 *vs_conf_input,*vs_conf_output;
	u32 i;
	u64* tempbuf;
	u32 max_num_cmds; /* meximum number of vertex shader cmds for the job */
	u32 cmd_num;

	jobtomake =	_mali_gp_job_new(ctxh);
	assert_fail((MALI_NO_HANDLE != jobtomake), "failed to create new job");
	if(MALI_NO_HANDLE == jobtomake) return MALI_NO_HANDLE;

	max_num_cmds = 20; /* less than 20 cmds needed for such job */
	tempbuf = MALLOC(max_num_cmds * sizeof(u64));
	assert_fail((NULL != tempbuf), "sys_malloc failed");
	if(NULL == tempbuf)
	{
		_mali_gp_job_free(jobtomake);
		return MALI_NO_HANDLE;
	}

	vs_conf_input = _mali_mem_ptr_map_area(ioconf_handle[jobnum], 0 , 256, 0, MALI_MEM_PTR_WRITABLE | MALI_MEM_PTR_READABLE);
	if(NULL == vs_conf_input)
	{
		assert_fail((NULL != vs_conf_input), "mem mapping failed");
		_mali_gp_job_free(jobtomake);
		FREE(tempbuf);
		return MALI_NO_HANDLE;
	}
    vs_conf_output = vs_conf_input + 32;

    /* Configurationregs: Input */
    for(i=0; i<31 ; i+=2)
    {
        int type, commaloc, stride;
        type        = GP_VS_VSTREAM_FORMAT_4_FIX_S16;
        commaloc    = 0;
        stride      = 16*4*2; /* 16Regs * 4Elmt * 2B = 128*/
        /* Address load 16 bit val from */
		*(vs_conf_input+i) = (u32)_mali_mem_mali_addr_get(mp_data_input, i*2*2);
        /**(vs_conf_input+i) = MALI_MEM_V2P(data_input+i*2);*/
        /* Input specifier */
        *(vs_conf_input+i+1) = GP_VS_CONF_REG_INP_SPEC_CREATE(type, commaloc, stride);
    }

    /* Configurationregs: Output */
    for(i=0; i<31 ; i+=2)
    {
        int type, commaloc, stride;
        type        = GP_VS_VSTREAM_FORMAT_4_FIX_U8;
        commaloc    = 0;
        stride      = 16*4*1; /* 16Regs * 4Elmt * 1B = 64 */
        /* Address load 16 bit val from */
        /**(vs_conf_output+i) = MALI_MEM_V2P(data_output+i*2);*/
        *(vs_conf_output+i) = (u32)_mali_mem_mali_addr_get(mp_data_output[jobnum], i*2);
        /* Output specifier */
        *(vs_conf_output+i+1) = GP_VS_CONF_REG_INP_SPEC_CREATE(type, commaloc, stride);
    }

    _mali_mem_ptr_unmap_area(ioconf_handle[jobnum]);

	cmd_num=0;
    /* ADDING THE DIFFERENT CONFIGURATION REGISTERS TO THE VS */
    tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_INPUT_OUTPUT_CONF_REGS(
    						(u32)_mali_mem_mali_addr_get(ioconf_handle[jobnum], 0),
    						0,
    						32 );
    cmd_num++;

    { /* GP_VS_CONF_REG_PROG_PARAM  */
        int start    = 0;
        int end      = 15;
        int idx_last = 15;

        u32 idx  = GP_VS_CONF_REG_PROG_PARAM;
        u32 data = GP_VS_CONF_REG_PROG_PARAM_CREATE(start,end,idx_last);;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(data,idx);
    	cmd_num++;
    }



    { /* GP_VS_CONF_REG_OPMOD  */
        /* input and output mode not used in GP2
        int input_operation_mode    = 0;
        int output_operation_mode   = 0;*/

        int num_input_regs                = 16;
        int num_output_regs               = 16;

        unsigned int idx  = GP_VS_CONF_REG_OPMOD;
        unsigned int data = GP_VS_CONF_REG_OPMOD_CREATE(num_input_regs, num_output_regs);

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(data,idx);
        cmd_num++;
    }

    { /* GP_VS_CONF_REG_VERTICES_ALT_STRIDE */
        int input_vertices_pr_alt_stride  = 2; /* we use alt_stride at inp-line: 8*x=16 */
        int output_vertices_pr_alt_stride = 2; /* we use alt_stride at out-line: 4*x=8 */

        unsigned int idx  = GP_VS_CONF_REG_VERTICES_ALT_STRIDE ;
        unsigned int data = GP_VS_CONF_REG_VERTICES_ALT_STRIDE_CREATE(\
                input_vertices_pr_alt_stride,\
                output_vertices_pr_alt_stride) ;;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(data,idx);
        cmd_num++;
    }


    /* Makes no difference unless we lower the xx_alt_stride above */
    {
        int alt_stride_input  = 16*4*2 ;
        int alt_stride_output = 16*4*1 ;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_input,  GP_VS_CONF_REG_INPUT_ALT_STRIDE_0);
        cmd_num++;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_input,  GP_VS_CONF_REG_INPUT_ALT_STRIDE_4);
        cmd_num++;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_input,  GP_VS_CONF_REG_INPUT_ALT_STRIDE_8);
        cmd_num++;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_input,  GP_VS_CONF_REG_INPUT_ALT_STRIDE_12);
        cmd_num++;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_output,  GP_VS_CONF_REG_OUTP_ALT_STRIDE_0);
        cmd_num++;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_output,  GP_VS_CONF_REG_OUTP_ALT_STRIDE_4);
        cmd_num++;

       	tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_output,  GP_VS_CONF_REG_OUTP_ALT_STRIDE_8);
        cmd_num++;

        tempbuf[cmd_num] = GP_VS_COMMAND_WRITE_CONF_REG(alt_stride_output,  GP_VS_CONF_REG_OUTP_ALT_STRIDE_12);
        cmd_num++;
    }

    /* ************* END CONFIGURATION REGISTERS *********************/

    /*      ************* ADDING JOB COMMANDS *********************/


    tempbuf[cmd_num] = GP_VS_COMMAND_LOAD_SHADER(
            (u32)_mali_mem_mali_addr_get(mp_vs_instr_copy_prog, 0),
            0,
            16);
    cmd_num++;

	MALI_DEBUG_PRINT(5, ("Shader addr: 0x%08x @ %d\n",
					 (u32)_mali_mem_mali_addr_get(mp_vs_instr_copy_prog, 0),
					 (u32)_mali_mem_size_get(mp_vs_instr_copy_prog)));


    tempbuf[cmd_num] = GP_VS_COMMAND_SHADE_VERTICES(
             1,
             4);
    cmd_num++;

    tempbuf[cmd_num] = GP_VS_COMMAND_FLUSH_WRITEBACK_BUF();
    cmd_num++;

	err = _mali_gp_job_add_vs_cmds(jobtomake, tempbuf, cmd_num);
   	assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned");

    FREE(tempbuf);

	if(err == MALI_ERR_NO_ERROR)
	{
		return jobtomake;
	}
	else
	{
		_mali_gp_job_free(jobtomake);
		return MALI_NO_HANDLE;
	}


}

u32 check_vs_result(suite* test_suite, u32 jobnum)
{
	int i;
	u32 fb_data_wrong = 0;

	_mali_sys_memset(work_buffer32, 0x1f, 256);

	_mali_mem_read_mali_to_cpu(work_buffer32, mp_data_output[jobnum], 0, 256, 1);

	/* Filling in the reference (expected values) after the readback value in work_buffer */
    for (i=0; i<256;i++) work_buffer8[1024+i]= (unsigned char) i;

	if(_mali_sys_memcmp(&work_buffer8[0], &work_buffer8[1024], 256) != 0) fb_data_wrong++;

	/* setting outputbuffer to 0 after check*/

	return fb_data_wrong;
}

void cleanup_after_vs(u32 num_jobs)
{
	u32 loop;

	for(loop = 0; loop < num_jobs; loop++)
	{
		_mali_mem_free(ioconf_handle[loop]);
		_mali_mem_free(mp_data_output[loop]);
	}

    _mali_mem_free(mp_vs_instr_copy_prog);
    _mali_mem_free(mp_data_input  );

	FREE(work_buffer32);

}

static u32 finished_bgfvs09;
static u32 priority_log[NUM_RUNS_BGFVS09];
volatile int jobs_bgfvs09_failed;

static mali_bool callback_bgfvs09(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status completion_status, mali_gp_job_handle job_handle)
{
	u32 arg = (u32)cb_param;
	priority_log[finished_bgfvs09] = arg;
	MALI_DEBUG_PRINT(5, ("Callback nr %i with arg %i from (%s) job %p which ended with result 0x%X\n", finished_bgfvs09, arg & 0xFFFF, get_pri_name(arg>>16), job_handle, completion_status));
	if(MALI_JOB_STATUS_END_SUCCESS != completion_status) jobs_bgfvs09_failed;
	finished_bgfvs09++;

	return MALI_TRUE;
}

u32 make_and_run_vs_dummyjobs_randompriority(u32 num_jobs, u32 cmdlist_length, suite* test_suite)
{
	mali_gp_job_handle job[NUM_RUNS_BGFVS09];
	mali_sync_handle sync;
	mali_base_wait_handle wait_handle;
	mali_base_ctx_handle ctxh;
	u32 i,j;
	mali_err_code err = MALI_ERR_NO_ERROR;
	jobs_bgfvs09_failed = 0;

	finished_bgfvs09 = 0;

	ctxh = _mali_base_context_create();
	assert_fail((NULL != ctxh), "ctx create failed");
	if(ctxh != NULL)
	{
		err = _mali_gp_open(ctxh);
		assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned GP open");
		if(MALI_ERR_NO_ERROR == err)
		{

		 	sync = _mali_sync_handle_new(ctxh);
			assert_fail((MALI_NO_HANDLE != sync), "Failed to create sync handle");
			if(MALI_NO_HANDLE != sync)
			{
				wait_handle = _mali_sync_handle_get_wait_handle( sync);
				assert_fail((MALI_NO_HANDLE != wait_handle), "Failed to create wait_handle");
				if(MALI_NO_HANDLE != wait_handle)
				{

					MALI_DEBUG_PRINT(5, ("making jobs\n"));
					for (i = 0; i < num_jobs; ++i)
					{
						u32 num_instructions;
						job[i] = _mali_gp_job_new(ctxh);
						assert_fail((MALI_NO_HANDLE != job), "failed to make job");

						if(MALI_NO_HANDLE == job[i])
						{
							err = MALI_ERR_OUT_OF_MEMORY;
							break;
						}
						else
						{
							if(cmdlist_length == 0) num_instructions = rand_range(1,1000);
							else num_instructions = cmdlist_length;

							for (j = 0; j < num_instructions; ++j)
							{
								err = _mali_gp_job_add_vs_cmd( job[i], GP_VS_COMMAND_WRITE_CONF_REG(0x0000000, 0x2));
								assert_fail((MALI_ERR_NO_ERROR == err), "failed to add vs cmd");
								if(MALI_ERR_NO_ERROR != err) break;

							}
							if(MALI_ERR_NO_ERROR != err)
							{
								_mali_gp_job_free(job[i]);
								break;
							}
						}

					}

					if(err != MALI_ERR_NO_ERROR)
					{
						for(j = 0; j < i; j++)
						{
							if(job[j] != MALI_NO_HANDLE){
								_mali_gp_job_free(job[j]);
							}
						}
					}
					else
					{

						MALI_DEBUG_PRINT(5, ("starting jobs\n"));
						for (i = 0; i < num_jobs; ++i)
						{
							int pri = rand_range(MALI_JOB_PRI_HIGH, MALI_JOB_PRI_LOW);
							_mali_gp_job_add_to_sync_handle( sync, job[i]);
							_mali_gp_job_set_callback( job[i], callback_bgfvs09, (void *)(pri << 16 | i));
							err = _mali_gp_job_start( job[i], MALI_REINTERPRET_CAST(mali_job_priority)pri);
							assert_fail((err == MALI_ERR_NO_ERROR), "job_start_failed");
							if(err != MALI_ERR_NO_ERROR) break;
						}
						if(MALI_ERR_NO_ERROR != err)
						{
							for(j = i; j<num_jobs; j++)
							{
								_mali_gp_job_free(job[j]);
							}
						}
					}

					_mali_sync_handle_flush( sync);

					MALI_DEBUG_PRINT(5, ("** Waiting **\n"));
					_mali_wait_on_handle( wait_handle);
					MALI_DEBUG_PRINT(5, ("** done **\n"));

				   	assert_ints_equal(finished_bgfvs09, num_jobs, "No callback recieved");
					MALI_DEBUG_PRINT(5, ("checking priorities\n"));
					MALI_DEBUG_PRINT(5, ("closing\n"));

				}
				else
				{
					_mali_sync_handle_flush(sync);
				}
			}
			_mali_gp_close(ctxh);
		}
		_mali_base_context_destroy(ctxh);
	}
	assert_ints_equal(jobs_bgfvs09_failed, 0, "At least one job finished wrong");
	return check_priority_log(priority_log, num_jobs, test_suite);
}

const char * get_pri_name(int pri)
{
	static const char * pri_name[] = {"High", "Normal", "Low"};
	return pri_name[pri];
}

u32 check_priority_log(u32 log[], u32 logsize, suite* test_suite)
{
	u32 i;
	u32 num_fails = 0;

	for(i = 0; i <logsize; i++ )
	{
		/* check if job with priority low is finished before start number, should not be possible */
		if( ((log[i]>>16)==MALI_JOB_PRI_LOW) && ((log[i] & 0xFFFF) > i) )
		{
			MALI_DEBUG_PRINT(1, ("priority is low and job finished_nr is lower than start nr(%i %i)\n", i, (log[i]>>16)));
			num_fails++;
		}
		if(( i < logsize - 1) && ((log[i]>>16) > (log[i+1]>>16)) && ((log[i] & 0xFFFF) > (log[i+1] & 0xFFFF)) )
		{

			MALI_DEBUG_PRINT(1, ("the second of to jobs that finish has highest priority, and second job was started first\n"));MALI_DEBUG_PRINT(5, ("first job finish nr %i started nr %i priority %i \n", i, (log[i] & 0xFFFF),(log[i]>>16)));
			MALI_DEBUG_PRINT(1, ("second job finish nr %i started nr %i priority %i \n", i+1, (log[i+1] & 0xFFFF),(log[i+1]>>16)));
			num_fails++;
		}
	}
	return num_fails;
}

void set_output_buffer(u32 buf_nr, u32 value)
{
	MEM_SET_MAGIC(mp_data_output[buf_nr], value);
}

#define TILES 1
#define HEAP_SIZE 512
#define POINTER_ARRAY_SIZE 1216
#define VERTEX_ARRAY_SIZE 3*4*sizeof(float)
#define VERTEX_INDEX_SIZE 3*sizeof(u32)
#define TILE_LIST_SIZE 512

volatile int global_started_bgfplbu04 = 0;
volatile unsigned int global_callbacks_bgfplbu04 = 0;
volatile int jobs_bgfplbu04_failed = 0;

mali_bool callback_bgfplbu04(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status completion_status, mali_gp_job_handle job_handle)
{
	if(MALI_JOB_STATUS_END_SUCCESS != completion_status) jobs_bgfplbu04_failed++;
    global_callbacks_bgfplbu04++;

    return MALI_TRUE;
}

static mali_err_code add_plbu_cmds(	mali_gp_job_handle jobh,
							mali_mem_handle vertex_array_handle,
							mali_mem_handle vertex_index_handle,
							mali_mem_handle heap_handle,
							mali_mem_handle pointer_array_handle,
							suite* test_suite)
{

	mali_err_code err;
	u32 payload;
	u32 top, bottom, near, far;

	/* setting up commandlist for plbu */

	/* ....write conf.reg ....*/
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( vertex_array_handle, 0), GP_PLBU_CONF_REG_VERTEX_ARRAY_ADDR));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( vertex_index_handle, 0), GP_PLBU_CONF_REG_INDEX_ARRAY_ADDR));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	/* point size addr not needed if using point size override*/
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( heap_handle, 0), GP_PLBU_CONF_REG_HEAP_START_ADDR));

	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
	MALI_DEBUG_PRINT(5, ("heap @ 0x%08X\n", _mali_mem_mali_addr_get( heap_handle, 0)));
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( heap_handle, 0) + HEAP_SIZE, GP_PLBU_CONF_REG_HEAP_END_ADDR));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	MALI_SET_AS_FLOAT(top, 0.f);
	MALI_SET_AS_FLOAT(bottom, 15.99f);

	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(top	, GP_PLBU_CONF_REG_SCISSOR_TOP));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(bottom	, GP_PLBU_CONF_REG_SCISSOR_BOTTOM));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(top	, GP_PLBU_CONF_REG_SCISSOR_LEFT));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(bottom	, GP_PLBU_CONF_REG_SCISSOR_RIGHT));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	payload = 0;
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_VIEWPORT));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	/* vertex array offset not necessary */

	payload = GP_PLBU_CONF_REGS_PARAMS(	0, /* rsw index */
										2, /* vertex idx format 32 bit*/
										1, /* point size override on*/
										1, /* point clipping select bounding box*/
										0, /* line width vertex select first vertex*/
										0, /* backf cull cw off*/
										0); /* backf cull ccw off*/

	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_PARAMS));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	payload = GP_PLBU_CONF_REGS_TILE_SIZE(0, 0);
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_TILE_SIZE));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	/* use this when point size override is turned on, 32 bit */
	MALI_SET_AS_FLOAT(payload, 1.f);
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_POINT_SIZE));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	MALI_SET_AS_FLOAT(near, 0.f);
	MALI_SET_AS_FLOAT(far, 1.f);
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(near, GP_PLBU_CONF_REG_Z_NEAR));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(far, GP_PLBU_CONF_REG_Z_FAR));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	/*.....array_base_addr....*/
	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_POINTER_ARRAY_BASE_ADDR_TILE_COUNT(_mali_mem_mali_addr_get( pointer_array_handle, 0), 2));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_PREPARE_FRAME(TILES));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_PROCESS_SHADED_VERTICES(
	                                0,
	                                3, /* ? */
	                                GP_PLBU_PRIM_TYPE_INDIVIDUAL_TRIANGLES,
	                                GP_PLBU_OPMODE_DRAW_ELEMENTS));
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

	err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_END_FRAME());
	if(err != MALI_ERR_NO_ERROR) goto err_cleanup;

err_cleanup:
	assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned from add plbu cmd");
	return err;
}

static mali_err_code add_plbu_cmds_one_go(	mali_gp_job_handle jobh,
									mali_mem_handle vertex_array_handle,
									mali_mem_handle vertex_index_handle,
									mali_mem_handle heap_handle,
									mali_mem_handle pointer_array_handle,
									suite* test_suite)
{

	mali_err_code err;
	u32 payload;
	u32 top, bottom, near, far;

	u64* tempbuf;
	u32 max_num_cmds; /* meximum number of vertex shader cmds for the job */
	u32 cmd_num;

	max_num_cmds = 20; /* less than 20 cmds needed for such job */
	tempbuf = MALLOC(max_num_cmds * sizeof(u64));
	assert_fail((tempbuf != NULL), "sys malloc failed\n");
	if(tempbuf != NULL)
	{

		/* setting up commandlist for plbu */
		cmd_num = 0;
		/* ....write conf.reg ....*/
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( vertex_array_handle, 0), GP_PLBU_CONF_REG_VERTEX_ARRAY_ADDR);
		cmd_num++;
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( vertex_index_handle, 0), GP_PLBU_CONF_REG_INDEX_ARRAY_ADDR);
		cmd_num++;

		/* point size addr not needed if using point size override*/
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( heap_handle, 0), GP_PLBU_CONF_REG_HEAP_START_ADDR);
		cmd_num++;

		MALI_DEBUG_PRINT(5, ("heap @ 0x%08X\n", _mali_mem_mali_addr_get( heap_handle, 0)));

		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( heap_handle, 0) + HEAP_SIZE, GP_PLBU_CONF_REG_HEAP_END_ADDR);
		cmd_num++;

		MALI_SET_AS_FLOAT(top, 0.f);
		MALI_SET_AS_FLOAT(bottom, 15.99f);

		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(top	, GP_PLBU_CONF_REG_SCISSOR_TOP);
		cmd_num++;
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(bottom	, GP_PLBU_CONF_REG_SCISSOR_BOTTOM);
		cmd_num++;
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(top	, GP_PLBU_CONF_REG_SCISSOR_LEFT);
		cmd_num++;
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(bottom	, GP_PLBU_CONF_REG_SCISSOR_RIGHT);
		cmd_num++;

		payload = 0;
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_VIEWPORT);
		cmd_num++;

		/* vertex array offset not necessary */

		payload = GP_PLBU_CONF_REGS_PARAMS(	0, /* rsw index */
											2, /* vertex idx format 32 bit*/
											1, /* point size override on*/
											1, /* point clipping select bounding box*/
											0, /* line width vertex select first vertex*/
											0, /* backf cull cw off*/
											0); /* backf cull ccw off*/

		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_PARAMS);
		cmd_num++;

		payload = GP_PLBU_CONF_REGS_TILE_SIZE(0, 0);
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_TILE_SIZE);
		cmd_num++;

		/* use this when point size override is turned on, 32 bit */
		MALI_SET_AS_FLOAT(payload, 1.f);
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_POINT_SIZE);
		cmd_num++;

		MALI_SET_AS_FLOAT(near, 0.f);
		MALI_SET_AS_FLOAT(far, 1.f);
		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(near, GP_PLBU_CONF_REG_Z_NEAR);
		cmd_num++;

		tempbuf[cmd_num] = GP_PLBU_COMMAND_WRITE_CONF_REG(far, GP_PLBU_CONF_REG_Z_FAR);
		cmd_num++;

		/*.....array_base_addr....*/
		tempbuf[cmd_num] = GP_PLBU_COMMAND_POINTER_ARRAY_BASE_ADDR_TILE_COUNT(_mali_mem_mali_addr_get( pointer_array_handle, 0), 2);
		cmd_num++;

		tempbuf[cmd_num] = GP_PLBU_COMMAND_PREPARE_FRAME(TILES);
		cmd_num++;

		tempbuf[cmd_num] = GP_PLBU_COMMAND_PROCESS_SHADED_VERTICES(
		                                0,
		                                3, /* ? */
		                                GP_PLBU_PRIM_TYPE_INDIVIDUAL_TRIANGLES,
		                                GP_PLBU_OPMODE_DRAW_ELEMENTS);
		cmd_num++;

		tempbuf[cmd_num] = GP_PLBU_COMMAND_END_FRAME();
		cmd_num++;

		err = _mali_gp_job_add_plbu_cmds(jobh, tempbuf, cmd_num);
	   	assert_fail((MALI_ERR_NO_ERROR == err), "failed to add plbu cmds");

	   	FREE(tempbuf);

	   	return err;

	}
	else
	{
		return MALI_ERR_OUT_OF_MEMORY;
	}
}

void reset_global_cbs()
{
	global_callbacks_bgfplbu04 = 0;
}

mali_err_code common_for_bgfplbu04_and_bgfplbu05(u32 run_nr, suite* test_suite, u32 one_go)
{
	u32 payload;
	mali_base_ctx_handle ctxh;
    mali_gp_job_handle jobh;
	mali_base_wait_handle wait_handle;
    mali_err_code err = MALI_ERR_NO_ERROR;

	float vertex_array[12];
	u32 vertex_index[3];
	u64 expected_result[2];
	u32 cor_memcmp;

	mali_mem_handle heap_handle;
	mali_mem_handle pointer_array_handle;
	mali_mem_handle vertex_array_handle;
	mali_mem_handle vertex_index_handle;
	mali_mem_handle tile_list;

	u64 *check_data;

	ctxh = _mali_base_context_create();
	assert_fail((NULL != ctxh), "ctx create failed");
	if(ctxh != NULL)
	{
		err = _mali_gp_open(ctxh);
		assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned GP open");
		if(MALI_ERR_NO_ERROR == err)
		{
		    err = _mali_mem_open(ctxh);
			assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned mem open");
			if(MALI_ERR_NO_ERROR == err)
			{

				jobh = _mali_gp_job_new(ctxh);
				assert_fail((MALI_NO_HANDLE != jobh), "nullpointer");
				if(MALI_NO_HANDLE != jobh)
				{
					wait_handle = _mali_gp_job_get_wait_handle( jobh);
					assert_fail((MALI_NO_HANDLE != wait_handle), "failed to get wait_handle");
					if(MALI_NO_HANDLE == wait_handle)
					{
						_mali_gp_job_free(jobh);
						_mali_mem_close(ctxh);
						_mali_gp_close(ctxh);
						_mali_base_context_destroy(ctxh);
						return MALI_ERR_OUT_OF_MEMORY;
					}
					heap_handle = _mali_mem_alloc(ctxh, HEAP_SIZE, 0, MALI_GP_READ | MALI_GP_WRITE);
					assert_fail((MALI_NO_HANDLE != heap_handle), "failed to allocate heap_handle");
					if(MALI_NO_HANDLE == heap_handle)
					{
						_mali_gp_job_free(jobh);
						_mali_wait_on_handle(wait_handle);
						_mali_mem_close(ctxh);
						_mali_gp_close(ctxh);
						_mali_base_context_destroy(ctxh);
						return MALI_ERR_OUT_OF_MEMORY;
					}

					_mali_gp_job_add_mem_to_free_list( jobh, heap_handle);
					_mali_gp_job_set_tile_heap(jobh, heap_handle);
					pointer_array_handle = _mali_mem_alloc(ctxh, POINTER_ARRAY_SIZE, 0, MALI_GP_READ);
					PRINT_MAGIC(pointer_array_handle );
					assert_fail((MALI_NO_HANDLE != pointer_array_handle), "failed to allocate pointer array");
					if(MALI_NO_HANDLE == pointer_array_handle)
					{
						_mali_gp_job_free(jobh);
						_mali_wait_on_handle(wait_handle);
						_mali_mem_close(ctxh);
						_mali_gp_close(ctxh);
						_mali_base_context_destroy(ctxh);
						return MALI_ERR_OUT_OF_MEMORY;
					}
					_mali_gp_job_add_mem_to_free_list( jobh, pointer_array_handle);
					vertex_array_handle = _mali_mem_alloc(ctxh, VERTEX_ARRAY_SIZE, 64, MALI_GP_READ);
					PRINT_MAGIC(vertex_array_handle);
					assert_fail((MALI_NO_HANDLE != vertex_array_handle), "failed to allocate vertex array");
					if(MALI_NO_HANDLE == vertex_array_handle)
					{
						_mali_gp_job_free(jobh);
						_mali_wait_on_handle(wait_handle);
						_mali_mem_close(ctxh);
						_mali_gp_close(ctxh);
						_mali_base_context_destroy(ctxh);
						return MALI_ERR_OUT_OF_MEMORY;
					}
					_mali_gp_job_add_mem_to_free_list( jobh, vertex_array_handle);
					vertex_index_handle = _mali_mem_alloc(ctxh, VERTEX_INDEX_SIZE, 0, MALI_GP_READ);
					PRINT_MAGIC(vertex_index_handle );
					assert_fail((MALI_NO_HANDLE != vertex_index_handle), "failed to allocate vertex index");
					if(MALI_NO_HANDLE == vertex_index_handle)
					{
						_mali_gp_job_free(jobh);
						_mali_wait_on_handle(wait_handle);
						_mali_mem_close(ctxh);
						_mali_gp_close(ctxh);
						_mali_base_context_destroy(ctxh);
						return MALI_ERR_OUT_OF_MEMORY;
					}
					_mali_gp_job_add_mem_to_free_list( jobh, vertex_index_handle);
					tile_list = _mali_mem_alloc(ctxh, TILE_LIST_SIZE, 128, MALI_GP_READ);
					PRINT_MAGIC(tile_list);
					assert_fail((MALI_NO_HANDLE != tile_list), "failed to alloacte tile_list");
					if(MALI_NO_HANDLE == tile_list)
					{
						_mali_gp_job_free(jobh);
						_mali_wait_on_handle(wait_handle);
						_mali_mem_close(ctxh);
						_mali_gp_close(ctxh);
						_mali_base_context_destroy(ctxh);
						return MALI_ERR_OUT_OF_MEMORY;
					}
					check_data = (u64 *) MALLOC(TILE_LIST_SIZE);
					if(NULL == check_data)
					{
						_mali_mem_free(tile_list);
						_mali_gp_job_free(jobh);
						_mali_wait_on_handle(wait_handle);
						_mali_mem_close(ctxh);
						_mali_gp_close(ctxh);
						_mali_base_context_destroy(ctxh);
						return MALI_ERR_OUT_OF_MEMORY;
					}
					_mali_sys_memset(check_data, 0, TILE_LIST_SIZE);

					_mali_mem_write(tile_list , 0, check_data, TILE_LIST_SIZE);

					vertex_array[0] = (float)2;  /* x */
					vertex_array[1] = (float)2;  /* y */
					vertex_array[2] = (float)0;  /* z */
					vertex_array[3] = (float)1;  /* alpha */

					vertex_array[4] = (float)10; /* x */
					vertex_array[5] = (float)2;  /* y */
					vertex_array[6] = (float)0;  /* z */
					vertex_array[7] = (float)1;  /* alpha */

					vertex_array[8] = (float)10; /* x */
					vertex_array[9] = (float)10; /* y */
					vertex_array[10]= (float)0;  /* z */
					vertex_array[11]= (float)1;  /* alpha */

					_mali_mem_write_cpu_to_mali(vertex_array_handle, 0, vertex_array, VERTEX_ARRAY_SIZE,sizeof(u64));

					vertex_index[0] = 0;
					vertex_index[1] = 1;
					vertex_index[2] = 2;

					_mali_mem_write_cpu_to_mali(vertex_index_handle, 0, vertex_index, 3*sizeof(u32),sizeof(u32));

					payload = _mali_mem_mali_addr_get(tile_list, 0);

					MALI_DEBUG_PRINT(5, ("tile_list @ 0x%08X\n", _mali_mem_mali_addr_get( tile_list, 0)));

					_mali_mem_write_cpu_to_mali(pointer_array_handle, 0, &payload, sizeof(mali_addr), sizeof(u32));

					expected_result[0] = vertex_index[0] | (vertex_index[1] << 17)  | (((u64)vertex_index[2]) << 34);
					expected_result[1] = (1LL << 63) | (12LL << 58) | (7LL << 29) | (3LL);

					MALI_DEBUG_PRINT(5, ("Expected result 0x%016llx  0x%016llx\n",
									 expected_result[0],
									 expected_result[1]));
					if(one_go == 1)
					{
					 	err = add_plbu_cmds_one_go(jobh, vertex_array_handle, vertex_index_handle, heap_handle, pointer_array_handle, test_suite);
					}
					else
					{
					 	err = add_plbu_cmds(jobh, vertex_array_handle, vertex_index_handle, heap_handle, pointer_array_handle, test_suite);
					}
					if(MALI_ERR_NO_ERROR != err)
					{
						FREE(check_data);
						_mali_mem_free(tile_list);
						_mali_gp_job_free(jobh);
						_mali_wait_on_handle(wait_handle);
						_mali_mem_close(ctxh);
						_mali_gp_close(ctxh);
						_mali_base_context_destroy(ctxh);
						return MALI_ERR_OUT_OF_MEMORY;
					}

				    _mali_gp_job_set_callback( jobh, callback_bgfplbu04, NULL);
					jobs_bgfplbu04_failed = 0;
					err = _mali_gp_job_start( jobh, MALI_JOB_PRI_HIGH);
					assert_fail((err == MALI_ERR_NO_ERROR), "Fail from job started");
					if(err == MALI_ERR_NO_ERROR)
					{

					    global_started_bgfplbu04++;

						_mali_wait_on_handle( wait_handle);

					    assert_ints_equal(global_callbacks_bgfplbu04, run_nr, "Wrong number of callbacks");

						_mali_mem_read_mali_to_cpu( check_data, tile_list, 0, TILE_LIST_SIZE, sizeof(u64));
						cor_memcmp = _mali_sys_memcmp(check_data, expected_result, 16);

						assert_ints_equal(cor_memcmp, 0, "Tile list produced is not correct");
						assert_ints_equal(jobs_bgfplbu04_failed, 0, "At least one of the jobs did not finsh OK");
					}
					else
					{
						_mali_gp_job_free(jobh);
						_mali_wait_on_handle(wait_handle);
					}
					_mali_mem_free(tile_list);
					FREE(check_data);
				}
				_mali_gp_close(ctxh);
			}
			_mali_mem_close(ctxh);
		}
		_mali_base_context_destroy(ctxh);
	}
	return err;
}

/***** Stuff for bgfplbu08 *****/

#define TILES 1
#undef HEAP_SIZE
#define HEAP_SIZE 1024
#define POINTER_ARRAY_SIZE 1216
#define VERTEX_ARRAY_SIZE 3*4*sizeof(float)
#define VERTEX_INDEX_SIZE 3*sizeof(u32)
#define TILE_LIST_SIZE 512
#define PLBU_CHUNK_NR 		16
#define PLBU_CHUNK_SIZE		(PLBU_CHUNK_NR*sizeof(u64))
#define CHECK_DATA_ELEMENTS 2

#define DUMMY_MEM_WORD0  0xcafebabe
#define DUMMY_MEM_WORD1  0xdeadbabe
#define DUMMY_MEM_WORD2  0xdeadbeef
#define DUMMY_MEM_WORD3  0xfeedbeef
#define DUMMY_MEM_WORD4  0xfeedbabe
#define DUMMY_MEM_WORD5  0xbeefbabe
#define DUMMY_MEM_WORD6  0x01234567
#define DUMMY_MEM_WORD7  0xfedcba98

volatile int global_started_bgfplbu08=0;
volatile int global_callbacks_bgfplbu08=0;

/*
 * check data in plbu heap as it is used in bgfplbu08, bgfplbu09, bgfplbu10
 *
 */

int check_plbu_heap_result(mali_mem_handle tile_list, mali_mem_handle heap_handle, u32 polygon_ctr, u64 * expected_result, u32 growable_heap)
{
	/*u64 * readback_tile = NULL;*/
	u64 readback;
	int err = 0;
	int i = 0 ;
	u32 addr = 0 ;
	u64 jump_cmd;

	int num_commands = polygon_ctr + (polygon_ctr/(PLBU_CHUNK_NR-1))
	+ (polygon_ctr%(PLBU_CHUNK_NR-1) ? 1 : 0);


	addr = _mali_mem_mali_addr_get(tile_list, 0);

	for (i = 0; i < num_commands; i++, addr+=8)
	{
		if (i < PLBU_CHUNK_NR)
		{
			_mali_mem_read_mali_to_cpu(&readback, tile_list, i*8, 8, sizeof(u64));
		}
		else
		{
#if !defined(HARDWARE_ISSUE_3251)
			if(growable_heap == 1)/* bgfplbu09-bgfplbu11  for r0p3 and newer */
			{
				/* TODO: need to implement _mali_mem_heap_read64_endian_safe(); */
				readback = _mali_mem_heap_read64(heap_handle, (i-PLBU_CHUNK_NR)*8);
				/* ...because, this is the only direct dependency of byte order routinens */
				_mali_byteorder_swap_endian(&readback, 8, 8);
			}
			else /* bgfplbu08 */
#endif
			{
				MALI_DEBUG_ASSERT(growable_heap == 0, ("growable_heap not 0, not supported in this build"));
				_mali_mem_read_mali_to_cpu(&readback, heap_handle, (i-PLBU_CHUNK_NR)*8, 8, sizeof(u64));
			}
		}

		if ((i & (PLBU_CHUNK_NR-1)) == (PLBU_CHUNK_NR-1) || (i == (num_commands-1)))
		{
			/* non-polygon */
			if (i == (num_commands-1))
			{
				/* last */
				MALI_DEBUG_PRINT(5, ("@0x%08X i:%02d Read: 0x%016llx  Expect: 0x%016llx \n",
								 addr,
								 i,
								 /* readback_tile[idx] , */
								 readback,
								 expected_result[1]));
				if ( readback  != expected_result[1])
				{
					MALI_DEBUG_PRINT(5, ("ERR: @0x%08X i:%02d Read: 0x%016llx  Expect: 0x%016llx\n",
									 addr,
									 i,
									 readback,
									 expected_result[1]));
					err++;

				}
			}
			else
			{
				/* jump */

				/* The physical-address we jump to */
				u32 new_addr_heap = _mali_mem_mali_addr_get(heap_handle, (i-(PLBU_CHUNK_NR-1))*8);
				MALI_DEBUG_PRINT(5, ("Getting address at offset %d: 0x%08X\n", (i-(PLBU_CHUNK_NR-1))*8, new_addr_heap));
				/* The command to jump to this address */
				jump_cmd      = ((((u64)0xB0000000)<<32) | (new_addr_heap>>3));

				if (0xffffffff == new_addr_heap) return err;

				/* Checking that this last command in the block is this jump_cmd */
				MALI_DEBUG_PRINT(5, ("@0x%08X i:%02d Read: 0x%016llx  Expect: 0x%016llx idx:%02d JMP\n",
								 addr,
								 i,
								 readback,
								 jump_cmd,
								 17));

				MALI_DEBUG_PRINT(5, ("New heap addr: 0x%x offset: 0x%x\n", new_addr_heap, 17));


				if ( readback != jump_cmd )
				{
					MALI_DEBUG_PRINT(5, ("ERR:@0x%08X i:%02d Read: 0x%016llx  Expect: 0x%016llx idx:%02d JMP\n",
									 addr,
									 i,
									 readback ,
									 jump_cmd,
									 17));
					err++;

				}

				/* Setting variables according to this new address */

				addr = new_addr_heap - 8;
			}
		}
		else
		{
			MALI_DEBUG_PRINT(5, ("@0x%08X i:%02d Read: 0x%016llx  Expect: 0x%016llx idx:%02d INS\n",
							 addr,
							 i,
							 readback,
							 expected_result[0],
							 17));
		}

	}

	return err;

}

volatile unsigned int jobs_bgfplbu08_failed;

mali_bool callback_bgfplbu08(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status completion_status, mali_gp_job_handle job_handle)
{
	global_callbacks_bgfplbu08++;
	if(MALI_JOB_STATUS_END_SUCCESS != completion_status) jobs_bgfplbu08_failed++;

	return MALI_TRUE;
}

void common_bgfplbu08_bgfplbu09(u32 polygon_ctr, u32 heap_default, u32 heap_max, u32 heap_block, suite* test_suite, u32 growable_heap, u32 should_fail)
{

	u32 payload;
	u32 top, bottom, near, far;
	mali_base_ctx_handle ctxh;
	mali_gp_job_handle jobh;
	mali_sync_handle sync;
	mali_base_wait_handle wait_handle;
	mali_err_code err = MALI_ERR_NO_ERROR;
	u32 i;
	mali_bool addfail = MALI_FALSE;

	float vertex_array[12];
	u32 vertex_index[3];
	u64 expected_result[2];
	u32 cor_memcmp;

	mali_mem_handle heap_handle;
	mali_mem_handle pointer_array_handle;
	mali_mem_handle vertex_array_handle;
	mali_mem_handle vertex_index_handle;
	mali_mem_handle tile_list;

	jobs_bgfplbu08_failed = 0;

	ctxh = _mali_base_context_create();
	assert_fail((NULL != ctxh), "ctx create failed");
	if(ctxh != NULL)
	{
		err = _mali_gp_open(ctxh);
		assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned GP open");
		if(MALI_ERR_NO_ERROR == err)
		{
		    err = _mali_mem_open(ctxh);
			assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned mem open");
			if(MALI_ERR_NO_ERROR == err)
			{

				sync = _mali_sync_handle_new(ctxh);
				assert_fail((MALI_NO_HANDLE != sync), "failed to create synchandle");
				if(MALI_NO_HANDLE != sync)
				{

					wait_handle = _mali_sync_handle_get_wait_handle( sync);
					assert_fail((MALI_NO_HANDLE != wait_handle), "failed to create waithandle");
					if(MALI_NO_HANDLE != wait_handle)
					{

						jobh = _mali_gp_job_new(ctxh);
						assert_fail((MALI_NO_HANDLE != jobh), "failed to create job");
						if(MALI_NO_HANDLE != jobh)
						{

#if !defined(HARDWARE_ISSUE_3251)
							if(growable_heap == 1)
							{
								heap_handle = _mali_mem_heap_alloc(ctxh, heap_default, heap_max, heap_block);
							} else
#endif
							{
								MALI_DEBUG_ASSERT(growable_heap == 0, ("growable_heap not 0, not supported in this build"));
								heap_handle = _mali_mem_alloc(ctxh, heap_max, 0, MALI_GP_READ | MALI_GP_WRITE);
							}

							assert_fail((MALI_NO_HANDLE != heap_handle), "failed to allocate heap handle");
							if(MALI_NO_HANDLE == heap_handle)
							{
								_mali_gp_job_free(jobh);
								_mali_sync_handle_flush( sync);
								_mali_wait_on_handle( wait_handle);
								_mali_mem_close(ctxh);
								_mali_gp_close(ctxh);
								_mali_base_context_destroy(ctxh);
								return;
							}

							/* MEM_SET_MAGIC(heap_handle, DUMMY_MEM_WORD0 ); */

							_mali_gp_job_set_tile_heap(jobh, heap_handle);

							tile_list = _mali_mem_alloc(ctxh, TILE_LIST_SIZE, 128, MALI_GP_READ);
							assert_fail((MALI_NO_HANDLE != tile_list), "failed to allocate tile_list");
							if(MALI_NO_HANDLE == tile_list)
							{
								_mali_mem_free(heap_handle);
								_mali_gp_job_free(jobh);
								_mali_sync_handle_flush( sync);
								_mali_wait_on_handle( wait_handle);
								_mali_mem_close(ctxh);
								_mali_gp_close(ctxh);
								_mali_base_context_destroy(ctxh);
								return;
							}
							MEM_SET_MAGIC(tile_list, DUMMY_MEM_WORD0 );

							pointer_array_handle = _mali_mem_alloc(ctxh, POINTER_ARRAY_SIZE, 0, MALI_GP_READ);
							assert_fail((MALI_NO_HANDLE != pointer_array_handle), "failed to allocate pointer array");
							if(MALI_NO_HANDLE == pointer_array_handle)
							{
								_mali_mem_free(tile_list);
								_mali_mem_free(heap_handle);
								_mali_gp_job_free(jobh);
								_mali_sync_handle_flush( sync);
								_mali_wait_on_handle( wait_handle);
								_mali_mem_close(ctxh);
								_mali_gp_close(ctxh);
								_mali_base_context_destroy(ctxh);
								return;
							}

							MEM_SET_MAGIC(pointer_array_handle , DUMMY_MEM_WORD1 );
							_mali_gp_job_add_mem_to_free_list( jobh, pointer_array_handle);

							vertex_array_handle = _mali_mem_alloc(ctxh, VERTEX_ARRAY_SIZE, 64, MALI_GP_READ);
							assert_fail((MALI_NO_HANDLE != vertex_array_handle), "failed to allocate pointer array");
							if(MALI_NO_HANDLE == vertex_array_handle)
							{
								_mali_mem_free(tile_list);
								_mali_mem_free(heap_handle);
								_mali_gp_job_free(jobh);
								_mali_sync_handle_flush( sync);
								_mali_wait_on_handle( wait_handle);
								_mali_mem_close(ctxh);
								_mali_gp_close(ctxh);
								_mali_base_context_destroy(ctxh);
								return;
							}
							MEM_SET_MAGIC(vertex_array_handle, DUMMY_MEM_WORD2 );
							_mali_gp_job_add_mem_to_free_list( jobh, vertex_array_handle);

							vertex_index_handle = _mali_mem_alloc(ctxh, VERTEX_INDEX_SIZE, 0, MALI_GP_READ);
							assert_fail((MALI_NO_HANDLE != vertex_index_handle), "failed to allocate pointer array");
							if(MALI_NO_HANDLE == vertex_index_handle)
							{
								_mali_mem_free(tile_list);
								_mali_mem_free(heap_handle);
								_mali_gp_job_free(jobh);
								_mali_sync_handle_flush( sync);
								_mali_wait_on_handle( wait_handle);
								_mali_mem_close(ctxh);
								_mali_gp_close(ctxh);
								_mali_base_context_destroy(ctxh);
								return;
							}
							MEM_SET_MAGIC(vertex_index_handle, DUMMY_MEM_WORD3 );
							_mali_gp_job_add_mem_to_free_list( jobh, vertex_index_handle);

							/* Allocated from stack */
							vertex_array[0] = (float)2;  /* x */
							vertex_array[1] = (float)2;  /* y */
							vertex_array[2] = (float)0;  /* z */
							vertex_array[3] = (float)1;  /* alpha */

							vertex_array[4] = (float)10; /* x */
							vertex_array[5] = (float)2;  /* y */
							vertex_array[6] = (float)0;  /* z */
							vertex_array[7] = (float)1;  /* alpha */

							vertex_array[8] = (float)10; /* x */
							vertex_array[9] = (float)10; /* y */
							vertex_array[10]= (float)0;  /* z */
							vertex_array[11]= (float)1;  /* alpha */

							_mali_mem_write_cpu_to_mali(vertex_array_handle, 0, vertex_array, VERTEX_ARRAY_SIZE, sizeof(u64));

							vertex_index[0] = 0;
							vertex_index[1] = 1;
							vertex_index[2] = 2;

							_mali_mem_write_cpu_to_mali(vertex_index_handle, 0, vertex_index, 3*sizeof(u32),sizeof(u32));

							payload = _mali_mem_mali_addr_get(tile_list, 0);

							_mali_mem_write_cpu_to_mali(pointer_array_handle, 0, &payload, sizeof(u32),sizeof(u32));


							/* setting up commandlist for plbu */

							/* ....write conf.reg ....*/
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( vertex_array_handle, 0), GP_PLBU_CONF_REG_VERTEX_ARRAY_ADDR));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( vertex_index_handle, 0), GP_PLBU_CONF_REG_INDEX_ARRAY_ADDR));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							/* point size addr not needed if using point size override*/
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( heap_handle, 0), GP_PLBU_CONF_REG_HEAP_START_ADDR));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

#if !defined(HARDWARE_ISSUE_3251)
							if(growable_heap == 1)
							{
								err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_heap_get_end_address( heap_handle), GP_PLBU_CONF_REG_HEAP_END_ADDR));
							} else
#endif
							{
								MALI_DEBUG_ASSERT(growable_heap == 0, ("growable_heap not 0, not supported in this build"));
								err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( heap_handle, 0) + heap_max, GP_PLBU_CONF_REG_HEAP_END_ADDR));
							}

							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							MALI_SET_AS_FLOAT(top, 0.f);
							MALI_SET_AS_FLOAT(bottom, 15.99f);

							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(top	, GP_PLBU_CONF_REG_SCISSOR_TOP));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(bottom	, GP_PLBU_CONF_REG_SCISSOR_BOTTOM));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(top	, GP_PLBU_CONF_REG_SCISSOR_LEFT));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(bottom	, GP_PLBU_CONF_REG_SCISSOR_RIGHT));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							payload = 0;
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_VIEWPORT));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
							/* vertex array offset not necessary */
							payload = GP_PLBU_CONF_REGS_PARAMS(	0, /* rsw index */
																2, /* vertex idx format 32 bit*/
																1, /* point size override on*/
																1, /* point clipping select bounding box*/
																0, /* line width vertex select first vertex*/
																0, /* backf cull cw off*/
																0); /* backf cull ccw off*/

							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_PARAMS));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							payload = GP_PLBU_CONF_REGS_TILE_SIZE(0, 0);
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_TILE_SIZE));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
							/* use this when point size override is turned on, 32 bit */
							MALI_SET_AS_FLOAT(payload, 1.f);
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_POINT_SIZE));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							MALI_SET_AS_FLOAT(near, 0.f);
							MALI_SET_AS_FLOAT(far, 1.f);
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(near, GP_PLBU_CONF_REG_Z_NEAR));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(far, GP_PLBU_CONF_REG_Z_FAR));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							/*.....array_base_addr....*/
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_POINTER_ARRAY_BASE_ADDR_TILE_COUNT(_mali_mem_mali_addr_get( pointer_array_handle, 0), 2));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_PREPARE_FRAME(TILES));
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

							for (i=0; (i<polygon_ctr)&&(MALI_ERR_NO_ERROR == err); i++ )
							{
								err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_PROCESS_SHADED_VERTICES(
										0,
										3,
										GP_PLBU_PRIM_TYPE_INDIVIDUAL_TRIANGLES,
										GP_PLBU_OPMODE_DRAW_ELEMENTS));

								if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;


							}
							err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_END_FRAME());
							if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;


							assert_fail((addfail == MALI_FALSE), "failed to add plbu cmd");
							if(addfail)
							{
								_mali_mem_free(tile_list);
								_mali_mem_free(heap_handle);
								_mali_gp_job_free(jobh);
								_mali_sync_handle_flush( sync);
								_mali_wait_on_handle( wait_handle);
								_mali_mem_close(ctxh);
								_mali_gp_close(ctxh);
								_mali_base_context_destroy(ctxh);
								return;
							}


							_mali_gp_job_add_to_sync_handle( sync, jobh);
							_mali_gp_job_set_callback( jobh, callback_bgfplbu08, NULL);

							MALI_DEBUG_PRINT(4, ("Size of heap before start:%d\n",_mali_mem_size_get(heap_handle)));

							global_callbacks_bgfplbu08 = 0;
							err = _mali_gp_job_start( jobh, MALI_JOB_PRI_HIGH);
							assert_fail((MALI_ERR_NO_ERROR == err), "gp_job_start failed");
							if(MALI_ERR_NO_ERROR == err)
							{
								global_started_bgfplbu08++;

								_mali_sync_handle_flush( sync);
								_mali_wait_on_handle( wait_handle);
								MALI_DEBUG_PRINT(4, ("Size of heap after run:%d\n",_mali_mem_size_get(heap_handle)));

								assert_ints_equal(global_callbacks_bgfplbu08, 1, "Wrong number of callbacks");
								/*_mali_mem_read( check_data, tile_list, 0, TILE_LIST_SIZE);
								cor_memcmp = _mali_sys_memcmp(check_data, expected_result, 16);*/

								expected_result[0] = vertex_index[0] | (vertex_index[1] << 17)  | (((u64)vertex_index[2]) << 34);
								expected_result[1] = (1LL << 63) | (12LL << 58) | (7LL << 29) | (3LL);

								MALI_DEBUG_PRINT(5, ("Expected result 0x%016llx  0x%016llx\n",
												 expected_result[0],
												 expected_result[1]));

								cor_memcmp = check_plbu_heap_result(tile_list, heap_handle, polygon_ctr, expected_result, growable_heap);

								assert_ints_equal(cor_memcmp, 0, "Tile list produced is not correct");
							}
							else
							{
								_mali_gp_job_free(jobh);
								_mali_sync_handle_flush( sync);
								_mali_wait_on_handle( wait_handle);
							}

							_mali_mem_free(tile_list);
							_mali_mem_free(heap_handle);
						}
						else /* failed to create job */
						{
							_mali_sync_handle_flush(sync);
							_mali_wait_on_handle(wait_handle);
						}
					}
					else /* failed to create wait_handle */
					{
						_mali_sync_handle_flush(sync);
					}
				}

				_mali_mem_close(ctxh);
			}
			_mali_gp_close(ctxh);
		}
		_mali_base_context_destroy(ctxh);
	}
	/*check if job finish status is as expected */
	assert_ints_equal(jobs_bgfplbu08_failed, should_fail, "Job finish status not as expected")

}

mali_err_code run_plbu_heap_job(mali_base_ctx_handle ctxh, suite* test_suite, mali_mem_handle heap_handle, u32 polygon_ctr, mali_mem_handle vertex_array_handle, mali_mem_handle vertex_index_handle, mali_mem_handle pointer_array_handle, mali_mem_handle stack, int isLast)
{
	u32 payload;
	u32 top, bottom, near, far;
	mali_gp_job_handle jobh;
	mali_sync_handle sync;
	mali_base_wait_handle wait_handle;
	mali_err_code err = MALI_ERR_NO_ERROR;
	u32 i;
	mali_bool addfail = MALI_FALSE;

	sync = _mali_sync_handle_new(ctxh);
	assert_fail((MALI_NO_HANDLE != sync), "failed to create synchandle");
	if(MALI_NO_HANDLE != sync)
	{

		wait_handle = _mali_sync_handle_get_wait_handle( sync);
		assert_fail((MALI_NO_HANDLE != wait_handle), "failed to create waithandle");
		if(MALI_NO_HANDLE != wait_handle)
		{

			jobh = _mali_gp_job_new(ctxh);
			assert_fail((MALI_NO_HANDLE != jobh), "failed to create job");
			if(MALI_NO_HANDLE != jobh)
			{

				_mali_gp_job_set_tile_heap(jobh, heap_handle);

				/* setting up commandlist for plbu */

				err =  _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_POINTER_ARRAY_BASE_ADDR_TILE_COUNT( _mali_mem_mali_addr_get( pointer_array_handle, 0 ),2));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				/* load heap addresses */
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_LOAD_CONF_REG( _mali_mem_mali_addr_get( stack, (0*4)), GP_PLBU_CONF_REG_HEAP_START_ADDR ));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_LOAD_CONF_REG( _mali_mem_mali_addr_get( stack, (1*4)), GP_PLBU_CONF_REG_HEAP_END_ADDR ));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				/* ....write conf.reg ....*/
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( vertex_array_handle, 0), GP_PLBU_CONF_REG_VERTEX_ARRAY_ADDR));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(_mali_mem_mali_addr_get( vertex_index_handle, 0), GP_PLBU_CONF_REG_INDEX_ARRAY_ADDR));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				MALI_SET_AS_FLOAT(top, 0.f);
				MALI_SET_AS_FLOAT(bottom, 15.99f);

				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(top	, GP_PLBU_CONF_REG_SCISSOR_TOP));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(bottom	, GP_PLBU_CONF_REG_SCISSOR_BOTTOM));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(top	, GP_PLBU_CONF_REG_SCISSOR_LEFT));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(bottom	, GP_PLBU_CONF_REG_SCISSOR_RIGHT));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				payload = 0;
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_VIEWPORT));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				/* vertex array offset not necessary */
				payload = GP_PLBU_CONF_REGS_PARAMS(	0, /* rsw index */
													2, /* vertex idx format 32 bit*/
													1, /* point size override on*/
													1, /* point clipping select bounding box*/
													0, /* line width vertex select first vertex*/
													0, /* backf cull cw off*/
													0); /* backf cull ccw off*/

				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_PARAMS));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				payload = GP_PLBU_CONF_REGS_TILE_SIZE(0, 0);
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_TILE_SIZE));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				/* use this when point size override is turned on, 32 bit */
				MALI_SET_AS_FLOAT(payload, 1.f);
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(payload, GP_PLBU_CONF_REG_POINT_SIZE));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				MALI_SET_AS_FLOAT(near, 0.f);
				MALI_SET_AS_FLOAT(far, 1.f);
				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(near, GP_PLBU_CONF_REG_Z_NEAR));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_WRITE_CONF_REG(far, GP_PLBU_CONF_REG_Z_FAR));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_PREPARE_FRAME(TILES));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				for (i=0; (i<polygon_ctr)&&(MALI_ERR_NO_ERROR == err); i++ )
				{
					err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_PROCESS_SHADED_VERTICES(
							0,
							3,
							GP_PLBU_PRIM_TYPE_INDIVIDUAL_TRIANGLES,
							GP_PLBU_OPMODE_DRAW_ELEMENTS));
					if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				}

				err =  _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_FLUSH_POINTER_CACHE());
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				err =  _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (0*4)), GP_PLBU_CONF_REG_HEAP_START_ADDR ));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				err =  _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (1*4)), GP_PLBU_CONF_REG_HEAP_END_ADDR ));
				if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;

				if (0 != isLast)
				{
					err = _mali_gp_job_add_plbu_cmd( jobh, GP_PLBU_COMMAND_END_FRAME());
					if(MALI_ERR_NO_ERROR != err) addfail = MALI_TRUE;
				}

				assert_fail((addfail == MALI_FALSE), "failed to add plbu cmd");
				if(addfail)
				{
					_mali_gp_job_free(jobh);
					_mali_sync_handle_flush(sync);
					_mali_wait_on_handle(wait_handle);
					return MALI_ERR_OUT_OF_MEMORY;
				}

				_mali_gp_job_add_to_sync_handle( sync, jobh);
				_mali_gp_job_set_callback( jobh, callback_bgfplbu08, NULL);

				jobs_bgfplbu08_failed = 0;
				err = _mali_gp_job_start( jobh, MALI_JOB_PRI_HIGH);
				assert_fail((MALI_ERR_NO_ERROR == err), "gp_job_start failed");
				if(err == MALI_ERR_NO_ERROR)
				{
					global_started_bgfplbu08++;

					_mali_sync_handle_flush( sync);
					_mali_wait_on_handle( wait_handle);

					MALI_DEBUG_PRINT(4, ("Size of heap after run:%d\n",_mali_mem_size_get(heap_handle)));
					assert_ints_equal(jobs_bgfplbu08_failed, 0, "At least one of the jobs failed\n")
				}
				else
				{
					_mali_gp_job_free(jobh);
					_mali_sync_handle_flush( sync);
					_mali_wait_on_handle( wait_handle);
				}

			}/* failed to create job */
			else
			{
				_mali_sync_handle_flush(sync);
				_mali_wait_on_handle(wait_handle);
				err = MALI_ERR_OUT_OF_MEMORY;
			}
		}/* failed to create wait_handle */
		else
		{
			_mali_sync_handle_flush(sync);
			err = MALI_ERR_OUT_OF_MEMORY;
		}
	}/* failed to create sync_handle */
	else
	{
		err = MALI_ERR_OUT_OF_MEMORY;
	}
	return err;
}

