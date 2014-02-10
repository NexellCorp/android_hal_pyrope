/**
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_kernel_linux.c
 * Implementation of the Linux device driver entrypoints
 */
#include <linux/module.h>   /* kernel module definitions */
#include <linux/fs.h>       /* file system operations */
#include <linux/cdev.h>     /* character device definitions */
#include <linux/mm.h>       /* memory manager definitions */
#include <linux/vr/vr_utgard_ioctl.h>
#include <linux/version.h>
#include <linux/device.h>
#include "vr_kernel_license.h"
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/vr/vr_utgard.h>
#include "vr_kernel_common.h"
#include "vr_session.h"
#include "vr_kernel_core.h"
#include "vr_osk.h"
#include "vr_kernel_linux.h"
#include "vr_ukk.h"
#include "vr_ukk_wrappers.h"
#include "vr_kernel_sysfs.h"
#include "vr_pm.h"
#include "vr_kernel_license.h"
#include "vr_dma_buf.h"
#if defined(CONFIG_VR400_INTERNAL_PROFILING)
#include "vr_profiling_internal.h"
#endif

/* Streamline support for the VR driver */
#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_VR400_PROFILING)
/* Ask Linux to create the tracepoints */
#define CREATE_TRACE_POINTS
#include "vr_linux_trace.h"
#endif /* CONFIG_TRACEPOINTS */

/* from the __vrdrv_build_info.c file that is generated during build */
extern const char *__vrdrv_build_info(void);

/* Module parameter to control log level */
int vr_debug_level = 2;
module_param(vr_debug_level, int, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH); /* rw-rw-r-- */
MODULE_PARM_DESC(vr_debug_level, "Higher number, more dmesg output");

