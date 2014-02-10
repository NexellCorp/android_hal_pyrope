/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <linux/fs.h>	   /* file system operations */
#include <asm/uaccess.h>	/* user space access */
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/rbtree.h>
#include <linux/platform_device.h>

#include "vr_ukk.h"
#include "vr_osk.h"
#include "vr_kernel_common.h"
#include "vr_session.h"
#include "vr_kernel_linux.h"

#include "vr_kernel_memory_engine.h"
#include "vr_memory.h"


struct vr_dma_buf_attachment {
	struct dma_buf *buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	_vr_osk_atomic_t ref;
	struct rb_node rb_node;
};

static struct rb_root vr_dma_bufs = RB_ROOT;
static DEFINE_SPINLOCK(vr_dma_bufs_lock);

static inline struct vr_dma_buf_attachment *vr_dma_buf_lookup(struct rb_root *root, struct dma_buf *target)
{
	struct rb_node *node = root->rb_node;
	struct vr_dma_buf_attachment *res;

	spin_lock(&vr_dma_bufs_lock);
	while (node)
	{
		res = rb_entry(node, struct vr_dma_buf_attachment, rb_node);

		if (target < res->buf) node = node->rb_left;
		else if (target > res->buf) node = node->rb_right;
		else
		{
			_vr_osk_atomic_inc(&res->ref);
			spin_unlock(&vr_dma_bufs_lock);
			return res;
		}
	}
	spin_unlock(&vr_dma_bufs_lock);

	return NULL;
}

static void vr_dma_buf_add(struct rb_root *root, struct vr_dma_buf_attachment *new)
{
	struct rb_node **node = &root->rb_node;
	struct rb_node *parent = NULL;
	struct vr_dma_buf_attachment *res;

	spin_lock(&vr_dma_bufs_lock);
	while (*node)
	{
		parent = *node;
		res = rb_entry(*node, struct vr_dma_buf_attachment, rb_node);

		if (new->buf < res->buf) node = &(*node)->rb_left;
		else node = &(*node)->rb_right;
	}

	rb_link_node(&new->rb_node, parent, node);
	rb_insert_color(&new->rb_node, &vr_dma_bufs);

	spin_unlock(&vr_dma_bufs_lock);

	return;
}


static void vr_dma_buf_release(void *ctx, void *handle)
{
	struct vr_dma_buf_attachment *mem;
	u32 ref;

	mem = (struct vr_dma_buf_attachment *)handle;

	VR_DEBUG_ASSERT_POINTER(mem);
	VR_DEBUG_ASSERT_POINTER(mem->attachment);
	VR_DEBUG_ASSERT_POINTER(mem->buf);

	spin_lock(&vr_dma_bufs_lock);
	ref = _vr_osk_atomic_dec_return(&mem->ref);

	if (0 == ref)
	{
		rb_erase(&mem->rb_node, &vr_dma_bufs);
		spin_unlock(&vr_dma_bufs_lock);

		VR_DEBUG_ASSERT(0 == _vr_osk_atomic_read(&mem->ref));

		dma_buf_unmap_attachment(mem->attachment, mem->sgt, DMA_BIDIRECTIONAL);

		dma_buf_detach(mem->buf, mem->attachment);
		dma_buf_put(mem->buf);

		_vr_osk_free(mem);
	}
	else
	{
		spin_unlock(&vr_dma_bufs_lock);
	}
}

