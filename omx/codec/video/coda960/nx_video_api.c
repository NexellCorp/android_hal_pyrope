//
//	Nexel Video En/Decoder API
//
#include <stdlib.h>		//	malloc & free
#include <string.h>		//	memset
#include <unistd.h>		//	close

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "nx_video_api.h"

#include <nx_fourcc.h>
#include <vpu_drv_ioctl.h>		//	Device Driver IOCTL
#include <nx_alloc_mem.h>		//	Memory Allocation Information


#define	WORK_BUF_SIZE		(  80*1024)
#define	STREAM_BUF_SIZE		(1024*1024*4)
#define	PS_SAVE_SIZE		( 320*1024)


//
//	Debug Message Configuration
//
#define	NX_DTAG		"[VPU|API] "		//	
#include "api_osapi.h"
#define	DBG_BUF_ALLOC		0
#define	DBG_ENC_OUT			1
#define DBG_BUF_INFO		0
#define	DBG_VBS				0
#define	DBG_WARNING			1
#define	DBG_USER_DATA		0

#define	DEV_NAME		"/dev/nx_vpu"
#define	RECON_CHROMA_INTERLEAVED	0

#define SLICE_SAVE_SIZE                 (MAX_DEC_PIC_WIDTH*MAX_DEC_PIC_HEIGHT*3/4)

//----------------------------------------------------------------------------
//	define static functions
static int AllocateEncoderMemory( NX_VID_ENC_HANDLE handle );
static int FreeEncoderMemory( NX_VID_ENC_HANDLE handle );
static int AllocateDecoderMemory( NX_VID_DEC_HANDLE hDec);
static int FreeDecoderMemory( NX_VID_DEC_HANDLE hDec );


//////////////////////////////////////////////////////////////////////////////
//
//		Video Encoder APIs
//

struct NX_VIDEO_ENC_INFO
{

	//	open information
	int hEncDrv;		//	Device Driver Handle
	int codecMode;		//	(AVC_ENC = 0x10 / MP4_ENC = 0x12 / NX_JPEG_ENC=0x20 )
	int instIndex;		//	Instance Index

	NX_MEMORY_HANDLE hInstanceBuf;				//	Encoder Instance Memory Buffer

	//	Frame Buffer Information ( for Initialization )
	int refChromaInterleave;					//	Reconstruct & Referernce Buffer Chroma Interleaved
	NX_VID_MEMORY_HANDLE hRefReconBuf[2];		//	Reconstruct & Referernce Buffer Information
	NX_MEMORY_HANDLE hSubSampleBuf[2];			//	Sub Sample Buffer Address
	NX_MEMORY_HANDLE hBitStreamBuf;				//	Bit Stream Buffer
	int isInitialized;

	//	Initialize Output Informations
	VPU_ENC_GET_HEADER_ARG seqInfo;

	//	Encoder Options ( Default CBR Mode )
	int width;
	int height;
	int gopSize;								//	Group Of Pictures' Size
	int frameRateNum;							//	Framerate numerator
	int frameRateDen;							//	Framerate denominator
	int bitRate;								//	BitRate
	int enableSkip;								//	Enable skip frame

	int userQScale;								//	Default User Qunatization Scale

	//	JPEG Specific
	unsigned int frameIndex;
	int rstIntval;
};

NX_VID_ENC_HANDLE NX_VidEncOpen( int codecType )
{
	VPU_OPEN_ARG openArg;
	int ret;

	//	Create Context
	NX_VID_ENC_HANDLE hEnc = (NX_VID_ENC_HANDLE)malloc( sizeof(struct NX_VIDEO_ENC_INFO) );
	memset( hEnc, 0, sizeof(struct NX_VIDEO_ENC_INFO) );

	//	Open Device Driver
	hEnc->hEncDrv = open(DEV_NAME, O_RDWR);
	if( hEnc->hEncDrv < 0 )
	{
		NX_ErrMsg( ("Cannot open device(%s)!!!\n", DEV_NAME) );
		goto ERROR_EXIT;
	}

	switch( codecType )
	{
		case NX_MP4_ENC:
			openArg.codecStd = CODEC_STD_MPEG4;
			break;
		case NX_AVC_ENC:
			openArg.codecStd = CODEC_STD_AVC;
			break;
		case NX_JPEG_ENC:
			openArg.codecStd = CODEC_STD_MJPG;
			break;
		default:
			NX_ErrMsg( ("Invalid codec type (%d)!!!\n", codecType) );
			goto ERROR_EXIT;
	}

	//	Allocate Instance Memory & Stream Buffer
	hEnc->hInstanceBuf =  NX_AllocateMemory( WORK_BUF_SIZE, 4096 );		//	x16 aligned
	if( 0 == hEnc->hInstanceBuf ){
		NX_ErrMsg(("hInstanceBuf allocation failed.\n"));
		goto ERROR_EXIT;
	}

	openArg.instIndex = -1;
	openArg.isEncoder = 1;
	openArg.instanceBuf = *hEnc->hInstanceBuf;

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_OPEN_INSTANCE, &openArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("NX_VidEncOpen() : IOCTL_VPU_OPEN_INSTANCE ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}
	hEnc->instIndex = openArg.instIndex;
	hEnc->codecMode = codecType;
	hEnc->refChromaInterleave = RECON_CHROMA_INTERLEAVED;
	return hEnc;

ERROR_EXIT:
	if( hEnc )
	{
		if( hEnc->hEncDrv > 0 )
		{
			close( hEnc->hEncDrv );
		}
		free( hEnc );
	}
	return NULL;
}

NX_VID_RET NX_VidEncClose( NX_VID_ENC_HANDLE hEnc )
{
	int ret;
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		free( hEnc );
		return -1;
	}

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_CLOSE_INSTANCE, 0 );
	if( ret < 0 )
	{
		NX_ErrMsg( ("NX_VidEncClose() : IOCTL_VPU_CLOSE_INSTANCE ioctl failed!!!\n") );
	}

	FreeEncoderMemory( hEnc );

	close( hEnc->hEncDrv );

	free( hEnc );

	return 0;
}


