/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
/* .../src/instrumented/hw/mali200/mali_pp_instrumented.c */

#include "regs/MALI200/mali200_core.h"
#include "mali_counters.h"
#include "mali_instrumented_context.h"
#include "base/instrumented/mali_pp_instrumented_internal.h"
#include "util/mali_names.h"

#define MALI_INSTRUMENTED_CHECK_ALL_PP_JOBS_ARE_INSTRUMENTED 0

MALI_EXPORT
mali_err_code _mali_pp_instrumented_init(mali_instrumented_context* ctx)
{
	unsigned int i;

	_instrumented_start_group(ctx,
		MALI_NAME_PP,
		"Hardware performance counters and derived counters for the " MALI_NAME_PP " core(s)",
		MALI_CORE_FREQ_KHZ/1000);

	_instrumented_start_group(ctx,
		"Performance",
		"Derived performance counters, computed from the " MALI_NAME_PP " hardware performance counters",
		0);
	/**
	 * Frame time, frame rate
	 * Instruction rate
	 * Triangle rate
	 * Fill rate
	 */

#if defined(USING_MALI200)
#define FRAME_TIME_TEXT \
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the following formula:<br/> "\
			"[Active clock cycles count] / [Frequency]"\
			"<br/> <br/> If the counter [Active clock cycles count] is not activated then it is calculated using this formula:<br/>"\
			"[Total clock cycles count] / [Frequency]"
#else
#define FRAME_TIME_TEXT \
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the following formula:<br/> "\
			"[Active clock cycles count] / [Frequency]"
#endif
	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_FRAME_TIME,
		"Frame time",
		"Time spent by " MALI_NAME_PP " rendering this frame"
        FRAME_TIME_TEXT,
		"us",			/* transferred: microseconds per frame */
		MALI_SCALE_INVERSE));

    MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_FPS,
		"Frame rate",
		"The number of frames produced per second of execution time by the " MALI_NAME_PP " (FPS)"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the following formula:<br/> "
			"1 / [Frame time]",
		"f/s/1000",		/* transferred: frames per 1000 s. */
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_INSTRUCTION_RATE,
		"Instruction rate",
		"Instructions completed per second by the " MALI_NAME_PP " core"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the following formula:<br/> "
			"[Instruction completed count] / [Frame time]",
		"I/s/0.001",		/* transferred: instructions per ms. */
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_PRIMITIVE_RATE,
		"Primitive rate",
		"Triangles and other graphics primitives handled per second by the " MALI_NAME_PP
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the following formula:<br/>"
			"([" MALI_NAME_GP "::Primitives fetched] - [" MALI_NAME_GP "::Primitives culled]) / [" MALI_NAME_PP "::Frame time]",
		"tri/s",			/* transferred: triangles per s. */
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_FILL_RATE,
		"Fill rate",
		"Fragments set up and rasterized per second by the " MALI_NAME_PP
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the following formula:<br/> "
			"[Fragment rasterized count] / [Frame time]",
		"pix/s/0.001",		/* transferred: pixels per ms. */
		MALI_SCALE_NORMAL));
	_instrumented_end_group(ctx); 	/* end of Mali PP Performance */

	_instrumented_start_group(ctx,
		"Bandwidth",
		"Derived bandwidth counters, computed from the " MALI_NAME_PP " hardware counters and the Frame time derived counter",
		0);
	/**
	 * Triangle Processing BW
	 * Texture BW
	 * Shader Input BW
	 * Internal Shader BW
	 * Output BW
	 */
	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_TRIANGLE_PROCESSING_BW,
		"Triangle Processing BW",
		"Bandwidth on system bus reading polygon list, vertex cache and render state words"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the "
			"following formula (There are eight bytes per word):<br/> "
			"([Polygon list reads] + [Vertex cache reads] + [RSW reads]) * 8 / [Frame time]",
		"B/s/0.001",		/* transferred: Bytes per ms. */
		MALI_SCALE_NORMAL));

#if defined(USING_MALI200)
#define TEXTURE_BW_TEXT \
		"Bandwidth on the system bus reading textures including palette, texture "\
			"descriptor and remapping reads"\
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the "\
			"following formula (There are eight bytes per word):<br/> "\
			"([Palette cache reads] +<br/> "\
			"[Texture cache uncompressed reads] +<br/> "\
			"[Texture descriptors reads] +<br/> "\
			"[Texture descriptor remapping reads]) * 8 / [Frame time]"
