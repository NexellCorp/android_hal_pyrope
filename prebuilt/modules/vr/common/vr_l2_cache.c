/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "vr_kernel_common.h"
#include "vr_osk.h"
#include "vr_l2_cache.h"
#include "vr_hw_core.h"
#include "vr_scheduler.h"

/**
 * Size of the VR L2 cache registers in bytes
 */
#define VR400_L2_CACHE_REGISTERS_SIZE 0x30

/**
 * VR L2 cache register numbers
 * Used in the register read/write routines.
 * See the hardware documentation for more information about each register
 */
typedef enum vr_l2_cache_register {
	VR400_L2_CACHE_REGISTER_STATUS       = 0x0008,
	/*unused                               = 0x000C */
	VR400_L2_CACHE_REGISTER_COMMAND      = 0x0010, /**< Misc cache commands, e.g. clear */
	VR400_L2_CACHE_REGISTER_CLEAR_PAGE   = 0x0014,
	VR400_L2_CACHE_REGISTER_MAX_READS    = 0x0018, /**< Limit of outstanding read requests */
	VR400_L2_CACHE_REGISTER_ENABLE       = 0x001C, /**< Enable misc cache features */
	VR400_L2_CACHE_REGISTER_PERFCNT_SRC0 = 0x0020,
	VR400_L2_CACHE_REGISTER_PERFCNT_VAL0 = 0x0024,
	VR400_L2_CACHE_REGISTER_PERFCNT_SRC1 = 0x0028,
	VR400_L2_CACHE_REGISTER_PERFCNT_VAL1 = 0x002C,
} vr_l2_cache_register;

/**
 * VR L2 cache commands
 * These are the commands that can be sent to the VR L2 cache unit
 */
typedef enum vr_l2_cache_command
{
	VR400_L2_CACHE_COMMAND_CLEAR_ALL = 0x01, /**< Clear the entire cache */
	/* Read HW TRM carefully before adding/using other commands than the clear above */
} vr_l2_cache_command;

/**
 * VR L2 cache commands
 * These are the commands that can be sent to the VR L2 cache unit
 */
typedef enum vr_l2_cache_enable
{
	VR400_L2_CACHE_ENABLE_DEFAULT = 0x0, /**< Default state of enable register */
	VR400_L2_CACHE_ENABLE_ACCESS = 0x01, /**< Permit cacheable accesses */
	VR400_L2_CACHE_ENABLE_READ_ALLOCATE = 0x02, /**< Permit cache read allocate */
} vr_l2_cache_enable;

/**
 * VR L2 cache status bits
 */
typedef enum vr_l2_cache_status
{
	VR400_L2_CACHE_STATUS_COMMAND_BUSY = 0x01, /**< Command handler of L2 cache is busy */
	VR400_L2_CACHE_STATUS_DATA_BUSY    = 0x02, /**< L2 cache is busy handling data requests */
} vr_l2_cache_status;

/**
 * Definition of the L2 cache core struct
 * Used to track a L2 cache unit in the system.
 * Contains information about the mapping of the registers
 */
struct vr_l2_cache_core
{
	struct vr_hw_core  hw_core;      /**< Common for all HW cores */
	u32                  core_id;      /**< Unique core ID */
	_vr_osk_lock_t    *command_lock; /**< Serialize all L2 cache commands */
	_vr_osk_lock_t    *counter_lock; /**< Synchronize L2 cache counter access */
	u32                  counter_src0; /**< Performance counter 0, VR_HW_CORE_NO_COUNTER for disabled */
	u32                  counter_src1; /**< Performance counter 1, VR_HW_CORE_NO_COUNTER for disabled */
	u32                  last_invalidated_id;
	vr_bool            power_is_enabled;
};

#define VR400_L2_MAX_READS_DEFAULT 0x1C

static struct vr_l2_cache_core *vr_global_l2_cache_cores[VR_MAX_NUMBER_OF_L2_CACHE_CORES];
static u32 vr_global_num_l2_cache_cores = 0;

int vr_l2_max_reads = VR400_L2_MAX_READS_DEFAULT;

/* Local helper functions */
static _vr_osk_errcode_t vr_l2_cache_send_command(struct vr_l2_cache_core *cache, u32 reg, u32 val);


struct vr_l2_cache_core *vr_l2_cache_create(_vr_osk_resource_t *resource)
{
	struct vr_l2_cache_core *cache = NULL;
	_vr_osk_lock_flags_t lock_flags;

#if defined(VR_UPPER_HALF_SCHEDULING)
	lock_flags = _VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK_IRQ | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE;
#else
	lock_flags = _VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE;
#endif

