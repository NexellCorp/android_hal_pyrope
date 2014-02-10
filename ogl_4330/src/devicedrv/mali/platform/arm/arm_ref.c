/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_platform.c
 * Platform specific VR driver functions for:
 * - Realview Versatile platforms with ARM11 Mpcore and virtex 5.
 * - Versatile Express platforms with ARM Cortex-A9 and virtex 6.
 */
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/vr/vr_utgard.h>
#include "vr_kernel_common.h"

//added by nexell
#error "must select right platform"

static void vr_platform_device_release(struct device *device);
static u32 vr_read_phys(u32 phys_addr);
#if defined(CONFIG_ARCH_REALVIEW)
static void vr_write_phys(u32 phys_addr, u32 value);
#endif

#if defined(CONFIG_ARCH_VEXPRESS)

static struct resource vr_gpu_resources_m450_mp8[] =
{
	VR_GPU_RESOURCES_VR450_MP8_PMU(0xFC040000, -1, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 68)
};

#elif defined(CONFIG_ARCH_REALVIEW)

static struct resource vr_gpu_resources_m200[] =
{
	VR_GPU_RESOURCES_VR200(0xC0000000, -1, -1, -1)
};

static struct resource vr_gpu_resources_m300[] =
{
	VR_GPU_RESOURCES_VR300_PMU(0xC0000000, -1, -1, -1, -1)
};

static struct resource vr_gpu_resources_m400_mp1[] =
{
	VR_GPU_RESOURCES_VR400_MP1_PMU(0xC0000000, -1, -1, -1, -1)
};

static struct resource vr_gpu_resources_m400_mp2[] =
{
	VR_GPU_RESOURCES_VR400_MP2_PMU(0xC0000000, -1, -1, -1, -1, -1, -1)
};

#endif

static struct platform_device vr_gpu_device =
{
	.name = VR_GPU_NAME_UTGARD,
	.id = 0,
	.dev.release = vr_platform_device_release,
};

static struct vr_gpu_device_data vr_gpu_data =
{
#if defined(CONFIG_ARCH_VEXPRESS)
	.shared_mem_size =256 * 1024 * 1024, /* 256MB */
#elif defined(CONFIG_ARCH_REALVIEW)
	.dedicated_mem_start = 0x80000000, /* Physical start address (use 0xD0000000 for old indirect setup) */
	.dedicated_mem_size = 0x10000000, /* 256MB */
#endif
	.fb_start = 0xe0000000,
	.fb_size = 0x01000000,
};