#else
#define TEXTURE_BW_TEXT \
		"Bandwidth on the system bus reading textures including texture "\
			"descriptor and remapping reads"\
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the "\
			"following formula (There are eight bytes per word):<br/> "\
			"[Texture cache uncompressed reads] +<br/> "\
			"[Texture descriptors reads] +<br/> "\
			"[Texture descriptor remapping reads]) * 8 / [Frame time]"
#endif
    MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_TEXTURE_BW,
		"Texture BW",
        TEXTURE_BW_TEXT,
		"B/s/0.001",
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_INPUT_BW,
		"Shader input BW",
		"Bandwidth on the system bus reading varyings and program and remapping uniforms "
			"for the fragment shader processor. Note that this counter does not include "
			"input bandwidth due to uniform reads as this is included in the [Internal shader BW] counter"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the "
			"following formula (There are eight bytes per word):<br/> "
			"([Uniform remapping reads] + [Program cache reads] + [Varying reads]) *8 / [Frame time]",
		"B/s/0.001",
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_INTERNAL_SHADER_BW,
		"Internal shader BW",
		"Bandwidth on system bus for load and store instructions (accessing "
			"stack of local variables) on the fragment shader processor. Note that this "
			"counter also includes bandwidth due to uniform reads"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the "
			"following formula (There are eight bytes per word):<br/> "
			"([Store unit writes] + [Load unit reads]) * 8 / [Frame time]",
		"B/s/0.001",
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_OUTPUT_BW,
		"Output BW",
		"Bandwidth on system bus writing finished tiles of pixels to the frame buffer"
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the "
			"following formula (There are eight bytes per word):<br/> "
			"[Tile writeback writes] * 8 / [Frame time]",
		"B/s/0.001",
		MALI_SCALE_NORMAL));

	MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
		CID_M200_TOTAL_BW,
		"Total BW",
		"Total bandwidth used by " MALI_NAME_PP
			"<br/> <br/> This derived counter is calculated from other " MALI_NAME_PP " counters using the "
			"following formula (There are eight bytes per word):<br/> "
			"([Total bus reads] + [Total bus writes]) * 8 / [Frame time]",
		"B/s/0.001",
		MALI_SCALE_NORMAL));
	_instrumented_end_group(ctx);  /* end of Mali PP Bandwidth */


	_instrumented_start_group(ctx,
		"HW Counters",
		"Hardware performance counters for " MALI_NAME_PP,
		0);

	for (i = 0; i < MALI_MAX_PP_SPLIT_COUNT; i++)
	{
		u32 offset = i*M200_MULTI_CORE_OFFSET;

#define CORE_STRING_SIZE 10
		char core_string[CORE_STRING_SIZE];
		_mali_sys_snprintf(core_string, CORE_STRING_SIZE, "Core %u", i);
#undef CORE_STRING_SIZE
		_instrumented_start_group(ctx, core_string, "", 0);

		_instrumented_register_counter_pp_begin(ctx, i);

		/**
		 * The hardware counters
		 */
		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_ACTIVE_CLOCK_CYCLES_COUNT + offset,
			"Active clock cycles count",
			"Active clock cycles, between Polygon start and IRQ.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TOTAL_BUS_READS + offset,
			"Total bus reads",
			"Total number of 64-bit words read from the bus.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TOTAL_BUS_WRITES + offset,
			"Total bus writes",
			"Total number of 64-bit words written to the bus.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_BUS_READ_REQUEST_CYCLES_COUNT + offset,
			"Bus read request cycles count",
			"Number of cycles of bus read request with signal HIGH.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_BUS_WRITE_REQUEST_CYCLES_COUNT + offset,
			"Bus write request cycles count",
			"Number of cycles of bus write request with signal HIGH.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_BUS_READ_TRANSACTIONS_COUNT + offset,
			"Bus read transactions count",
			"Number of read requests accepted by the bus.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_BUS_WRITE_TRANSACTIONS_COUNT + offset,
			"Bus write transactions count",
			"Number of write requests accepted by the bus.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TILE_WRITEBACK_WRITES + offset,
			"Tile writeback writes",
			"64-bit words written to the bus by the writeback unit.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_STORE_UNIT_WRITES + offset,
			"Store unit writes",
			"64-bit words written to the bus by the store unit.",
			"num",
			MALI_SCALE_NONE));

		/* CID_M200_RESERVED_11 */

#if defined(USING_MALI200)
		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PALETTE_CACHE_READS + offset,
			"Palette cache reads",
			"Number of 64-bit words read from the bus into the"
				" compressed-texture palette cache",
			"num",
			MALI_SCALE_NONE));