/* Callback from memory engine which will map into VR virtual address space */
static vr_physical_memory_allocation_result vr_dma_buf_commit(void* ctx, vr_allocation_engine * engine, vr_memory_allocation * descriptor, u32* offset, vr_physical_memory_allocation * alloc_info)
{
	struct vr_session_data *session;
	struct vr_page_directory *pagedir;
	struct vr_dma_buf_attachment *mem;
	struct scatterlist *sg;
	int i;
	u32 virt;

	VR_DEBUG_ASSERT_POINTER(ctx);
	VR_DEBUG_ASSERT_POINTER(engine);
	VR_DEBUG_ASSERT_POINTER(descriptor);
	VR_DEBUG_ASSERT_POINTER(offset);
	VR_DEBUG_ASSERT_POINTER(alloc_info);

	/* Mapping dma-buf with an offset is not supported. */
	VR_DEBUG_ASSERT(0 == *offset);

	virt = descriptor->vr_address;
	session = (struct vr_session_data *)descriptor->vr_addr_mapping_info;
	pagedir = vr_session_get_page_directory(session);

	VR_DEBUG_ASSERT_POINTER(session);

	mem = (struct vr_dma_buf_attachment *)ctx;

	VR_DEBUG_ASSERT_POINTER(mem);

	mem->sgt = dma_buf_map_attachment(mem->attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(mem->sgt))
	{
		VR_PRINT_ERROR(("Failed to map dma-buf attachment\n"));
		return VR_MEM_ALLOC_INTERNAL_FAILURE;
	}

	for_each_sg(mem->sgt->sgl, sg, mem->sgt->nents, i)
	{
		u32 size = sg_dma_len(sg);
		dma_addr_t phys = sg_dma_address(sg);

		/* sg must be page aligned. */
		VR_DEBUG_ASSERT(0 == size % VR_MMU_PAGE_SIZE);

		vr_mmu_pagedir_update(pagedir, virt, phys, size, VR_CACHE_STANDARD);

		virt += size;
		*offset += size;
	}

	if (descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_GUARD_PAGE)
	{
		u32 guard_phys;
		VR_DEBUG_PRINT(7, ("Mapping in extra guard page\n"));

		guard_phys = sg_dma_address(mem->sgt->sgl);
		vr_mmu_pagedir_update(vr_session_get_page_directory(session), virt, guard_phys, VR_MMU_PAGE_SIZE, VR_CACHE_STANDARD);
	}

	VR_DEBUG_ASSERT(*offset == descriptor->size);

	alloc_info->ctx = NULL;
	alloc_info->handle = mem;
	alloc_info->next = NULL;
	alloc_info->release = vr_dma_buf_release;

	return VR_MEM_ALLOC_FINISHED;
}

int vr_attach_dma_buf(struct vr_session_data *session, _vr_uk_attach_dma_buf_s __user *user_arg)
{
	vr_physical_memory_allocator external_memory_allocator;
	struct dma_buf *buf;
	struct vr_dma_buf_attachment *mem;
	_vr_uk_attach_dma_buf_s args;
	vr_memory_allocation *descriptor;
	int md;
	int fd;

	/* Get call arguments from user space. copy_from_user returns how many bytes which where NOT copied */
	if (0 != copy_from_user(&args, (void __user *)user_arg, sizeof(_vr_uk_attach_dma_buf_s)))
	{
		return -EFAULT;
	}


	fd = args.mem_fd;

	buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(buf))
	{
		VR_DEBUG_PRINT(2, ("Failed to get dma-buf from fd: %d\n", fd));
		return PTR_RET(buf);
	}

	/* Currently, mapping of the full buffer are supported. */
	if (args.size != buf->size)
	{
		VR_DEBUG_PRINT(2, ("dma-buf size doesn't match mapping size.\n"));
		dma_buf_put(buf);
		return -EINVAL;
	}


	mem = vr_dma_buf_lookup(&vr_dma_bufs, buf);
	if (NULL == mem)
	{
		/* dma-buf is not already attached to VR */
		mem = _vr_osk_calloc(1, sizeof(struct vr_dma_buf_attachment));
		if (NULL == mem)
		{
			VR_PRINT_ERROR(("Failed to allocate dma-buf tracing struct\n"));
			dma_buf_put(buf);
			return -ENOMEM;
		}
		_vr_osk_atomic_init(&mem->ref, 1);
		mem->buf = buf;

		mem->attachment = dma_buf_attach(mem->buf, &vr_platform_device->dev);
		if (NULL == mem->attachment)
		{
			VR_DEBUG_PRINT(2, ("Failed to attach to dma-buf %d\n", fd));
			dma_buf_put(mem->buf);
			_vr_osk_free(mem);
			return -EFAULT;
		}

		vr_dma_buf_add(&vr_dma_bufs, mem);
	}
	else
	{
		/* dma-buf is already attached to VR */
		/* Give back the reference we just took, vr_dma_buf_lookup grabbed a new reference for us. */
		dma_buf_put(buf);
	}

	/* Map dma-buf into this session's page tables */

	/* Set up VR memory descriptor */
	descriptor = _vr_osk_calloc(1, sizeof(vr_memory_allocation));
	if (NULL == descriptor)
	{
		VR_PRINT_ERROR(("Failed to allocate descriptor dma-buf %d\n", fd));
		vr_dma_buf_release(NULL, mem);
		return -ENOMEM;
	}

	descriptor->size = args.size;
	descriptor->mapping = NULL;
	descriptor->vr_address = args.vr_address;
	descriptor->vr_addr_mapping_info = (void*)session;
	descriptor->process_addr_mapping_info = NULL; /* do not map to process address space */
	descriptor->lock = session->memory_lock;

	if (args.flags & _VR_MAP_EXTERNAL_MAP_GUARD_PAGE)
	{
		descriptor->flags = VR_MEMORY_ALLOCATION_FLAG_MAP_GUARD_PAGE;
	}
	_vr_osk_list_init( &descriptor->list );

	/* Get descriptor mapping for memory. */
	if (_VR_OSK_ERR_OK != vr_descriptor_mapping_allocate_mapping(session->descriptor_mapping, descriptor, &md))
	{
		VR_PRINT_ERROR(("Failed to create descriptor mapping for dma-buf %d\n", fd));
		_vr_osk_free(descriptor);
		vr_dma_buf_release(NULL, mem);
		return -EFAULT;
	}

	external_memory_allocator.allocate = vr_dma_buf_commit;
	external_memory_allocator.allocate_page_table_block = NULL;
	external_memory_allocator.ctx = mem;
	external_memory_allocator.name = "DMA-BUF Memory";
	external_memory_allocator.next = NULL;

	/* Map memory into session's VR virtual address space. */
	_vr_osk_lock_wait(session->memory_lock, _VR_OSK_LOCKMODE_RW);
	if (_VR_OSK_ERR_OK != vr_allocation_engine_allocate_memory(vr_mem_get_memory_engine(), descriptor, &external_memory_allocator, NULL))
	{
		_vr_osk_lock_signal(session->memory_lock, _VR_OSK_LOCKMODE_RW);

		VR_PRINT_ERROR(("Failed to map dma-buf %d into VR address space\n", fd));
		vr_descriptor_mapping_free(session->descriptor_mapping, md);
		vr_dma_buf_release(NULL, mem);
		return -ENOMEM;
	}
	_vr_osk_lock_signal(session->memory_lock, _VR_OSK_LOCKMODE_RW);

	/* Return stuff to user space */
	if (0 != put_user(md, &user_arg->cookie))
	{
		/* Roll back */
		VR_PRINT_ERROR(("Failed to return descriptor to user space for dma-buf %d\n", fd));
		vr_descriptor_mapping_free(session->descriptor_mapping, md);
		vr_dma_buf_release(NULL, mem);
		return -EFAULT;
	}

	return 0;
}