//NX_VID_RET NX_VidEncInit( NX_VID_ENC_HANDLE hEnc, int width, int height, int gopSize, int bitrate, int fpsNum, int fpsDen, int enableSkip )
NX_VID_RET NX_VidEncInit( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_INIT_PARAM *pParam )
{
	int ret;
	VPU_ENC_SEQ_ARG seqArg;
	VPU_ENC_SET_FRAME_ARG frameArg;
	VPU_ENC_GET_HEADER_ARG *pHdrArg = &hEnc->seqInfo;
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	memset( &seqArg, 0, sizeof( seqArg ) );
	memset( &frameArg, 0, sizeof( frameArg ) );
	memset( pHdrArg, 0, sizeof(VPU_ENC_GET_HEADER_ARG) );

	//	Initialize Encoder
	if( hEnc->isInitialized  )
	{
		NX_ErrMsg( ("Already initialized\n") );
		return -1;
	}

	hEnc->width = pParam->width;
	hEnc->height = pParam->height;

	if( 0 != AllocateEncoderMemory( hEnc ) )
	{
		NX_ErrMsg( ("AllocateEncoderMemory() failed!!\n") );
		return -1;
	}

	seqArg.srcWidth = pParam->width;
	seqArg.srcHeight = pParam->height;

	seqArg.chromaInterleave = pParam->chromaInterleave;
	seqArg.refChromaInterleave = hEnc->refChromaInterleave;

	seqArg.strmBufPhyAddr = hEnc->hBitStreamBuf->phyAddr;
	seqArg.strmBufVirAddr = hEnc->hBitStreamBuf->virAddr;
	seqArg.strmBufSize = hEnc->hBitStreamBuf->size;

	if( hEnc->codecMode != NX_JPEG_ENC )
	{
		seqArg.frameRate = pParam->fpsNum/pParam->fpsDen;
		seqArg.gopSize = pParam->gopSize;

		//	Rate Control
		seqArg.enableRC = pParam->enableRC;
		seqArg.bitrate = pParam->bitrate;
		seqArg.rcAutoSkip = pParam->enableSkip;
		seqArg.maxQScale = pParam->maxQScale;

		if( !seqArg.enableRC && pParam->userQScale==0 )
		{
			hEnc->userQScale = 23;
		}

		//	ME Search Range
		seqArg.searchRange = ME_SEARCH_RAGME_2;
	}
	else
	{
		seqArg.frameRate = 1;
		seqArg.gopSize = 1;
		seqArg.rotAngle = pParam->rotAngle;
		seqArg.mirDirection = pParam->mirDirection;
		seqArg.quality = pParam->jpgQuality;
	}

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_SET_SEQ_PARAM, &seqArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_ENC_SET_SEQ_PARAM ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	if( hEnc->codecMode != NX_JPEG_ENC )
	{
		frameArg.numFrameBuffer = 2;		//	We use always 2 frame
		frameArg.frameBuffer[0] = *hEnc->hRefReconBuf[0];
		frameArg.frameBuffer[1] = *hEnc->hRefReconBuf[1];
		frameArg.subSampleBuffer[0] = *hEnc->hSubSampleBuf[0];
		frameArg.subSampleBuffer[1] = *hEnc->hSubSampleBuf[1];
		//	data partition mode always disabled ( for MPEG4 )
		frameArg.dataPartitionBuffer.phyAddr = 0;
		frameArg.dataPartitionBuffer.virAddr = 0;

		ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_SET_FRAME_BUF, &frameArg );
		if( ret < 0 )
		{
			NX_ErrMsg( ("IOCTL_VPU_ENC_SET_FRAME_BUF ioctl failed!!!\n") );
			goto ERROR_EXIT;
		}

		ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_GET_HEADER, pHdrArg );
		if( ret < 0 )
		{
			NX_ErrMsg( ("IOCTL_VPU_ENC_GET_HEADER ioctl failed!!!\n") );
			goto ERROR_EXIT;
		}
	}
	else
	{
		frameArg.numFrameBuffer = 0;
	}

	hEnc->isInitialized = 1;

ERROR_EXIT:
	return ret;
}


NX_VID_RET NX_VidEncGetSeqInfo( NX_VID_ENC_HANDLE hEnc, unsigned char* seqBuf, int *seqBufSize )
{
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( !hEnc->isInitialized )
	{
		NX_ErrMsg( ("Invalid encoder operation initialize first!!!\n") );
		return -1;
	}

	if( hEnc->codecMode == NX_JPEG_ENC )
	{
		*seqBufSize = 0;
		return 0;
	}

	memcpy( seqBuf, hEnc->seqInfo.avcHeader.spsData, hEnc->seqInfo.avcHeader.spsSize );
	memcpy( seqBuf+hEnc->seqInfo.avcHeader.spsSize, hEnc->seqInfo.avcHeader.ppsData, hEnc->seqInfo.avcHeader.ppsSize );
	*seqBufSize = hEnc->seqInfo.avcHeader.spsSize + hEnc->seqInfo.avcHeader.ppsSize;
	return 0;
}


NX_VID_RET NX_VidEncEncodeFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_MEMORY_HANDLE hInImage, NX_VID_ENC_OUT *pEncOut )
{
	int ret;
	VPU_ENC_RUN_FRAME_ARG runArg;
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	memset( &runArg, 0, sizeof(runArg) );

	runArg.inImgBuffer = *hInImage;
	runArg.changeFlag = 0;
	runArg.enableRc = 1;					//	N/A
	runArg.forceIPicture = 0;
	runArg.quantParam = hEnc->userQScale;
	runArg.skipPicture = 0;
	
	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_RUN_FRAME, &runArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_ENC_RUN_FRAME ioctl failed!!!\n") );
		return -1;
	}

	pEncOut->width = hEnc->width;
	pEncOut->height = hEnc->height;
	pEncOut->isKey = runArg.frameType;
	pEncOut->bufSize = runArg.outStreamSize;
	pEncOut->outBuf = runArg.outStreamAddr;