#endif

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TEXTURE_CACHE_UNCOMPRESSED_READS + offset,
			"Texture cache uncompressed reads",
			"Number of 64-bit words read from the bus into the"
				" uncompressed textures cache",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_POLYGON_LIST_READS + offset,
			"Polygon list reads",
			"Number of 64-bit words read from the bus by the polygon list reader",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_RSW_READS + offset,
			"RSW reads",
			"Number of 64-bit words read from the bus into the Render State Word register",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_VERTEX_CACHE_READS + offset,
			"Vertex cache reads",
			"Number of 64-bit words read from the bus into the vertex cache",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_UNIFORM_REMAPPING_READS + offset,
			"Uniform remapping reads",
			"Number of 64-bit words read from the bus when reading from the uniform remapping table",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PROGRAM_CACHE_READS + offset,
			"Program cache reads",
			"Number of 64-bit words read from the bus into the fragment"
				" shader program cache",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_VARYING_READS + offset,
			"Varying reads",
			"Number of 64-bit words containing varyings generated by the vertex processing read from the bus",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TEXTURE_DESCRIPTORS_READS + offset,
			"Texture descriptors reads",
			"Number of 64-bit words containing texture descriptors read from the bus.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TEXTURE_DESCRIPTORS_REMAPPING_READS + offset,
			"Texture descriptor remapping reads",
			"Number of 64-bit words read from the bus when reading from the texture descriptor remapping table",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_LOAD_UNIT_READS + offset,
			"Load unit reads",
			"Number of 64-bit words read from the bus by the LOAD sub-instruction",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_POLYGON_COUNT + offset,
			"Polygon count",
			"Number of triangles read from the polygon list",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PIXEL_RECTANGLE_COUNT + offset,
			"Pixel rect count",
			"Number of pixel rectangles read from the polygon list.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_LINES_COUNT + offset,
			"Lines count",
			"Number of lines read from the polygon list.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_POINTS_COUNT + offset,
			"Points count",
			"Number of points read from the polygon list.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_STALL_CYCLES_POLYGON_LIST_READER + offset,
			"Stall cycles PolygonListReader",
			"Number of clock cycles the Polygon List Reader waited for output being collected",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_STALL_CYCLES_TRIANGLE_SETUP + offset,
			"Stall cycles triangle setup",
			"Number of clock cycles the TSC waited for input",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_QUAD_RASTERIZED_COUNT + offset,
			"Quad rasterized count",
			"Number of 22 quads output from rasterizer.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_FRAGMENT_RASTERIZED_COUNT + offset,
			"Fragment rasterized count",
			"Number of fragment rasterized. Fragments/(Quads*4) gives average actual fragments per quad.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_FRAGMENT_REJECTED_FRAGMENT_KILL_COUNT + offset,
			"Fragment rejected fragment-kill count",
			"Number of fragments exiting the fragment shader as killed.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_FRAGMENT_REJECTED_FWD_FRAGMENT_KILL_COUNT + offset,
			"Fragment rejected fwd-fragment-kill count",
			"Number of fragments killed by forward fragment kill.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_FRAGMENT_PASSED_ZSTENCIL_COUNT + offset,
			"Fragment passed z/stencil count",
			"Number of fragments passing Z and stencil test.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PATCHES_REJECTED_EARLY_Z_STENCIL_COUNT + offset,
			"Patches rejected early z/stencil count",
			"Number of patches rejected by EarlyZ. A patch can be 8x8, 4x4 or 2x2 pixels.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PATCHES_EVALUATED + offset,
			"Patches evaluated",
			"Number of patches evaluated for EarlyZ rejection.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_INSTRUCTION_COMPLETED_COUNT + offset,
			"Instruction completed count",
			"Number of fragment shader instruction words completed. It is a function "
				"of pixels processed and the length of the shader programs.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_INSTRUCTION_FAILED_RENDEZVOUS_COUNT + offset,
			"Instruction failed rendezvous count",
			"Number of fragment shader instructions not completed because of "
				"failed Rendezvous.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_INSTRUCTION_FAILED_VARYING_MISS_COUNT + offset,
			"Instruction failed varying-miss count",
			"Number of fragment shader instructions not completed because of "
				"failed varying operation.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_INSTRUCTION_FAILED_TEXTURE_MISS_COUNT + offset,
			"Instruction failed texture-miss count",
			"Number of fragment shader instructions not completed because of "
				"failed texture operation.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_INSTRUCTION_FAILED_LOAD_MISS_COUNT + offset,
			"Instruction failed load-miss count",
			"Number of fragment shader instructions not completed because of "
				"failed load operation.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_INSTRUCTION_FAILED_TILE_READ_MISS_COUNT + offset,
			"Instruction failed tile read-miss count",
			"Number of fragment shader instructions not completed because of "
				"failed read from the tilebuffer.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_INSTRUCTION_FAILED_STORE_MISS_COUNT + offset,
			"Instruction failed store-miss count",
			"Number of fragment shader instructions not completed because of "
				"failed store operation.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_RENDEZVOUS_BREAKAGE_COUNT + offset,
			"Rendezvous breakage count",
			"Number of Rendezvous breakages reported.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PIPELINE_BUBBLES_CYCLE_COUNT + offset,
			"Pipeline bubbles cycle count",
			"Number of unused cycles in the fragment shader while rendering is active.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TEXTURE_MAPPER_MULTIPASS_COUNT + offset,
			"Texture mapper multipass count",
			"Number of texture operations looped because of more texture passes needed.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TEXTURE_MAPPER_CYCLE_COUNT + offset,
			"Texture mapper cycle count",
			"Number of texture operation cycles.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_VERTEX_CACHE_HIT_COUNT + offset,
			"Vertex cache hit count",
			"Number of times a requested vertex was found in the cache (Number of vertex cache hits)",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_VERTEX_CACHE_MISS_COUNT + offset,
			"Vertex cache miss count",
			"Number of times a requested vertex was not found in the cache (Number of vertex cache misses)",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_VARYING_CACHE_HIT_COUNT + offset,
			"Varying cache hit count",
			"Number of times a requested varying was found in the cache (Number of varying cache hits)",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_VARYING_CACHE_MISS_COUNT + offset,
			"Varying cache miss count",
			"Number of times a requested varying was not found in the cache (Number of varying cache misses)",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_VARYING_CACHE_CONFLICT_MISS_COUNT + offset,
			"Varying cache conflict miss count",
			"Number of times a requested varying was not in the cache and its value, retrieved "
				"from memory, must overwrite an older cache entry. This happens when an "
				"access pattern cannot be serviced by the cache",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TEXTURE_CACHE_HIT_COUNT + offset,
			"Texture cache hit count",
			"Number of times a requested texel was found in the texture cache (Number of texture cache hits)",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TEXTURE_CACHE_MISS_COUNT + offset,
			"Texture cache miss count",
			"Number of times a requested texel was not found in the texture cache (Number of texture cache misses)",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_TEXTURE_CACHE_CONFLICT_MISS_COUNT + offset,
			"Texture cache conflict miss count",
			"Number of times a requested texel was not in the cache and its value, retrieved from "
				"memory, must overwrite an older cache entry. This happens when an access "
				"pattern cannot be serviced by the cache",
			"num",
			MALI_SCALE_NONE));

