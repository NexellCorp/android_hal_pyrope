/*
 * (C) Copyright 2010
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 */
#include "platform.h"

#ifdef UNDER_CE
#define	VMEMAPI_EXPORTS
#ifdef VMEMAPI_DLL	// Make DLL Functions
	#ifdef VMEMAPI_EXPORTS
	#define VMEMAPI __declspec(dllexport)
	#else
	#define VMEMAPI __declspec(dllimport)
	#endif 	/* VMEMAPI_EXPORTS */
#else	/* Make Library Functions */
#define	VMEMAPI
#endif	/* VMEMAPI_DLL */
#else	/* UNDER_CE */
#define	VMEMAPI
#endif

/*-----------------------------------------------------------------------------*/
int				g_VmemIOHandle = -1;
unsigned int 	g_InitStatus = 0;
VM_MEMBASE		g_VM_MemBase;

BOOL		 	g_Is_ValidLinMem  = FALSE;
BOOL		 	g_Is_ValidBlkMem  = FALSE;

/*-----------------------------------------------------------------------------*/
static int 	GetMemoryBase(VM_MEMBASE * pMemBase);
static int 	SetVideoFormat(VIDALLOCMEMORY * pVAlloc, VM_VMEMORY * pVMem);
static int  AllocVideoMemory(VM_VMEMORY * pVMem);
static int  MapVideoToLinear(VM_VMEMORY * pVMem, VIDALLOCMEMORY * pVAlloc);
static int  MapVideoToBlock (VM_VMEMORY * pVMem, VIDALLOCMEMORY * pVAlloc);

/*------------------------------------------------------------------------------
 * VM Memory Allocator Library Application Interface functions.
 -----------------------------------------------------------------------------*/
VMEMAPI int
NX_MemoryBase(
		ALLOCMEMBASE *pBase
		)
{
	int Ret = NX_MEM_NOERROR;
	DBGOUT(DBG_LIB,(_T("+%s\n"), __FUNCTION__));

	Ret = GetMemoryBase(pBase);
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	DBGOUT(DBG_LIB,(_T("-%s\n"), __FUNCTION__));

	return NX_MEM_NOERROR;
}

VMEMAPI int
NX_MemoryStatus(
		unsigned int Type
		)
{
	int	Ret = NX_MEM_NOERROR;

	DBGOUT(DBG_LIB,(_T("+%s\n"), __FUNCTION__));

	/* check whether initialzed. */
	Ret = Mem_Init();
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	/* Reset allocator. */
	Ret = Mem_IoCtrl(IOCTL_VMEM_STATUS, &Type, sizeof(unsigned int), 0, 0);
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	DBGOUT(DBG_LIB,(_T("-%s\n"), __FUNCTION__));

	return NX_MEM_NOERROR;
}

VMEMAPI int
NX_MemoryReset(
		unsigned int Type
		)
{
	int Ret = NX_MEM_NOERROR;

	DBGOUT(DBG_LIB,(_T("+%s\n"), __FUNCTION__));

	/* check whether initialzed. */
	Ret = Mem_Init();
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	Ret = Mem_IoCtrl(IOCTL_VMEM_RESET, &Type, sizeof(unsigned int), 0, 0);
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	DBGOUT(DBG_LIB,(_T("-%s\n"), __FUNCTION__));

	return NX_MEM_NOERROR;
}

VMEMAPI int
NX_MemAlloc(
		ALLOCMEMORY *pAlloc
		)
{
	VM_IMEMORY IMem;
	unsigned int VirtAddr, PhysAddr, Address, Length;
	unsigned int MemType;
	int Ret = NX_MEM_NOERROR;

	if(! pAlloc) {
		ERROUT(DBG_ERR,(_T("Fail: Input params is NULL\n")));
		return NX_MEM_INVALID_PARAMS;
	}

	/* check whether initialzed. */
	Ret = Mem_Init();
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	MemType = pAlloc->Flags & NX_MEM_BLOCK_BUFFER;

	if(NX_MEM_BLOCK_BUFFER == MemType && !g_Is_ValidBlkMem)	{
		ERROUT(DBG_ERR,(_T("Fail: Not valid, block  memory \n")));
		return NX_MEM_ERROR;
	}

	if(NX_MEM_LINEAR_BUFFER == MemType && !g_Is_ValidLinMem) {
		ERROUT(DBG_ERR,(_T("Fail: Not valid, linear memory \n")));
		return NX_MEM_ERROR;
	}

	/* check align type. */
	if(pAlloc->HorAlign == 0)	pAlloc->HorAlign = 1;
	if(pAlloc->VerAlign == 0)	pAlloc->VerAlign = 1;

	/* set memory alloc info. */
	memset(&IMem, 0, sizeof(VM_IMEMORY));

	IMem.Flags	    = pAlloc->Flags;
	IMem.HorAlign	= pAlloc->HorAlign;
	IMem.VerAlign	= pAlloc->VerAlign;
	IMem.MemWidth	= pAlloc->MemWidth;
	IMem.MemHeight = pAlloc->MemHeight;

	/* Allocate single memory. */
	Ret = Mem_IoCtrl(IOCTL_VMEM_ALLOC, &IMem, sizeof(VM_IMEMORY), &IMem, sizeof(VM_IMEMORY));
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	/* Map allocated address to virtual address. */
	Address = IMem.Address;
	Length	= IMem.MemWidth * IMem.MemHeight;
	Ret 	= Mem_MapAddr(Address, Length, MemType, &VirtAddr, &PhysAddr);
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	pAlloc->VirAddr    = VirtAddr;
	pAlloc->Address    = IMem.Address;
	pAlloc->PhyAddr    = (MemType == NX_MEM_BLOCK_BUFFER ? PhysAddr: IMem.Address);
	pAlloc->Stride     = (MemType == NX_MEM_BLOCK_BUFFER ? VMEM_BLOCK_STRIDE : IMem.MemWidth);
	pAlloc->AllocPoint = IMem.MemPoint;

	return NX_MEM_NOERROR;
}