	VR_DEBUG_PRINT(2, ("VR L2 cache: Creating VR L2 cache: %s\n", resource->description));

	if (vr_global_num_l2_cache_cores >= VR_MAX_NUMBER_OF_L2_CACHE_CORES)
	{
		VR_PRINT_ERROR(("VR L2 cache: Too many L2 cache core objects created\n"));
		return NULL;
	}

	cache = _vr_osk_malloc(sizeof(struct vr_l2_cache_core));
	if (NULL != cache)
	{
		cache->core_id =  vr_global_num_l2_cache_cores;
		cache->counter_src0 = VR_HW_CORE_NO_COUNTER;
		cache->counter_src1 = VR_HW_CORE_NO_COUNTER;
		if (_VR_OSK_ERR_OK == vr_hw_core_create(&cache->hw_core, resource, VR400_L2_CACHE_REGISTERS_SIZE))
		{
			cache->command_lock = _vr_osk_lock_init(lock_flags, 0, _VR_OSK_LOCK_ORDER_L2_COMMAND);
			if (NULL != cache->command_lock)
			{
				cache->counter_lock = _vr_osk_lock_init(lock_flags, 0, _VR_OSK_LOCK_ORDER_L2_COUNTER);
				if (NULL != cache->counter_lock)
				{
					vr_l2_cache_reset(cache);

					cache->last_invalidated_id = 0;
					cache->power_is_enabled = VR_TRUE;

					vr_global_l2_cache_cores[vr_global_num_l2_cache_cores] = cache;
					vr_global_num_l2_cache_cores++;

					return cache;
				}
				else
				{
					VR_PRINT_ERROR(("VR L2 cache: Failed to create counter lock for L2 cache core %s\n", cache->hw_core.description));
				}

				_vr_osk_lock_term(cache->command_lock);
			}
			else
			{
				VR_PRINT_ERROR(("VR L2 cache: Failed to create command lock for L2 cache core %s\n", cache->hw_core.description));
			}

			vr_hw_core_delete(&cache->hw_core);
		}

		_vr_osk_free(cache);
	}
	else
	{
		VR_PRINT_ERROR(("VR L2 cache: Failed to allocate memory for L2 cache core\n"));
	}

	return NULL;
}

void vr_l2_cache_delete(struct vr_l2_cache_core *cache)
{
	u32 i;

	/* reset to defaults */
	vr_hw_core_register_write(&cache->hw_core, VR400_L2_CACHE_REGISTER_MAX_READS, (u32)VR400_L2_MAX_READS_DEFAULT);
	vr_hw_core_register_write(&cache->hw_core, VR400_L2_CACHE_REGISTER_ENABLE, (u32)VR400_L2_CACHE_ENABLE_DEFAULT);

	_vr_osk_lock_term(cache->counter_lock);
	_vr_osk_lock_term(cache->command_lock);
	vr_hw_core_delete(&cache->hw_core);

	for (i = 0; i < VR_MAX_NUMBER_OF_L2_CACHE_CORES; i++)
	{
		if (vr_global_l2_cache_cores[i] == cache)
		{
			vr_global_l2_cache_cores[i] = NULL;
			vr_global_num_l2_cache_cores--;
		}
	}

	_vr_osk_free(cache);
}

void vr_l2_cache_power_is_enabled_set(struct vr_l2_cache_core * core, vr_bool power_is_enabled)
{
       core->power_is_enabled = power_is_enabled;
}

vr_bool vr_l2_cache_power_is_enabled_get(struct vr_l2_cache_core * core)
{
       return core->power_is_enabled;
}

u32 vr_l2_cache_get_id(struct vr_l2_cache_core *cache)
{
	return cache->core_id;
}

vr_bool vr_l2_cache_core_set_counter_src0(struct vr_l2_cache_core *cache, u32 counter)
{
	u32 value = 0; /* disabled src */
	vr_bool core_is_on;

	VR_DEBUG_ASSERT_POINTER(cache);

	core_is_on = vr_l2_cache_lock_power_state(cache);

	_vr_osk_lock_wait(cache->counter_lock, _VR_OSK_LOCKMODE_RW);

	cache->counter_src0 = counter;

	if (VR_HW_CORE_NO_COUNTER != counter)
	{
		value = counter;
	}

	if (VR_TRUE == core_is_on)
	{
		vr_hw_core_register_write(&cache->hw_core, VR400_L2_CACHE_REGISTER_PERFCNT_SRC0, value);
	}

	_vr_osk_lock_signal(cache->counter_lock, _VR_OSK_LOCKMODE_RW);

	vr_l2_cache_unlock_power_state(cache);

	return VR_TRUE;
}

