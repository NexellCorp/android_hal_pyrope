/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_DMA_BUF_H__
#define __VR_DMA_BUF_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "vr_osk.h"

int vr_attach_dma_buf(struct vr_session_data *session, _vr_uk_attach_dma_buf_s __user *arg);
int vr_release_dma_buf(struct vr_session_data *session, _vr_uk_release_dma_buf_s __user *arg);
int vr_dma_buf_get_size(struct vr_session_data *session, _vr_uk_dma_buf_get_size_s __user *arg);

#ifdef __cplusplus
}
#endif

#endif /* __VR_KERNEL_LINUX_H__ */
