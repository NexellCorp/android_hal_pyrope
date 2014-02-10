/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef M200_GP_FRAME_BUILDER_STRUCT_H
#define M200_GP_FRAME_BUILDER_STRUCT_H

#include <shared/m200_gp_frame_builder.h>
#include <shared/m200_tilelists.h>
#include <regs/MALIGP2/mali_gp_plbu_config.h>
#include <regs/MALI200/mali_render_regs.h>
#include <shared/mali_mem_pool.h>
#include <shared/mali_surface.h>
#include <shared/m200_projob.h>
#include <shared/m200_surfacetracking.h>

#include <shared/mali_sw_counters.h>

#define GP_CONTEXT_STACK_SIZE (6*4*sizeof(u32))

/* internal helper for getting the current internal frame from a frame builder */
#define GET_CURRENT_INTERNAL_FRAME( FBUILDER ) ((FBUILDER)->iframes[ (FBUILDER)->iframe_current ])

/* internal helper for rotating the internal frames of a frame builder */
#define ROTATE_INTERNAL_FRAMES( FBUILDER ) (((FBUILDER)->iframe_current + 1 == (FBUILDER)->iframe_count ) ? 0 : (FBUILDER)->iframe_current + 1)

/* size for the callback list */
#define INITIAL_LIST_ROOM 32

/* Bitflags to provide the framebuilder awareness of targets state.
 * 4 buffer states per [color, depth, stencil] plane.
 * Only one state per buffer can be true.
*/
#define MALI_COLOR_PLANE_UNDEFINED 		(1<<0)
#define MALI_COLOR_PLANE_UNCHANGED 		(1<<1)
#define MALI_COLOR_PLANE_CLEAR_TO_COLOR		(1<<2)
#define MALI_COLOR_PLANE_DIRTY			(1<<3)
#define MALI_DEPTH_PLANE_UNDEFINED 		(1<<4)
#define MALI_DEPTH_PLANE_UNCHANGED 		(1<<5)
#define MALI_DEPTH_PLANE_CLEAR_TO_COLOR		(1<<6)
#define MALI_DEPTH_PLANE_DIRTY			(1<<7)
#define MALI_STENCIL_PLANE_UNDEFINED 		(1<<8)
#define MALI_STENCIL_PLANE_UNCHANGED 		(1<<9)
#define MALI_STENCIL_PLANE_CLEAR_TO_COLOR	(1<<10)
#define MALI_STENCIL_PLANE_DIRTY		(1<<11)
/* Mask to set an specific buffer states to true. This mask will be used with a bitwise AND and inverted (~) */
#define MALI_COLOR_PLANE_MASK (MALI_COLOR_PLANE_UNDEFINED | MALI_COLOR_PLANE_UNCHANGED | MALI_COLOR_PLANE_CLEAR_TO_COLOR | MALI_COLOR_PLANE_DIRTY)
#define MALI_DEPTH_PLANE_MASK (MALI_DEPTH_PLANE_UNDEFINED | MALI_DEPTH_PLANE_UNCHANGED | MALI_DEPTH_PLANE_CLEAR_TO_COLOR | MALI_DEPTH_PLANE_DIRTY)
#define MALI_STENCIL_PLANE_MASK (MALI_STENCIL_PLANE_UNDEFINED | MALI_STENCIL_PLANE_UNCHANGED | MALI_STENCIL_PLANE_CLEAR_TO_COLOR | MALI_STENCIL_PLANE_DIRTY)

/** Wrapper for the callbacks added to an internal frame */
typedef struct mali_frame_callback_wrapper
{
	mali_frame_cb_func  func;
	mali_frame_cb_param param;
	mali_bool can_be_deferred;
} mali_frame_callback_wrapper;

typedef enum mali_internal_frame_state
{
	FRAME_CLEAN = 0,  /**< There is nothing to be rendered (this is the default initial state) */
	FRAME_UNMODIFIED, /**< There is nothing to be rendered - including the clear values (optional initial state) */
	FRAME_DIRTY,      /**< There is something that must be rendered in the frame */
	FRAME_RENDERING,  /**< base is rendering frame, and structures must not be modified */
	FRAME_COMPLETE    /**< base has released the frame as rendering has completed */
} mali_internal_frame_state;