//	NX_DbgMsg( DBG_ENC_OUT, ("Encoder Output : Success(outputSize = %d, isKey=%d)\n", pEncOut->bufSize, pEncOut->isKey) );
	return 0;
}

//
//		End of Encoder APIs
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//	Jpeg Encoder APIs
//

NX_VID_RET NX_VidEncJpegGetHeader( NX_VID_ENC_HANDLE hEnc, unsigned char *jpgHeader, int *headerSize )
{
	int ret;
	VPU_ENC_GET_HEADER_ARG *pHdrArg = (VPU_ENC_GET_HEADER_ARG *)malloc(sizeof(VPU_ENC_GET_HEADER_ARG));
	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_JPG_GET_HEADER, pHdrArg );
	printf("pHdrArg->jpgHeader.headerSize = %d\n", pHdrArg->jpgHeader.headerSize);
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_JPG_GET_HEADER ioctl failed!!!\n") );
	}
	else
	{
		memcpy( jpgHeader, pHdrArg->jpgHeader.jpegHeader, pHdrArg->jpgHeader.headerSize );
		*headerSize = pHdrArg->jpgHeader.headerSize;
	}
	return ret;
}

NX_VID_RET NX_VidEncJpegRunFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_MEMORY_HANDLE hInImage, NX_VID_ENC_OUT *pEncOut )
{
	int ret;
	VPU_ENC_RUN_FRAME_ARG runArg;
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	memset( &runArg, 0, sizeof(runArg) );
	runArg.inImgBuffer = *hInImage;

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_JPG_RUN_FRAME, &runArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_JPG_RUN_FRAME ioctl failed!!!\n") );
		return -1;
	}

	pEncOut->width = hEnc->width;
	pEncOut->height = hEnc->height;
	pEncOut->bufSize = runArg.outStreamSize;
	pEncOut->outBuf = runArg.outStreamAddr;
	return 0;
}

//
//	Jpeg Encoder APIs
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//	Video Decoder APIs
//


struct NX_VIDEO_DEC_INFO
{
	//	open information
	int hDecDrv;		//	Device Driver Handle
	int codecStd;		//	NX_VPU_CODEC_MODE 	( AVC_DEC = 0, MP2_DEC = 2, MP4_DEC = 3, DV3_DEC = 3, RV_DEC = 4  )
	int instIndex;		//	Instance Index

	int width;
	int height;

	//	Frame Buffer Information ( for Initialization )
	int numFrameBuffers;
	NX_MEMORY_HANDLE hInstanceBuf;				//	Decoder Instance Memory Buffer
	NX_MEMORY_HANDLE hBitStreamBuf;				//	Bit Stream Buffer
	NX_VID_MEMORY_HANDLE hFrameBuffer[MAX_DEC_FRAME_BUFFERS];		//	Reconstruct & Referernce Buffer Information
	NX_MEMORY_HANDLE hColMvBuffer;				//	All Codecs
	NX_MEMORY_HANDLE hSliceBuffer;				//	AVC codec
	NX_MEMORY_HANDLE hPvbSliceBuffer;			//	PVX codec

	int enableUserData;							//	User Data Mode Enable/Disable
	NX_MEMORY_HANDLE hUserDataBuffer;			//	User Data ( MPEG2 Only )

	int isInitialized;

	int useExternalFrameBuffer;
	int numBufferableBuffers;

	//	Initialize Output Informations
	unsigned char	pSeqData[2048];				//	SPS PPS (H.264) or Decoder Specific Information(for MPEG4)
	int seqDataSize;
};


NX_VID_DEC_HANDLE NX_VidDecOpen( int codecType, unsigned int mp4Class )
{
	int ret;
	VPU_OPEN_ARG openArg;
	int workBufSize = WORK_BUF_SIZE;
	//	Create Context
	NX_VID_DEC_HANDLE hDec = (NX_VID_DEC_HANDLE)malloc( sizeof(struct NX_VIDEO_DEC_INFO) );

	memset( hDec, 0, sizeof(struct NX_VIDEO_DEC_INFO) );

	//	Open Device Driver
	hDec->hDecDrv = open(DEV_NAME, O_RDWR);
	if( hDec->hDecDrv < 0 )
	{
		NX_ErrMsg(("Cannot open device(%s)!!!\n", DEV_NAME));
		goto ERROR_EXIT;
	}

	if( codecType == NX_AVC_DEC || codecType == NX_AVC_ENC )
	{
		workBufSize += PS_SAVE_SIZE;
	}
	hDec->hBitStreamBuf = NX_AllocateMemory( STREAM_BUF_SIZE, 4096 );	//	x16 aligned
	if( 0 == hDec->hBitStreamBuf ){
		NX_ErrMsg(("hBitStreamBuf allocation failed.\n"));
		goto ERROR_EXIT;
	}

	//	Allocate Instance Memory & Stream Buffer
	hDec->hInstanceBuf =  NX_AllocateMemory( workBufSize, 4096 );		//	x16 aligned
	if( 0 == hDec->hInstanceBuf ){
		NX_ErrMsg(("hInstanceBuf allocation failed.\n"));
		goto ERROR_EXIT;
	}
	//	Check Driver Validity

	openArg.mp4Class = 0;
	switch( codecType )
	{
		case NX_AVC_DEC:
			openArg.codecStd = CODEC_STD_AVC;
			break;
		case NX_MP2_DEC:
			openArg.codecStd = CODEC_STD_MPEG2;
			break;
		case NX_MP4_DEC:
			openArg.codecStd = CODEC_STD_MPEG4;
			openArg.mp4Class = mp4Class;
			break;
		case NX_H263_DEC:	//	
			openArg.codecStd = CODEC_STD_H263;
			break;
		case NX_DIV3_DEC:	//	
			openArg.codecStd = CODEC_STD_DIV3;
			break;
		case NX_RV_DEC:		// Real Video
			openArg.codecStd = CODEC_STD_RV;
			break;
		case NX_VC1_DEC:	//	WMV
			openArg.codecStd = CODEC_STD_VC1;
			break;
		case NX_VPX_THEORA:	//	Theora
			openArg.codecStd = CODEC_STD_THO;
			break;
		case NX_VPX_VP8:	//	VP8
			openArg.codecStd = CODEC_STD_VP8;
			break;
		default:
			NX_ErrMsg( ("IOCTL_VPU_OPEN_INSTANCE codec Type\n") );
			goto ERROR_EXIT;
	}

	openArg.isEncoder = 0;
	openArg.instIndex = -1;
	openArg.instanceBuf = *hDec->hInstanceBuf;
	openArg.streamBuf = *hDec->hBitStreamBuf;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_OPEN_INSTANCE, &openArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_OPEN_INSTANCE ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}
	hDec->instIndex = openArg.instIndex;
	hDec->codecStd = openArg.codecStd;
	return hDec;
