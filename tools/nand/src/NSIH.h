//-----------------------------------------------------------------------------
//	Copyright (C) 2012 Nexell Co., All Rights Reserved
//	Nexell Co. Proprietary < Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//	AND WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//	BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//	FOR A PARTICULAR PURPOSE.
//
//	Module		: second boot
//	File		: secondboot.h
//	Description	: This must be synchronized with NSIH.txt
//	Author		: Firmware Team
//	History		:
// 				Hans 2013-06-23 Create
//------------------------------------------------------------------------------
#ifndef __NX_SECONDBOOT_H__
#define __NX_SECONDBOOT_H__
typedef unsigned char	U8;
typedef unsigned int	U32;

struct NX_NANDBootInfo
{
	U8	AddrStep;
	U8	tCOS;
	U8	tACC;
	U8	tOCH;
	U32 PageSize;
	U32	_Reserved;
};

struct NX_SPIBootInfo
{
	U8	AddrStep;
	U8	_Reserved0[3];
	U32 _Reserved[2];
};

struct NX_SDFSBootInfo
{
	char BootFileName[12];		// 8.3 format ex)"NXBoot.3rd"	
};

union NX_DeviceBootInfo
{
	struct NX_NANDBootInfo NANDBI;
	struct NX_SPIBootInfo SPIBI;
	struct NX_SDFSBootInfo SDFSBI;
};

struct NX_SecondBootInfo
{
	U32 VECTOR[8];			// 0x000 ~ 0x01C
	U32 VECTOR_Rel[8];		// 0x020 ~ 0x03C

	U32 DEVICEADDR;			// 0x040

	U32 LOADSIZE;			// 0x044
	U32 LOADADDR;			// 0x048
	U32 LAUNCHADDR;			// 0x04C
	union NX_DeviceBootInfo DBI;	// 0x050~0x058

	U32 PLL[4];				// 0x05C ~ 0x068
	U32 PLLSPREAD[2];		// 0x06C ~ 0x070
	U32 DVO[5];				// 0x074 ~ 0x084

	U32 MEMTiming[3];		// 0x084 ~ 0x08C
	U32 MEMCtrlCfg[5];		// 0x090 ~ 0x0A0
	U32 PHYConfig[3];		// 0x0A4 ~ 0x0AC
	U32 DDRMemCfg[4];		// 0x0B0 ~ 0x0BC

	U32 AXIBottomSlot[32];	// 0x0C0 ~ 0x13C
	U32 AXIDisplaySlot[32];	// 0x140 ~ 0x1BC

	U32 Stub[(0x1FC-0x1C8)/4];
	U32 SIGNATURE;			// 0x1FC	"NSIH"
};

// [0] : Use ICache
// [1] : Change PLL
// [2] : Decrypt
// [3] : Suspend Check

#endif //__NX_SECONDBOOT_H__