typedef enum mali_internal_frame_cow_flavour
{
	FRAME_COW_REALLOC = 0,        /**< COW is just resource re-allocation (we are going to override all content) */
	FRAME_COW_DEEP,               /**< COW must preserve partial data (e.g. when the framebuilder exploits the writeback-dirty bit */
	FRAME_COW_DEEP_COPY_PENDING   /**< Indicates that we have hit COW and need to copy the data before actually starting the PP job */
} mali_internal_frame_cow_flavour;

struct mali_frame_builder_output_chain
{
	struct mali_surface* buffer;
	u32 usage; 
};

/* Internal frames finishing Out Of Order Synchronizer
   This is only used embedded into the mali_internal_frame.
   It is however kept as a separate struct since it is protected by a
   different mutex than the rest of the internal_frame members.
   The members of this struct can only be touched (R/W) from callback functions from
   the dependency system, that holds a global mutex, and thus prevents others
   from touching the variables at the same time.
   The exception from this rule is when the internal_frame
   struct is created and is destroyed.*/
struct mali_internal_frame;
typedef struct mali_frame_order_synchronizer
{
	struct mali_internal_frame * prev;
	struct mali_internal_frame * next;
	u32 swap_nr;
	mali_bool in_flight;
	mali_ds_consumer_handle release_on_finish;
	mali_mutex_handle frame_order_mutex;		/**< mutex to serialize the callback and the main thread */
}mali_frame_order_synchronizer;

typedef struct mali_heap_holder
{
	mali_mem_handle plbu_heap;					/**< PLBU heap. Initial block stays _allocated_ as long as the fbuilder size is unchanged */
	u32  init_size;
	u32  use_count;								/**< Count the number of times this has gone through a job. Zero after a reset */
	mali_ds_resource_handle plbu_ds_resource;	/**< The array of PLBU-resources */

}mali_heap_holder;

