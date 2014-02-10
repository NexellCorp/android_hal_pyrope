/*
 * (C) Copyright 2010
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 */
#ifndef UNDER_CE
#include "platform.h"

extern unsigned int	g_InitStatus;
extern VM_MEMBASE	g_VM_MemBase;
extern BOOL		 	g_Is_ValidLinMem;
extern BOOL		 	g_Is_ValidBlkMem;
extern int			g_VmemIOHandle;

int Mem_Init(void)
{
	if(! g_InitStatus) {
		if(NX_MEM_NOERROR != Mem_InitBase())
			return NX_MEM_ERROR;
		g_InitStatus = 1;
	}

	return NX_MEM_NOERROR;
}

int Mem_InitBase(void)
{
	int Ret = NX_MEM_NOERROR;

	/* get memory(linear/block) physical base address. */
	Ret = Mem_IoCtrl(IOCTL_VMEM_INFO, 0, 0, &g_VM_MemBase, sizeof(VM_MEMBASE));
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	DBGOUT(DBG_FUNC,(_T("Done: Linear phys:0x%08x, virt:0x%08x, size:0x%08x \n"),
		g_VM_MemBase.LinPhyBase, g_VM_MemBase.LinVirBase, g_VM_MemBase.LinVirSize));
	DBGOUT(DBG_FUNC,(_T("Done: Block  phys:0x%08x, virt:0x%08x, size:0x%08x \n"),
		g_VM_MemBase.BlkPhyBase, g_VM_MemBase.BlkVirBase, g_VM_MemBase.BlkPhySize));

	/*
	 * map linear physical memory(linear/block) base to virtual base.
	 *  note> liner memory allocated from system memory
	 */
	g_Is_ValidLinMem = TRUE;
	g_Is_ValidBlkMem = FALSE;

	if (! g_VM_MemBase.BlkPhySize)
			return Ret;

	/*	map block physical memory(linear/block) base to virtual base. */
	Ret = Mem_MapBase(g_VM_MemBase.BlkPhyBase, g_VM_MemBase.BlkPhySize, &g_VM_MemBase.BlkVirBase, FALSE);
	if(NX_MEM_NOERROR != Ret)
		return Ret;

	if(g_VM_MemBase.BlkVirBase)
		g_Is_ValidBlkMem = TRUE;

	return Ret;
}

int Mem_MapBase(
		unsigned int   Physical,
		unsigned int   Size,
		unsigned int * pVirtual,
		BOOL		   Cached
		)
{
	const char *Name = "/dev/mem";
	int Ret  = NX_MEM_NOERROR;
	int File = 0;

	unsigned int Flags = O_RDWR;

	if (FALSE == Cached)
		Flags |= O_SYNC;

	File = open(Name, Flags, 644);
	if(0 > File) {
		ERROUT(DBG_ERR,(_T("Fail: %s No such file or directory\n"), Name));
		return NX_MEM_OPEN_FAIL;
	}

	*pVirtual = (unsigned int)mmap(0, Size, PROT_READ|PROT_WRITE, MAP_SHARED, File, Physical);
	if((unsigned)-1 == (unsigned)*pVirtual) {
		ERROUT(DBG_ERR,(_T("Fail: %s mmap, phys:0x%x, size:0x%x\n"), Name, Physical, Size));
		Ret = NX_MEM_MMAPP_FAIL;
	}

#if (CLEAR_MMAP_ZONE == 1)
	memset((void*)(*pVirtual), 0, Size);
#endif

	DBGOUT(DBG_FUNC,(_T("Done: mmap phys:0x%08x to virt:0x%08x, size:0x%08x, %s\n"),
		Physical, *pVirtual, Size, Cached?"Cached":"Non Cached"));

	close(File);

	return Ret;
}