ERROR_EXIT:
	if( hDec->hDecDrv > 0 )
	{
		if( hDec->hInstanceBuf )
		{
			NX_FreeMemory(hDec->hInstanceBuf);
		}
		if( hDec->hBitStreamBuf )
		{
			NX_FreeMemory(hDec->hBitStreamBuf);
		}
		close( hDec->hDecDrv );
		free( hDec );
	}
	return 0;
}


NX_VID_RET NX_VidDecClose( NX_VID_DEC_HANDLE hDec )
{
	int ret;
	if( !hDec )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hDec->hDecDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_CLOSE_INSTANCE, 0 );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_CLOSE_INSTANCE ioctl failed!!!\n") );
	}

	FreeDecoderMemory( hDec );

	close( hDec->hDecDrv );

	free( hDec );

	return 0;
}

//
//	width & height for divx3 mode
//
NX_VID_RET NX_VidDecInit(NX_VID_DEC_HANDLE hDec, unsigned char *seqInfo, int seqSize, int width, int height, NX_VID_SEQ_OUT *seqOut)
{
	int i, ret;
	VPU_DEC_SEQ_INIT_ARG *seqArg = (VPU_DEC_SEQ_INIT_ARG *)calloc(sizeof(VPU_DEC_SEQ_INIT_ARG), 1);
	VPU_DEC_REG_FRAME_ARG *frameArg = (VPU_DEC_REG_FRAME_ARG *)calloc(sizeof(VPU_DEC_REG_FRAME_ARG), 1);
	if( !hDec )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hDec->hDecDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	//	Initialize Encoder
	if( hDec->isInitialized  )
	{
		NX_ErrMsg( ("Already initialized\n") );
		return -1;
	}

	seqArg->seqData = seqInfo;
	seqArg->seqDataSize = seqSize;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_SET_SEQ_INFO, seqArg );

	if(ret == VID_NEED_MORE_BUF)
	{
		goto ERROR_EXIT;
	}
	if( ret != 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed!!!(ret=%d)\n", ret) );
		goto ERROR_EXIT;
	}

	if( seqArg->minFrameBufCnt < 1 || seqArg->minFrameBufCnt > MAX_DEC_FRAME_BUFFERS )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed(nimFrameBufCnt = %d)!!!\n", seqArg->minFrameBufCnt) );
		goto ERROR_EXIT;
	}

	//	Minimum + 3
	hDec->numFrameBuffers = seqArg->minFrameBufCnt+4;
	hDec->width = seqArg->outWidth;
	hDec->height = seqArg->outHeight;

	if( 0 != AllocateDecoderMemory( hDec ) )
	{
		NX_ErrMsg(("AllocateDecoderMemory() Failed!!!\n"));
		goto ERROR_EXIT;
	}

	frameArg->numFrameBuffer = hDec->numFrameBuffers;
	for( i=0 ; i< hDec->numFrameBuffers ; i++ )
	{
		frameArg->frameBuffer[i] = *hDec->hFrameBuffer[i];
	}
	if( hDec->hSliceBuffer )
		frameArg->sliceBuffer = *hDec->hSliceBuffer;
	if( hDec->hColMvBuffer)
		frameArg->colMvBuffer = *hDec->hColMvBuffer;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_REG_FRAME_BUF, frameArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_REG_FRAME_BUF ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	memset( seqOut, 0, sizeof(NX_VID_SEQ_OUT) );
	seqOut->width  = seqArg->outWidth;
	seqOut->height = seqArg->outHeight;
	seqOut->nimBuffers = seqArg->minFrameBufCnt;
	seqOut->numBuffers = hDec->numFrameBuffers;

	hDec->isInitialized = 1;
	if( seqArg )
		free( seqArg );
	if( frameArg )
		free( frameArg );
	return 0;

ERROR_EXIT:
	if( seqArg )
		free( seqArg );
	if( frameArg )
		free( frameArg );
	return ret;
}