VMEMAPI int
NX_MemFree(
		ALLOCMEMORY *pAlloc
		)
{
	VM_IMEMORY IMem;
	unsigned int MemType, Address, Length;
	int Ret = NX_MEM_NOERROR;

	if(! pAlloc) {
		ERROUT(DBG_ERR,(_T("Fail: Input params is NULL\n")));
		return NX_MEM_INVALID_PARAMS;
	}

	/* check whether initialzed. */
	Ret = Mem_Init();
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	/* get release information. */
	memset(&IMem, 0, sizeof(VM_IMEMORY));

	IMem.MemPoint  = pAlloc->AllocPoint;
	IMem.Address   = pAlloc->Address;
	IMem.Flags	   = pAlloc->Flags;
	IMem.MemWidth  = pAlloc->MemWidth;
	IMem.MemHeight = pAlloc->MemHeight;

	MemType = pAlloc->Flags & NX_MEM_BLOCK_BUFFER;

	/* Free allocated memory physical address. */
	Ret = Mem_IoCtrl(IOCTL_VMEM_FREE, &IMem, sizeof(VM_IMEMORY), 0, 0);

	/* Releas allocated memory virtual address. */
	Address = pAlloc->VirAddr;
	Length  = pAlloc->MemWidth * pAlloc->MemHeight;
	Ret    |= Mem_MapFree(Address, Length, MemType);

	return Ret;
}

VMEMAPI int
NX_VideoMemAlloc(
		VIDALLOCMEMORY * pVAlloc
		)
{
	VM_VMEMORY * pVMem = NULL;
	unsigned int MemType;
	int Ret = NX_MEM_NOERROR;

	if(! pVAlloc) {
		ERROUT(DBG_ERR,(_T("Fail: Input params is NULL\n")));
		return NX_MEM_INVALID_PARAMS;
	}

	/* check whether initialzed. */
	Ret = Mem_Init();
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	MemType = pVAlloc->Flags & NX_MEM_BLOCK_BUFFER;

	if(NX_MEM_BLOCK_BUFFER == MemType && !g_Is_ValidBlkMem)	{
		ERROUT(DBG_ERR,(_T("Fail: Not valid, block  memory \n")));
		return NX_MEM_ERROR;
	}

	if(NX_MEM_LINEAR_BUFFER == MemType && !g_Is_ValidLinMem) {
		ERROUT(DBG_ERR,(_T("Fail: Not valid, linear  memory \n")));
		return NX_MEM_ERROR;
	}

	pVMem = (VM_VMEMORY *)malloc(sizeof(VM_VMEMORY));
	if(! pVMem) {
		ERROUT(DBG_ERR,(_T("Fail: malloc, create memory instance\n")));
		return NX_MEM_ALLOC_FAIL;
	}
	memset((void*)pVMem, 0, sizeof(VM_VMEMORY));

	/* Get each video filed size. */
	Ret = SetVideoFormat(pVAlloc, pVMem);
	if(NX_MEM_NOERROR != Ret) {
		free(pVMem);
		return Ret;
	}

	/* Allocate video memory. */
	Ret = AllocVideoMemory(pVMem);
	if(NX_MEM_NOERROR != Ret) {
		free(pVMem);
		return Ret;
	}

	/* Map virtual address */
	if(! (pVAlloc->Flags & NX_MEM_BLOCK_BUFFER))
		Ret = MapVideoToLinear(pVMem, pVAlloc);
	else
		Ret = MapVideoToBlock(pVMem, pVAlloc);

	return Ret;
}

VMEMAPI int
NX_VideoMemFree(
		VIDALLOCMEMORY * pVAlloc
		)
{
	VM_VMEMORY * pVMem   = pVAlloc->VAllocPoint;
	unsigned int MemType = pVAlloc->Flags  & NX_MEM_BLOCK_BUFFER;
	unsigned int Address;
	unsigned int Length;
	int Ret = NX_MEM_NOERROR;

	if(! pVAlloc) {
		ERROUT(DBG_ERR,(_T("Fail: Input params is NULL\n")));
		return NX_MEM_INVALID_PARAMS;
	}

	Ret = Mem_Init();
	if(NX_MEM_NOERROR != Ret) {
		free(pVMem);
		return Ret;
	}

	if((pVAlloc->Flags | NX_MEM_NOT_SEPARATED) && (pVAlloc->FourCC != FOURCC_YUYV)) {
		Length	= pVMem->LuMemory.MemWidth * pVMem->LuMemory.MemHeight;
		Length += pVMem->CbMemory.MemWidth * pVMem->CbMemory.MemHeight;
		Length += pVMem->CrMemory.MemWidth * pVMem->CrMemory.MemHeight;
		pVMem->LuMemory.MemWidth  = Length;
		pVMem->LuMemory.MemHeight = 1;
	}

	/* free allocated video physical address. */
	Ret = Mem_IoCtrl(IOCTL_VMEM_VFREE, pVMem, sizeof(VM_VMEMORY), 0, 0);

	if((pVAlloc->Flags | NX_MEM_NOT_SEPARATED) && (pVAlloc->FourCC != FOURCC_YUYV)) {
		/* Free Lu */
		Address = pVAlloc->LuAddress;
		Length	= pVMem->LuMemory.MemWidth * pVMem->LuMemory.MemHeight;
		Length += pVMem->CbMemory.MemWidth * pVMem->CbMemory.MemHeight;
		Length += pVMem->CrMemory.MemWidth * pVMem->CrMemory.MemHeight;
		Ret 	= Mem_MapFree(Address, Length, MemType);

	} else {
		/* Free Lu map */
		Address = pVAlloc->LuVirAddr;
		Length	= pVMem->LuMemory.MemWidth * pVMem->LuMemory.MemHeight;
		Ret    |= Mem_MapFree(Address, Length, MemType);

		/* Free Cb map */
		Address = pVAlloc->CbVirAddr;
		Length	= pVMem->CbMemory.MemWidth * pVMem->CbMemory.MemHeight;
		Ret    |= Mem_MapFree(Address, Length, MemType);

		/* Free Cr map */
		Address = pVAlloc->CrVirAddr;
		Length	= pVMem->CrMemory.MemWidth * pVMem->CrMemory.MemHeight;
		Ret    |= Mem_MapFree(Address, Length, MemType);
	}

	/* Free struct */
	free(pVMem);

	return NX_MEM_NOERROR;
}

