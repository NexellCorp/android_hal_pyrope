/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file	gles_debug_stats.h
 * @brief	Intrusive statistics gathering code.
 *
 * begin:	Mon Dec 4 2006
 * copyright: (C) 2006 by ARM Limited
 */

#ifndef _GLES_DEBUG_STATS_H_
#define _GLES_DEBUG_STATS_H_

#include "base/mali_debug.h"

/*
 *	In the absence of an external definition for GLES_DEBUG_STATS_ENABLED,
 *	we choose whether to enable stats based on the debug/release build.
 *
 *	Valid values for GLES_DEBUG_STATS_ENABLED are:
 *
 *		0	No stats gathering at all
 *		1	Frame rate only
 *		2	Frame rate and high-level stats
 *		3	Frame rate, high-level and low-level intrusive stats
 */

#ifndef GLES_DEBUG_STATS_ENABLED
	#ifdef __ARMCC_VERSION
		#define GLES_DEBUG_STATS_ENABLED 0	/*ARMCC does not compile when this is set - this will change once we get RVCT+GLIBC working */
	#else /* __ARMCC_VERSION */
		#ifdef DEBUG
			#define GLES_DEBUG_STATS_ENABLED 1	/* Intrusive stats by default */
		#else /* DEBUG */
			#define GLES_DEBUG_STATS_ENABLED 1	/* Framerates only in release build */
		#endif /* DEBUG */
	#endif /* __ARMCC_VERSION */
#endif /* GLES_DEBUG_STATS_ENABLED */

/* If 1, print a trace message at each call to a stats function */
#ifdef NDEBUG
/* org */
#define GLES_DEBUG_STATS_TRACE 0
#else /* nexell add */
#define GLES_DEBUG_STATS_TRACE 1
#define GLES_DEBUG_PRINT_ENTRYPOINTS 1
#endif

/* Default definitions - disappears when disabled */
#define GLES_DEBUG_STATS_FRAME()
#define GLES_DEBUG_STATS_INTERVAL_START(index,name)
#define GLES_DEBUG_STATS_INTERVAL_END(index)
#define GLES_DEBUG_STATS_COUNT(index,name,increment)
#define GLES_DEBUG_STATS_INTERVAL_START_INTRUSIVE(index,name)
#define GLES_DEBUG_STATS_INTERVAL_END_INTRUSIVE(index)
#define GLES_DEBUG_STATS_COUNT_INTRUSIVE(index,name,increment)