#if defined(USING_MALI200)
		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PALETTE_CACHE_HIT_COUNT + offset,
			"Palette cache hit count",
			"Number of times a requested palette was found in the palette cache. "
				"(Number of palette cache hits)",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PALETTE_CACHE_MISS_COUNT + offset,
			"Palette cache miss count",
			"Number of times a requested palette was not found in the palette cache. "
				"(Number of palette cache misses)",
			"num",
			MALI_SCALE_NONE));
#else
		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M400_COMPRESSED_TEXTURE_CACHE_HIT_COUNT + offset,
			"Compressed texture cache hit count",
			"Number of times a requested texel for a compressed texture was found in the texture cache. "
				"(Number of compressed texture cache hits)",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M400_COMPRESSED_TEXTURE_CACHE_MISS_COUNT + offset,
			"Compressed texture cache miss count",
			"Number of times a requested texel for a compressed texture was not found in the texture cache. "
				"(Number of compressed texture cache misses)",
			"num",
			MALI_SCALE_NONE));
#endif

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_LOAD_STORE_CACHE_HIT_COUNT + offset,
			"Load/Store cache hit count",
			"Number of hits in the load/store cache.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_LOAD_STORE_CACHE_MISS_COUNT + offset,
			"Load/Store cache miss count",
			"Number of misses in the load/store cache.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PROGRAM_CACHE_HIT_COUNT + offset,
			"Program cache hit count",
			"Number of hits in the program cache.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_PROGRAM_CACHE_MISS_COUNT + offset,
			"Program cache miss count",
			"Number of misses in the program cache.",
			"num",
			MALI_SCALE_NONE));

		MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx,
			CID_M200_JOB_COUNT_CORE_0 + i,
			MALI_NAME_PP " job count",
			"Number of " MALI_NAME_PP " jobs on this core for this frame",
			"num",
			MALI_SCALE_NONE));

		_instrumented_register_counter_pp_end(ctx, i);

		_instrumented_end_group(ctx);
	}

	_instrumented_end_group(ctx);  /* end of Mali PP Counters */
	_instrumented_end_group(ctx);  /* end of Mali PP */
	MALI_SUCCESS;
}  /* _mali_pp_instrumented_init */