int vr_platform_device_register(void)
{
	int err = -1;
#if defined(CONFIG_ARCH_REALVIEW)
	u32 m400_gp_version;
#endif

	VR_DEBUG_PRINT(4, ("vr_platform_device_register() called\n"));

	/* Detect present VR GPU and connect the correct resources to the device */
#if defined(CONFIG_ARCH_VEXPRESS)

	if (vr_read_phys(0xFC020000) == 0x00010100)
	{
		VR_DEBUG_PRINT(4, ("Registering VR-450 MP8 device\n"));
		err = platform_device_add_resources(&vr_gpu_device, vr_gpu_resources_m450_mp8, sizeof(vr_gpu_resources_m450_mp8) / sizeof(vr_gpu_resources_m450_mp8[0]));
	}

#elif defined(CONFIG_ARCH_REALVIEW)

	m400_gp_version = vr_read_phys(0xC000006C);
	if (m400_gp_version == 0x00000000 && (vr_read_phys(0xC000206c) & 0xFFFF0000) == 0x0A070000)
	{
		VR_DEBUG_PRINT(4, ("Registering VR-200 device\n"));
		err = platform_device_add_resources(&vr_gpu_device, vr_gpu_resources_m200, sizeof(vr_gpu_resources_m200) / sizeof(vr_gpu_resources_m200[0]));
		vr_write_phys(0xC0010020, 0xA); /* Enable direct memory mapping for FPGA */
	}
	else if ((m400_gp_version & 0xFFFF0000) == 0x0C070000)
	{
		VR_DEBUG_PRINT(4, ("Registering VR-300 device\n"));
		err = platform_device_add_resources(&vr_gpu_device, vr_gpu_resources_m300, sizeof(vr_gpu_resources_m300) / sizeof(vr_gpu_resources_m300[0]));
		vr_write_phys(0xC0010020, 0xA); /* Enable direct memory mapping for FPGA */
	}
	else if ((m400_gp_version & 0xFFFF0000) == 0x0B070000)
	{
		u32 fpga_fw_version = vr_read_phys(0xC0010000);
		if (fpga_fw_version == 0x130C008F || fpga_fw_version == 0x110C008F)
		{
			/* VR-400 MP1 r1p0 or r1p1 */
			VR_DEBUG_PRINT(4, ("Registering VR-400 MP1 device\n"));
			err = platform_device_add_resources(&vr_gpu_device, vr_gpu_resources_m400_mp1, sizeof(vr_gpu_resources_m400_mp1) / sizeof(vr_gpu_resources_m400_mp1[0]));
			vr_write_phys(0xC0010020, 0xA); /* Enable direct memory mapping for FPGA */
		}
		else if (fpga_fw_version == 0x130C000F)
		{
			/* VR-400 MP2 r1p1 */
			VR_DEBUG_PRINT(4, ("Registering VR-400 MP2 device\n"));
			err = platform_device_add_resources(&vr_gpu_device, vr_gpu_resources_m400_mp2, sizeof(vr_gpu_resources_m400_mp2) / sizeof(vr_gpu_resources_m400_mp2[0]));
			vr_write_phys(0xC0010020, 0xA); /* Enable direct memory mapping for FPGA */
		}
	}

#endif

	if (0 == err)
	{
		err = platform_device_add_data(&vr_gpu_device, &vr_gpu_data, sizeof(vr_gpu_data));
		if (0 == err)
		{
			/* Register the platform device */
			err = platform_device_register(&vr_gpu_device);
			if (0 == err)
			{
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
				pm_runtime_set_autosuspend_delay(&(vr_gpu_device.dev), 1000);
				pm_runtime_use_autosuspend(&(vr_gpu_device.dev));
#endif
				pm_runtime_enable(&(vr_gpu_device.dev));
#endif

				return 0;
			}
		}

		platform_device_unregister(&vr_gpu_device);
	}

	return err;
}

void vr_platform_device_unregister(void)
{
	VR_DEBUG_PRINT(4, ("vr_platform_device_unregister() called\n"));

	platform_device_unregister(&vr_gpu_device);

#if defined(CONFIG_ARCH_REALVIEW)
	vr_write_phys(0xC0010020, 0x9); /* Restore default (legacy) memory mapping */
#endif
}

static void vr_platform_device_release(struct device *device)
{
	VR_DEBUG_PRINT(4, ("vr_platform_device_release() called\n"));
}

static u32 vr_read_phys(u32 phys_addr)
{
	u32 phys_addr_page = phys_addr & 0xFFFFE000;
	u32 phys_offset    = phys_addr & 0x00001FFF;
	u32 map_size       = phys_offset + sizeof(u32);
	u32 ret = 0xDEADBEEF;
	void *mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		ret = (u32)ioread32(((u8*)mem_mapped) + phys_offset);
		iounmap(mem_mapped);
	}

	return ret;
}

#if defined(CONFIG_ARCH_REALVIEW)
static void vr_write_phys(u32 phys_addr, u32 value)
{
	u32 phys_addr_page = phys_addr & 0xFFFFE000;
	u32 phys_offset    = phys_addr & 0x00001FFF;
	u32 map_size       = phys_offset + sizeof(u32);
	void *mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		iowrite32(value, ((u8*)mem_mapped) + phys_offset);
		iounmap(mem_mapped);
	}
}
#endif