#if GLES_DEBUG_STATS_ENABLED > 0

	#undef GLES_DEBUG_STATS_FRAME

	/**
	 *	Enumeration of intervals.
	 */
	typedef enum
	{
		GLES_DRAW = 0,			/**< Time spent drawing */
		GLES_TRANSFORM,			/**< Time spent transforming vertices */
		GLES_DRAW_TRIANGLE,		/**< Time spent drawing triangle as a whole, of which... */
		GLES_DRAWTRI_CULL,		/**< Time spent culling triangles */
		GLES_DRAWTRI_TNL,		/**< Time spent lighting triangles */
		GLES_DRAWTRI_TNL_TP,		/**< Time spent transforming vertex positions */
		GLES_DRAWTRI_TNL_TX,		/**< Time spent transforming vertex texcoords */
		GLES_DRAWTRI_TNL_TN,		/**< Time spent transforming vertex normals */
		GLES_DRAWTRI_TNL_L,		/**< Time spent lighting vertices */
		GLES_DRAWTRI_CLIP,		/**< Time spent clipping */
		GLES_DRAWTRI_TRI_PROJECT,	/**< Time spent projecting to screen */
		GLES_DRAWTRI_TRI_PACK,		/**< Time spent packing data for Mali */
		GLES_DRAWTRI_TRI_BIN,		/**< Time spent binning data */

		MAX_INTERVALS,			/**< Number of intervals defined */

		GLES_TEMP_INTERVAL,     /**< for temp counters, to be used as TEMP_INTERVAL, TEMP_INTERVAL+1 etc */

		MAX_NAMED_INTERVALS = MAX_INTERVALS + 100,	/**< Maximum counters referred to my name only, not enum */
		GLES_NAMED_INTERVAL = -1	/**< Look up using the interval's name, not number */
	} gles_debug_interval;

	/**
	 *	Enumeration of counters.
	 */
	typedef enum
	{
		GLES_DRAW_INDEXED =0,		/**< Number of calls to draw_indexed */
		GLES_TRIANGLES,			/**< Triangles passed in */
		GLES_TRIANGLES_CULLED,		/**< Triangles culled via clipping */
		GLES_TRIANGLES_CLIPPED_0,	/**< Triangles clipped to nothing */
		GLES_TRIANGLES_BACKFACE,	/**< Triangles culled via backface elimination */
		GLES_TRIANGLES_FAST,		/**< Triangles processed in fast path */
		GLES_TRIANGLES_SLOW,		/**< Triangles processed in slow path */
		GLES_TRIANGLES_CLIPPED,		/**< Triangles clipped */
		GLES_TRIANGLES_ABANDONED,	/**< Triangles abandoned at the last minute due to bbox issues */
		GLES_TRIANGLES_DRAWN,		/**< Triangles passed to the rasterizer */
		GLES_TNL_CACHE_HITS,		/**< T&L vertices fetched directly from cache */
		GLES_TNL_CACHE_MISSES,		/**< T&L vertices computed */
		GLES_PACK_CACHE_HITS,		/**< Packed vertices reused */
		GLES_PACK_CACHE_MISSES,		/**< New packed vertices created */
		GLES_VERTICES_LIT,		/**< Vertices lit */
		GLES_VERTICES_PRELIT,		/**< Prelit vertices */
		GLES_VERTICES_CURRENT_COLOR,	/**< 'current' coloured vertices */
		GLES_VERTICES_FOGGED,		/**< Fogged vertices */

		MAX_COUNTERS,			/**< Number of counters defined */

		GLES_TEMP_COUNT,        /**< for temp counters, to be used as TEMP_COUNT, TEMP_COUNT+1 etc */

		MAX_NAMED_COUNTERS = MAX_COUNTERS + 100,	/**< Maximum counters referred to my name only, not enum */
		GLES_NAMED_COUNTER = -1		/**< Look up using the counter's name, not number */
	} gles_debug_counters;

	/**
	 *	Record the end of a frame. Also prints the output at set intervals.
	 */
	#define GLES_DEBUG_STATS_FRAME() gles_debug_stats_frame()
	void gles_debug_stats_frame(void);


	#if GLES_DEBUG_STATS_ENABLED > 1

		#undef GLES_DEBUG_STATS_INTERVAL_START
		#undef GLES_DEBUG_STATS_INTERVAL_END
		#undef GLES_DEBUG_STATS_COUNT

		/**
		 * Records the start time of a interval. The time between the start and
		 * end of the interval is recorded.
		 * @param index The interval index - usually an enum value, although you can also use
		 *		GLES_NAMED_INTERVAL, and the interval will be looked up by name (and added
		 *		to the list if it does not exist). You should not use this method for time-critical
		 *		sections of code, as it is much slower than supplying a real enum value.
		 * @param name  A human-readable name for this interval.
		 */
		#define GLES_DEBUG_STATS_INTERVAL_START(index,name) gles_debug_stats_interval_start((index) ,(name))
		void gles_debug_stats_interval_start(gles_debug_interval index, const char* name);

		/**
		 *	Records the end time of a interval. The time between the start and
		 *	end of the interval is recorded.
		 *
		 * @param index The interval index - usually an enum value.
		 *		This is used to check that the intervals are correctly nested.
		 *		If you supply GLES_NAMED_INTERVAL, we assume that the interval is correctly
		 *		nested and do no checking.
		 */
		#define GLES_DEBUG_STATS_INTERVAL_END(index) gles_debug_stats_interval_end((index))
		void gles_debug_stats_interval_end(gles_debug_interval index);

		/**
		 *	Records stats in a counter. The increment value is added to the nominated counter.
		 *
		 * @param index The counter index - usually an enum value, although you can also use
		 *		GLES_NAMED_COUNTER, and the interval will be looked up by name (and added
		 *		to the list if it does not exist). You should not use this method for time-critical
		 *		sections of code, as it is much slower than supplying a real enum value.
		 * @param name A human-readable name for this counter.
		 * @param increment An integer to add to the counter value
		 */
		#define GLES_DEBUG_STATS_COUNT(index,name,increment) gles_debug_stats_count((index) ,(name), (increment))
		void gles_debug_stats_count(gles_debug_counters index, const char* name, int increment);

		#if GLES_DEBUG_STATS_ENABLED > 2

			/* Enable intrusive stats */
			/* These are stats inside inner loops where statistics gathering may cause a serious overhead */

			#undef GLES_DEBUG_STATS_INTERVAL_START_INTRUSIVE
			#undef GLES_DEBUG_STATS_INTERVAL_END_INTRUSIVE
			#undef GLES_DEBUG_STATS_COUNT_INTRUSIVE

			#define GLES_DEBUG_STATS_INTERVAL_START_INTRUSIVE	GLES_DEBUG_STATS_INTERVAL_START
			#define GLES_DEBUG_STATS_INTERVAL_END_INTRUSIVE		GLES_DEBUG_STATS_INTERVAL_END
			#define GLES_DEBUG_STATS_COUNT_INTRUSIVE			GLES_DEBUG_STATS_COUNT

		#endif /* GLES_DEBUG_STATS_ENABLED > 2 */

	#endif /* GLES_DEBUG_STATS_ENABLED > 1 */

