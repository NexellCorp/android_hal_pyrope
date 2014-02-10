/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_counters.h"
#include "mali_instrumented_context.h"
#include "base/instrumented/mali_gp_instrumented_internal.h"
#include "util/mali_names.h"

#define MALI_INSTRUMENTED_CHECK_ALL_GP_JOBS_ARE_INSTRUMENTED 0

MALI_EXPORT
mali_err_code _mali_gp_instrumented_init(mali_instrumented_context *ctx)
{
	_instrumented_start_group(ctx,
		MALI_NAME_GP,
		"Hardware performance counters and derived counters for the " MALI_NAME_GP,
		MALI_CORE_FREQ_KHZ/1000);

	_instrumented_start_group(ctx,
		"Performance",
		"Derived performance counters, computed from the " MALI_NAME_GP " hardware performance counters",
		0);
	/**
	 * Frame time, frame rate,
	 * vertex shader -, plbu time,
	 * Vertices rate,
	 * Backface cullings
	 */

#if defined(USING_MALI200)
#define FRAME_TIME_TEXT \
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "\
			"[Active cycles] / [Frequency]"\
			"<br/> <br/> If the counter [Active clock cycles count] is not activated then it is calculated using this formula:<br/> "\
			"[Total cycles] / [Frequency]"
#else
#define FRAME_TIME_TEXT \
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "\
			"[Active cycles] / [Frequency]"
#endif

    MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_FRAME_TIME,
		"Frame time",
		"Time spent by the " MALI_NAME_GP " on this frame"
        FRAME_TIME_TEXT,
		"us",			/* transferred: microseconds per frame */
		MALI_SCALE_INVERSE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_FPS,
		"Frame rate",
		"The number of frames produced per second of execution time by the " MALI_NAME_GP " (FPS)"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"1 / [Frame time]",
		"f/s/1000",		/* transferred: frames per 1000 seconds */
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_VERTEX_SHADER_TIME,
		"Vertex shader time",
		"Time spent by the " MALI_NAME_GP " each frame doing vertex shading"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"[Active cycles, vertex shader command processor] / [Frequency]",
		"us",			/* transferred: microseconds per frame */
		MALI_SCALE_INVERSE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_PLBU_TIME,
		"PLBU time",
		"Time spent by the " MALI_NAME_GP " Polygon List Build Unit (PLBU) each frame doing "
			"primitive assembly and writing out the primitive lists"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"[Active cycles, PLBU command processor] / [Frequency]",
		"us",			/* transferred: microseconds per frame */
		MALI_SCALE_INVERSE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_VERTICES_RATE,
		"Vertex rate",
		"Number of vertices per second (includes duplicates)"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"[Vertices processed] / [Vertex shader time]",
		"vert/s",		/* transferred: vertices per second */
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_PRIMITIVE_RATE,
		"Primitive rate",
		"Number of primitives per second"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"[Primitives fetched] / [PLBU time]",
		"prim/s",
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_BACKFACE_CULLINGS_PER_PRIMITIVES_FETCHED,
		"Cullings per primitive",
		"Backface and frustum cullings per fetched primitive"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"[Primitives culled] / [Primitives fetched]",
		"cull/prim/1000",	/* transferred: cullings per 1000 primitives */
		MALI_SCALE_NONE));
	_instrumented_end_group(ctx);  /* end of Mali GP Performance */


	_instrumented_start_group(ctx,
		"Bandwidth",
		"Derived bandwidth counters, computed from the " MALI_NAME_GP " hardware counters and the Frame time derived counter",
		0);
	/**
	 * Read -, Write -, Total BW,
	 * Average burst length, - read, - write,
	 */
	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_READ_BW,
		"Read bandwidth",
		"Bytes read from the main bus per second"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using "
			"the following formula (There are eight bytes per word):<br/> "
			"[Words read, system bus] * 8 / [Frame time]",
		"B/s",			/* transferred: Bytes per second */
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_WRITE_BW,
		"Write bandwidth",
		"Bytes written to the main bus per second"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using "
			"the following formula (There are eight bytes per word):<br/> "
			"[Words written, system bus] * 8 / [Frame time]",
		"B/s",			/* transferred: Bytes per second */
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_TOTAL_BW,
		"Total bandwidth",
		"Total number of bytes read from or written to the main bus"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using "
			"the following formula (There are eight bytes per word):<br/> "
			"([Words read, system bus] + [Words written, system bus]) * 8 / [Frame time]",
		"B/s",			/* transferred: Bytes per second */
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_AVERAGE_BURST_LENGTH,
		"Burst length",
		"Average number of 64 bit words read or written in each burst"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"([Words read, system bus] + [Words written, system bus]) / "
			"([Read bursts, system bus] + [Write bursts, system bus])",
		"W/1000",			/* transferred: Words (64 bit) per burst */
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_AVERAGE_BURST_LENGTH_READ,
		"Read burst length",
		"Average number of 64 bit words read in each burst"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"[Words read, system bus] / [Read bursts, system bus]",
		"W/1000",			/* transferred: Words (64 bit) per burst */
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_AVERAGE_BURST_LENGTH_WRITE,
		"Write burst length",
		"Average number of 64 bit words written in each burst"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_GP " counters using the following formula:<br/> "
			"[Words written, system bus] / [Write bursts, system bus]",
		"W/1000",			/* transferred: Words (64 bit) per burst */
		MALI_SCALE_NONE));
	_instrumented_end_group(ctx);  /* end of Mali GP Main memory access */


	_instrumented_start_group(ctx,
		"HW Counters",
		"Hardware performance counters for " MALI_NAME_GP,
		0);
	/**
	 * The hardware counters
	 */

	_instrumented_register_counter_gp_begin(ctx);

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_MGP2,
		"Active cycles",
		"Number of cycles per frame the " MALI_NAME_GP " was active",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_VERTEX_SHADER,
		"Active cycles, vertex shader",
		"Number of cycles per frame the " MALI_NAME_GP " vertex shader unit was active",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_VERTEX_STORER,
		"Active cycles, vertex storer",
		"Number of cycles per frame the " MALI_NAME_GP " vertex storer unit was active",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_VERTEX_LOADER,
		"Active cycles, vertex loader",
		"Number of cycles per frame the " MALI_NAME_GP " vertex loader unit was active",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_CYCLES_VERTEX_LOADER_WAITING_FOR_VERTEX_SHADER,
		"Cycles vertex loader waiting for vertex shader",
		"Number of cycles per frame the vertex loader was idle while waiting on the "
			"vertex shader",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_WORDS_READ,
		"Words read, system bus",
		"Total number of 64 bit words read by the " MALI_NAME_GP " from the system bus per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_WORDS_WRITTEN,
		"Words written, system bus",
		"Total number of 64 bit words written by the " MALI_NAME_GP " to the system bus per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_READ_BURSTS,
		"Read bursts, system bus",
		"Number of read bursts by the " MALI_NAME_GP " from the system bus per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_WRITE_BURSTS,
		"Write bursts, system bus",
		"Number of write bursts from the " MALI_NAME_GP " to the system bus per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_VERTICES_PROCESSED,
		"Vertices processed",
		"Number of vertices processed by the " MALI_NAME_GP " per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_VERTICES_FETCHED,
		"Vertices fetched",
		"Number of vertices fetched by the " MALI_NAME_GP " per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED,
		"Primitives fetched",
		"Number of graphics primitives fetched by the " MALI_NAME_GP " per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_BACKFACE_CULLINGS_DONE,
		"Primitives culled",
		"Number of graphics primitives discarded (culled) per frame, because they "
			"were seen from the back side or because they were out of view "
			"(out-of-frustum cullings)",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_COMMANDS_WRITTEN_TO_TILES,
		"Commands written to tiles",
		"Number of commands (8 Bytes, mainly primitives) written by " MALI_NAME_GP " "
			"to the " MALI_NAME_PP " input data structure per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_MEMORY_BLOCKS_ALLOCATED,
		"Memory blocks allocated",
		"Number of overflow data blocks needed for outputting the " MALI_NAME_PP " input "
			"data structure per frame ",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_NUMBER_OF_VERTEX_LOADER_CACHE_MISSES,
		"Vertex loader cache misses",
		"Number of cache misses for the vertex shader's vertex input unit per frame",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_VERTEX_SHADER_COMMAND_PROCESSOR,
		"Active cycles, vertex shader command processor",
		"Number of cycles per frame the " MALI_NAME_GP " vertex shader command processor was "
			"active. This includes time waiting for semaphores",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_PLBU_COMMAND_PROCESSOR,
		"Active cycles, PLBU command processor",
		"Number of cycles per frame the " MALI_NAME_GP " PLBU command processor was active. "
			"This includes time waiting for semaphores",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_PLBU_LIST_WRITER,
		"Active cycles, PLBU list writer",
		"Number of cycles per frame the " MALI_NAME_GP " PLBU output unit was active (Writing "
			"the " MALI_NAME_PP " input data structure). This includes time spent waiting on the bus",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_THROUGH_THE_PREPARE_LIST_COMMANDS,
		"Active cycles, PLBU geometry processing",
		"Number of cycles per frame the " MALI_NAME_GP " PLBU was active, excepting final data "
			"output. In other words: active cycles through the prepare list commands. "
			"This includes time spent waiting on the bus",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_PRIMITIVE_ASSEMBLY,
		"Active cycles, PLBU primitive assembly",
		"Number of active cycles per frame spent by the " MALI_NAME_GP " PLBU doing primitive "
			"assembly. This does not include scissoring or final output. This includes "
			"time spent waiting on the bus",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_PLBU_VERTEX_FETCHER,
		"Active cycles, PLBU vertex fetcher",
		"Number of active cycles per frame spent by the " MALI_NAME_GP " PLBU fetching vertex data. "
			"This includes time spent waiting on the bus",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_BOUNDINGBOX_AND_COMMAND_GENERATOR,
		"Active cycles, Bounding-box and command generator",
		"Number of active cycles per frame spent by the " MALI_NAME_GP " PLBU setting up bounding "
			"boxes and commands (mainly graphics primitives). This includes time spent "
			"waiting on the bus",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_SCISSOR_TILE_ITERATOR,
		"Active cycles, Scissor tile iterator",
		"Number of active cycles per frame spent by the " MALI_NAME_GP " PLBU iterating over "
			"tiles to perform scissoringi. This includes time spent waiting on the bus",
		"num",
		MALI_SCALE_NONE));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_ACTIVE_CYCLES_PLBU_TILE_ITERATOR,
		"Active cycles, PLBU tile iterator",
		"Number of active cycles per frame spent by the " MALI_NAME_GP " PLBU iterating over "
			"the tiles in the bounding box generating commands (mainly graphics "
			"primitives). This includes time spent waiting on the bus",
		"num",
		MALI_SCALE_NONE));

    MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_MGP2_JOB_COUNT,
		MALI_NAME_GP " job count",
		"Number of " MALI_NAME_GP " jobs for this frame",
		"num",
		MALI_SCALE_NONE));

	_instrumented_register_counter_gp_end(ctx);

	_instrumented_end_group(ctx);  /* end of Mali GP Counters */
	_instrumented_end_group(ctx);  /* end of Mali GP */
	MALI_SUCCESS;
}  /* end of _mali_gp_instrumented_init */


