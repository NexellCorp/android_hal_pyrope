/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_pmu.c
 * VR driver functions for VR 400 PMU hardware
 */
#include "vr_hw_core.h"
#include "vr_pmu.h"
#include "vr_pp.h"
#include "vr_kernel_common.h"
#include "vr_osk.h"

static u32 vr_pmu_detect_mask(u32 number_of_pp_cores, u32 number_of_l2_caches);

/** @brief VR inbuilt PMU hardware info and PMU hardware has knowledge of cores power mask
 */
struct vr_pmu_core
{
	struct vr_hw_core hw_core;
	u32 vr_registered_cores_power_mask;
};

static struct vr_pmu_core *vr_global_pmu_core = NULL;

/** @brief Register layout for hardware PMU
 */
typedef enum {
	PMU_REG_ADDR_MGMT_POWER_UP                  = 0x00,     /*< Power up register */
	PMU_REG_ADDR_MGMT_POWER_DOWN                = 0x04,     /*< Power down register */
	PMU_REG_ADDR_MGMT_STATUS                    = 0x08,     /*< Core sleep status register */
	PMU_REG_ADDR_MGMT_INT_MASK                  = 0x0C,     /*< Interrupt mask register */
	PMU_REGISTER_ADDRESS_SPACE_SIZE             = 0x10,     /*< Size of register space */
} pmu_reg_addr_mgmt_addr;

struct vr_pmu_core *vr_pmu_create(_vr_osk_resource_t *resource, u32 number_of_pp_cores, u32 number_of_l2_caches)
{
	struct vr_pmu_core* pmu;

	VR_DEBUG_ASSERT(NULL == vr_global_pmu_core);
	VR_DEBUG_PRINT(2, ("VR PMU: Creating VR PMU core\n"));

	pmu = (struct vr_pmu_core *)_vr_osk_malloc(sizeof(struct vr_pmu_core));
	if (NULL != pmu)
	{
		pmu->vr_registered_cores_power_mask = vr_pmu_detect_mask(number_of_pp_cores, number_of_l2_caches);
		if (_VR_OSK_ERR_OK == vr_hw_core_create(&pmu->hw_core, resource, PMU_REGISTER_ADDRESS_SPACE_SIZE))
		{
			if (_VR_OSK_ERR_OK == vr_pmu_reset(pmu))
			{
				vr_global_pmu_core = pmu;
				return pmu;
			}
			vr_hw_core_delete(&pmu->hw_core);
		}
		_vr_osk_free(pmu);
	}

	return NULL;
}

void vr_pmu_delete(struct vr_pmu_core *pmu)
{
	VR_DEBUG_ASSERT_POINTER(pmu);

	vr_hw_core_delete(&pmu->hw_core);
	_vr_osk_free(pmu);
	pmu = NULL;
}

_vr_osk_errcode_t vr_pmu_reset(struct vr_pmu_core *pmu)
{
	/* Don't use interrupts - just poll status */
	vr_hw_core_register_write(&pmu->hw_core, PMU_REG_ADDR_MGMT_INT_MASK, 0);
	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t vr_pmu_powerdown_all(struct vr_pmu_core *pmu)
{
	u32 stat;
	u32 timeout;

	VR_DEBUG_ASSERT_POINTER(pmu);
	VR_DEBUG_ASSERT( pmu->vr_registered_cores_power_mask != 0 );
	VR_DEBUG_PRINT( 4, ("VR PMU: power down (0x%08X)\n", pmu->vr_registered_cores_power_mask) );

	vr_hw_core_register_write(&pmu->hw_core, PMU_REG_ADDR_MGMT_POWER_DOWN, pmu->vr_registered_cores_power_mask);

	/* Wait for cores to be powered down (100 x 100us = 100ms) */
	timeout = VR_REG_POLL_COUNT_SLOW ;
	do
	{
		/* Get status of sleeping cores */
		stat = vr_hw_core_register_read(&pmu->hw_core, PMU_REG_ADDR_MGMT_STATUS);
		stat &= pmu->vr_registered_cores_power_mask;
		if( stat == pmu->vr_registered_cores_power_mask ) break; /* All cores we wanted are now asleep */
		timeout--;
	} while( timeout > 0 );

	if( timeout == 0 )
	{
		return _VR_OSK_ERR_TIMEOUT;
	}

	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t vr_pmu_powerup_all(struct vr_pmu_core *pmu)
{
	u32 stat;
	u32 timeout;
       
	VR_DEBUG_ASSERT_POINTER(pmu);
	VR_DEBUG_ASSERT( pmu->vr_registered_cores_power_mask != 0 ); /* Shouldn't be zero */
	VR_DEBUG_PRINT( 4, ("VR PMU: power up (0x%08X)\n", pmu->vr_registered_cores_power_mask) );

	vr_hw_core_register_write(&pmu->hw_core, PMU_REG_ADDR_MGMT_POWER_UP, pmu->vr_registered_cores_power_mask);

	/* Wait for cores to be powered up (100 x 100us = 100ms) */
	timeout = VR_REG_POLL_COUNT_SLOW;
	do
	{
		/* Get status of sleeping cores */
		stat = vr_hw_core_register_read(&pmu->hw_core,PMU_REG_ADDR_MGMT_STATUS);
		stat &= pmu->vr_registered_cores_power_mask;
		if ( stat == 0 ) break; /* All cores we wanted are now awake */
		timeout--;
	} while ( timeout > 0 );

	if ( timeout == 0 )
	{
		return _VR_OSK_ERR_TIMEOUT;
	}

	return _VR_OSK_ERR_OK;
}

struct vr_pmu_core *vr_pmu_get_global_pmu_core(void)
{
	return vr_global_pmu_core;
}

static u32 vr_pmu_detect_mask(u32 number_of_pp_cores, u32 number_of_l2_caches)
{
	u32 mask = 0;

	if (number_of_l2_caches == 1)
	{
		/* VR-300 or VR-400 */
		u32 i;

		/* GP */
		mask = 0x01;

		/* L2 cache */
		mask |= 0x01<<1;

		/* Set bit for each PP core */
		for (i = 0; i < number_of_pp_cores; i++)
		{
			mask |= 0x01<<(i+2);
		}
	}
	else if (number_of_l2_caches > 1)
	{
		/* VR-450 */

		/* GP (including its L2 cache) */
		mask = 0x01;

		/* There is always at least one PP (including its L2 cache) */
		mask |= 0x01<<1;

		/* Additional PP cores in same L2 cache */
		if (number_of_pp_cores >= 2)
		{
			mask |= 0x01<<2;
		}

		/* Additional PP cores in a third L2 cache */
		if (number_of_pp_cores >= 5)
		{
			mask |= 0x01<<3;
		}
	}

	VR_DEBUG_PRINT(4, ("VR PMU: Power mask is 0x%08X (%u + %u)\n", mask, number_of_pp_cores, number_of_l2_caches));

	return mask;
}