struct mali_frame_builder;
typedef struct mali_internal_frame
{
#ifdef MALI_INSTRUMENTED
	mali_instrumented_frame   *instrumented_frame;
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
	mali_base_flush_id         flush_id;           /**< the flush id used when starting to render this frame - for profiling purpose only */
#endif
	struct mali_frame_builder *frame_builder;      /**< the frame builder this frame belongs to - for profiling purpose only */

	volatile mali_internal_frame_state state;
	mali_mutex_handle mutex;              /**< mutex to protect the frame state */
	mali_lock_handle lock;                /**< a lock held while the GPU is active */

	mali_ds_consumer_handle ds_consumer_pp_render;    /**< Dependency object starting PP jobs and signaling when they are finished */
	mali_ds_resource_handle current_plbu_ds_resource; /**< Holds the handle to the current PLBU-resource */
	mali_ds_consumer_handle ds_consumer_gp_job;       /**< Dependency object starting GP jobs, holding the plbu-resource until pp-job is finished */

	mali_atomic_int completion_status;    /**< an completion status that will be set iff an error occur while rendering.
	                                           The possible values are the same as the values in the enum
	                                           mali_job_completion_status */

	struct
	{
		float top, bottom, left, right;
	} vpbox;

	mali_mem_handle frame_mem;            /**< Misc memory for "garbage collection" on frame_reset and after a successful bufferswap */
	mali_mem_handle gp_job_mem_list;      /**< Misc GP related memory for "garbage collection" on completion of a flush */
	struct mali_tilelist* tilelists;     /**< Holds the master, slave and pointer memory */ 
	mali_heap_holder* plbu_heap_held;     /**< PLBU heap holder structure. Initial block stays _allocated_ as long as the fbuilder size is unchanged */
	struct
	{
		u32             start;
		u32             grow;
		mali_mem_handle fs_stack_mem;     /**< Memory for the fragment shader stack */
	} fs_stack;

	mali_pp_job_handle pp_job;
	u32 curr_pp_split_count;

	mali_gp_job_handle gp_job;
	mali_gp_job_handle current_gp_job;             /**< Tells us which job is the current-job to be executed when the gp-consumer is activated */

	struct mali_projob projob;                     /**< Holds all projobs of the current frame */

	mali_bool plbu_heap_reset_on_job_start;        /**< MALI_TRUE if the frame's PLBU heap should be reset on GP job start */
	mali_bool readback_first_drawcall; 		       /**< MALI_TRUE when the frame is reset. Once a readback is required, it will become MALI_FALSE */
	volatile mali_bool reset_on_finish;            /**< MALI_TRUE if the frame should be reset after rendering */
	u32 num_flushes_since_reset;                   /**< used for incremental rendering */

	struct mali_surfacetracking* surfacetracking;  /* tracks all read surfaces in this frame, for lock purposes */

	mali_frame_callback_wrapper *callback_list;
	int callback_list_room, callback_list_size;

	/* Set if the frame have callbacks that can not be deferred.
	 * This is needed by OpenVG.
	 * This feature is to be removed at some later point in time
	 */
	mali_bool have_non_deferred_callbacks; 

	volatile mali_frame_cb_func cb_func_lock_output;
	volatile mali_frame_cb_param cb_param_lock_output;

	volatile mali_frame_cb_func cb_func_acquire_output;
	volatile mali_frame_cb_param cb_param_acquire_output;

	volatile mali_frame_cb_func cb_func_complete_output;
	volatile mali_frame_cb_param cb_param_complete_output;

#if MALI_EXTERNAL_SYNC

	volatile mali_frame_pp_job_start_cb_func cb_func_pp_job_start;
	volatile mali_frame_cb_param cb_param_pp_job_start;

	mali_stream_handle stream;

	/* used by EGL callback to accumulate fence syncs */
	void *fence_sync;

#endif /* MALI_EXTERNAL_SYNC */

	mali_base_frame_id frame_id;                   /**< The Base driver id for jobs for this frame. Used for job-abort */

#if HARDWARE_ISSUE_4126
	int last_vshader_instruction_count;
#endif

	/* Internal frames finishing Out Of Order Synchronizer */
	mali_frame_order_synchronizer order_synch;

	mali_mem_handle gp_context_stack;     /**< Used to store state shared between gp jobs */

	mali_mem_pool						frame_pool;			/**< A pointer to the current memory pool */
	mali_bool							pool_mapped;		/**< The mapped status of the current memory pool */
	mali_internal_frame_cow_flavour   	cow_flavour;		/**< The CoW flavour */
	mali_surface_deep_cow_descriptor  	cow_desc;			/**< Deep CoW information */
	int 								job_inited;			/**< Check that the job has been inited */

	u64* pre_plbu_cmd_head;                       /**< If we have context switch, this is to hold the cmd for pre_gp_job>**/

#if MALI_SW_COUNTERS_ENABLED
	mali_sw_counters *sw_counters;                /**< Table of software counters for this frame */
#endif

} mali_internal_frame;

struct mali_frame_builder
{
	mali_base_ctx_handle base_ctx;

	/* output surface information */
	struct mali_frame_builder_output_chain output_buffers[ MALI200_WRITEBACK_UNIT_COUNT ]; /* holding all the outputs */
	struct mali_frame_builder_output_chain readback_buffers[ MALI_READBACK_SURFACES_COUNT ]; /* holding all the readback surfaces */
	u32 output_width, output_height;         /**< The dimensions of the output attachments - before downscale. This is divided into 16x16 tiles */
	u32 output_log2_scale_x, output_log2_scale_y;  /**< Scaling factors of the largest attachment. Used in M400_FRAME_REG_SCALING_CONFIG */
	mali_bool output_valid;                  /**< True if all attachments are of the same dimension >0 and have the same number of swapbuffer count.
	                                              Calling use() or writelock() with invalid outputs will trigger an assert. */

	mali_bool fp16_format_used; /* mali fp16 format is in use flag */

	/* bounding box state */
	int bounding_box_width;
	int bounding_box_height;
	mali_bool bounding_box_used;
	mali_bool bounding_box_enable[ MALI200_WRITEBACK_UNIT_COUNT ];

	/** A set of static properties for this frame builder. See #mali_frame_builder_properties */
	u32 properties;
	