vr_bool vr_l2_cache_core_set_counter_src1(struct vr_l2_cache_core *cache, u32 counter)
{
	u32 value = 0; /* disabled src */
	vr_bool core_is_on;

	VR_DEBUG_ASSERT_POINTER(cache);

	core_is_on = vr_l2_cache_lock_power_state(cache);

	_vr_osk_lock_wait(cache->counter_lock, _VR_OSK_LOCKMODE_RW);

	cache->counter_src1 = counter;

	if (VR_HW_CORE_NO_COUNTER != counter)
	{
		value = counter;
	}

	if (VR_TRUE == core_is_on)
	{
		vr_hw_core_register_write(&cache->hw_core, VR400_L2_CACHE_REGISTER_PERFCNT_SRC1, value);
	}

	_vr_osk_lock_signal(cache->counter_lock, _VR_OSK_LOCKMODE_RW);

	vr_l2_cache_unlock_power_state(cache);

	return VR_TRUE;
}

u32 vr_l2_cache_core_get_counter_src0(struct vr_l2_cache_core *cache)
{
	return cache->counter_src0;
}

u32 vr_l2_cache_core_get_counter_src1(struct vr_l2_cache_core *cache)
{
	return cache->counter_src1;
}

void vr_l2_cache_core_get_counter_values(struct vr_l2_cache_core *cache, u32 *src0, u32 *value0, u32 *src1, u32 *value1)
{
	VR_DEBUG_ASSERT(NULL != src0);
	VR_DEBUG_ASSERT(NULL != value0);
	VR_DEBUG_ASSERT(NULL != src1);
	VR_DEBUG_ASSERT(NULL != value1);

	/* Caller must hold the PM lock and know that we are powered on */

	_vr_osk_lock_wait(cache->counter_lock, _VR_OSK_LOCKMODE_RW);

	*src0 = cache->counter_src0;
	*src1 = cache->counter_src1;

	if (cache->counter_src0 != VR_HW_CORE_NO_COUNTER)
	{
		*value0 = vr_hw_core_register_read(&cache->hw_core, VR400_L2_CACHE_REGISTER_PERFCNT_VAL0);
	}

	if (cache->counter_src1 != VR_HW_CORE_NO_COUNTER)
	{
		*value1 = vr_hw_core_register_read(&cache->hw_core, VR400_L2_CACHE_REGISTER_PERFCNT_VAL1);
	}

	_vr_osk_lock_signal(cache->counter_lock, _VR_OSK_LOCKMODE_RW);
}

struct vr_l2_cache_core *vr_l2_cache_core_get_glob_l2_core(u32 index)
{
	if (VR_MAX_NUMBER_OF_L2_CACHE_CORES > index)
	{
		return vr_global_l2_cache_cores[index];
	}

	return NULL;
}

u32 vr_l2_cache_core_get_glob_num_l2_cores(void)
{
	return vr_global_num_l2_cache_cores;
}

void vr_l2_cache_reset(struct vr_l2_cache_core *cache)
{
	/* Invalidate cache (just to keep it in a known state at startup) */
	vr_l2_cache_invalidate_all(cache);

	/* Enable cache */
	vr_hw_core_register_write(&cache->hw_core, VR400_L2_CACHE_REGISTER_ENABLE, (u32)VR400_L2_CACHE_ENABLE_ACCESS | (u32)VR400_L2_CACHE_ENABLE_READ_ALLOCATE);
	vr_hw_core_register_write(&cache->hw_core, VR400_L2_CACHE_REGISTER_MAX_READS, (u32)vr_l2_max_reads);

	/* Restart any performance counters (if enabled) */
	_vr_osk_lock_wait(cache->counter_lock, _VR_OSK_LOCKMODE_RW);

	if (cache->counter_src0 != VR_HW_CORE_NO_COUNTER)
	{
		vr_hw_core_register_write(&cache->hw_core, VR400_L2_CACHE_REGISTER_PERFCNT_SRC0, cache->counter_src0);
	}

	if (cache->counter_src1 != VR_HW_CORE_NO_COUNTER)
	{
		vr_hw_core_register_write(&cache->hw_core, VR400_L2_CACHE_REGISTER_PERFCNT_SRC1, cache->counter_src1);
	}

	_vr_osk_lock_signal(cache->counter_lock, _VR_OSK_LOCKMODE_RW);
}

void vr_l2_cache_reset_all(void)
{
	int i;
	u32 num_cores = vr_l2_cache_core_get_glob_num_l2_cores();

	for (i = 0; i < num_cores; i++)
	{
		vr_l2_cache_reset(vr_l2_cache_core_get_glob_l2_core(i));
	}
}