/** Check whether a counter is activated for any of the cores in a multicore Mali PP setup.
 * \param ctx the instrumented context
 * \param counter_id id of the counter without M200_MULTI_CORE_OFFSET added
 * \return TRUE if the counter is active for any of the cores
 */
MALI_STATIC_INLINE mali_bool is_m200_counter_active(mali_instrumented_context *ctx, u32 counter_id)
{
	int i;

	for (i = 0; i < MALI_MAX_PP_SPLIT_COUNT; i++)
	{
		if (_instrumented_is_counter_active(ctx, counter_id + (M200_MULTI_CORE_OFFSET * i)))
		{
			return MALI_TRUE;
		}
	}
	return MALI_FALSE;
}

MALI_EXPORT
void _instrumented_pp_setup(mali_instrumented_context *context, mali_pp_job_handle job_handle)
{

	/* OPT: this result_ptr does not often change, and could be cached. (it
	 * changes when the user activates other counters) */

	u32 i;
	u32 *active_counters;
	u32 num_active_counters = 0, last_pointer_index = 0;
	u32 *result_ptr[_MALI_PP_MAX_SUB_JOBS];

#if MALI_INSTRUMENTED_CHECK_ALL_PP_JOBS_ARE_INSTRUMENTED
	MALI_DEBUG_ASSERT(MALI_TRUE != _mali_instrumented_pp_is_job_instrumented(job_handle), ("Job is already setup for instrumentation"));
#endif

	/* first find the number of active counters */
	for (i = MALI_M200_COUNTER_OFFSET; i < CID_M200_ENUM_MAX; ++i)
	{
		if (MALI_TRUE == is_m200_counter_active(context, i))
		{
			num_active_counters++;
		}
	}

	/* allocate space for the actual counter ids and their result values */

	active_counters = _mali_sys_malloc(sizeof(u32) * num_active_counters);
	if (NULL == active_counters)
	{
		return;
	}

	for (i = 0; i < _MALI_PP_MAX_SUB_JOBS; i++)
	{
		result_ptr[i] = _mali_sys_calloc(num_active_counters, sizeof(u32));
		if (NULL == result_ptr[i])
		{
			u32 j;
			for (j = 0; j < i; j++)
			{
				_mali_sys_free(result_ptr[j]);
			}
			_mali_sys_free(active_counters);
			return;
		}
	}

	for (i = MALI_M200_COUNTER_OFFSET; i < CID_M200_ENUM_MAX; ++i)
	{
		if (MALI_TRUE == is_m200_counter_active(context, i))
		{
			active_counters[last_pointer_index] = i - MALI_M200_COUNTER_OFFSET;
			++last_pointer_index;
		}
	}

	/* now tell the job which counters we want results from */
	_mali_instrumented_pp_job_setup(job_handle, active_counters, num_active_counters, result_ptr);

	return;
}

MALI_EXPORT
void _instrumented_pp_done(mali_instrumented_frame *frame, mali_pp_job_handle job_handle)
{
	u32 i;
	u32 j;
	u32 num_cores;
	u32 num_counters = 0;
	u32 *active_counters = NULL;
	u32 multi_core_offset;
	u32 *results[_MALI_PP_MAX_SUB_JOBS];

	MALI_DEBUG_ASSERT_POINTER(frame);
#if MALI_INSTRUMENTED_CHECK_ALL_PP_JOBS_ARE_INSTRUMENTED
	MALI_DEBUG_ASSERT(MALI_TRUE == _mali_instrumented_pp_is_job_instrumented(job_handle), ("Job was not setup for instrumentation"));
#endif

	_mali_instrumented_pp_job_get_data(job_handle, &num_cores, &active_counters, &num_counters, results);

	for (j = 0; j < num_cores; j++)
	{
		multi_core_offset = j * M200_MULTI_CORE_OFFSET;

		if (NULL != results[j] && NULL != active_counters && 0 < num_counters)
		{
			for (i = 0; i < num_counters; ++i)
			{
				_instrumented_add_to_counter(active_counters[i] + MALI_M200_COUNTER_OFFSET + multi_core_offset, frame, results[j][i]);
			}
		}

		_instrumented_add_to_counter(CID_M200_JOB_COUNT_CORE_0 + j, frame, 1);
	}
	_mali_instrumented_pp_context_init(job_handle);

	if (NULL != active_counters)
	{
		_mali_sys_free(active_counters);
	}

	for (i = 0; i < _MALI_PP_MAX_SUB_JOBS; i++)
	{
		if (NULL != results[i])
		{
			_mali_sys_free(results[i]);
		}
	}

} /* _instrumented_pp_done */