#endif /* GLES_DEBUG_STATS_ENABLED > 0 */


#ifndef NDEBUG

/*
 *	If GLES_DEBUG_PRINT_ENTRYPOINTS exists and is non-0,
 *	we should print information about entry and exit points to stdout.
 */
#ifndef GLES_DEBUG_PRINT_ENTRYPOINTS
	#define GLES_DEBUG_PRINT_ENTRYPOINTS 0
#endif /* GLES_DEBUG_PRINT_ENTRYPOINTS */

/*
 *	If GLES_DEBUG_STATS_FOR_ENTRYPOINTS exists and is non-0,
 *	we should gather statistics for GLES and EGL entrypoints.
 */
#ifndef GLES_DEBUG_STATS_FOR_ENTRYPOINTS
	#define GLES_DEBUG_STATS_FOR_ENTRYPOINTS 0
#endif /* GLES_DEBUG_STATS_FOR_ENTRYPOINTS */

/*
 *	Wrapper macros for start and end of instrumented functions
 */
#define GLES_ENTER_FUNC() GLES_DEBUG_ENTER_STATS GLES_DEBUG_ENTER_PRINT
#define GLES_LEAVE_FUNC() GLES_DEBUG_LEAVE_STATS GLES_DEBUG_LEAVE_PRINT


/*
 *  Toggle printing of entry points
 */
#if GLES_DEBUG_PRINT_ENTRYPOINTS
	#define GLES_DEBUG_ENTER_PRINT _gles_debug_enter_func(MALI_FUNCTION);
	#define GLES_DEBUG_LEAVE_PRINT _gles_debug_leave_func(MALI_FUNCTION);

	/**
	 * enter a function and print its name
	 * @param func_name The name of the function to print
	*/
	void _gles_debug_enter_func(const char *func_name);

	/**
	 * leave a function and print its name
	 * @param func_name The name of the function to print
	*/
	void _gles_debug_leave_func(const char *func_name);

#else /* GLES_DEBUG_PRINT_ENTRYPOINTS */

	#define GLES_DEBUG_ENTER_PRINT
	#define GLES_DEBUG_LEAVE_PRINT

#endif /* GLES_DEBUG_PRINT_ENTRYPOINTS */

/*
 *  Toggle printing entrypoint stats
 */
#if GLES_DEBUG_PRINT_ENTRYPOINTS
	#define GLES_DEBUG_ENTER_STATS GLES_DEBUG_STATS_INTERVAL_START(GLES_NAMED_INTERVAL,MALI_FUNCTION); GLES_DEBUG_STATS_COUNT(GLES_NAMED_COUNTER,MALI_FUNCTION,1);
	#define GLES_DEBUG_LEAVE_STATS GLES_DEBUG_STATS_INTERVAL_END(GLES_NAMED_INTERVAL);
#else /* GLES_DEBUG_PRINT_ENTRYPOINTS */
	#define GLES_DEBUG_ENTER_STATS
	#define GLES_DEBUG_LEAVE_STATS
#endif /* GLES_DEBUG_PRINT_ENTRYPOINTS */

#else /* !defined(NDEBUG) */

#define GLES_ENTER_FUNC()
#define GLES_LEAVE_FUNC()

#endif /* !defined(NDEBUG) */


#endif /* _GLES_DEBUG_STATS_H_ */