//
//	For Android Native Buffer
//
NX_VID_RET NX_VidDecInitWidthBuffer(NX_VID_DEC_HANDLE hDec, unsigned char *seqInfo, int seqSize, int width, int height, NX_VID_MEMORY_HANDLE *pMemHandle, int numBuffers, NX_VID_SEQ_OUT *seqOut)
{
	int i, ret;
	VPU_DEC_SEQ_INIT_ARG *seqArg = NULL;
	VPU_DEC_REG_FRAME_ARG *frameArg = NULL;
	if( !hDec )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hDec->hDecDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( numBuffers > MAX_DEC_FRAME_BUFFERS )
	{
		NX_ErrMsg( ("Too many input buffers for Decoder(Decoder Max Buffer(%d))!!!\n", MAX_DEC_FRAME_BUFFERS) );
		return -1;
	}

	//	Initialize Encoder
	if( hDec->isInitialized  )
	{
		NX_ErrMsg( ("Already initialized\n") );
		return -1;
	}

	seqArg = (VPU_DEC_SEQ_INIT_ARG *)calloc(sizeof(VPU_DEC_SEQ_INIT_ARG), 1);
	frameArg = (VPU_DEC_REG_FRAME_ARG *)calloc(sizeof(VPU_DEC_REG_FRAME_ARG), 1);

	seqArg->seqData = seqInfo;
	seqArg->seqDataSize = seqSize;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_SET_SEQ_INFO, seqArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	NX_DbgMsg(1, ("SeqArg.Width=%d, SeqArg.Height=%d, minCnt=%d\n", seqArg->outWidth, seqArg->outHeight, seqArg->minFrameBufCnt) );

	if( seqArg->minFrameBufCnt < 1 || seqArg->minFrameBufCnt > numBuffers )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed(nimFrameBufCnt = %d)!!!\n", seqArg->minFrameBufCnt) );
		goto ERROR_EXIT;
	}


	//	Allocation & Save Parameter in the decoder handle.
	hDec->numFrameBuffers = numBuffers;
	hDec->width = seqArg->outWidth;
	hDec->height = seqArg->outHeight;
	hDec->useExternalFrameBuffer = 1;

	if( 0 != AllocateDecoderMemory( hDec ) )
	{
		NX_ErrMsg(("AllocateDecoderMemory() Failed!!!\n"));
		goto ERROR_EXIT;
	}

	//	Set Frame Argement Valiable
	frameArg->numFrameBuffer = hDec->numFrameBuffers;
	for( i=0 ; i< hDec->numFrameBuffers ; i++ )
	{
		hDec->hFrameBuffer[i] = pMemHandle[i];
		frameArg->frameBuffer[i] = *hDec->hFrameBuffer[i];
	}
	if( hDec->hSliceBuffer )
		frameArg->sliceBuffer = *hDec->hSliceBuffer;
	if( hDec->hColMvBuffer)
		frameArg->colMvBuffer = *hDec->hColMvBuffer;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_REG_FRAME_BUF, frameArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_REG_FRAME_BUF ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	memset( seqOut, 0, sizeof(NX_VID_SEQ_OUT) );
	seqOut->width  = seqArg->outWidth;
	seqOut->height = seqArg->outHeight;
	seqOut->nimBuffers = seqArg->minFrameBufCnt;
	seqOut->numBuffers = hDec->numFrameBuffers;

	if( seqOut->numBuffers - seqOut->nimBuffers < 4 )
	{
		NX_ErrMsg(("Too Many Requre Frame Buffer !!!(Min=%d)\n", seqOut->nimBuffers) );
		goto ERROR_EXIT;
	}

	hDec->isInitialized = 1;
	if( seqArg )
		free( seqArg );
	if( frameArg )
		free( frameArg );
	return 0;

ERROR_EXIT:
	if( seqArg )
		free( seqArg );
	if( frameArg )
		free( frameArg );
	return ret;
}

NX_VID_RET NX_VidDecInit2(NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *seqIn, NX_VID_SEQ_OUT *seqOut)
{
	int i, ret=-1;
	VPU_DEC_SEQ_INIT_ARG *seqArg = (VPU_DEC_SEQ_INIT_ARG *)calloc(sizeof(VPU_DEC_SEQ_INIT_ARG), 1);
	VPU_DEC_REG_FRAME_ARG *frameArg = (VPU_DEC_REG_FRAME_ARG *)calloc(sizeof(VPU_DEC_REG_FRAME_ARG), 1);
	if( !hDec )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		goto ERROR_EXIT;
	}
	if( hDec->hDecDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		goto ERROR_EXIT;
	}
	//	Initialize Encoder
	if( hDec->isInitialized  )
	{
		NX_ErrMsg( ("Already initialized\n") );
		goto ERROR_EXIT;
	}

	seqArg->seqData = seqIn->seqInfo;
	seqArg->seqDataSize = seqIn->seqSize;
	seqArg->enableUserData = seqIn->enableUserData && (hDec->codecStd == CODEC_STD_MPEG2);

	//NX_DbgMsg(DBG_USER_DATA, ("================================================\n"));
	//NX_DbgMsg(DBG_USER_DATA, ("============== hDec->codecStd = %d\n", hDec->codecStd));
	//NX_DbgMsg(DBG_USER_DATA, ("============== seqArg->enableUserData = %d\n", seqArg->enableUserData));
	//NX_DbgMsg(DBG_USER_DATA, ("============== seqIn->enableUserData = %d\n", seqIn->enableUserData));

	if( seqArg->enableUserData )
	{
		NX_DbgMsg(DBG_USER_DATA, ("Enabled user data\n"));
		hDec->enableUserData = 1;
		hDec->hUserDataBuffer = NX_AllocateMemory( 0x10000, 4096 );		//	x16 aligned
		if( 0 == hDec->hUserDataBuffer ){
			NX_ErrMsg(("hUserDataBuffer allocation failed.(size=%d,align=%d)\n", 0x10000, 4096));
			goto ERROR_EXIT;
		}
		seqArg->userDataBuffer = *hDec->hUserDataBuffer;
	}
	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_SET_SEQ_INFO, seqArg );
	if( ret == VID_NEED_MORE_BUF )
		goto ERROR_EXIT;
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	if( seqArg->minFrameBufCnt < 1 || seqArg->minFrameBufCnt > MAX_DEC_FRAME_BUFFERS )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed(nimFrameBufCnt = %d)!!!\n", seqArg->minFrameBufCnt) );
		goto ERROR_EXIT;
	}

	if( seqIn->numBuffers > 0 )
	{
		hDec->useExternalFrameBuffer = 1;
		hDec->numFrameBuffers = seqIn->numBuffers;
	}
	else
	{
		hDec->numFrameBuffers = seqArg->minFrameBufCnt+4;
	}
	//	Allocation & Save Parameter in the decoder handle.
	hDec->width = seqArg->outWidth;
	hDec->height = seqArg->outHeight;

	if( 0 != AllocateDecoderMemory( hDec ) )
	{
		NX_ErrMsg(("AllocateDecoderMemory() Failed!!!\n"));
		goto ERROR_EXIT;
	}

	//	Set Frame Argement Valiable
	frameArg->numFrameBuffer = hDec->numFrameBuffers;
	for( i=0 ; i< hDec->numFrameBuffers ; i++ )
	{
		if( hDec->useExternalFrameBuffer )
			hDec->hFrameBuffer[i] = seqIn->pMemHandle[i];
		frameArg->frameBuffer[i] = *hDec->hFrameBuffer[i];
	}
	if( hDec->hSliceBuffer )
		frameArg->sliceBuffer = *hDec->hSliceBuffer;
	if( hDec->hColMvBuffer)
		frameArg->colMvBuffer = *hDec->hColMvBuffer;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_REG_FRAME_BUF, frameArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_REG_FRAME_BUF ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	memset( seqOut, 0, sizeof(NX_VID_SEQ_OUT) );
	seqOut->width  = seqArg->outWidth;
	seqOut->height = seqArg->outHeight;
	seqOut->nimBuffers = seqArg->minFrameBufCnt;
	seqOut->numBuffers = hDec->numFrameBuffers;

	if( (seqOut->numBuffers - seqOut->nimBuffers < 2) && hDec->useExternalFrameBuffer )
	{
		NX_DbgMsg( DBG_WARNING, ("[Warning] External Buffer too small.(min=%d, buffers=%d)\n", seqOut->nimBuffers, hDec->numFrameBuffers) );
	}

	hDec->isInitialized = 1;
	if( seqArg )
		free( seqArg );
	if( frameArg )
		free( frameArg );
	return ret;

