#ifndef __VPU_DRV_IOCTL_H__
#define __VPU_DRV_IOCTL_H__

#include "vpu_types.h"

#define	NX_VPU_DRV_MAGIC			0xC0DE

enum {
	IOCTL_VPU_OPEN_INSTANCE = (NX_VPU_DRV_MAGIC)<<16,			//	+0
	IOCTL_VPU_CLOSE_INSTANCE,									//	+1

	//	Encoder Specific ( MAGIC + 2~5 )
	IOCTL_VPU_ENC_SET_SEQ_PARAM,								//	+2
	IOCTL_VPU_ENC_SET_FRAME_BUF,								//	+3
	IOCTL_VPU_ENC_GET_HEADER,									//	+4
	IOCTL_VPU_ENC_RUN_FRAME,									//	+5

	//	Decoder Specific ( MAGIC + 6~9 )
	IOCTL_VPU_DEC_SET_SEQ_INFO,									//	+6
	IOCTL_VPU_DEC_REG_FRAME_BUF,								//	+7
	IOCTL_VPU_DEC_RUN_FRAME,									//	+8
	IOCTL_VPU_DEC_FLUSH,										//	+9
	IOCTL_VPU_DEC_CLR_DSP_FLAG,									//	+10

	//	Jpeg Specific ( MAGIC + 10 ~ 11 )
	IOCTL_VPU_JPG_GET_HEADER,									//	+11
	IOCTL_VPU_JPG_RUN_FRAME,									//	+12
};



#endif	//	__VPU_DRV_IOCTL_H__
