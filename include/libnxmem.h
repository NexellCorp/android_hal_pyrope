/*
 * (C) Copyright 2010
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 */
#ifndef __LIBNXMEM_H__
#define __LIBNXMEM_H__

/*------------------------------------------------------------------------------
 * Define type
 -----------------------------------------------------------------------------*/
#ifndef BOOL
#define	BOOL	unsigned int					///< boolean type is 32bits signed integer
#endif
#ifndef TRUE
#define TRUE	1							///< true value is  integer one
#endif
#ifndef FALSE
#define FALSE	0							///< false value is  integer zero
#endif

#ifndef UINT
#define	UINT	unsigned int
#endif

/*------------------------------------------------------------------------------
 * VM Driver interface functions.
 -----------------------------------------------------------------------------*/
typedef struct {
	void		*	MemPoint;
	unsigned int	Flags;
	unsigned short 	HorAlign;
	unsigned short 	VerAlign;
	unsigned int 	MemWidth;
	unsigned int 	MemHeight;
	unsigned int 	Address;	//	Phsyical address.
	unsigned int 	Virtual;	//	Kernel virtual address.
} VM_IMEMORY;

/* video memory data struct */
typedef struct {
	VM_IMEMORY	LuMemory;
	VM_IMEMORY	CbMemory;
	VM_IMEMORY	CrMemory;
} VM_VMEMORY;

/* base memory info */
typedef struct {
	/* Linear memory base info */
	unsigned int LinPhyBase;
	unsigned int LinPhySize;
	unsigned int LinVirBase;
	unsigned int LinVirSize;
	/* Block memory base info */
	unsigned int BlkPhyBase;
	unsigned int BlkPhySize;
	unsigned int BlkVirBase;
	unsigned int BlkVirSize;
} VM_MEMBASE;

/*------------------------------------------------------------------------------
 * IOCTL CODE
 -----------------------------------------------------------------------------*/
#include "ioc_magic.h"

enum {
	IOCTL_VMEM_STATUS	= _IO(IOC_NX_MAGIC, 1),
	IOCTL_VMEM_RESET	= _IO(IOC_NX_MAGIC, 2),
	IOCTL_VMEM_INFO		= _IO(IOC_NX_MAGIC, 3),
	IOCTL_VMEM_ALLOC	= _IO(IOC_NX_MAGIC, 4),
	IOCTL_VMEM_FREE		= _IO(IOC_NX_MAGIC, 5),
	IOCTL_VMEM_VALLOC	= _IO(IOC_NX_MAGIC, 6),
	IOCTL_VMEM_VFREE	= _IO(IOC_NX_MAGIC, 7),
	IOCTL_VMEM_MAP		= _IO(IOC_NX_MAGIC, 8),	/* virtual mapping */
};

/*------------------------------------------------------------------------------
 * VM Application interface functions.
 -----------------------------------------------------------------------------*/
/* Reservedmemory information */
typedef VM_MEMBASE	ALLOCMEMBASE;

/* API memoy data struct. */
typedef struct
{
	void	      *	AllocPoint;
	unsigned int	Flags;
	unsigned int	HorAlign;
	unsigned int	VerAlign;
	unsigned int	MemWidth;
	unsigned int	MemHeight;
	unsigned int 	Address;	// Physical linear address(1D allocated) or 2D memory offset address(2D allocated)
	unsigned int 	PhyAddr;	// Physical linear address.
	unsigned int 	VirAddr;	// Virtual  linear address.
	unsigned int 	Stride;
} ALLOCMEMORY;