MALI_EXPORT
void _instrumented_pp_abort(mali_pp_job_handle job_handle)
{
	u32 i;
	u32 num_cores;
	u32 num_counters = 0;
	u32 *active_counters = NULL;
	u32 *results[_MALI_PP_MAX_SUB_JOBS];

#if MALI_INSTRUMENTED_CHECK_ALL_PP_JOBS_ARE_INSTRUMENTED
	MALI_DEBUG_ASSERT(MALI_TRUE == _mali_instrumented_pp_is_job_instrumented(job_handle), ("Job was not setup for instrumentation"));
#endif

	_mali_instrumented_pp_job_get_data(job_handle, &num_cores, &active_counters, &num_counters, results);

	_mali_instrumented_pp_context_init(job_handle);

	if (NULL != active_counters)
	{
		_mali_sys_free(active_counters);
	}

	for (i = 0; i < _MALI_PP_MAX_SUB_JOBS; i++)
	{
		if (NULL != results[i])
		{
			_mali_sys_free(results[i]);
		}
	}
}

/**
 * Get the sum of a counter for all PP cores.
 */
MALI_STATIC s64 get_sum_counter(u32 counter_id, mali_instrumented_frame *frame)
{
	s64 result = 0;
	u32 i;
	for (i = 0; i < MALI_MAX_PP_SPLIT_COUNT; i++)
	{
		result += _instrumented_get_counter(counter_id + i*M200_MULTI_CORE_OFFSET, frame);
	}
	return result;
}

/**
 * Get the maximum value of a counter for all PP cores.
 */
MALI_STATIC s64 get_max_counter(u32 counter_id, mali_instrumented_frame *frame)
{
	s64 result = 0;
	u32 i;
	for (i = 0; i < MALI_MAX_PP_SPLIT_COUNT; i++)
	{
		s64 val = _instrumented_get_counter(counter_id + i*M200_MULTI_CORE_OFFSET, frame);
		if (val > result) result = val;
	}
	return result;
}