MALI_EXPORT
mali_err_code
_mali_gp_derive_counts(mali_instrumented_context* ctx, mali_instrumented_frame *frame)
{
	mali_bool find_frame_time, find_frame_rate,
		find_read_bw, find_write_bw,
		find_total_bw;


	if (!_instrumented_is_sampling_enabled(ctx))
	{
		MALI_SUCCESS;
	}

	find_frame_time = _instrumented_is_counter_active(ctx, CID_MGP2_FRAME_TIME);
	find_frame_rate = _instrumented_is_counter_active(ctx, CID_MGP2_FPS);
	find_read_bw = _instrumented_is_counter_active(ctx, CID_MGP2_READ_BW);
	find_write_bw = _instrumented_is_counter_active(ctx, CID_MGP2_WRITE_BW);
	find_total_bw = _instrumented_is_counter_active(ctx, CID_MGP2_TOTAL_BW);
	if (find_frame_time || find_frame_rate
	|| find_read_bw || find_write_bw || find_total_bw)
	{				/* if time needed - */

		s64 tus = 0; 		/* frame time, in microseconds, needed by all in scope */

		/* Active cycles should be preferred over total cycles */
		if (_instrumented_is_counter_active(ctx, CID_MGP2_ACTIVE_CYCLES_MGP2))
		{
			tus = _instrumented_get_counter( CID_MGP2_ACTIVE_CYCLES_MGP2, frame)
				/(MALI_CORE_FREQ_KHZ/1000);
		}

		if (0==tus) 	MALI_ERROR(MALI_ERR_FUNCTION_FAILED);  /* comprises a return stmt */

		if (find_frame_time)
		{ 				/* records  us/frame */
			_instrumented_set_counter( CID_MGP2_FRAME_TIME, frame, tus);
		}
		if (find_frame_rate)
		{ 				/* records  frames/s/1000 */
			_instrumented_set_counter( CID_MGP2_FPS, frame, 1000000000/tus);
		}
		if (find_read_bw)
		{ 				/* records  Bytes/s */
			if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WORDS_READ))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			_instrumented_set_counter( CID_MGP2_READ_BW, frame,
				_instrumented_get_counter( CID_MGP2_NUMBER_OF_WORDS_READ,
					frame)*8000000/tus);
		}
		if (find_write_bw)
		{ 				/* records  Bytes/s */
			if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WORDS_WRITTEN))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			_instrumented_set_counter( CID_MGP2_WRITE_BW, frame,
				_instrumented_get_counter( CID_MGP2_NUMBER_OF_WORDS_WRITTEN,
					frame)*8000000/tus);
		}
		if (find_total_bw)
		{ 				/* records Bytes/s */
			s64 tot;
			if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WORDS_READ)
			|| !_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WORDS_WRITTEN))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			tot = _instrumented_get_counter( CID_MGP2_NUMBER_OF_WORDS_READ, frame)
				+ _instrumented_get_counter( CID_MGP2_NUMBER_OF_WORDS_WRITTEN, frame);
			_instrumented_set_counter( CID_MGP2_TOTAL_BW, frame, tot*8000000/tus);
		}
	}  /* end of time needed */

	if (_instrumented_is_counter_active(ctx, CID_MGP2_VERTEX_SHADER_TIME))
	{ 				/* records  us */
		if (!_instrumented_is_counter_active(ctx,
			CID_MGP2_ACTIVE_CYCLES_VERTEX_SHADER_COMMAND_PROCESSOR))
		{
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}

		_instrumented_set_counter( CID_MGP2_VERTEX_SHADER_TIME, frame,
			_instrumented_get_counter(
				CID_MGP2_ACTIVE_CYCLES_VERTEX_SHADER_COMMAND_PROCESSOR, frame)
			/(MALI_CORE_FREQ_KHZ/1000));
	}
	if (_instrumented_is_counter_active(ctx, CID_MGP2_VERTICES_RATE))
	{ 				/* records  vertices/s */
		s64 shader_time = 0;
		if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_VERTICES_PROCESSED)
		|| !_instrumented_is_counter_active(ctx, CID_MGP2_VERTEX_SHADER_TIME))
		{
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		shader_time = _instrumented_get_counter(CID_MGP2_VERTEX_SHADER_TIME, frame);
		if (shader_time != 0)
		{
			_instrumented_set_counter( CID_MGP2_VERTICES_RATE, frame,
				_instrumented_get_counter( CID_MGP2_NUMBER_OF_VERTICES_PROCESSED, frame) *
				1000000 / shader_time );
		}
		else
		{
			_instrumented_set_counter( CID_MGP2_VERTICES_RATE, frame, 0);
		}
	}
	if (_instrumented_is_counter_active(ctx, CID_MGP2_PLBU_TIME))
	{ 				/* records  us */
		if (!_instrumented_is_counter_active(ctx, CID_MGP2_ACTIVE_CYCLES_PLBU_COMMAND_PROCESSOR))
		{
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		_instrumented_set_counter( CID_MGP2_PLBU_TIME, frame,
			_instrumented_get_counter(
				CID_MGP2_ACTIVE_CYCLES_PLBU_COMMAND_PROCESSOR, frame)
			/(MALI_CORE_FREQ_KHZ/1000));
	}
	if (_instrumented_is_counter_active(ctx, CID_MGP2_PRIMITIVE_RATE))
	{
		s64 plbu_time = 0;
		if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED)
		|| !_instrumented_is_counter_active(ctx, CID_MGP2_PLBU_TIME))
		{
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		plbu_time = _instrumented_get_counter(CID_MGP2_PLBU_TIME, frame);
		if (plbu_time != 0)
		{
			_instrumented_set_counter( CID_MGP2_PRIMITIVE_RATE, frame,
				_instrumented_get_counter( CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED, frame) *
				1000000 / plbu_time );
		}
		else
		{
			_instrumented_set_counter( CID_MGP2_PRIMITIVE_RATE, frame, 0);
		}
	}
	if (_instrumented_is_counter_active(ctx, CID_MGP2_BACKFACE_CULLINGS_PER_PRIMITIVES_FETCHED))
	{ 				/* records culls/(1000*prims) */
		s64 pri;
		if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_BACKFACE_CULLINGS_DONE)
		|| !_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED))
		{
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		pri = _instrumented_get_counter( CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED, frame);
		_instrumented_set_counter( CID_MGP2_BACKFACE_CULLINGS_PER_PRIMITIVES_FETCHED, frame,
			(pri==0)? 0 :
			_instrumented_get_counter( CID_MGP2_NUMBER_OF_BACKFACE_CULLINGS_DONE,
				frame)*1000/pri);
	}
	if (_instrumented_is_counter_active(ctx, CID_MGP2_AVERAGE_BURST_LENGTH))
	{ 				/* records  tot words IO / tot bursts / 1000.*/
		s64 wio, burs;
		if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WORDS_READ)
		|| !_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WORDS_WRITTEN)
		|| !_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_READ_BURSTS)
		|| !_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WRITE_BURSTS))
		{
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		wio = 1000 * (_instrumented_get_counter( CID_MGP2_NUMBER_OF_WORDS_READ, frame)
			+ _instrumented_get_counter( CID_MGP2_NUMBER_OF_WORDS_WRITTEN, frame));
		burs = _instrumented_get_counter( CID_MGP2_NUMBER_OF_READ_BURSTS, frame)
			+ _instrumented_get_counter( CID_MGP2_NUMBER_OF_WRITE_BURSTS, frame);
		_instrumented_set_counter( CID_MGP2_AVERAGE_BURST_LENGTH, frame,
			(burs==0)? 0 : wio/burs);
	}
	if (_instrumented_is_counter_active(ctx, CID_MGP2_AVERAGE_BURST_LENGTH_READ))
	{ 				/* records  words read / read bursts / 1000 */
		s64 burs;
		if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WORDS_READ)
		|| !_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_READ_BURSTS))
		{
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		burs = _instrumented_get_counter( CID_MGP2_NUMBER_OF_READ_BURSTS, frame);
		_instrumented_set_counter( CID_MGP2_AVERAGE_BURST_LENGTH_READ, frame,
			(burs==0)? 0 :
			(1000*_instrumented_get_counter( CID_MGP2_NUMBER_OF_WORDS_READ, frame))/burs);
	}
	if (_instrumented_is_counter_active(ctx, CID_MGP2_AVERAGE_BURST_LENGTH_WRITE))
	{ 				/* records  words written / write bursts / 1000 */
		s64 burs;
		if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WORDS_WRITTEN)
		|| !_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_WRITE_BURSTS))
		{
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		burs = _instrumented_get_counter( CID_MGP2_NUMBER_OF_WRITE_BURSTS, frame);
		_instrumented_set_counter( CID_MGP2_AVERAGE_BURST_LENGTH_WRITE, frame,
			(burs==0)? 0 :
			(1000*_instrumented_get_counter( CID_MGP2_NUMBER_OF_WORDS_WRITTEN, frame))/burs);
	}
	MALI_SUCCESS;
} /* end of _mali_gp_derive_counts */


