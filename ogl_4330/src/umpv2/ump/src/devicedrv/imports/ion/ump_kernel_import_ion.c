/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <ump/ump_kernel_interface.h>
#include <linux/dma-mapping.h>
#include <linux/ion.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

struct ion_wrapping_info
{
	struct ion_client *   ion_client;
	struct ion_handle *   ion_handle;
	int                   num_phys_blocks;
	struct scatterlist *  sglist;
};

static struct ion_device * ion_device_get(void)
{
	return NULL;
}

static unsigned int ion_heap_mask_get(void)
{
	/* Return a heap mask that allow us to use any heap. (Despite the
	 * description in the ION documentatin, this mask is not just for
	 * *allocations*.)  */
	return -1;
}

static int import_ion_client_create(void** custom_session_data)
{
	struct ion_client ** ion_client;

	ion_client = (struct ion_client**)custom_session_data;
	*ion_client = ion_client_create(ion_device_get(), ion_heap_mask_get(), "ump");

	if (IS_ERR(*ion_client))
	{
		return PTR_ERR(*ion_client);
	}
}


static void import_ion_client_destroy(void* custom_session_data)
{
	struct ion_client * ion_client;

	ion_client = (struct ion_client*)custom_session_data;
	BUG_ON(!ion_client);

	ion_client_destroy(ion_client);
}


static void import_ion_final_release_callback(const ump_dd_handle handle, void * info)
{
	struct ion_wrapping_info * ion_info;

	BUG_ON(!info);

	(void)handle;
	ion_info = (struct ion_wrapping_info*)info;

	dma_unmap_sg(NULL, ion_info->sglist, ion_info->num_phys_blocks, DMA_BIDIRECTIONAL);
	ion_unmap_dma(ion_info->ion_client, ion_info->ion_handle);
	ion_free(ion_info->ion_client, ion_info->ion_handle);
	kfree(ion_info);
	module_put(THIS_MODULE);
}

static ump_dd_handle import_ion_import(void * custom_session_data, void * pfd, ump_alloc_flags flags)
{
	int fd;
	ump_dd_handle ump_handle;
	struct scatterlist * sg;
	int num_dma_blocks;
	ump_dd_physical_block_64 * phys_blocks;
	unsigned long i;

	struct ion_wrapping_info * ion_info;

	BUG_ON(!custom_session_data);
	BUG_ON(!pfd);

	ion_info = kzalloc(GFP_KERNEL, sizeof(*ion_info));
	if (NULL == ion_info)
	{
		return UMP_DD_INVALID_MEMORY_HANDLE;
	}

	ion_info->ion_client = (struct ion_client*)custom_session_data;

	if (get_user(fd, (int*)pfd))
	{
		goto out;
	}

	ion_info->ion_handle = ion_import_fd(ion_info->ion_client, fd);

	if (IS_ERR_OR_NULL(ion_info->ion_handle))
	{
		goto out;
	}

	ion_info->sglist = ion_map_dma(ion_info->ion_client, ion_info->ion_handle);
	if (IS_ERR_OR_NULL(ion_info->sglist))
	{
		goto ion_dma_map_failed;
	}

	sg = ion_info->sglist;
	while (sg)
	{
		ion_info->num_phys_blocks++;
		sg = sg_next(sg);
	}

	num_dma_blocks = dma_map_sg(NULL, ion_info->sglist, ion_info->num_phys_blocks, DMA_BIDIRECTIONAL);

	if (0 == num_dma_blocks)
	{
		goto linux_dma_map_failed;
	}

	phys_blocks = vmalloc(num_dma_blocks * sizeof(*phys_blocks));
	if (NULL == phys_blocks)
	{
		goto vmalloc_failed;
	}

	for_each_sg(ion_info->sglist, sg, num_dma_blocks, i)
	{
		phys_blocks[i].addr = sg_phys(sg);
		phys_blocks[i].size = sg_dma_len(sg);
	}

	ump_handle = ump_dd_create_from_phys_blocks_64(phys_blocks, num_dma_blocks, flags, NULL, import_ion_final_release_callback, ion_info);

	vfree(phys_blocks);

	if (ump_handle != UMP_DD_INVALID_MEMORY_HANDLE)
	{
		/* 
		 * As we have a final release callback installed
		 * we must keep the module locked until
		 * the callback has been triggered
		 * */
		__module_get(THIS_MODULE);
		return ump_handle;
	}

	/* failed*/
vmalloc_failed:
	dma_unmap_sg(NULL, ion_info->sglist, ion_info->num_phys_blocks, DMA_BIDIRECTIONAL);
linux_dma_map_failed:
	ion_unmap_dma(ion_info->ion_client, ion_info->ion_handle);
ion_dma_map_failed:
	ion_free(ion_info->ion_client, ion_info->ion_handle);
out:
	kfree(ion_info);
	return UMP_DD_INVALID_MEMORY_HANDLE;
}

struct ump_import_handler import_handler_ion =
{
	.linux_module =  THIS_MODULE,
	.session_begin = import_ion_client_create,
	.session_end =   import_ion_client_destroy,
	.import =        import_ion_import
};

static int __init import_ion_initialize_module(void)
{
	/* register with UMP */
	return ump_import_module_register(UMP_EXTERNAL_MEM_TYPE_ION, &import_handler_ion);
}

static void __exit import_ion_cleanup_module(void)
{
	/* unregister import handler */
	ump_import_module_unregister(UMP_EXTERNAL_MEM_TYPE_ION);
}

/* Setup init and exit functions for this module */
module_init(import_ion_initialize_module);
module_exit(import_ion_cleanup_module);

/* And some module informatio */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARM Ltd.");
MODULE_VERSION("1.0");