module_param(vr_max_job_runtime, int, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(vr_max_job_runtime, "Maximum allowed job runtime in msecs.\nJobs will be killed after this no matter what");

extern int vr_l2_max_reads;
module_param(vr_l2_max_reads, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(vr_l2_max_reads, "Maximum reads for VR L2 cache");

extern int vr_dedicated_mem_start;
module_param(vr_dedicated_mem_start, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(vr_dedicated_mem_start, "Physical start address of dedicated VR GPU memory.");

extern int vr_dedicated_mem_size;
module_param(vr_dedicated_mem_size, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(vr_dedicated_mem_size, "Size of dedicated VR GPU memory.");

extern int vr_shared_mem_size;
module_param(vr_shared_mem_size, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(vr_shared_mem_size, "Size of shared VR GPU memory.");

#if defined(CONFIG_VR400_PROFILING)
extern int vr_boot_profiling;
module_param(vr_boot_profiling, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(vr_boot_profiling, "Start profiling as a part of VR driver initialization");
#endif

extern int vr_max_pp_cores_group_1;
module_param(vr_max_pp_cores_group_1, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(vr_max_pp_cores_group_1, "Limit the number of PP cores to use from first PP group.");

extern int vr_max_pp_cores_group_2;
module_param(vr_max_pp_cores_group_2, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(vr_max_pp_cores_group_2, "Limit the number of PP cores to use from second PP group (VR-450 only).");

/* Export symbols from common code: vr_user_settings.c */
#include "vr_user_settings_db.h"
EXPORT_SYMBOL(vr_set_user_setting);
EXPORT_SYMBOL(vr_get_user_setting);

static char vr_dev_name[] = "vr"; /* should be const, but the functions we call requires non-cost */

/* This driver only supports one VR device, and this variable stores this single platform device */
struct platform_device *vr_platform_device = NULL;

/* This driver only supports one VR device, and this variable stores the exposed misc device (/dev/vr) */
static struct miscdevice vr_miscdevice = { 0, };

static int vr_miscdevice_register(struct platform_device *pdev);
static void vr_miscdevice_unregister(void);

static int vr_open(struct inode *inode, struct file *filp);
static int vr_release(struct inode *inode, struct file *filp);
#ifdef HAVE_UNLOCKED_IOCTL
static long vr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
static int vr_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static int vr_mmap(struct file * filp, struct vm_area_struct * vma);

static int vr_probe(struct platform_device *pdev);
static int vr_remove(struct platform_device *pdev);

static int vr_driver_suspend_scheduler(struct device *dev);
static int vr_driver_resume_scheduler(struct device *dev);

#ifdef CONFIG_PM_RUNTIME
static int vr_driver_runtime_suspend(struct device *dev);
static int vr_driver_runtime_resume(struct device *dev);
static int vr_driver_runtime_idle(struct device *dev);
#endif

#if defined(VR_FAKE_PLATFORM_DEVICE)
extern int vr_platform_device_register(void);
extern int vr_platform_device_unregister(void);
#endif

/* Linux power management operations provided by the VR device driver */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29))
struct pm_ext_ops vr_dev_ext_pm_ops =
{
	.base =
	{
		.suspend = vr_driver_suspend_scheduler,
		.resume = vr_driver_resume_scheduler,
		.freeze = vr_driver_suspend_scheduler,
		.thaw =   vr_driver_resume_scheduler,
	},
};
#else
static const struct dev_pm_ops vr_dev_pm_ops =
{
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = vr_driver_runtime_suspend,
	.runtime_resume = vr_driver_runtime_resume,
	.runtime_idle = vr_driver_runtime_idle,
#endif
	.suspend = vr_driver_suspend_scheduler,
	.resume = vr_driver_resume_scheduler,
	.freeze = vr_driver_suspend_scheduler,
	.thaw = vr_driver_resume_scheduler,
};
#endif

/* The VR device driver struct */
static struct platform_driver vr_platform_driver =
{
	.probe  = vr_probe,
	.remove = vr_remove,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29))
	.pm = &vr_dev_ext_pm_ops,
#endif
	.driver =
	{
		.name   = VR_GPU_NAME_UTGARD,
		.owner  = THIS_MODULE,
		.bus = &platform_bus_type,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
		.pm = &vr_dev_pm_ops,
#endif
	},
};

/* Linux misc device operations (/dev/vr) */
struct file_operations vr_fops =
{
	.owner = THIS_MODULE,
	.open = vr_open,
	.release = vr_release,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl = vr_ioctl,
#else
	.ioctl = vr_ioctl,
#endif
	.mmap = vr_mmap
};



//added by nexell
#include <asm/io.h>
#include <linux/clk.h>
#include <linux/delay.h>	/* mdelay */

#include <asm/mach/time.h>
#include <asm/mach/irq.h>
#include <asm/smp_twd.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#define PHY_BASEADDR_VR				(0xC0070000)
#define PHY_BASEADDR_VR_GP			(0xC0070000 + 0x0)
#define PHY_BASEADDR_VR_PMU			(0xC0070000 + 0x2000)
#define PHY_BASEADDR_VR_MMU_GP		(0xC0070000 + 0x3000)
#define PHY_BASEADDR_VR_MMU_PP0		(0xC0070000 + 0x4000)
#define PHY_BASEADDR_VR_MMU_PP1		(0xC0070000 + 0x5000)
#define PHY_BASEADDR_VR_PP0			(0xC0070000 + 0x8000)
#define PHY_BASEADDR_VR_PP1			(0xC0070000 + 0xA000)
#define PHY_BASEADDR_PMU_ISOLATE	(0xC0010D00)
#define PHY_BASEADDR_POWER_GATE		(0xC0010800)
#define PHY_BASEADDR_CLOCK_GATE		(0xC00C3000)
#define PHY_BASEADDR_RESET  		(0xC0012000)

#define POWER_DELAY_MS	100
#if 0
#define VR_DBG printk
#define VR_PM_DBG PM_DBGOUT
#define VR_IOCTL_DBG printk
#else
#define VR_DBG 
#define VR_PM_DBG PM_DBGOUT
#define VR_IOCTL_DBG 
#endif

static unsigned int gTestWaitCnt, gTestGPCnt, gTestPPCnt;
void dbg_info(void)
{
	u32 phys_addr_page, map_size;
	#if 0
	{
		void *mem_mapped;
		phys_addr_page = 0xC0003000;
		map_size       = 0x30;
		mem_mapped = ioremap_nocache(phys_addr_page, map_size);
		VR_DBG(" ===> nxp43330 read reg\n");			
		if (NULL != mem_mapped)
		{
			int i = 0;
			for(i = 0 ; i < 12 ; i++)
			{
				unsigned int read_val = ioread32((u8*)mem_mapped + (i*4));	
				VR_DBG("nxp43330 read reg addr(0x%x), data(0x%x)\n", (int)mem_mapped + (i*4), read_val);			
			}
			iounmap(mem_mapped);
		}	
	}
	#endif

	{
		void *mem_mapped;
		phys_addr_page = PHY_BASEADDR_VR_GP+0x20;
		map_size	   = 0x20;
		mem_mapped = ioremap_nocache(phys_addr_page, map_size);
		if (NULL != mem_mapped)
		{
			unsigned int read_val_raw, read_val_status;
			read_val_raw = ioread32((u8*)mem_mapped + 0x4);	
			read_val_status = ioread32((u8*)mem_mapped + 0x10);				
			VR_DBG("GP read reg raw(0x%x), status(0x%x)\n", read_val_raw, read_val_status);			
			iounmap(mem_mapped);
		}	
	}
	{
		void *mem_mapped;
		phys_addr_page = PHY_BASEADDR_VR_PMU+0x10;
		map_size	   = 0x20;
		mem_mapped = ioremap_nocache(phys_addr_page, map_size);
		if (NULL != mem_mapped)
		{
			unsigned int read_val_raw, read_val_status;
			read_val_raw = ioread32((u8*)mem_mapped);	
			read_val_status = ioread32((u8*)mem_mapped + 0x4);				
			VR_DBG("PMU read reg raw(0x%x), status(0x%x)\n", read_val_raw, read_val_status);			
			iounmap(mem_mapped);
		}	
	}
	{
		void *mem_mapped;
		phys_addr_page = PHY_BASEADDR_VR_MMU_GP+0x10;
		map_size	   = 0x20;
		mem_mapped = ioremap_nocache(phys_addr_page, map_size);
		if (NULL != mem_mapped)
		{
			unsigned int read_val_raw, read_val_status;
			read_val_raw = ioread32((u8*)mem_mapped + 0x4);	
			read_val_status = ioread32((u8*)mem_mapped + 0x10);				
			VR_DBG("MMU_GP read reg raw(0x%x), status(0x%x)\n", read_val_raw, read_val_status);			
			iounmap(mem_mapped);
		}	
	}
	{
		void *mem_mapped;
		phys_addr_page = PHY_BASEADDR_VR_MMU_PP0+0x10;
		map_size	   = 0x20;
		mem_mapped = ioremap_nocache(phys_addr_page, map_size);
		if (NULL != mem_mapped)
		{
			unsigned int read_val_raw, read_val_status;
			read_val_raw = ioread32((u8*)mem_mapped + 0x4);	
			read_val_status = ioread32((u8*)mem_mapped + 0x10);				
			VR_DBG("MMU_PP0 read reg raw(0x%x), status(0x%x)\n", read_val_raw, read_val_status);			
			iounmap(mem_mapped);
		}	
	}	
	{
		void *mem_mapped;
		phys_addr_page = PHY_BASEADDR_VR_MMU_PP1+0x10;
		map_size	   = 0x20;
		mem_mapped = ioremap_nocache(phys_addr_page, map_size);
		if (NULL != mem_mapped)
		{
			unsigned int read_val_raw, read_val_status;
			read_val_raw = ioread32((u8*)mem_mapped + 0x4);	
			read_val_status = ioread32((u8*)mem_mapped + 0x10);				
			VR_DBG("MMU_PP1 read reg raw(0x%x), status(0x%x)\n", read_val_raw, read_val_status);			
			iounmap(mem_mapped);
		}	
	}	
	{
		void *mem_mapped;
		phys_addr_page = PHY_BASEADDR_VR_PP0+0x1020;
		map_size	   = 0x20;
		mem_mapped = ioremap_nocache(phys_addr_page, map_size);
		if (NULL != mem_mapped)
		{
			unsigned int read_val_raw, read_val_status;
			read_val_raw = ioread32((u8*)mem_mapped);	
			read_val_status = ioread32((u8*)mem_mapped + 0xC);				
			VR_DBG("PP0 read reg raw(0x%x), status(0x%x)\n", read_val_raw, read_val_status);			
			iounmap(mem_mapped);
		}	
	}	
	{
		void *mem_mapped;
		phys_addr_page = PHY_BASEADDR_VR_PP1+0x1020;
		map_size	   = 0x20;
		mem_mapped = ioremap_nocache(phys_addr_page, map_size);
		if (NULL != mem_mapped)
		{
			unsigned int read_val_raw, read_val_status;
			read_val_raw = ioread32((u8*)mem_mapped);	
			read_val_status = ioread32((u8*)mem_mapped + 0xC);				
			VR_DBG("PP1 read reg raw(0x%x), status(0x%x)\n", read_val_raw, read_val_status);			
			iounmap(mem_mapped);
		}	
	}		
}

static void nx_vr_power_down_all(void)
{
	u32 phys_addr_page, map_size;
	void *mem_mapped;

	//temp test
#if 0
	//clear vr400 pmu interrupt
	phys_addr_page = PHY_BASEADDR_VR_PMU + 0x18;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		VR_PM_DBG("clear PMU int, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}	
#endif
#if 0
	//clear NXP4330 interrupt controller
	phys_addr_page = 0xC0003014;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		VR_PM_DBG("clear INTCLEAR, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0x100, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
#else
#endif

	//clk disalbe
	phys_addr_page = PHY_BASEADDR_CLOCK_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);		
		VR_DBG("setting ClockGen, set 0\n");
		iowrite32(read_val & ~0x3, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
	
	//ready
	phys_addr_page = PHY_BASEADDR_POWER_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		VR_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
	
	//enable ISolate
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);		
		VR_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 0\n");
		iowrite32(read_val & ~1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}

	//pre charge down
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 4;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);		
		VR_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
	mdelay(1);

#if 1
	//powerdown
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 8;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);		
		VR_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
	mdelay(1);
	
	//wait ack
	VR_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 0xC;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	do{
		unsigned int powerUpAck = ioread32((u8*)mem_mapped);
		VR_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( (powerUpAck & 0x1) )
			break;
		VR_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);	
#endif	
}

static void nx_vr_power_up_all_first(void)
{
	u32 phys_addr_page, map_size;
	void *mem_mapped;

	//ready
	phys_addr_page = PHY_BASEADDR_POWER_GATE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		VR_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
	
	//pre charge up
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 4;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		VR_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}

	//power up
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 8;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		VR_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
	mdelay(1);

	//disable ISolate
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);		
		VR_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}	
	mdelay(1);
	
	//wait ack
	VR_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 0xC;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	do{
		unsigned int powerUpAck = ioread32((u8*)mem_mapped);
		VR_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( !(powerUpAck & 0x1) )
			break;
		VR_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);

	//clk enable
	phys_addr_page = PHY_BASEADDR_CLOCK_GATE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);	
		VR_DBG("setting ClockGen, set 1\n");
		iowrite32(0x3 | read_val, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}	
}