_vr_osk_errcode_t vr_l2_cache_invalidate_all(struct vr_l2_cache_core *cache)
{
	return vr_l2_cache_send_command(cache, VR400_L2_CACHE_REGISTER_COMMAND, VR400_L2_CACHE_COMMAND_CLEAR_ALL);
}

vr_bool vr_l2_cache_invalidate_all_conditional(struct vr_l2_cache_core *cache, u32 id)
{
       VR_DEBUG_ASSERT_POINTER(cache);

       if (NULL != cache)
       {
               /* If the last cache invalidation was done by a job with a higher id we
                * don't have to flush. Since user space will store jobs w/ their
                * corresponding memory in sequence (first job #0, then job #1, ...),
                * we don't have to flush for job n-1 if job n has already invalidated
                * the cache since we know for sure that job n-1's memory was already
                * written when job n was started. */
               if (((s32)id) <= ((s32)cache->last_invalidated_id))
               {
                       return VR_FALSE;
               }
               else
               {
                       cache->last_invalidated_id = vr_scheduler_get_new_id();
               }

               vr_l2_cache_invalidate_all(cache);
       }
       return VR_TRUE;
}

void vr_l2_cache_invalidate_all_force(struct vr_l2_cache_core *cache)
{
       VR_DEBUG_ASSERT_POINTER(cache);

       if (NULL != cache)
       {
               cache->last_invalidated_id = vr_scheduler_get_new_id();
               vr_l2_cache_invalidate_all(cache);
       }
}

_vr_osk_errcode_t vr_l2_cache_invalidate_pages(struct vr_l2_cache_core *cache, u32 *pages, u32 num_pages)
{
	u32 i;
	_vr_osk_errcode_t ret1, ret = _VR_OSK_ERR_OK;

	for (i = 0; i < num_pages; i++)
	{
		ret1 = vr_l2_cache_send_command(cache, VR400_L2_CACHE_REGISTER_CLEAR_PAGE, pages[i]);
		if (_VR_OSK_ERR_OK != ret1)
		{
			ret = ret1;
		}
	}

	return ret;
}

void vr_l2_cache_invalidate_pages_conditional(u32 *pages, u32 num_pages)
{
       u32 i;

       for (i = 0; i < vr_global_num_l2_cache_cores; i++)
       {
               /*additional check*/
               if (VR_TRUE == vr_l2_cache_lock_power_state(vr_global_l2_cache_cores[i]))
               {
                       vr_l2_cache_invalidate_pages(vr_global_l2_cache_cores[i], pages, num_pages);
               }
               vr_l2_cache_unlock_power_state(vr_global_l2_cache_cores[i]);
               /*check for failed power locking???*/
       }
}

vr_bool vr_l2_cache_lock_power_state(struct vr_l2_cache_core *cache)
{
	return _vr_osk_pm_dev_ref_add_no_power_on();
}

void vr_l2_cache_unlock_power_state(struct vr_l2_cache_core *cache)
{
	_vr_osk_pm_dev_ref_dec_no_power_on();
}

/* -------- local helper functions below -------- */


static _vr_osk_errcode_t vr_l2_cache_send_command(struct vr_l2_cache_core *cache, u32 reg, u32 val)
{
	int i = 0;
	const int loop_count = 100000;

	/*
	 * Grab lock in order to send commands to the L2 cache in a serialized fashion.
	 * The L2 cache will ignore commands if it is busy.
	 */
	_vr_osk_lock_wait(cache->command_lock, _VR_OSK_LOCKMODE_RW);

	/* First, wait for L2 cache command handler to go idle */

	for (i = 0; i < loop_count; i++)
	{
		if (!(vr_hw_core_register_read(&cache->hw_core, VR400_L2_CACHE_REGISTER_STATUS) & (u32)VR400_L2_CACHE_STATUS_COMMAND_BUSY))
		{
			break;
		}
	}

	if (i == loop_count)
	{
		_vr_osk_lock_signal(cache->command_lock, _VR_OSK_LOCKMODE_RW);
		VR_DEBUG_PRINT(1, ( "VR L2 cache: aborting wait for command interface to go idle\n"));
		VR_ERROR( _VR_OSK_ERR_FAULT );
	}

	/* then issue the command */
	vr_hw_core_register_write(&cache->hw_core, reg, val);

	_vr_osk_lock_signal(cache->command_lock, _VR_OSK_LOCKMODE_RW);

	VR_SUCCESS;
}