static void activate_tus(mali_instrumented_context* ctx)
{
	_instrumented_activate_counter_derived(ctx, CID_MGP2_ACTIVE_CYCLES_MGP2);
}

MALI_EXPORT
void _mali_gp_activate_derived(mali_instrumented_context* ctx)
{
	if ( _instrumented_is_counter_active(ctx, CID_MGP2_FRAME_TIME)
		|| _instrumented_is_counter_active(ctx, CID_MGP2_FPS))
	{
		activate_tus(ctx);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_VERTICES_RATE))
	{
		_instrumented_activate_counter_derived(ctx, CID_MGP2_VERTEX_SHADER_TIME);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_VERTICES_PROCESSED);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_PRIMITIVE_RATE))
	{
		_instrumented_activate_counter_derived(ctx, CID_MGP2_PLBU_TIME);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_READ_BW))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WORDS_READ);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_WRITE_BW))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WORDS_WRITTEN);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_TOTAL_BW))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WORDS_READ);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WORDS_WRITTEN);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_VERTEX_SHADER_TIME))
	{
		_instrumented_activate_counter_derived(ctx,
			CID_MGP2_ACTIVE_CYCLES_VERTEX_SHADER_COMMAND_PROCESSOR);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_PLBU_TIME))
	{
		_instrumented_activate_counter_derived(ctx, CID_MGP2_ACTIVE_CYCLES_PLBU_COMMAND_PROCESSOR);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_BACKFACE_CULLINGS_PER_PRIMITIVES_FETCHED))
	{
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_BACKFACE_CULLINGS_DONE);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_AVERAGE_BURST_LENGTH))
	{
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WORDS_READ);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WORDS_WRITTEN);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_READ_BURSTS);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WRITE_BURSTS);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_AVERAGE_BURST_LENGTH_READ))
	{
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WORDS_READ);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_READ_BURSTS);
	}

	if (_instrumented_is_counter_active(ctx, CID_MGP2_AVERAGE_BURST_LENGTH_WRITE))
	{
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WORDS_WRITTEN);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_WRITE_BURSTS);
	}
} /* _mali_gp_activate_derived */