int Mem_MapAddr(
		unsigned int   Address,
		unsigned int   Size,
		unsigned int   MemType,
		unsigned int * pVirtual,
		unsigned int * pPhysical
		)
{
	VM_MEMBASE * pMemBase = &g_VM_MemBase;

	/* allocate linear memory */
	if(NX_MEM_LINEAR_BUFFER == MemType) {
		const char *Name = "/dev/mem";
		int Ret  = NX_MEM_NOERROR;
		int File = 0;

		File = open(Name, O_RDWR|O_SYNC);
		if(0 > File) {
			ERROUT(DBG_ERR,(_T("Fail: %s No such file or directory\n"), Name));
			return NX_MEM_OPEN_FAIL;
		}

		*pVirtual = (unsigned int)mmap(0, Size, PROT_READ|PROT_WRITE, MAP_SHARED, File, Address);
		if((unsigned)-1 == (unsigned)*pVirtual) {
			ERROUT(DBG_ERR,(_T("Fail: %s mmap, phys:0x%x, size:0x%x\n"), Name, Address, Size));
			Ret = NX_MEM_MMAPP_FAIL;
		}

		DBGOUT(DBG_FUNC,(_T("Done: mmap, phys:0x%08x to virt:0x%08x, size:0x%x\n"),
			Address, *pVirtual, Size));

		close(File);

		return Ret;

	/* allocate block memory */
	} else {
		unsigned int xc, blxc, yc, blyc;

		Address -= VMEM_BLOCK_ZONE;

		if( pMemBase->BlkPhyBase > Address ||
			Address > (pMemBase->BlkPhyBase + pMemBase->BlkPhySize) ) {
			ERROUT(DBG_ERR,(_T("Fail: block mmap, phys:0x%08x (0x%08x ~ 0x%08x) \n"),
				Address, pMemBase->BlkPhyBase, (pMemBase->BlkPhyBase + pMemBase->BlkPhySize)));
			return NX_MEM_MMAPP_FAIL;
		}

		xc	= (Address & 0x0000003f);			// X coordinate.
		blxc= (Address & 0x00000fc0) << 5;	// block X coordinate.
		yc  = (Address & 0x0001f000) >> 6;	// Y coordinate.
		blyc= (Address & 0xfffe0000);			// block T coordinate.

		*pPhysical = (blyc | blxc | yc | xc);
		*pVirtual  = pMemBase->BlkVirBase + (*pPhysical - pMemBase->BlkPhyBase);

		DBGOUT(DBG_FUNC,(_T("Done: mmap, left=%04d, top=%04d \n"),
			(Address&0xFFF), (Address&0xFFF000)>>12));
	}

	return NX_MEM_NOERROR;
}

int Mem_MapFree(
		unsigned int Address,
		unsigned int Size,
		unsigned int MemType
		)
{
	if(NX_MEM_LINEAR_BUFFER == MemType)
		munmap((unsigned int*)Address, Size);

	DBGOUT(DBG_FUNC,(_T("Done: %s ummap, addr:0x%08x \n"),
		NX_MEM_LINEAR_BUFFER==MemType?"linear":"block ", Address));

	return NX_MEM_NOERROR;
}

int Mem_IoCtrl(
		unsigned int IoCode,
		void 	   * InArgs,
		int 		 InSize,
		void 	   * OutArgs,
		int 		 OutSize
		)
{
	const char *Name = VM_DEV_NAME;
	void *pArgs = InArgs;

	DBGOUT(DBG_FUNC,(_T("%s(IoCode:0x%x, IOHandle=0x%x) \n"), __FUNCTION__, IoCode, g_VmemIOHandle));

	if( g_VmemIOHandle < 0 ){
		int File = open(Name, O_RDWR, 644);
		if(0 > File) {
			ERROUT(DBG_ERR,(_T("Fail: %s No such file or directory\n"), Name));
			return NX_MEM_OPEN_FAIL;
		}
		g_VmemIOHandle = File;
	}

	if(0 == InSize)
		pArgs = OutArgs;

	if(NX_MEM_NOERROR != ioctl(g_VmemIOHandle, IoCode, pArgs)) {
		ERROUT(DBG_ERR,(_T("Fail: %s Inappropriate iocode:0x%x, nr:%d \n"),
			Name, IoCode, _IOC_NR(IoCode)));
		return NX_MEM_IOCTL_FAIL;
	}

	return NX_MEM_NOERROR;
}

#endif