	/* Bitflags to provide the framebuilder awareness of targets state.
	 * 4 buffer states per [color, depth, stencil] plane */
	u32 buffer_state_per_plane;

	/* This state variable indicates if the multisample bit needs to be preserved when doing incremental rendering */
	mali_bool preserve_multisample;

	u32 color_clear_value[4];             /**< The value use for clearing the color buffer */
	u32 depth_clear_value;                /**< The value use for clearing the depth buffer */
	u32 stencil_clear_value;              /**< The value use for clearing the stencil buffer */

	u32 plbu_heap_size;                    /**< current plbu heap size */

	u32                   iframe_count;   /**< number of internal frames */
	u32                   iframe_current; /**< index of the current internal frame */
	mali_internal_frame **iframes;  /**< array of internal frames */

	u32              plbu_heap_count;   /**< The number of PLBU-heaps available */
	u32              plbu_heap_current; /**< The index of the current PLBU-heap */
	mali_heap_holder *plbu_heaps;   /**< The array of PLBU-heap holders */

	/* The total number of _mali_frame_builder_swap's performed on this frame-builder */
	u32 swap_performed;

	/* number of flushes since a frame reset before doing an incremental render */
	u32 incremental_render_flush_threshold;

	mali_mem_handle dummy_rsw_mem;  /**< dummy RSW memory block, used by all frames in the framebuilder. Read only, should never change after allocation.*/

	u32 curr_pp_split_count;

	struct
	{
		u32 top, bottom, left, right;
	} scissor;
	struct
	{
		float top, bottom, left, right;
	} vpbox;

	mali_bool inside_writelock;                   /**< TRUE when inside a writelock, otherwise FALSE */

#if HARDWARE_ISSUE_7320
	mali_mem_handle flush_commandlist_subroutine; /**< vertex shader commandlist subroutine to call instead of flush */
#endif
	mali_bool inc_render_on_flush;                /**< Next flush should start Incremental rendering */
	u32	samples;                                  /**< samples per pixel for the surface */
	egl_api_shared_function_ptrs *egl_funcptrs;   /**< EGL shared function pointers for internal EGL handling */

	u32	tilebuffer_color_format;                  /**< The number of bits per color channel ARGB in the tilebuffer. This will always be 0x00008888 for mali200 */

#if MALI_TIMELINE_PROFILING_ENABLED
	u32 identifier;                               /**< Framebuilder identifier consisting of type and unique id */
	u32 flush_count;                              /**< Running counter for flush IDs */ 
#endif	

	u32 past_plbu_size[4];
	u32 current_period;

#if MALI_EXTERNAL_SYNC
	mali_fence_handle output_fence;
#endif /* MALI_EXTERNAL_SYNC */
};

/**
 * Add the mem_handle to the internal frame's cleanup pool
 */
MALI_STATIC_INLINE void _mali_frame_builder_add_internal_frame_mem( mali_internal_frame *frame, mali_mem_handle mem_handle )
{
	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT_HANDLE( mem_handle );

	frame->frame_mem = _mali_mem_list_insert_after( frame->frame_mem, mem_handle );
}

/**
 * Executes and removes all callbacks that have been enqueued for a frame. This is
 * called when a frame is released or reset.
 * @param frame The frame to which the callbacks are bound
 */
MALI_IMPORT void _mali_frame_builder_frame_execute_callbacks( mali_internal_frame * frame );

/**
 * Executes any callbacks marked as non-deferred that have been enqueued for a frame.
 * This is called when a frame is released or reset.
 * @param frame The frame to which the callbacks are bound
 */
MALI_IMPORT void _mali_frame_builder_frame_execute_callbacks_non_deferred( mali_internal_frame * frame );

#if HARDWARE_ISSUE_7320
/**
 * Allocate a memory block holding a PLBU subroutine for flushign the command list
 * N amount of times, where N is defined by HARDWARE_ISSUE_7320. 
 *
 * @param base_ctx - The base ctx
 * @return A mali mem handle holding this memory block. 
 */
mali_mem_handle _mali_frame_builder_create_flush_command_list_for_bug_7320(mali_base_ctx_handle base_ctx);
#endif

#endif /* M200_GP_FRAME_BUILDER_STRUCT_H */