MALI_EXPORT
void _instrumented_gp_setup(mali_instrumented_context *context, mali_gp_job_handle job_handle)
{
	/* OPT: this result_ptr does not often change, and could be cached. (it
	 * changes when the user activates other counters) */

	u32 *active_counters = NULL;
	u32 i;
	u32 num_active_counters = 0;
	u32* result_ptr;

#if MALI_INSTRUMENTED_CHECK_ALL_GP_JOBS_ARE_INSTRUMENTED
	MALI_DEBUG_ASSERT(MALI_TRUE != _mali_instrumented_gp_is_job_instrumented(job_handle), ("Job is already setup for instrumentation"));
#endif

	active_counters = _mali_sys_malloc(sizeof(u32) * (CID_MGP2_COUNTER_ENUM_MAX - MALI_MGP2_COUNTER_OFFSET));
	if (NULL == active_counters)
	{
		return;
	}

	result_ptr = _mali_sys_calloc(CID_MGP2_COUNTER_ENUM_MAX - MALI_MGP2_COUNTER_OFFSET, sizeof(u32));
	if (NULL == result_ptr)
	{
		_mali_sys_free(active_counters);
		return;
	}

	for (i = MALI_MGP2_COUNTER_OFFSET; i < CID_MGP2_COUNTER_ENUM_MAX; ++i)
	{
		if ( MALI_TRUE == _instrumented_is_counter_active(context, i))
		{
			active_counters[num_active_counters] = i - MALI_MGP2_COUNTER_OFFSET;
			num_active_counters++;
		}
	}

	/* now tell the job which counters we want results from */
	_mali_instrumented_gp_job_set_counters(job_handle, active_counters, num_active_counters);
	_mali_instrumented_gp_set_results_pointer(job_handle, result_ptr);
}