VMEMAPI ALLOCMEMORY *
NX_MemoryAlloc(
		unsigned int Flags,
		int 		 Width,
		int 		 Height,
		int 		 HorAlign,
		int 		 VerAlign
		)
{
	ALLOCMEMORY * pAlloc = (ALLOCMEMORY *)malloc(sizeof(ALLOCMEMORY));
	if(! pAlloc) {
		ERROUT(DBG_ERR,(_T("Fail: malloc, create memory instance\n")));
		return NULL;
	}

	pAlloc->Flags      = Flags;
	pAlloc->HorAlign   = HorAlign;
	pAlloc->VerAlign   = VerAlign;
	pAlloc->MemWidth   = Width;
	pAlloc->MemHeight  = Height;

	DBGOUT(DBG_LIB,(_T("+%s (Flags:%x, Width:%d, Height:%d)\n"),
		__FUNCTION__, Flags, Width, Height));

	/* allocate memory bffers */
	if(NX_MEM_NOERROR != NX_MemAlloc(pAlloc)) {
		free(pAlloc);
		return NULL;
	}

	DBGOUT(DBG_LIB,(_T("addr:0x%08x, phys:0x%08x, virt:0x%08x, stride:%d \n"),
		pAlloc->Address, pAlloc->PhyAddr, pAlloc->VirAddr, pAlloc->Stride));
	DBGOUT(DBG_LIB,(_T("-%s\n"), __FUNCTION__));

	return pAlloc;
}

VMEMAPI int
NX_MemoryFree(
		ALLOCMEMORY *pAlloc
		)
{
	int Ret = NX_MEM_NOERROR;

	DBGOUT(DBG_LIB,(_T("%s\n"), __FUNCTION__));
	DBGOUT(DBG_LIB,(_T("addr:0x%08x, phys:0x%08x, virt:0x%08x, stride:%d \n"),
		pAlloc->Address, pAlloc->PhyAddr, pAlloc->VirAddr, pAlloc->Stride));

	Ret = NX_MemFree(pAlloc);
	free(pAlloc);

	return Ret;
}

VMEMAPI VIDALLOCMEMORY *
NX_VideoMemoryAlloc(
		unsigned int Flags,
		unsigned int uiFourCC,
		int Width,
		int Height,
		int HorAlign,
		int VerAlign
		)
{
	VIDALLOCMEMORY * pVAlloc = (VIDALLOCMEMORY *)malloc(sizeof(VIDALLOCMEMORY));
	if(! pVAlloc) {
		ERROUT(DBG_ERR,(_T("Fail: malloc, create memory instance\n")));
		return NULL;
	}

	pVAlloc->Flags      = Flags;
	pVAlloc->FourCC     = uiFourCC;
	pVAlloc->HorAlign   = HorAlign;
	pVAlloc->VerAlign   = VerAlign;
	pVAlloc->ImgWidth   = Width;
	pVAlloc->ImgHeight  = Height;

	DBGOUT(DBG_LIB,(_T("+%s (Flags:%x, FourCC:%x, Width:%d, Height:%d)\n"),
		__FUNCTION__, Flags, uiFourCC, Width, Height));

	/* allocate video buffers */
	if(NX_MEM_NOERROR != NX_VideoMemAlloc(pVAlloc)) {
		free(pVAlloc);
		return NULL;
	}

	DBGOUT(DBG_LIB,(_T("LuAddr:0x%08x, LuPhys:0x%08x, LuVirt:0x%08x, LuStride:%d \n"),
		pVAlloc->LuAddress, pVAlloc->LuPhyAddr, pVAlloc->LuVirAddr, pVAlloc->LuStride));
	DBGOUT(DBG_LIB,(_T("CbAddr:0x%08x, CbPhys:0x%08x, CbVirt:0x%08x, CbStride:%d \n"),
		pVAlloc->CbAddress, pVAlloc->CbPhyAddr, pVAlloc->CbVirAddr, pVAlloc->CbStride));
	DBGOUT(DBG_LIB,(_T("CrAddr:0x%08x, CrPhys:0x%08x, CrVirt:0x%08x, CrStride:%d \n"),
		pVAlloc->CrAddress, pVAlloc->CrPhyAddr, pVAlloc->CrVirAddr, pVAlloc->CrStride));

#if	(DBG_LIB == 1)
	if(pVAlloc->Flags & NX_MEM_BLOCK_BUFFER)
	DBGOUT(DBG_LIB,(_T("LuNode(L:%04d, T:%04d), CbNode(L:%04d, T:%04d), CrNode(L:%04d, T:%04d) \n"),
		(pVAlloc->LuAddress&0xFFF), (pVAlloc->LuAddress&0xFFF000)>>12,
		(pVAlloc->CbAddress&0xFFF), (pVAlloc->CbAddress&0xFFF000)>>12,
		(pVAlloc->CrAddress&0xFFF), (pVAlloc->CrAddress&0xFFF000)>>12));
#endif

	DBGOUT(DBG_LIB,(_T("-%s\n"), __FUNCTION__));

	return pVAlloc;
}