static void nx_vr_power_up_all_second(void)
{
	u32 phys_addr_page, map_size;
	void *mem_mapped;

#if 1
	phys_addr_page = PHY_BASEADDR_RESET + 8;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size); 
	if (NULL != mem_mapped)
	{
		unsigned int temp32 = ioread32(((u8*)mem_mapped));
		unsigned int bit_mask = 1<<1; //65th
		VR_DBG("setting Reset VR addr(0x%x)\n", (int)mem_mapped);

		temp32 &= ~bit_mask;
		iowrite32(temp32, ((u8*)mem_mapped));
		temp32 |= bit_mask;
		iowrite32(temp32, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
#endif
	mdelay(1);

	//mask vr400 PMU interrupt 
	phys_addr_page = PHY_BASEADDR_VR_PMU + 0xC;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		VR_PM_DBG("mask PMU INT, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0x0, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}	

	//power up vr400
	phys_addr_page = PHY_BASEADDR_VR_PMU;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		VR_DBG("setting PHY_BASEADDR_VR_PMU addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0xF/*GP, L2C, PP0, PP1*/, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}

	//ocurr ERROR ! at 10ms delay
	VR_PM_DBG("mdelay 20ms\n");
	mdelay(20);

#if 0
	//mask vr400 GP2 AXI_BUS_ERROR
	phys_addr_page = PHY_BASEADDR_VR_GP + 0x2C;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int temp32 = ioread32(((u8*)mem_mapped));
		VR_PM_DBG("mask GP2 BUSERR_INT, addr(0x%x, 0x%x)\n", (int)mem_mapped, temp32);
		iounmap(mem_mapped);
	}

	//temp test
	VR_PM_DBG("\n============================\n", (int)mem_mapped);
	//clear vr400 pmu interrupt
	//PHY_BASEADDR_RESET + 8 에 reset 해줘야만 한다
	phys_addr_page = PHY_BASEADDR_VR_PMU + 0x18;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		VR_PM_DBG("clear PMU int, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}	
#endif
#if 0
	//clear NXP4330 interrupt controller
	phys_addr_page = 0xC0003014;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		VR_PM_DBG("clear INTCLEAR, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0x100, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}	
	VR_PM_DBG("============================\n", (int)mem_mapped);
#else
#endif

	#if 0
	//temp test
	phys_addr_page = 0xC0010000;
	map_size       = 0x30;
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		int i = 0;
		for(i = 0 ; i < 12 ; i++)
		{
			unsigned int read_val = ioread32((u8*)mem_mapped + (i*4));	
			VR_DBG("read reg addr(0x%x), data(0x%x)\n", (int)mem_mapped + (i*4), read_val);			
		}
		iounmap(mem_mapped);
	}	
	phys_addr_page = 0xC0010D00;
	map_size       = 0x10;
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		int i = 0;
		for(i = 0 ; i < 4 ; i++)
		{
			unsigned int read_val = ioread32((u8*)mem_mapped + (i*4));	
			VR_DBG("read reg addr(0x%x), data(0x%x)\n", (int)mem_mapped + (i*4), read_val);			
		}
		iounmap(mem_mapped);
	}	
	#endif
}

static void nx_vr_power_up_all_at_insmod(void)
{
	u32 phys_addr_page, map_size;
	void *mem_mapped;

	nx_vr_power_up_all_first();	
	nx_vr_power_up_all_second();	
	
	#if 0
	//temp test
	phys_addr_page = 0xC0050428;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	VR_DBG("0xC0050428 start(0x%x)\n", (int)mem_mapped);
	if (NULL != mem_mapped)
	{
		iowrite32(0x08000000, ((u8*)mem_mapped));
		VR_DBG("-----1------\n", (int)mem_mapped);
		iowrite32(0x09000001, ((u8*)mem_mapped));
		VR_DBG("-----2------\n", (int)mem_mapped);
		//iowrite32(0x01000002, ((u8*)mem_mapped));
		//VR_DBG("-----3------\n", (int)mem_mapped);
		iounmap(mem_mapped);
	}
	
	phys_addr_page = 0xC005042C;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	VR_DBG("0xC005042C start(0x%x)\n", (int)mem_mapped);
	if (NULL != mem_mapped)
	{
		iowrite32(0x08000000, ((u8*)mem_mapped));
		VR_DBG("-----1------\n", (int)mem_mapped);
		iowrite32(0x09000001, ((u8*)mem_mapped));
		VR_DBG("-----2------\n", (int)mem_mapped);
		//iowrite32(0x2, ((u8*)mem_mapped));
		//VR_DBG("-----3------\n", (int)mem_mapped);
		iounmap(mem_mapped);
	}
	#endif
}

int vr_module_init(void)
{
	int err = 0;
	
	//added by nexell
	nx_vr_power_up_all_at_insmod();
	vr_pmu_powerup();

	VR_DEBUG_PRINT(2, ("Inserting VR v%d device driver. \n",_VR_API_VERSION));
	VR_DEBUG_PRINT(2, ("Compiled: %s, time: %s.\n", __DATE__, __TIME__));
	VR_DEBUG_PRINT(2, ("Driver revision: %s\n", SVN_REV_STRING));

	/* Initialize module wide settings */
	vr_osk_low_level_mem_init();

#if defined(VR_FAKE_PLATFORM_DEVICE)
	VR_DEBUG_PRINT(2, ("vr_module_init() registering device\n"));
	err = vr_platform_device_register();
	if (0 != err)
	{
		return err;
	}
#endif

	VR_DEBUG_PRINT(2, ("vr_module_init() registering driver\n"));

	err = platform_driver_register(&vr_platform_driver);

	if (0 != err)
	{
		VR_DEBUG_PRINT(2, ("vr_module_init() Failed to register driver (%d)\n", err));
#if defined(VR_FAKE_PLATFORM_DEVICE)
		vr_platform_device_unregister();
#endif
		vr_platform_device = NULL;
		return err;
	}

#if defined(CONFIG_VR400_INTERNAL_PROFILING)
        err = _vr_internal_profiling_init(vr_boot_profiling ? VR_TRUE : VR_FALSE);
        if (0 != err)
        {
                /* No biggie if we wheren't able to initialize the profiling */
                VR_PRINT_ERROR(("Failed to initialize profiling, feature will be unavailable\n"));
        }
#endif

	VR_PRINT(("VR device driver loaded(ver1.0)\n"));


	return 0; /* Success */
}

void vr_module_exit(void)
{
	VR_DEBUG_PRINT(2, ("Unloading VR v%d device driver.\n",_VR_API_VERSION));

	VR_DEBUG_PRINT(2, ("vr_module_exit() unregistering driver\n"));

#if defined(CONFIG_VR400_INTERNAL_PROFILING)
        _vr_internal_profiling_term();
#endif

	platform_driver_unregister(&vr_platform_driver);

	//added by nexell
	//vr_pmu_powerdown();

#if defined(VR_FAKE_PLATFORM_DEVICE)
	VR_DEBUG_PRINT(2, ("vr_module_exit() unregistering device\n"));
	vr_platform_device_unregister();
#endif

	vr_osk_low_level_mem_term();

	//added by nexell
	nx_vr_power_down_all();

	VR_PRINT(("VR device driver unloaded\n"));
}

static int vr_probe(struct platform_device *pdev)
{
	int err;

	VR_DEBUG_PRINT(2, ("vr_probe(): Called for platform device %s\n", pdev->name));

	if (NULL != vr_platform_device)
	{
		/* Already connected to a device, return error */
		VR_PRINT_ERROR(("vr_probe(): The VR driver is already connected with a VR device."));
		return -EEXIST;
	}

	vr_platform_device = pdev;

	if (_VR_OSK_ERR_OK == _vr_osk_wq_init())
	{
		/* Initialize the VR GPU HW specified by pdev */
		if (_VR_OSK_ERR_OK == vr_initialize_subsystems())
		{
			/* Register a misc device (so we are accessible from user space) */
			err = vr_miscdevice_register(pdev);
			if (0 == err)
			{
				/* Setup sysfs entries */
				err = vr_sysfs_register(vr_dev_name);
				if (0 == err)
				{
					VR_DEBUG_PRINT(2, ("vr_probe(): Successfully initialized driver for platform device %s\n", pdev->name));
					return 0;
				}
				else
				{
					VR_PRINT_ERROR(("vr_probe(): failed to register sysfs entries"));
				}
				vr_miscdevice_unregister();
			}
			else
			{
				VR_PRINT_ERROR(("vr_probe(): failed to register VR misc device."));
			}
			vr_terminate_subsystems();
		}
		else
		{
			VR_PRINT_ERROR(("vr_probe(): Failed to initialize VR device driver."));
		}
		_vr_osk_wq_term();
	}

	vr_platform_device = NULL;
	return -EFAULT;
}

static int vr_remove(struct platform_device *pdev)
{
	VR_DEBUG_PRINT(2, ("vr_remove() called for platform device %s\n", pdev->name));
	vr_sysfs_unregister();
	vr_miscdevice_unregister();
	vr_terminate_subsystems();
	_vr_osk_wq_term();
	vr_platform_device = NULL;
	return 0;
}

static int vr_miscdevice_register(struct platform_device *pdev)
{
	int err;

	vr_miscdevice.minor = MISC_DYNAMIC_MINOR;
	vr_miscdevice.name = vr_dev_name;
	vr_miscdevice.fops = &vr_fops;
	vr_miscdevice.parent = get_device(&pdev->dev);

	err = misc_register(&vr_miscdevice);
	if (0 != err)
	{
		VR_PRINT_ERROR(("Failed to register misc device, misc_register() returned %d\n", err));
	}

	return err;
}

static void vr_miscdevice_unregister(void)
{
	misc_deregister(&vr_miscdevice);
}
static int vr_driver_suspend_scheduler(struct device *dev)
{
	VR_PM_DBG("-----------------------------------------------------\n");
	VR_PM_DBG("	VR POWERDown Start\n");

	vr_pm_os_suspend();
	nx_vr_power_down_all();
	
	VR_PM_DBG("	VR POWERDown End\n");
	VR_PM_DBG("-----------------------------------------------------\n");
	return 0;
}

static int vr_driver_resume_scheduler(struct device *dev)
{
	VR_PM_DBG("-----------------------------------------------------\n");
	VR_PM_DBG("	VR POWERUp Start\n");

	nx_vr_power_up_all_first();
	nx_vr_power_up_all_second();
	vr_pm_os_resume();

	VR_PM_DBG("	VR POWERUp End\n");
	VR_PM_DBG("-----------------------------------------------------\n");	
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int vr_driver_runtime_suspend(struct device *dev)
{
	VR_PM_DBG("-----------------------------------------------------\n");
	VR_PM_DBG("	VR POWERDown Start(CONFIG_PM_RUNTIME)\n");

	vr_pm_runtime_suspend();
	//nx_vr_power_down_all();
	
	VR_PM_DBG("	VR POWERDown End(CONFIG_PM_RUNTIME)\n");
	VR_PM_DBG("-----------------------------------------------------\n");
	return 0;
}

static int vr_driver_runtime_resume(struct device *dev)
{
	VR_PM_DBG("-----------------------------------------------------\n");
	VR_PM_DBG("	VR POWERUp Start(CONFIG_PM_RUNTIME)\n");

	//nx_vr_power_up_all_first();
	//nx_vr_power_up_all_second();
	vr_pm_runtime_resume();
	
	VR_PM_DBG("	VR POWERUp End(CONFIG_PM_RUNTIME)\n");
	VR_PM_DBG("-----------------------------------------------------\n");
	return 0;
}

static int vr_driver_runtime_idle(struct device *dev)
{
	/* Nothing to do */
	return 0;
}
#endif

/** @note munmap handler is done by vma close handler */
static int vr_mmap(struct file * filp, struct vm_area_struct * vma)
{
	struct vr_session_data * session_data;
	_vr_uk_mem_mmap_s args = {0, };

    session_data = (struct vr_session_data *)filp->private_data;
	if (NULL == session_data)
	{
		VR_PRINT_ERROR(("mmap called without any session data available\n"));
		return -EFAULT;
	}

	VR_DEBUG_PRINT(4, ("MMap() handler: start=0x%08X, phys=0x%08X, size=0x%08X vma->flags 0x%08x\n", (unsigned int)vma->vm_start, (unsigned int)(vma->vm_pgoff << PAGE_SHIFT), (unsigned int)(vma->vm_end - vma->vm_start), vma->vm_flags));

	/* Re-pack the arguments that mmap() packed for us */
	args.ctx = session_data;
	args.phys_addr = vma->vm_pgoff << PAGE_SHIFT;
	args.size = vma->vm_end - vma->vm_start;
	args.ukk_private = vma;

	if ( VM_SHARED== (VM_SHARED  & vma->vm_flags))
	{
		args.cache_settings = VR_CACHE_STANDARD ;
		VR_DEBUG_PRINT(3,("Allocate - Standard - Size: %d kb\n", args.size/1024));
	}
	else
	{
		args.cache_settings = VR_CACHE_GP_READ_ALLOCATE;
		VR_DEBUG_PRINT(3,("Allocate - GP Cached - Size: %d kb\n", args.size/1024));
	}
	/* Setting it equal to VM_SHARED and not Private, which would have made the later io_remap fail for VR_CACHE_GP_READ_ALLOCATE */
	vma->vm_flags = 0x000000fb;

	/* Call the common mmap handler */
	VR_CHECK(_VR_OSK_ERR_OK ==_vr_ukk_mem_mmap( &args ), -EFAULT);

    return 0;
}

static int vr_open(struct inode *inode, struct file *filp)
{
	struct vr_session_data * session_data;
    _vr_osk_errcode_t err;

	/* input validation */
	if (vr_miscdevice.minor != iminor(inode))
	{
		VR_PRINT_ERROR(("vr_open() Minor does not match\n"));
		return -ENODEV;
	}

	/* allocated struct to track this session */
    err = _vr_ukk_open((void **)&session_data);
    if (_VR_OSK_ERR_OK != err) return map_errcode(err);

	/* initialize file pointer */
	filp->f_pos = 0;

	/* link in our session data */
	filp->private_data = (void*)session_data;

	return 0;
}

static int vr_release(struct inode *inode, struct file *filp)
{
    _vr_osk_errcode_t err;

	/* input validation */
	if (vr_miscdevice.minor != iminor(inode))
	{
		VR_PRINT_ERROR(("vr_release() Minor does not match\n"));
		return -ENODEV;
	}

    err = _vr_ukk_close((void **)&filp->private_data);
    if (_VR_OSK_ERR_OK != err) return map_errcode(err);

	return 0;
}

int map_errcode( _vr_osk_errcode_t err )
{
    switch(err)
    {
        case _VR_OSK_ERR_OK : return 0;
        case _VR_OSK_ERR_FAULT: return -EFAULT;
        case _VR_OSK_ERR_INVALID_FUNC: return -ENOTTY;
        case _VR_OSK_ERR_INVALID_ARGS: return -EINVAL;
        case _VR_OSK_ERR_NOMEM: return -ENOMEM;
        case _VR_OSK_ERR_TIMEOUT: return -ETIMEDOUT;
        case _VR_OSK_ERR_RESTARTSYSCALL: return -ERESTARTSYS;
        case _VR_OSK_ERR_ITEM_NOT_FOUND: return -ENOENT;
        default: return -EFAULT;
    }
}

#ifdef HAVE_UNLOCKED_IOCTL
static long vr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int vr_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	int err;
	struct vr_session_data *session_data;

#ifndef HAVE_UNLOCKED_IOCTL
	/* inode not used */
	(void)inode;
#endif

	VR_DEBUG_PRINT(7, ("Ioctl received 0x%08X 0x%08lX\n", cmd, arg));

	session_data = (struct vr_session_data *)filp->private_data;
	if (NULL == session_data)
	{
		VR_DEBUG_PRINT(7, ("filp->private_data was NULL\n"));
		return -ENOTTY;
	}

	if (NULL == (void *)arg)
	{
		VR_DEBUG_PRINT(7, ("arg was NULL\n"));
		return -ENOTTY;
	}

	switch(cmd)
	{
		case VR_IOC_WAIT_FOR_NOTIFICATION:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_WAIT_FOR_NOTIFICATION\n");	
			err = wait_for_notification_wrapper(session_data, (_vr_uk_wait_for_notification_s __user *)arg);
			break;

		case VR_IOC_GET_API_VERSION:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_GET_API_VERSION\n");	
			err = get_api_version_wrapper(session_data, (_vr_uk_get_api_version_s __user *)arg);
			break;

		case VR_IOC_POST_NOTIFICATION:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_POST_NOTIFICATION\n");	
			err = post_notification_wrapper(session_data, (_vr_uk_post_notification_s __user *)arg);
			break;

		case VR_IOC_GET_USER_SETTINGS:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_GET_USER_SETTINGS\n");	
			err = get_user_settings_wrapper(session_data, (_vr_uk_get_user_settings_s __user *)arg);
			break;

#if defined(CONFIG_VR400_PROFILING)
		case VR_IOC_PROFILING_START:
			err = profiling_start_wrapper(session_data, (_vr_uk_profiling_start_s __user *)arg);
			break;

		case VR_IOC_PROFILING_ADD_EVENT:
			err = profiling_add_event_wrapper(session_data, (_vr_uk_profiling_add_event_s __user *)arg);
			break;

		case VR_IOC_PROFILING_STOP:
			err = profiling_stop_wrapper(session_data, (_vr_uk_profiling_stop_s __user *)arg);
			break;

		case VR_IOC_PROFILING_GET_EVENT:
			err = profiling_get_event_wrapper(session_data, (_vr_uk_profiling_get_event_s __user *)arg);
			break;

		case VR_IOC_PROFILING_CLEAR:
			err = profiling_clear_wrapper(session_data, (_vr_uk_profiling_clear_s __user *)arg);
			break;

		case VR_IOC_PROFILING_GET_CONFIG:
			/* Deprecated: still compatible with get_user_settings */
			err = get_user_settings_wrapper(session_data, (_vr_uk_get_user_settings_s __user *)arg);
			break;

		case VR_IOC_PROFILING_REPORT_SW_COUNTERS:
			err = profiling_report_sw_counters_wrapper(session_data, (_vr_uk_sw_counters_report_s __user *)arg);
			break;

#else

		case VR_IOC_PROFILING_START:              /* FALL-THROUGH */
		case VR_IOC_PROFILING_ADD_EVENT:          /* FALL-THROUGH */
		case VR_IOC_PROFILING_STOP:               /* FALL-THROUGH */
		case VR_IOC_PROFILING_GET_EVENT:          /* FALL-THROUGH */
		case VR_IOC_PROFILING_CLEAR:              /* FALL-THROUGH */
		case VR_IOC_PROFILING_GET_CONFIG:         /* FALL-THROUGH */
		case VR_IOC_PROFILING_REPORT_SW_COUNTERS: /* FALL-THROUGH */
			VR_DEBUG_PRINT(2, ("Profiling not supported\n"));
			err = -ENOTTY;
			break;

#endif

		case VR_IOC_MEM_INIT:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_INIT\n");	
			err = mem_init_wrapper(session_data, (_vr_uk_init_mem_s __user *)arg);
			break;

		case VR_IOC_MEM_TERM:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_TERM\n");	
			err = mem_term_wrapper(session_data, (_vr_uk_term_mem_s __user *)arg);
			break;

		case VR_IOC_MEM_MAP_EXT:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_MAP_EXT\n");	
			err = mem_map_ext_wrapper(session_data, (_vr_uk_map_external_mem_s __user *)arg);
			break;

		case VR_IOC_MEM_UNMAP_EXT:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_UNMAP_EXT\n");	
			err = mem_unmap_ext_wrapper(session_data, (_vr_uk_unmap_external_mem_s __user *)arg);
			break;

		case VR_IOC_MEM_QUERY_MMU_PAGE_TABLE_DUMP_SIZE:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_QUERY_MMU_PAGE_TABLE_DUMP_SIZE\n");	
			err = mem_query_mmu_page_table_dump_size_wrapper(session_data, (_vr_uk_query_mmu_page_table_dump_size_s __user *)arg);
			break;

		case VR_IOC_MEM_DUMP_MMU_PAGE_TABLE:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_DUMP_MMU_PAGE_TABLE\n");	
			err = mem_dump_mmu_page_table_wrapper(session_data, (_vr_uk_dump_mmu_page_table_s __user *)arg);
			break;

#if defined(CONFIG_VR400_UMP)

		case VR_IOC_MEM_ATTACH_UMP:
			err = mem_attach_ump_wrapper(session_data, (_vr_uk_attach_ump_mem_s __user *)arg);
			break;

		case VR_IOC_MEM_RELEASE_UMP:
			err = mem_release_ump_wrapper(session_data, (_vr_uk_release_ump_mem_s __user *)arg);
			break;

#else

		case VR_IOC_MEM_ATTACH_UMP:
		case VR_IOC_MEM_RELEASE_UMP: /* FALL-THROUGH */
			VR_DEBUG_PRINT(2, ("UMP not supported\n"));
			err = -ENOTTY;
			break;
#endif

#ifdef CONFIG_DMA_SHARED_BUFFER
		case VR_IOC_MEM_ATTACH_DMA_BUF:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_ATTACH_DMA_BUF\n");	
			err = vr_attach_dma_buf(session_data, (_vr_uk_attach_dma_buf_s __user *)arg);
			break;

		case VR_IOC_MEM_RELEASE_DMA_BUF:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_RELEASE_DMA_BUF\n");	
			err = vr_release_dma_buf(session_data, (_vr_uk_release_dma_buf_s __user *)arg);
			break;

		case VR_IOC_MEM_DMA_BUF_GET_SIZE:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_MEM_DMA_BUF_GET_SIZE\n");	
			err = vr_dma_buf_get_size(session_data, (_vr_uk_dma_buf_get_size_s __user *)arg);
			break;
#else

		case VR_IOC_MEM_ATTACH_DMA_BUF:   /* FALL-THROUGH */
		case VR_IOC_MEM_RELEASE_DMA_BUF:  /* FALL-THROUGH */
		case VR_IOC_MEM_DMA_BUF_GET_SIZE: /* FALL-THROUGH */
			VR_DEBUG_PRINT(2, ("DMA-BUF not supported\n"));
			err = -ENOTTY;
			break;
#endif

		case VR_IOC_PP_START_JOB:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_PP_START_JOB\n");	
			err = pp_start_job_wrapper(session_data, (_vr_uk_pp_start_job_s __user *)arg);
			break;

		case VR_IOC_PP_NUMBER_OF_CORES_GET:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_PP_NUMBER_OF_CORES_GET\n");	
			err = pp_get_number_of_cores_wrapper(session_data, (_vr_uk_get_pp_number_of_cores_s __user *)arg);
			break;

		case VR_IOC_PP_CORE_VERSION_GET:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_PP_CORE_VERSION_GET\n");	
			err = pp_get_core_version_wrapper(session_data, (_vr_uk_get_pp_core_version_s __user *)arg);
			break;

		case VR_IOC_PP_DISABLE_WB:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_PP_DISABLE_WB\n");	
			err = pp_disable_wb_wrapper(session_data, (_vr_uk_pp_disable_wb_s __user *)arg);
			break;

		case VR_IOC_GP2_START_JOB:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_GP2_START_JOB\n");	
			err = gp_start_job_wrapper(session_data, (_vr_uk_gp_start_job_s __user *)arg);
			break;

		case VR_IOC_GP2_NUMBER_OF_CORES_GET:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_GP2_NUMBER_OF_CORES_GET\n");	
			err = gp_get_number_of_cores_wrapper(session_data, (_vr_uk_get_gp_number_of_cores_s __user *)arg);
			break;

		case VR_IOC_GP2_CORE_VERSION_GET:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_GP2_CORE_VERSION_GET\n");	
			err = gp_get_core_version_wrapper(session_data, (_vr_uk_get_gp_core_version_s __user *)arg);
			break;

		case VR_IOC_GP2_SUSPEND_RESPONSE:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_GP2_SUSPEND_RESPONSE\n");	
			err = gp_suspend_response_wrapper(session_data, (_vr_uk_gp_suspend_response_s __user *)arg);
			break;

		case VR_IOC_VSYNC_EVENT_REPORT:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_VSYNC_EVENT_REPORT\n");	
			err = vsync_event_report_wrapper(session_data, (_vr_uk_vsync_event_report_s __user *)arg);
			break;

		case VR_IOC_STREAM_CREATE:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_STREAM_CREATE\n");	
#if defined(CONFIG_SYNC)
			err = stream_create_wrapper(session_data, (_vr_uk_stream_create_s __user *)arg);
			break;
#endif
		case VR_IOC_FENCE_VALIDATE:
			VR_IOCTL_DBG("[MALI] IOC:VR_IOC_FENCE_VALIDATE\n");	
#if defined(CONFIG_SYNC)
			err = sync_fence_validate_wrapper(session_data, (_vr_uk_fence_validate_s __user *)arg);
			break;
#else
			VR_DEBUG_PRINT(2, ("Sync objects not supported\n"));
			err = -ENOTTY;
			break;
#endif

		case VR_IOC_MEM_GET_BIG_BLOCK: /* Fallthrough */
		case VR_IOC_MEM_FREE_BIG_BLOCK:
			VR_PRINT_ERROR(("Non-MMU mode is no longer supported.\n"));
			err = -ENOTTY;
			break;

		default:
			VR_DEBUG_PRINT(2, ("No handler for ioctl 0x%08X 0x%08lX\n", cmd, arg));
			err = -ENOTTY;
	};

	return err;
}


module_init(vr_module_init);
module_exit(vr_module_exit);

MODULE_LICENSE(VR_KERNEL_LINUX_LICENSE);
MODULE_AUTHOR("ARM Ltd.");
MODULE_VERSION(SVN_REV_STRING);
