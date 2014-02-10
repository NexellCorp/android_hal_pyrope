#ifndef _NX_ALLOCATOR_H
#define _NX_ALLOCATOR_H

#include <stdio.h>
#include <sys/types.h>

#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <ion/ion.h>
#include <ion-private.h>
#include <nxp-v4l2.h>

bool allocateBuffer(struct nxp_vid_buffer *buf, int bufSize, int width, int height, uint32_t format);
void freeBuffer(struct nxp_vid_buffer *buf, int bufSize);
void dumpBuffer(struct nxp_vid_buffer *buf, int bufSize);

#endif