VMEMAPI int
NX_VideoMemoryFree(
		VIDALLOCMEMORY *pVAlloc
		)
{
	int Ret = NX_MEM_NOERROR;

	DBGOUT(DBG_LIB,(_T("+%s\n"), __FUNCTION__));
	DBGOUT(DBG_LIB,(_T("LuAddr=0x%08x, LuPhys=0x%08x, LuVirt=0x%08x, LuStride=%d \n"),
		pVAlloc->LuAddress, pVAlloc->LuPhyAddr, pVAlloc->LuVirAddr, pVAlloc->LuStride));
	DBGOUT(DBG_LIB,(_T("CbAddr=0x%08x, CbPhys=0x%08x, CbVirt=0x%08x, CbStride=%d \n"),
		pVAlloc->CbAddress, pVAlloc->CbPhyAddr, pVAlloc->CbVirAddr, pVAlloc->CbStride));
	DBGOUT(DBG_LIB,(_T("CrAddr=0x%08x, CrPhys=0x%08x, CrVirt=0x%08x, CrStride=%d \n"),
		pVAlloc->CrAddress, pVAlloc->CrPhyAddr, pVAlloc->CrVirAddr, pVAlloc->CrStride));

#if	(DBG_LIB == 1)
	if(pVAlloc->Flags & NX_MEM_BLOCK_BUFFER)
	DBGOUT(DBG_LIB,(_T("LuNode(L:%04d, T:%04d), CbNode(L:%04d, T:%04d), CrNode(L:%04d, T:%04d) \n"),
		(pVAlloc->LuAddress&0xFFF), (pVAlloc->LuAddress&0xFFF000)>>12,
		(pVAlloc->CbAddress&0xFFF), (pVAlloc->CbAddress&0xFFF000)>>12,
		(pVAlloc->CrAddress&0xFFF), (pVAlloc->CrAddress&0xFFF000)>>12));
#endif

	/* release video buffers */
	Ret = NX_VideoMemFree(pVAlloc);
	free(pVAlloc);

	DBGOUT(DBG_LIB,(_T("-%s\n"), __FUNCTION__));

	return Ret;
}