ERROR_EXIT:
	if( seqArg )
		free( seqArg );
	if( frameArg )
		free( frameArg );
	return ret;
}



NX_VID_RET NX_VidDecDecodeFrame( NX_VID_DEC_HANDLE hDec, unsigned char *strmBuf, int strmSize, NX_VID_DEC_OUT *pDecOut )
{
	int ret;
	VPU_DEC_DEC_FRAME_ARG decArg;

	//	Initialize Encoder
	if( !hDec->isInitialized  )
	{
		NX_ErrMsg( ("%s Line(%d) : Not initialized!!!\n", __func__, __LINE__));
		return -1;
	}

	memset( &decArg, 0, sizeof(decArg) );
	decArg.strmData = strmBuf;
	decArg.strmDataSize = strmSize;
	decArg.iFrameSearchEnable = 0;
	decArg.skipFrameMode = 0;
	decArg.decSkipFrameNum = 0;
	pDecOut->outImgIdx = -1;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_RUN_FRAME, &decArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_RUN_FRAME ioctl failed!!!\n") );
		return -1;
	}

	if( ret > 0 )
		return ret;

	pDecOut->width     = decArg.outWidth;
	pDecOut->height    = decArg.outHeight;
	pDecOut->isKey     = decArg.picType;
	pDecOut->outImgIdx = decArg.indexFrameDisplay;

	if( pDecOut->outImgIdx >= 0 && hDec->numFrameBuffers > pDecOut->outImgIdx )
	{
		pDecOut->outImg = *hDec->hFrameBuffer[decArg.indexFrameDisplay];
#if DBG_BUF_INFO
		{
			int j=0;
			NX_MEMORY_INFO *memInfo;
			for( j=0 ; j<3 ; j++ )
			{
				memInfo = (NX_MEMORY_INFO *)pDecOut->outImg.privateDesc[j];
				NX_DbgMsg( DBG_BUF_INFO, ("privateDesc = 0x%.8x\n", memInfo->privateDesc ) );
				NX_DbgMsg( DBG_BUF_INFO, ("align       = 0x%.8x\n", memInfo->align       ) );
				NX_DbgMsg( DBG_BUF_INFO, ("size        = 0x%.8x\n", memInfo->size        ) );
				NX_DbgMsg( DBG_BUF_INFO, ("virAddr     = 0x%.8x\n", memInfo->virAddr     ) );
				NX_DbgMsg( DBG_BUF_INFO, ("phyAddr     = 0x%.8x\n", memInfo->phyAddr     ) );
			}
		}
#endif
	}
	else
	{
		pDecOut->outImgIdx = -1;
	}

	NX_RelMsg( 0, ("NX_VidDecDecodeFrame() Resol:%dx%d, iskey=%d, imgIdx = %d\n", pDecOut->width, pDecOut->height, pDecOut->isKey, pDecOut->outImgIdx) );

	return 0;
}

NX_VID_RET NX_VidDecFlush( NX_VID_DEC_HANDLE hDec )
{
	int ret;
	if( !hDec->isInitialized  )
	{
		NX_ErrMsg( ("%s Line(%d) : Not initialized!!!\n", __func__, __LINE__));
		return -1;
	}
	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_FLUSH, NULL );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_FLUSH ioctl failed!!!\n") );
		return -1;
	}

	return 0;
}

NX_VID_RET NX_VidDecClrDspFlag( NX_VID_DEC_HANDLE hDec, NX_VID_MEMORY_HANDLE hFrameBuf, int frameIdx )
{
	int ret;
	VPU_DEC_CLR_DSP_FLAG_ARG clrFlagArg;
	if( !hDec->isInitialized  )
	{
		return -1;
	}
	clrFlagArg.indexFrameDisplay = frameIdx;
	if( NULL != hFrameBuf )
	{
		//	Optional
		clrFlagArg.frameBuffer = *hFrameBuf;
	}
	else
	{
		memset( &clrFlagArg.frameBuffer, 0, sizeof(clrFlagArg.frameBuffer) );
	}

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_CLR_DSP_FLAG, &clrFlagArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_CLR_DSP_FLAG ioctl failed!!!\n") );
		return -1;
	}

	return 0;
}


//
//	Video Decoder APIs
//
//////////////////////////////////////////////////////////////////////////////





