/*
 * (C) Copyright 2010
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 */
#ifndef __PLATFORM_H__
#define __PLATFORM_H__

/* Platform: WinCE */
#ifdef UNDER_CE

#else	/* UNDER_CE */

/* Platform: Linux */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>		/* for open/close */
#include <fcntl.h>		/* for O_RDWR */
#include <sys/ioctl.h>	/* for ioctl */
#include <sys/types.h>	/* for mmap options */
#include <sys/mman.h>	/* for mmap options */

/* Local headers */
#include "fourcc.h"
#include "libnxmem.h"
#include "debug.h"

#define VM_DEV_NAME			"/dev/vmem"

#endif	/* END UNDER_CE */

/*------------------------------------------------------------------------------
 * Independent Platform
 ------------------------------------------------------------------------------*/
/* Define block memory zone */
#define	VMEM_BLOCK_STRIDE		4096
#define VMEM_BLOCK_ZONE			0x20000000

int Mem_Init	(void);
int Mem_InitBase(void);
int Mem_MapBase (unsigned int Physical, unsigned int Size, unsigned int *pVirtual, BOOL Cached);
int Mem_MapAddr (unsigned int Address , unsigned int Size, unsigned int MemType,
				unsigned  int *pVirtual, unsigned int *pPhysical);
int Mem_MapFree (unsigned int Address, unsigned int Size, unsigned int MemType);
int	Mem_IoCtrl	(unsigned int IoCode, void *InArgs, int InSize, void *OutArgs, int OutSize);

#endif /*  __PLATFORM_H__ */

