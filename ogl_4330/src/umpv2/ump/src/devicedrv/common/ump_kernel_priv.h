/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _UMP_KERNEL_PRIV_H_
#define _UMP_KERNEL_PRIV_H_

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/cacheflush.h>
#endif


#define UMP_EXPECTED_IDS 64
#define UMP_MAX_IDS 32768

#ifdef __KERNEL__
#define UMP_ASSERT(expr) \
		if (!(expr)) { \
			printk(KERN_ERR "UMP: Assertion failed! %s,%s,%s,line=%d\n",\
					#expr,__FILE__,__func__,__LINE__); \
					BUG(); \
		}

static inline void ump_sync_to_memory(uint64_t paddr, void* vaddr, size_t sz)
{
#ifdef CONFIG_ARM
	dmac_flush_range(vaddr, vaddr+sz-1);
	outer_flush_range(paddr, paddr+sz-1);
#elif defined(CONFIG_X86)
	struct scatterlist scl = {0, };
	sg_set_page(&scl, pfn_to_page(PFN_DOWN(paddr)), sz,
			paddr & (PAGE_SIZE -1 ));
	dma_sync_sg_for_cpu(NULL, &scl, 1, DMA_TO_DEVICE);
	mb(); /* for outer_sync (if needed) */
#else
#error Implement cache maintenance for your architecture here
#endif
}

static inline void ump_sync_to_cpu(uint64_t paddr, void* vaddr, size_t sz)
{
#ifdef CONFIG_ARM
	dmac_flush_range(vaddr, vaddr+sz-1);
	outer_flush_range(paddr, paddr+sz-1);
#elif defined(CONFIG_X86)
	struct scatterlist scl = {0, };
	sg_set_page(&scl, pfn_to_page(PFN_DOWN(paddr)), sz,
			paddr & (PAGE_SIZE -1 ));
	dma_sync_sg_for_cpu(NULL, &scl, 1, DMA_FROM_DEVICE);
#else
#error Implement cache maintenance for your architecture here
#endif
}
#endif /* __KERNEL__*/
#endif /* _UMP_KERNEL_PRIV_H_ */