/* API vide memory data struct. */
typedef struct
{
	void	      * VAllocPoint;
	unsigned int	Flags;
	unsigned int 	FourCC;
	unsigned int 	ImgWidth;
	unsigned int 	ImgHeight;

	unsigned int	HorAlign;
	unsigned int 	VerAlign;

	unsigned int 	LuAddress;	// Lu physical linear address(1D allocated) or 2D memory offset address(2D allocated)
	unsigned int 	LuPhyAddr;	// Lu physical linear address.
	unsigned int 	LuVirAddr;	// Lu virtual  linear address.
	unsigned int 	LuStride;

	unsigned int 	CbAddress; 	// Cb physical linear address(1D allocated) or 2D memory offset address(2D allocated)
	unsigned int 	CbPhyAddr;	// Cb physical linear address.
	unsigned int 	CbVirAddr;	// Cb virtual  linear address.
	unsigned int 	CbStride;

	unsigned int 	CrAddress;	// Cr physical linear address(1D allocated) or 2D memory offset address(2D allocated)
	unsigned int 	CrPhyAddr;	// Cr physical linear address.
	unsigned int 	CrVirAddr;	// Cr virtual  linear address.
	unsigned int 	CrStride;
} VIDALLOCMEMORY;

/* Applicaton interface flags macro value. */
#define NX_MEM_LINEAR_BUFFER   		0x00000000		// Linear memory region
#define NX_MEM_BLOCK_BUFFER    		0x00000001		// Block  memory region
#define NX_MEM_NOT_SEPARATED	   	0x00000010		// Y,Cb,Cr memory region is sticking each other.

#define	NX_MEM_VIDEO_ORDER_YUV		0x00000000
#define	NX_MEM_VIDEO_ORDER_YVU		0x10000000
#define	NX_MEM_VIDEO_420_TYPE		0x01000000

/* Allocator error code */
#define NX_MEM_NOERROR           	( 0)
#define NX_MEM_ERROR		        (-1)
#define NX_MEM_OPEN_FAIL            (-2)
#define NX_MEM_IOCTL_FAIL           (-4)
#define NX_MEM_MMAPP_FAIL         	(-5)
#define NX_MEM_ALLOC_FAIL           (-6)
#define NX_MEM_FREE_FAIL            (-7)
#define NX_MEM_INVALID_PARAMS      	(-8)

/*------------------------------------------------------------------------------
 * Application interface functions.
 *----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

/* Reserved multimedia memory(linear/block) info. */
int 			NX_MemoryBase(ALLOCMEMBASE * pBase);

/* Reserved multimedia memory(linear/block) allocated status print */
int 			NX_MemoryStatus(unsigned int Type);

/* Reserved multimedia memory(linear/block) allocator DB reset */
int 			NX_MemoryReset(unsigned int Type);

/* Allocate/Releas single memory from reserved multimedia memory(linear/block). */
int 			NX_MemAlloc(ALLOCMEMORY *pAlloc);
int 			NX_MemFree (ALLOCMEMORY *pAlloc);

ALLOCMEMORY  *	NX_MemoryAlloc(UINT Flags, int Width, int Height, int HorAlign, int VerAlign);
int 			NX_MemoryFree (ALLOCMEMORY  *);

/* Allocate/Releas video memory from reserved multimedia memory(linear/block). */
int 			NX_VideoMemAlloc(VIDALLOCMEMORY *pVdAlloc);
int 			NX_VideoMemFree (VIDALLOCMEMORY *pVdAlloc);

VIDALLOCMEMORY* NX_VideoMemoryAlloc(UINT Flags, UINT FourCC, int Width, int Height, int HorAlign, int VerAlign);
int				NX_VideoMemoryFree (VIDALLOCMEMORY *);


/* Reserved multimedia memory copy */
void 			NX_SetBlockMemory   (UINT VirDst, UINT BlkSrc, char Data, int Width, int Height);
void 			NX_CopyBlockToLinear(UINT VirDst, UINT BlkSrc, UINT VirSrc, int Width, int Height);
void 			NX_CopyLinearToBlock(UINT BlkDst, UINT VirDst, UINT VirSrc, int Width, int Height);
void			NX_CopyYUYVToMVS0	(UINT srcVir, UINT luVir,UINT luBlk, UINT cbVir,UINT cbBlk, UINT crVir,UINT crBlk, int Width,int Height);


/* Map physical address to virtual address. */
UINT			NX_MapVirtualAddress  (UINT Physical, UINT Size, BOOL Cached);
void			NX_UnMapVirtualAddress(UINT Virtual , UINT Size);

#ifdef __cplusplus
 }
#endif

#endif /* __LIBNXMEM_H__ */