VMEMAPI void
NX_SetBlockMemory (
		unsigned int uiBlkDst,
		unsigned int uiVirtDst,
		char Data,
		int  Width,
		int  Height
		)
{
	int x, y;
	unsigned int blkaddr, linaddr, phydst;

	DBGOUT(DBG_LIB,(_T("%s\n"), __FUNCTION__));

	phydst =( uiBlkDst & 0xFFFE0000)		|	// by(32, 32*1, 32*2, ... 4096)
		 	((uiBlkDst & 0x0001F000) >> 6)	|	//  y(32)
		 	((uiBlkDst & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
		 	( uiBlkDst & 0x0000003F);			//  x(64)

	DBGOUT(DBG_LIB,(_T("data:0x%2x (block:0x%08x, phys:0x%08x), size(%d * %d) \n"),
		Data, uiBlkDst, phydst, Width, Height));

	for(y =0; y< (int)Height; y++)
	{
		for(x =0; x< (int)Width; x++)
		{
			blkaddr = uiBlkDst + (y<<12) + x;
			linaddr = 	( blkaddr & 0xFFFE0000)			|	// by(32, 32*1, 32*2, ... 4096)
			     	  	((blkaddr & 0x0001F000) >> 6)	|	//  y(32)
			 	  		((blkaddr & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
				  		( blkaddr & 0x0000003F);	 		//  x(64)

			*(unsigned char *)(uiVirtDst + (linaddr - phydst)) = Data;
		}
	}
}

VMEMAPI void
NX_CopyBlockToLinear(
		unsigned int uiVirtDst,
		unsigned int uiBlkSrc,
		unsigned int uiVirtSrc,
		int Width,
		int Height
		)
{
	int x, y, offset;
	unsigned int blkaddr, linaddr, physrc, tmpblk;
	unsigned char *dst = (unsigned char *)uiVirtDst;

	DBGOUT(DBG_LIB,(_T("%s\n"), __FUNCTION__));

	physrc = 	( uiBlkSrc & 0xFFFE0000)		|	// by(32, 32*1, 32*2, ... 4096)
		 		((uiBlkSrc & 0x0001F000) >> 6)	|	//  y(32)
		 		((uiBlkSrc & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
		 		( uiBlkSrc & 0x0000003F);			//  x(64)

	DBGOUT(DBG_LIB,(_T("src:0x%08x (block:0x%08x, phys:0x%08x) -> dst:0x%08x, size(%d * %d) \n"),
		uiVirtSrc, uiBlkSrc, physrc, uiVirtDst, Width, Height));

	offset = physrc - uiVirtSrc;
	for(y =0; y<Height ; y++)
	{
		blkaddr = uiBlkSrc + (y<<12);
		for(x =0; x<Width ; x++)
		{
			linaddr = 	( blkaddr & 0xFFFE0000)			|	// by(32, 32*1, 32*2, ... 4096)
			     	  	((blkaddr & 0x0001F000) >> 6)	|	//  y(32)
			 	  		((blkaddr & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
				  		( blkaddr & 0x0000003F);	 		//  x(64)
			blkaddr ++;
			*dst++ = *(unsigned char *)(linaddr - offset);
		}
	}
}

VMEMAPI void
NX_CopyLinearToBlock (
		unsigned int uiBlkDst,
		unsigned int uiVirtDst,
		unsigned int uiVirtSrc,
		int Width,
		int Height
		)
{
	int x, y;
	unsigned int blkaddr, linaddr, phydst;
	unsigned char *src = (unsigned char *)uiVirtSrc;

	DBGOUT(DBG_LIB,(_T("%s\n"), __FUNCTION__));

	phydst = 	( uiBlkDst & 0xFFFE0000)		|	// by(32, 32*1, 32*2, ... 4096)
		 		((uiBlkDst & 0x0001F000) >> 6)	|	//  y(32)
		 		((uiBlkDst & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
		 		( uiBlkDst & 0x0000003F);			//  x(64)

	DBGOUT(DBG_LIB,(_T("src:0x%08x->dst:0x%08x (block:0x%08x, phys:0x%08x), size(%d * %d) \n"),
		uiVirtSrc, uiVirtDst, uiBlkDst, phydst, Width, Height));

	for(y =0; y< (int)Height; y++)
	{
		for(x =0; x< (int)Width; x++)
		{
			blkaddr = uiBlkDst + (y<<12) + x;
			linaddr = 	( blkaddr & 0xFFFE0000)			|	// by(32, 32*1, 32*2, ... 4096)
			     	  	((blkaddr & 0x0001F000) >> 6)	|	//  y(32)
			 	  		((blkaddr & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
				  		( blkaddr & 0x0000003F);	 		//  x(64)

			*(unsigned char *)(uiVirtDst + (linaddr - phydst)) = *src++;
		}
	}
}

#if 0

VMEMAPI void NX_CopyYUYVToMVS0 (
		UINT srcVir,
		UINT luVir,
		UINT luBlk,
		UINT cbVir,
		UINT cbBlk,
		UINT crVir,
		UINT crBlk,
		int Width, int Height
		)
{
	int x, y, yEven, yOdd;
	unsigned int luDstLin, cbDstLin, crDstLin;
	unsigned int luDstBlk, cbDstBlk, crDstBlk, tmpLuBlk, tmpCbBlk, tmpCrBlk;
	unsigned int luPhy, cbPhy, crPhy;
	unsigned char *src = (unsigned char *)srcVir;

	luPhy = 	( luBlk & 0xFFFE0000)		|	// by(32, 32*1, 32*2, ... 4096)
		 		((luBlk & 0x0001F000) >> 6)	|	//  y(32)
		 		((luBlk & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
		 		( luBlk & 0x0000003F);			//  x(64)

	cbPhy = 	( cbBlk & 0xFFFE0000)		|	// by(32, 32*1, 32*2, ... 4096)
		 		((cbBlk & 0x0001F000) >> 6)	|	//  y(32)
		 		((cbBlk & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
		 		( cbBlk & 0x0000003F);			//  x(64)

	crPhy = 	( crBlk & 0xFFFE0000)		|	// by(32, 32*1, 32*2, ... 4096)
		 		((crBlk & 0x0001F000) >> 6)	|	//  y(32)
		 		((crBlk & 0x00000FC0) << 5)	|	// bx(64, 64*1, 64*2, ... 4096)
		 		( crBlk & 0x0000003F);			//  x(64)

	for(y =0; y< (int)Height/2; y++)
	{
		//	even
		yEven = y*2;
		tmpLuBlk = luBlk + (yEven<<12);
		tmpCbBlk = cbBlk + (yEven<<12);
		tmpCrBlk = crBlk + (yEven<<12);
		for(x =0; x< (int)Width; x++)
		{
			//	LU
			luDstBlk = tmpLuBlk + x;
			luDstLin = (luDstBlk&0xFFFE0000) | ((luDstBlk&0x0001F000)>>6) | ((luDstBlk&0x00000FC0)<<5) | (luDstBlk&0x0000003F);
			*(unsigned char *)(luVir + (luDstLin - luPhy)) = *src++;

			if( x<<31 ){
				//	Cb
				cbDstBlk = tmpCbBlk + (x>>1);
				cbDstLin = (cbDstBlk&0xFFFE0000) | ((cbDstBlk&0x0001F000)>>6) | ((cbDstBlk&0x00000FC0)<<5) | (cbDstBlk&0x0000003F);
				*(unsigned char *)(cbVir + (cbDstLin - cbPhy)) = *src++;
			}else{
				//	Cr
				crDstBlk = tmpCrBlk + (x>>1);
				crDstLin = (crDstBlk&0xFFFE0000) | ((crDstBlk&0x0001F000)>>6) | ((crDstBlk&0x00000FC0)<<5) | (crDstBlk&0x0000003F);
				*(unsigned char *)(crVir + (crDstLin - crPhy)) = *src++;
			}
		}

		//	odd
		yOdd = y*2+1;
		tmpLuBlk = luBlk + (yOdd<<12);
		for(x =0; x< (int)Width; x++)
		{
			//	LU operation
			luDstBlk = tmpLuBlk + x;
			luDstLin = ( luDstBlk & 0xFFFE0000) | ((luDstBlk & 0x0001F000) >> 6) | ((luDstBlk & 0x00000FC0) << 5) | ( luDstBlk & 0x0000003F);
			*(unsigned char *)(luVir + (luDstLin - luPhy)) = *src++;
			//	skip cb & cr
			src++;
		}
	}
}

#else

VMEMAPI void NX_CopyYUYVToMVS0 (
		UINT srcVir,
		UINT luVir,
		UINT luBlk,
		UINT cbVir,
		UINT cbBlk,
		UINT crVir,
		UINT crBlk,
		int Width, int Height
		)
{
	int x, y, yEven, yOdd;
	unsigned int dstLin, dstBlk, phy;
	unsigned char *src = (unsigned char *)srcVir;
	int offset;

	//	LU
	phy = (luBlk&0xFFFE0000) | ((luBlk&0x0001F000)>>6) | ((luBlk&0x00000FC0)<<5) | (luBlk&0x0000003F);
	offset = phy - luVir;
	for( y=0 ; y<Height ; y++ )
	{
		dstBlk = luBlk + (y<<12);
		for(x=0; x<Width; x++)
		{
			dstLin = (dstBlk&0xFFFE0000) | ((dstBlk&0x0001F000)>>6) | ((dstBlk&0x00000FC0)<<5) | (dstBlk&0x0000003F);
			dstBlk++;
			*(unsigned char *)(dstLin - offset) = *src++;
			src ++;
		}
	}

	//	Cb
	src = (unsigned char *)srcVir;
	phy = (cbBlk&0xFFFE0000) | ((cbBlk&0x0001F000)>>6) | ((cbBlk&0x00000FC0)<<5) | (cbBlk&0x0000003F);
	offset = phy - cbVir;
	for( y=0 ; y<Height ; y+=2 )
	{
		dstBlk = cbBlk + (y<<12);
		for( x=0; x<Width/2 ; x++ )
		{
			dstLin = (dstBlk&0xFFFE0000) | ((dstBlk&0x0001F000)>>6) | ((dstBlk&0x00000FC0)<<5) | (dstBlk&0x0000003F);
			dstBlk ++;
			src ++;
			*(unsigned char *)(dstLin - offset) = *src;
			src += 3;
		}
	}

	//	Cr
	src = (unsigned char *)srcVir;
	phy = (crBlk&0xFFFE0000) | ((crBlk&0x0001F000)>>6) | ((crBlk&0x00000FC0)<<5) | (crBlk&0x0000003F);
	offset = phy - crVir;
	for( y=0; y<Height ; y+=2)
	{
		dstBlk = crBlk + (y<<12);
		for( x=0 ; x<Width/2; x++ )
		{
			dstLin = (dstBlk&0xFFFE0000) | ((dstBlk&0x0001F000)>>6) | ((dstBlk&0x00000FC0)<<5) | (dstBlk&0x0000003F);
			dstBlk ++;
			src += 3;
			*(unsigned char *)(dstLin - offset) = *src;
			src ++;
		}
	}
}

#endif

UINT NX_MapVirtualAddress(UINT uiPhysical, UINT Size, BOOL Cached)
{
	UINT Virtual;
	if(NX_MEM_NOERROR != Mem_MapBase(uiPhysical, Size, &Virtual, Cached))
		return 0;

	return Virtual;
}

void NX_UnMapVirtualAddress(UINT Virtual, UINT Size)
{
	Mem_MapFree(Virtual, Size, NX_MEM_LINEAR_BUFFER);
}

/*------------------------------------------------------------------------------
 * Internal functions.
 *----------------------------------------------------------------------------*/
static int
GetMemoryBase(
		VM_MEMBASE * pMemBase
		)
{
	int Ret = NX_MEM_NOERROR;

	/* check whether initialzed. */
	Ret = Mem_Init();
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	memcpy(pMemBase, &g_VM_MemBase, sizeof(VM_MEMBASE));

	return NX_MEM_NOERROR;
}

static int
SetVideoFormat(
		VIDALLOCMEMORY* pVAlloc,
		VM_VMEMORY    * pVMem
		)
{
	unsigned int FourCC   = pVAlloc->FourCC;
	unsigned int *pFormat = &pVAlloc->Flags;
	unsigned int LFw, LFh, CFw, CFh;

	LFw = pVAlloc->ImgWidth;
	LFh = pVAlloc->ImgHeight;

	*pFormat &= (NX_MEM_BLOCK_BUFFER | NX_MEM_NOT_SEPARATED | NX_MEM_VIDEO_ORDER_YVU | NX_MEM_VIDEO_420_TYPE);
	*pFormat &= ~NX_MEM_VIDEO_420_TYPE;

	switch(FourCC) {
	/* 420 support feacture. */
	case FOURCC_MVS0:	CFw = (LFw>>1),	CFh = (LFh>>1);
						*pFormat |= NX_MEM_VIDEO_420_TYPE;
			break;

	/* 422 support feacture. */
	case FOURCC_MVS2:	CFw = (LFw>>1),	CFh = (LFh);
			break;

	/* 444 support feacture. */
	case FOURCC_MVS4:	CFw  = (LFw),	CFh = (LFh);
			break;

	/* 422 support feacture. */
	case FOURCC_MVN2:	CFw = (LFw>>1),	CFh = (LFh);
						*pFormat |= NX_MEM_NOT_SEPARATED;
			break;

	/* YCbYCr(422) support feacture. */
	case FOURCC_YUYV:	LFw *= 2, CFw  = 0,	CFh = 0;
						*pFormat &= ~NX_MEM_BLOCK_BUFFER;		// Only support Linear memory
						*pFormat &= ~NX_MEM_VIDEO_ORDER_YVU;	// Can't change YCbCr order.
						*pFormat |= NX_MEM_NOT_SEPARATED;		// Only support not serparted memory type.
			break;

	/* 422 support feacture. */
	case FOURCC_YUY2:	CFw = (LFw>>1),	CFh = (LFh);
						*pFormat &= ~NX_MEM_BLOCK_BUFFER;		// Only support Linear memory
						*pFormat &= ~NX_MEM_VIDEO_ORDER_YVU;	// Can't change YCbCr order.
						*pFormat |= NX_MEM_NOT_SEPARATED;		// Only support not serparted memory type.
			break;

	/* 420 support feacture. */
	case FOURCC_YV12:	CFw = (LFw>>1),	CFh = (LFh>>1);
						*pFormat |= NX_MEM_VIDEO_420_TYPE;		// It's 420 type
						*pFormat &= ~NX_MEM_BLOCK_BUFFER;		// Only support Linear memory
						*pFormat |= NX_MEM_VIDEO_ORDER_YVU;		// change YCbCr order to YCrCb
						*pFormat |= NX_MEM_NOT_SEPARATED;		// Only support not serparted memory type.
			break;

	default:
		ERROUT(DBG_ERR,(_T("Fail: Invalid FourCC(%c%c%c%c)\r\n"),
			(char)((FourCC>> 0)&0xFF), (char)((FourCC>> 8)&0xFF),
			(char)((FourCC>>16)&0xFF), (char)((FourCC>>24)&0xFF) ));
			return NX_MEM_INVALID_PARAMS;
	}

	/* Set Video memory struct content. */
	pVMem->LuMemory.Flags     = *pFormat;
	pVMem->LuMemory.HorAlign  = pVAlloc->HorAlign;
	pVMem->LuMemory.VerAlign  = pVAlloc->VerAlign;
	pVMem->LuMemory.MemWidth  = LFw;
	pVMem->LuMemory.MemHeight = LFh;

	pVMem->CbMemory.Flags     = *pFormat;
	pVMem->CbMemory.HorAlign  = pVAlloc->HorAlign;
	pVMem->CbMemory.VerAlign  = pVAlloc->VerAlign;
	pVMem->CbMemory.MemWidth  = CFw;
	pVMem->CbMemory.MemHeight = CFh;

	pVMem->CrMemory.Flags     = *pFormat;
	pVMem->CrMemory.HorAlign  = pVAlloc->HorAlign;
	pVMem->CrMemory.VerAlign  = pVAlloc->VerAlign;
	pVMem->CrMemory.MemWidth  = CFw;
	pVMem->CrMemory.MemHeight = CFh;

	return NX_MEM_NOERROR;
}

static int
AllocVideoMemory(
		VM_VMEMORY * pVMem
		)
{
	unsigned int Flag = pVMem->LuMemory.Flags;
	unsigned int LFw  = pVMem->LuMemory.MemWidth;
	unsigned int CFw  = pVMem->CbMemory.MemWidth;
	unsigned int CFh  = pVMem->CbMemory.MemHeight;

	int Ret = NX_MEM_NOERROR;

	if(Flag & NX_MEM_NOT_SEPARATED)	{
		/* Lu get total video memory. */
		pVMem->LuMemory.MemWidth  = ((Flag & NX_MEM_VIDEO_420_TYPE) ? (LFw + CFw) : (LFw + CFw + CFw));

		/* Not allocate Cb, Cr field. */
		pVMem->CbMemory.MemWidth  = 0;
		pVMem->CbMemory.MemHeight = 0;
		pVMem->CrMemory.MemWidth  = 0;
		pVMem->CrMemory.MemHeight = 0;
	}

	/* Allocate video memory. */
	Ret = Mem_IoCtrl(IOCTL_VMEM_VALLOC, pVMem, sizeof(VM_VMEMORY), pVMem, sizeof(VM_VMEMORY));
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	if(Flag & NX_MEM_NOT_SEPARATED)	{
		pVMem->LuMemory.MemWidth  = LFw;
		pVMem->CbMemory.MemWidth  = CFw;
		pVMem->CbMemory.MemHeight = CFh;
		pVMem->CrMemory.MemWidth  = CFw;
		pVMem->CrMemory.MemHeight = CFh;
	}

	if(! pVMem->LuMemory.Address) {
		ERROUT(DBG_ERR,(_T("Fail: allocate, memory \n")));
		return NX_MEM_ALLOC_FAIL;
	}

	return Ret;
}

static int
MapVideoToLinear(
		VM_VMEMORY    *	pVMem,
		VIDALLOCMEMORY*	pVAlloc
		)
{
	unsigned int FourCC = pVAlloc->FourCC;
	unsigned int Flag   = pVMem->LuMemory.Flags & (NX_MEM_NOT_SEPARATED|NX_MEM_VIDEO_ORDER_YVU);
	unsigned int LFw    = pVMem->LuMemory.MemWidth;
	unsigned int LFh    = pVMem->LuMemory.MemHeight;
	unsigned int CFw    = pVMem->CbMemory.MemWidth;
	unsigned int CFh    = pVMem->CbMemory.MemHeight;
	unsigned int Cml    = CFw ? 1 : 0;	// when YUYV, C filed is 0

	unsigned int   Address;
	unsigned int   Length;
	unsigned int * pVirtual;
	unsigned int * pPhysical;

	int Ret = NX_MEM_NOERROR;

	pVAlloc->LuAddress = pVMem->LuMemory.Address;

	if(NX_MEM_NOT_SEPARATED == Flag) {
		pVAlloc->CbAddress = pVAlloc->LuAddress + (LFw*LFh*Cml);
		pVAlloc->CrAddress = pVAlloc->CbAddress + (CFw*CFh);
	} else if((NX_MEM_NOT_SEPARATED|NX_MEM_VIDEO_ORDER_YVU) == Flag) {
		pVAlloc->CrAddress = pVAlloc->LuAddress + (LFw*LFh*Cml);
		pVAlloc->CbAddress = pVAlloc->CrAddress + (CFw*CFh);
	} else if(NX_MEM_VIDEO_ORDER_YVU == Flag) {
		pVAlloc->CrAddress = pVMem->CbMemory.Address;
		pVAlloc->CbAddress = pVMem->CrMemory.Address;
	} else {
		pVAlloc->CbAddress = pVMem->CbMemory.Address;
		pVAlloc->CrAddress = pVMem->CrMemory.Address;
	}

	pVAlloc->LuPhyAddr = pVAlloc->LuAddress;
	pVAlloc->CbPhyAddr = pVAlloc->CbAddress;
	pVAlloc->CrPhyAddr = pVAlloc->CrAddress;

	DBGOUT(DBG_FUNC,(_T("%s (LuAddr:0x%08x, Cb:0x%08x Cr:0x%08x, Flag:0x%x) \n"),
		__FUNCTION__, pVAlloc->LuAddress, pVAlloc->CbAddress, pVAlloc->CrAddress, Flag));

	if((Flag | NX_MEM_NOT_SEPARATED) && (FourCC != FOURCC_YUYV)) {
		/* Map Lu */
		Address   = pVAlloc->LuAddress;
		Length 	  = (LFw * LFh) + 2 * (CFw * CFh);
		pVirtual  = &pVAlloc->LuVirAddr;
		pPhysical = &pVAlloc->LuPhyAddr;

		if(Length) {
			Ret = Mem_MapAddr(Address, Length, NX_MEM_LINEAR_BUFFER, pVirtual, pPhysical);
			if(NX_MEM_NOERROR != Ret)
				return Ret;
		}

		pVAlloc->CbVirAddr = pVAlloc->LuVirAddr + (pVAlloc->CbPhyAddr - pVAlloc->LuPhyAddr);
		pVAlloc->CrVirAddr = pVAlloc->LuVirAddr + (pVAlloc->CrPhyAddr - pVAlloc->LuPhyAddr);

	} else {
		/* Map Lu */
		Address   = pVAlloc->LuAddress;
		Length 	  = LFw * LFh;
		pVirtual  = &pVAlloc->LuVirAddr;
		pPhysical = &pVAlloc->LuPhyAddr;

		if(Length) {
			Ret = Mem_MapAddr(Address, Length, NX_MEM_LINEAR_BUFFER, pVirtual, pPhysical);
			if(NX_MEM_NOERROR != Ret)
				return Ret;
		}

		/* Map Cb */
		Address   = pVAlloc->CbAddress;
		Length 	  = CFw * CFh;
		pVirtual  = &pVAlloc->CbVirAddr;
		pPhysical = &pVAlloc->CbPhyAddr;

		if(Length) {
			Mem_MapAddr(Address, Length, NX_MEM_LINEAR_BUFFER, pVirtual, pPhysical);
			if(NX_MEM_NOERROR != Ret)
				return Ret;
		}

		/* Map Cr */
		Address   = pVAlloc->CrAddress;
		Length 	  = CFw * CFh;
		pVirtual  = &pVAlloc->CrVirAddr;
		pPhysical = &pVAlloc->CrPhyAddr;

		if(Length) {
			Mem_MapAddr(Address, Length, NX_MEM_LINEAR_BUFFER, pVirtual, pPhysical);
			if(NX_MEM_NOERROR != Ret)
				return Ret;
		}
	}

	/* Set stride */
	pVAlloc->LuStride  = pVMem->LuMemory.MemWidth;
	pVAlloc->CbStride  = pVMem->CbMemory.MemWidth;
	pVAlloc->CrStride  = pVMem->CrMemory.MemWidth;

	/* Set pointer */
	pVAlloc->VAllocPoint = pVMem;

	return Ret;
}

static int
MapVideoToBlock(
		VM_VMEMORY    *	pVMem,
		VIDALLOCMEMORY*	pVAlloc
		)
{
	BOOL FMT420 = ((pVMem->LuMemory.Flags & NX_MEM_VIDEO_420_TYPE) ? TRUE : FALSE);

	unsigned int Flag = pVMem->LuMemory.Flags & (NX_MEM_NOT_SEPARATED | NX_MEM_VIDEO_ORDER_YVU);

	unsigned int CbPX = pVMem->LuMemory.MemWidth;
	unsigned int CrPX = FMT420 ? 0 : pVMem->CbMemory.MemWidth;
	unsigned int CrPY = FMT420 ? pVMem->CbMemory.MemHeight : 0;

	unsigned int   Address;
	unsigned int   Length;
	unsigned int * pVirtual;
	unsigned int * pPhysical;

	int Ret = NX_MEM_NOERROR;

	pVAlloc->LuAddress = pVMem->LuMemory.Address;

	if(Flag == NX_MEM_NOT_SEPARATED) {
		pVAlloc->CbAddress = pVAlloc->LuAddress + CbPX;
		pVAlloc->CrAddress = pVAlloc->CbAddress + (CrPY<<12) + CrPX;
	} else if(Flag == (NX_MEM_NOT_SEPARATED|NX_MEM_VIDEO_ORDER_YVU)) {
		pVAlloc->CrAddress = pVAlloc->LuAddress + CbPX;
		pVAlloc->CbAddress = pVAlloc->CrAddress + (CrPY<<12) + CrPX;
	} else if(Flag == NX_MEM_VIDEO_ORDER_YVU) {
		pVAlloc->CrAddress = pVMem->CbMemory.Address;
		pVAlloc->CbAddress = pVMem->CrMemory.Address;
	} else {
		pVAlloc->CbAddress = pVMem->CbMemory.Address;
		pVAlloc->CrAddress = pVMem->CrMemory.Address;
	}

	/* Map Lu */
	Address   = pVAlloc->LuAddress;
	Length 	  = pVMem->LuMemory.MemWidth*pVMem->LuMemory.MemHeight;
	pVirtual  = &pVAlloc->LuVirAddr;
	pPhysical = &pVAlloc->LuPhyAddr;
	Ret 	  = Mem_MapAddr(Address, Length, NX_MEM_BLOCK_BUFFER, pVirtual, pPhysical);

	if(NX_MEM_NOERROR != Ret)
		return Ret;

	/* Map Cb */
	Address   = pVAlloc->CbAddress;
	Length 	  = pVMem->CbMemory.MemWidth*pVMem->CbMemory.MemHeight;
	pVirtual  = &pVAlloc->CbVirAddr;
	pPhysical = &pVAlloc->CbPhyAddr;
	Ret		  =	Mem_MapAddr(Address, Length, NX_MEM_BLOCK_BUFFER, pVirtual, pPhysical);

	if(NX_MEM_NOERROR != Ret)
		return Ret;

	/* Map Cr */
	Address   = pVAlloc->CrAddress;
	Length 	  = pVMem->CrMemory.MemWidth*pVMem->CrMemory.MemHeight;
	pVirtual  = &pVAlloc->CrVirAddr;
	pPhysical = &pVAlloc->CrPhyAddr;
	Ret		  = Mem_MapAddr(Address, Length, NX_MEM_BLOCK_BUFFER, pVirtual, pPhysical);

	if(NX_MEM_NOERROR != Ret)
		return Ret;

	/* Set stride */
	pVAlloc->LuStride  = VMEM_BLOCK_STRIDE;
	pVAlloc->CbStride  = VMEM_BLOCK_STRIDE;
	pVAlloc->CrStride  = VMEM_BLOCK_STRIDE;

	/* Set pointer */
	pVAlloc->VAllocPoint = pVMem;

	return Ret;
}