//////////////////////////////////////////////////////////////////////////////
//
//	Static Internal Functions
//

static int AllocateEncoderMemory( NX_VID_ENC_HANDLE hEnc )
{
	int width, height;

	if( !hEnc || hEnc->hEncDrv<=0 )
	{
		NX_ErrMsg(("invalid encoder handle or driver handle!!!\n"));
		return -1;
	}

	//	Make alligned x16
	width  = ((hEnc->width  + 15) >> 4)<<4;
	height = ((hEnc->height + 15) >> 4)<<4;

	if( hEnc->codecMode == NX_JPEG_ENC )
	{
		int jpegStreamBufSize = width * height * 1.5;
		hEnc->hRefReconBuf[0] = NULL;
		hEnc->hRefReconBuf[1] = NULL;
		hEnc->hSubSampleBuf[0] = NULL;
		hEnc->hSubSampleBuf[1] = NULL;
		hEnc->hBitStreamBuf = NX_AllocateMemory( jpegStreamBufSize, 16 );		//	x16 aligned
		if( 0 == hEnc->hBitStreamBuf ){
			NX_ErrMsg(("hBitStreamBuf allocation failed.(size=%d,align=%d)\n", ENC_BITSTREAM_BUFFER, 16));
			goto ERROR_EXIT;
		}
	}
	else
	{
		int fourcc = FOURCC_MVS0;
		if( hEnc->refChromaInterleave ){
			fourcc = FOURCC_NV12;	//	2 Planar 420( Luminunce Plane + Cb/Cr Interleaved Plane )
		}
		else
		{
			fourcc = FOURCC_MVS0;	//	3 Planar 420( Luminunce plane + Cb Plane + Cr Plane )
		}

		hEnc->hRefReconBuf[0] = NX_VideoAllocateMemory( 64, width, height, NX_MEM_MAP_LINEAR, fourcc );
		if( 0 == hEnc->hRefReconBuf[0] ){
			NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(recon0)\n", width, height));
			goto ERROR_EXIT;
		}

		hEnc->hRefReconBuf[1] = NX_VideoAllocateMemory( 64, width, height, NX_MEM_MAP_LINEAR, fourcc );
		if( 0 == hEnc->hRefReconBuf[1] ){
			NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(recon1)\n", width, height));
			goto ERROR_EXIT;
		}

		hEnc->hSubSampleBuf[0] = NX_AllocateMemory( width*height/4, 16 );	//	x16 aligned
		if( 0 == hEnc->hSubSampleBuf[0] ){
			NX_ErrMsg(("hSubSampleBuf allocation failed.(size=%d,align=%d)\n", width*height, 16));
			goto ERROR_EXIT;
		}

		hEnc->hSubSampleBuf[1] = NX_AllocateMemory( width*height/4, 16 );	//	x16 aligned
		if( 0 == hEnc->hSubSampleBuf[1] ){
			NX_ErrMsg(("hSubSampleBuf allocation failed.(size=%d,align=%d)\n", width*height, 16));
			goto ERROR_EXIT;
		}

		hEnc->hBitStreamBuf = NX_AllocateMemory( ENC_BITSTREAM_BUFFER, 16 );		//	x16 aligned
		if( 0 == hEnc->hBitStreamBuf ){
			NX_ErrMsg(("hBitStreamBuf allocation failed.(size=%d,align=%d)\n", ENC_BITSTREAM_BUFFER, 16));
			goto ERROR_EXIT;
		}
	}

#if (DBG_BUF_ALLOC)
	NX_DbgMsg( DBG_BUF_ALLOC, ("Allocate Encoder Memory\n") );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hRefReconBuf[0]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", hEnc->hRefReconBuf[0]->luPhyAddr, hEnc->hRefReconBuf[0]->cbPhyAddr, hEnc->hRefReconBuf[0]->crPhyAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hRefReconBuf[1]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", hEnc->hRefReconBuf[1]->luPhyAddr, hEnc->hRefReconBuf[1]->cbPhyAddr, hEnc->hRefReconBuf[1]->crPhyAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hSubSampleBuf[0] : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hSubSampleBuf[0]->phyAddr, hEnc->hSubSampleBuf[0]->virAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hSubSampleBuf[1] : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hSubSampleBuf[1]->phyAddr, hEnc->hSubSampleBuf[1]->virAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hBitStreamBuf    : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hBitStreamBuf->phyAddr, hEnc->hBitStreamBuf->virAddr) );
#endif	//	DBG_BUF_ALLOC

	return 0;

ERROR_EXIT:
	FreeEncoderMemory( hEnc );
	return -1;
}

static int FreeEncoderMemory( NX_VID_ENC_HANDLE handle )
{
	if( !handle )
	{
		NX_ErrMsg(("invalid encoder handle!!!\n"));
		return -1;
	}

	//	Free Reconstruct Buffer & Reference Buffer
	if( handle->hRefReconBuf[0] )
	{
		NX_FreeVideoMemory( handle->hRefReconBuf[0] );
		handle->hRefReconBuf[0] = 0;
	}
	if( handle->hRefReconBuf[1] )
	{
		NX_FreeVideoMemory( handle->hRefReconBuf[1] );
		handle->hRefReconBuf[1] = 0;
	}

	//	Free SubSampleb Buffer
	if( handle->hSubSampleBuf[0] )
	{
		NX_FreeMemory( handle->hSubSampleBuf[0] );
		handle->hSubSampleBuf[0] = 0;
	}
	if( handle->hSubSampleBuf[1] )
	{
		NX_FreeMemory( handle->hSubSampleBuf[1] );
		handle->hSubSampleBuf[1] = 0;
	}

	//	Free Bitstream Buffer
	if( handle->hBitStreamBuf )
	{
		NX_FreeMemory( handle->hBitStreamBuf );
		handle->hBitStreamBuf = 0;
	}

	if( handle->hInstanceBuf )
	{
		NX_FreeMemory(handle->hInstanceBuf);
		handle->hInstanceBuf = 0;
	}

	return 0;
}