MALI_EXPORT
mali_err_code
_mali_pp_derive_counts(mali_instrumented_context* ctx, mali_instrumented_frame *frame)
{
	mali_bool find_frame_time, find_frame_rate,
		find_instruction_rate, find_primitive_rate,
		find_fill_rate,
		find_triangle_bw, find_texture_bw,
		find_input_bw, find_internal_bw,
		find_output_bw, find_total_bw;

	if (!_instrumented_is_sampling_enabled(ctx))
	{
		MALI_SUCCESS;
	}

	find_frame_time = _instrumented_is_counter_active(ctx, CID_M200_FRAME_TIME);
	find_frame_rate = _instrumented_is_counter_active(ctx, CID_M200_FPS);
	find_instruction_rate = _instrumented_is_counter_active(ctx, CID_M200_INSTRUCTION_RATE);
	find_primitive_rate = _instrumented_is_counter_active(ctx, CID_M200_PRIMITIVE_RATE);
	find_fill_rate = _instrumented_is_counter_active(ctx, CID_M200_FILL_RATE);
	find_triangle_bw = _instrumented_is_counter_active(ctx, CID_M200_TRIANGLE_PROCESSING_BW);
	find_texture_bw = _instrumented_is_counter_active(ctx, CID_M200_TEXTURE_BW);
	find_input_bw = _instrumented_is_counter_active(ctx, CID_M200_INPUT_BW);
	find_internal_bw = _instrumented_is_counter_active(ctx, CID_M200_INTERNAL_SHADER_BW);
	find_output_bw = _instrumented_is_counter_active(ctx, CID_M200_OUTPUT_BW);
	find_total_bw = _instrumented_is_counter_active(ctx, CID_M200_TOTAL_BW);
	if (find_frame_time || find_frame_rate || find_instruction_rate
	|| find_primitive_rate || find_fill_rate || find_triangle_bw
	|| find_texture_bw || find_input_bw || find_internal_bw
	|| find_output_bw || find_total_bw)
	{
		s64 tus = 0; /* frame time, in microseconds, needed by all in scope */

		if (_instrumented_is_counter_active(ctx, CID_M200_ACTIVE_CLOCK_CYCLES_COUNT))
		{
			tus = get_max_counter(CID_M200_ACTIVE_CLOCK_CYCLES_COUNT, frame) / (MALI_CORE_FREQ_KHZ / 1000);
		}

		if (0==tus) MALI_ERROR(MALI_ERR_FUNCTION_FAILED);

		if (find_frame_time)
		{
			_instrumented_set_counter(CID_M200_FRAME_TIME, frame, tus);
		}

		if (find_frame_rate)
		{
			_instrumented_set_counter( CID_M200_FPS, frame, 1000000000/tus);
		}

		if (find_instruction_rate)
		{
			if (!_instrumented_is_counter_active(ctx,
				CID_M200_INSTRUCTION_COMPLETED_COUNT))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			_instrumented_set_counter(CID_M200_INSTRUCTION_RATE, frame,
				get_sum_counter(CID_M200_INSTRUCTION_COMPLETED_COUNT,
					frame)*1000/tus);
		}

		if (find_primitive_rate)
		{
			if (!_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED)
			|| !_instrumented_is_counter_active(ctx, CID_MGP2_NUMBER_OF_BACKFACE_CULLINGS_DONE))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			_instrumented_set_counter(CID_M200_PRIMITIVE_RATE, frame,
				(  get_sum_counter(CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED,     frame)
				 - get_sum_counter(CID_MGP2_NUMBER_OF_BACKFACE_CULLINGS_DONE, frame) )*1000000/tus);
		}

		if (find_fill_rate)
		{
			if (!_instrumented_is_counter_active(ctx, CID_M200_FRAGMENT_RASTERIZED_COUNT))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}

			_instrumented_set_counter(CID_M200_FILL_RATE, frame,
				get_sum_counter(CID_M200_FRAGMENT_RASTERIZED_COUNT,
					frame)*1000/tus);
		}
		if (find_triangle_bw)
		{
			s64 cnt;
			if (!_instrumented_is_counter_active(ctx, CID_M200_POLYGON_LIST_READS)
				|| !_instrumented_is_counter_active(ctx, CID_M200_VERTEX_CACHE_READS)
				|| !_instrumented_is_counter_active(ctx, CID_M200_RSW_READS))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			cnt = get_sum_counter(CID_M200_POLYGON_LIST_READS, frame)
				+ get_sum_counter(CID_M200_VERTEX_CACHE_READS, frame)
				+ get_sum_counter(CID_M200_RSW_READS, frame);
			_instrumented_set_counter(CID_M200_TRIANGLE_PROCESSING_BW, frame,
				cnt*8000/tus);
		}

		if (find_texture_bw)
		{
			s64 cnt;
			if (
#if defined(USING_MALI200)
				!_instrumented_is_counter_active(ctx, CID_M200_PALETTE_CACHE_READS) ||
#endif
				!_instrumented_is_counter_active(ctx, CID_M200_TEXTURE_CACHE_UNCOMPRESSED_READS) ||
				!_instrumented_is_counter_active(ctx, CID_M200_TEXTURE_DESCRIPTORS_READS) ||
				!_instrumented_is_counter_active(ctx, CID_M200_TEXTURE_DESCRIPTORS_REMAPPING_READS))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			cnt =
#if defined(USING_MALI200)
				get_sum_counter(CID_M200_PALETTE_CACHE_READS, frame) +
#endif
				get_sum_counter(CID_M200_TEXTURE_CACHE_UNCOMPRESSED_READS, frame) +
				get_sum_counter(CID_M200_TEXTURE_DESCRIPTORS_READS, frame) +
				get_sum_counter(CID_M200_TEXTURE_DESCRIPTORS_REMAPPING_READS, frame);
			_instrumented_set_counter(CID_M200_TEXTURE_BW, frame, cnt*8000/tus);
		}

		if (find_input_bw)
		{
			s64 cnt;
			if (!_instrumented_is_counter_active(ctx, CID_M200_UNIFORM_REMAPPING_READS)
				|| !_instrumented_is_counter_active(ctx, CID_M200_PROGRAM_CACHE_READS)
				|| !_instrumented_is_counter_active(ctx, CID_M200_VARYING_READS))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}

			cnt = get_sum_counter(CID_M200_UNIFORM_REMAPPING_READS, frame)
				+ get_sum_counter(CID_M200_PROGRAM_CACHE_READS, frame)
				+ get_sum_counter(CID_M200_VARYING_READS, frame);

			_instrumented_set_counter(CID_M200_INPUT_BW, frame, cnt*8000/tus);
		}

		if (find_internal_bw)
		{
			s64 cnt;
			if (!_instrumented_is_counter_active(ctx, CID_M200_STORE_UNIT_WRITES)
			|| !_instrumented_is_counter_active(ctx, CID_M200_LOAD_UNIT_READS))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}

			cnt = get_sum_counter( CID_M200_STORE_UNIT_WRITES, frame)
				+ get_sum_counter( CID_M200_LOAD_UNIT_READS, frame);

			_instrumented_set_counter( CID_M200_INTERNAL_SHADER_BW, frame, cnt*8000/tus);
		}

		if (find_output_bw)
		{
			s64 cnt;
			if (!_instrumented_is_counter_active(ctx, CID_M200_TILE_WRITEBACK_WRITES))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			cnt = get_sum_counter( CID_M200_TILE_WRITEBACK_WRITES,frame);
			_instrumented_set_counter( CID_M200_OUTPUT_BW, frame, cnt*8000/tus);
		}

		if (find_total_bw)
		{
			if (!_instrumented_is_counter_active(ctx, CID_M200_TOTAL_BUS_WRITES)
				|| !_instrumented_is_counter_active(ctx, CID_M200_TOTAL_BUS_READS))
			{
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}

			_instrumented_set_counter( CID_M200_TOTAL_BW, frame,
				(   get_sum_counter(CID_M200_TOTAL_BUS_WRITES, frame)
				  + get_sum_counter(CID_M200_TOTAL_BUS_READS,  frame) )*8000/tus);
		}

	}  /* if find something */
	MALI_SUCCESS;
}  /* _mali_pp_derive_counts */