int vr_release_dma_buf(struct vr_session_data *session, _vr_uk_release_dma_buf_s __user *user_arg)
{
	_vr_uk_release_dma_buf_s args;
	vr_memory_allocation *descriptor;

	/* get call arguments from user space. copy_from_user returns how many bytes which where NOT copied */
	if ( 0 != copy_from_user(&args, (void __user *)user_arg, sizeof(_vr_uk_release_dma_buf_s)) )
	{
		return -EFAULT;
	}

	if (_VR_OSK_ERR_OK != vr_descriptor_mapping_get(session->descriptor_mapping, args.cookie, (void**)&descriptor))
	{
		VR_DEBUG_PRINT(1, ("Invalid memory descriptor %d used to release dma-buf\n", args.cookie));
		return -EINVAL;
	}

	descriptor = vr_descriptor_mapping_free(session->descriptor_mapping, args.cookie);

	if (NULL != descriptor)
	{
		_vr_osk_lock_wait( session->memory_lock, _VR_OSK_LOCKMODE_RW );

		/* Will call back to vr_dma_buf_release() which will release the dma-buf attachment. */
		vr_allocation_engine_release_memory(vr_mem_get_memory_engine(), descriptor);

		_vr_osk_lock_signal( session->memory_lock, _VR_OSK_LOCKMODE_RW );

		_vr_osk_free(descriptor);
	}

	/* Return the error that _vr_ukk_map_external_ump_mem produced */
	return 0;
}

int vr_dma_buf_get_size(struct vr_session_data *session, _vr_uk_dma_buf_get_size_s __user *user_arg)
{
	_vr_uk_dma_buf_get_size_s args;
	int fd;
	struct dma_buf *buf;

	/* get call arguments from user space. copy_from_user returns how many bytes which where NOT copied */
	if ( 0 != copy_from_user(&args, (void __user *)user_arg, sizeof(_vr_uk_dma_buf_get_size_s)) )
	{
		return -EFAULT;
	}

	/* Do DMA-BUF stuff */
	fd = args.mem_fd;

	buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(buf))
	{
		VR_DEBUG_PRINT(2, ("Failed to get dma-buf from fd: %d\n", fd));
		return PTR_RET(buf);
	}

	if (0 != put_user(buf->size, &user_arg->size))
	{
		dma_buf_put(buf);
		return -EFAULT;
	}

	dma_buf_put(buf);

	return 0;
}