static int AllocateDecoderMemory( NX_VID_DEC_HANDLE hDec )
{
	int i, width, height, mvSize;

	if( !hDec || !hDec->hDecDrv )
	{
		NX_ErrMsg(("invalid encoder handle or driver handle!!!\n"));
		return -1;
	}

	//	Make alligned x16
	width  = ((hDec->width  + 15) >> 4)<<4;
	height = ((hDec->height + 15) >> 4)<<4;

	//	
	mvSize = ((hDec->width+31)&~31)*((hDec->height+31)&~31);
	mvSize = (mvSize*3)/2;
	mvSize = (mvSize+4) / 5;
	mvSize = ((mvSize+7)/ 8) * 8;
	mvSize = ((mvSize + 4096-1)/4096) * 4096;

	if( width==0 || height==0 || mvSize==0 )
	{
		NX_ErrMsg(("Invalid memory parameters!!!(width=%d, height=%d, mvSize=%d)\n", width, height, mvSize));
		return -1;
	}

	if( !hDec->useExternalFrameBuffer )
	{
		NX_RelMsg( 1, ( "resole : %dx%d, numFrameBuffers=%d\n", width, height, hDec->numFrameBuffers ));
		for( i=0 ; i<hDec->numFrameBuffers ; i++ )
		{
			hDec->hFrameBuffer[i] = NX_VideoAllocateMemory( 4096, width, height, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );	//	Planar Lu/Cb/Cr
			if( 0 == hDec->hFrameBuffer[i] ){
				NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(i=%d)\n", width, height, i));
				goto ERROR_EXIT;
			}
		}
	}


	hDec->hColMvBuffer = NX_AllocateMemory( mvSize*hDec->numFrameBuffers, 4096 );	//	Planar Lu/Cb/Cr
	if( 0 == hDec->hColMvBuffer ){
		NX_ErrMsg(("hColMvBuffer allocation failed(size=%d, align=%d)\n", mvSize*hDec->numFrameBuffers, 4096));
		goto ERROR_EXIT;
	}

	if( hDec->codecStd == CODEC_STD_AVC )
	{
		hDec->hSliceBuffer = NX_AllocateMemory( 2048*2048*3/4, 4096 );		//	x16 aligned
		if( 0 == hDec->hSliceBuffer ){
			NX_ErrMsg(("hSliceBuffer allocation failed.(size=%d,align=%d)\n",  2048*2048*3/4, 4096));
			goto ERROR_EXIT;
		}
	}

	if( hDec->codecStd == CODEC_STD_THO || hDec->codecStd == CODEC_STD_VP3 || hDec->codecStd == CODEC_STD_VP8 )
	{
		hDec->hPvbSliceBuffer = NX_AllocateMemory( 17*4*(2048*2048/256), 4096 );		//	x16 aligned
		if( 0 == hDec->hPvbSliceBuffer ){
			NX_ErrMsg(("hPvbSliceBuffer allocation failed.(size=%d,align=%d)\n", 17*4*(2048*2048/256), 4096));
			goto ERROR_EXIT;
		}
	}

#if DBG_BUF_ALLOC
	NX_DbgMsg( DBG_BUF_ALLOC, ("Allocate Decoder Memory\n") );
	for( i=0 ; i<hDec->numFrameBuffers ; i++ )
	{
		NX_DbgMsg( DBG_BUF_ALLOC, ("    hFrameBuffer[%d]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", i, hDec->hFrameBuffer[i]->luPhyAddr, hDec->hFrameBuffer[i]->cbPhyAddr, hDec->hFrameBuffer[i]->crPhyAddr) );
		NX_DbgMsg( DBG_BUF_ALLOC, ("    hFrameBuffer[%d]  : LuVir(0x%08x), CbVir(0x%08x), CrVir(0x%08x)\n", i, hDec->hFrameBuffer[i]->luVirAddr, hDec->hFrameBuffer[i]->cbVirAddr, hDec->hFrameBuffer[i]->crVirAddr) );
	}
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hBitStreamBuf    : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hDec->hBitStreamBuf->phyAddr, hDec->hBitStreamBuf->virAddr) );
#endif	//	DBG_BUF_ALLOC

	return 0;

ERROR_EXIT:
	FreeDecoderMemory( hDec );
	return -1;
}

static int FreeDecoderMemory( NX_VID_DEC_HANDLE hDec )
{
	int i;
	if( !hDec )
	{
		NX_ErrMsg(("invalid encoder handle!!!\n"));
		return -1;
	}

	if( !hDec->useExternalFrameBuffer )
	{
		//	Free Frame Buffer
		for( i=0 ; i<hDec->numFrameBuffers ; i++ )
		{
			if( hDec->hFrameBuffer[i] )
			{
				NX_FreeVideoMemory( hDec->hFrameBuffer[i] );
				hDec->hFrameBuffer[i] = 0;
			}
		}
	}

	if( hDec->hColMvBuffer )
	{
		NX_FreeMemory( hDec->hColMvBuffer );
		hDec->hColMvBuffer = 0;
	}

	if( hDec->hSliceBuffer )
	{
		NX_FreeMemory( hDec->hSliceBuffer );
		hDec->hSliceBuffer = 0;
	}

	//	Allocate Instance Memory & Stream Buffer
	if( hDec->hInstanceBuf )
	{
		NX_FreeMemory( hDec->hInstanceBuf );
		hDec->hInstanceBuf = 0;
	}

	//	Free Bitstream Buffer
	if( hDec->hBitStreamBuf )
	{
		NX_FreeMemory( hDec->hBitStreamBuf );
		hDec->hBitStreamBuf = 0;
	}

	//	Free USerdata Buffer
	if( hDec->hUserDataBuffer )
	{
		NX_FreeMemory( hDec->hUserDataBuffer );
		hDec->hUserDataBuffer = 0;
	}

	return 0;
}

//
//	End of Static Functions
//
//////////////////////////////////////////////////////////////////////////////