MALI_STATIC_INLINE void activate_tus(mali_instrumented_context* ctx)
{
	_instrumented_activate_counter_derived(ctx, CID_M200_ACTIVE_CLOCK_CYCLES_COUNT);
}

MALI_EXPORT
void _mali_pp_activate_derived(mali_instrumented_context* ctx)
{		/* NB change this function if you change _mali_pp_activate_derived */
	if (  _instrumented_is_counter_active(ctx, CID_M200_FRAME_TIME)
		|| _instrumented_is_counter_active(ctx, CID_M200_FPS))
	{
		activate_tus(ctx);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_INSTRUCTION_RATE))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_M200_INSTRUCTION_COMPLETED_COUNT);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_PRIMITIVE_RATE))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_PRIMITIVES_FETCHED);
		_instrumented_activate_counter_derived(ctx, CID_MGP2_NUMBER_OF_BACKFACE_CULLINGS_DONE);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_FILL_RATE))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_M200_FRAGMENT_RASTERIZED_COUNT);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_TRIANGLE_PROCESSING_BW))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_M200_POLYGON_LIST_READS);
		_instrumented_activate_counter_derived(ctx, CID_M200_VERTEX_CACHE_READS);
		_instrumented_activate_counter_derived(ctx, CID_M200_RSW_READS);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_TEXTURE_BW))
	{
		activate_tus(ctx);
#if defined(USING_MALI200)
		_instrumented_activate_counter_derived(ctx, CID_M200_PALETTE_CACHE_READS);
#endif
		_instrumented_activate_counter_derived(ctx, CID_M200_TEXTURE_CACHE_UNCOMPRESSED_READS);
		_instrumented_activate_counter_derived(ctx, CID_M200_TEXTURE_DESCRIPTORS_READS);
		_instrumented_activate_counter_derived(ctx, CID_M200_TEXTURE_DESCRIPTORS_REMAPPING_READS);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_INPUT_BW))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_M200_UNIFORM_REMAPPING_READS);
		_instrumented_activate_counter_derived(ctx, CID_M200_PROGRAM_CACHE_READS);
		_instrumented_activate_counter_derived(ctx, CID_M200_VARYING_READS);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_INTERNAL_SHADER_BW))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_M200_STORE_UNIT_WRITES);
		_instrumented_activate_counter_derived(ctx, CID_M200_LOAD_UNIT_READS);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_OUTPUT_BW))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_M200_TILE_WRITEBACK_WRITES);
	}

	if (_instrumented_is_counter_active(ctx, CID_M200_TOTAL_BW))
	{
		activate_tus(ctx);
		_instrumented_activate_counter_derived(ctx, CID_M200_TOTAL_BUS_READS);
		_instrumented_activate_counter_derived(ctx, CID_M200_TOTAL_BUS_WRITES);
	}
} /* _mali_pp_activate_derived */