MALI_EXPORT
void _instrumented_gp_done(mali_instrumented_frame *frame, mali_gp_job_handle job_handle)
{
	u32 i;
	u32 num_counters;
	u32 *active_counters;
	u32 *results;

	MALI_DEBUG_ASSERT_POINTER(frame);

#if MALI_INSTRUMENTED_CHECK_ALL_GP_JOBS_ARE_INSTRUMENTED
	MALI_DEBUG_ASSERT(MALI_TRUE == _mali_instrumented_gp_is_job_instrumented(job_handle), ("Job was not setup for instrumentation"));
#endif

	results = _mali_instrumented_gp_get_results_pointer(job_handle);
	_mali_instrumented_gp_job_get_counters(job_handle, &active_counters, &num_counters);
	_mali_instrumented_gp_context_init(job_handle);

	if (NULL != results && NULL != active_counters && 0 < num_counters)
	{
		for (i = 0; i < num_counters; ++i)
		{
			_instrumented_add_to_counter(active_counters[i] + MALI_MGP2_COUNTER_OFFSET, frame, results[i]);
		}
	}

	_instrumented_add_to_counter(CID_MGP2_JOB_COUNT, frame, 1);

	if (NULL != results)
	{
		_mali_sys_free(results);
	}

	if (NULL != active_counters)
	{
		_mali_sys_free(active_counters);
	}
} /* _instrumented_gp_done */

MALI_EXPORT
void _instrumented_gp_abort(mali_gp_job_handle job_handle)
{
	u32 num_counters;
	u32 *active_counters;
	u32 *results;

#if MALI_INSTRUMENTED_CHECK_ALL_GP_JOBS_ARE_INSTRUMENTED
	MALI_DEBUG_ASSERT(MALI_TRUE == _mali_instrumented_gp_is_job_instrumented(job_handle), ("Job was not setup for instrumentation"));
#endif

	results = _mali_instrumented_gp_get_results_pointer(job_handle);
	_mali_instrumented_gp_job_get_counters(job_handle, &active_counters, &num_counters);
	_mali_instrumented_gp_context_init(job_handle);

	if (NULL != results)
	{
		_mali_sys_free(results);
	}

	if (NULL != active_counters)
	{
		_mali_sys_free(active_counters);
	}
} /* _instrumented_gp_abort */

