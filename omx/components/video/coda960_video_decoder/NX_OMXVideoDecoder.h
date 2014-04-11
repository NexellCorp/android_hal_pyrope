#ifndef __NX_OMXVideoDecoder_h__
#define __NX_OMXVideoDecoder_h__

#ifdef NX_DYNAMIC_COMPONENTS
//	This Function need for dynamic registration
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComponent);
#else
//	static registration
OMX_ERRORTYPE NX_VideoDecoder_ComponentInit (OMX_HANDLETYPE hComponent);
#endif

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Video.h>
#include <NX_OMXBasePort.h>
#include <NX_OMXBaseComponent.h>
#include <NX_OMXSemaphore.h>
#include <NX_OMXQueue.h>

#include <hardware/gralloc.h>
#include <cutils/native_handle.h>
#include <gralloc_priv.h>

#include <nx_fourcc.h>
#include <nx_video_api.h>

#define FFDEC_VID_VER_MAJOR			0
#define FFDEC_VID_VER_MINOR			1
#define FFDEC_VID_VER_REVISION		0
#define FFDEC_VID_VER_NSTEP			0

#define	VPUDEC_VID_NUM_PORT			2
#define	VPUDEC_VID_INPORT_INDEX		0
#define	VPUDEC_VID_OUTPORT_INDEX	1

#define	NX_OMX_MAX_BUF				32

#define	VID_INPORT_MIN_BUF_CNT		10					//	Max 6 Avaliable
#define	VID_INPORT_MIN_BUF_SIZE		(1024*1024*4)		//	32 Mbps( 32Mbps, 1fps )

//	Default Native Buffer Mode's buffers & buffer size
#define	VID_OUTPORT_MIN_BUF_CNT_THUMB	4
#define	VID_OUTPORT_MIN_BUF_CNT		12					//	Max Avaiable Frames
#define	VID_OUTPORT_MIN_BUF_SIZE	(4*1024)			//	Video Memory Structure Size

#define	VID_TEMP_IN_BUF_SIZE		(4*1024*1024)

#define	MAX_DEC_SPECIFIC_DATA		(1024)

#ifndef UNUSED_PARAM
#define	UNUSED_PARAM(X)		X=X
#endif


#define	DEBUG_ANDROID	1
#define	DEBUG_BUFFER	0
#define	DEBUG_FUNC		0
#define	TRACE_ON		0
#define	DEBUG_FLUSH		0
#define	DEBUG_STATE		0

#if DEBUG_BUFFER
#define	DbgBuffer(fmt,...)	DbgMsg(fmt, ##__VA_ARGS__)
#else
#define DbgBuffer(fmt,...)	do{}while(0)
#endif

#if	TRACE_ON
#define	TRACE(fmt,...)		DbgMsg(fmt, ##__VA_ARGS__)
#else
#define	TRACE(fmt,...)		do{}while(0)
#endif

#if	DEBUG_FUNC
#define	FUNC_IN				DbgMsg("%s() In\n", __FUNCTION__)
#define	FUNC_OUT			DbgMsg("%s() OUT\n", __FUNCTION__)
#else
#define	FUNC_IN				do{}while(0)
#define	FUNC_OUT			do{}while(0)
#endif

#if DEBUG_STATE
#define	DBG_STATE(fmt,...)		DbgMsg(fmt, ##__VA_ARGS__)
#else
#define	DBG_STATE(fmt,...)		do{}while(0)
#endif

#if DEBUG_FLUSH
#define	DBG_FLUSH(fmt,...)		DbgMsg(fmt, ##__VA_ARGS__)
#else
#define	DBG_FLUSH(fmt,...)		do{}while(0)
#endif



struct OutBufferTimeInfo{
	OMX_TICKS			timestamp;
	OMX_U32				flag;
};

typedef struct tNX_VIDDEC_VIDEO_COMP_TYPE NX_VIDDEC_VIDEO_COMP_TYPE;

//	Define Transform Template Component Type
struct tNX_VIDDEC_VIDEO_COMP_TYPE{
	NX_BASECOMPONENTTYPE		//	Nexell Base Component Type	
	/*					Buffer Thread							*/
	pthread_t					hBufThread;
	pthread_mutex_t				hBufMutex;
	NX_THREAD_CMD				eCmdBufThread;
	NX_SEMAPHORE				*hBufAllocSem;						//	Buffer allocation semaphore ( Semaphore )
	NX_SEMAPHORE				*hBufCtrlSem;						//	Buffer thread control semaphore ( Mutex )
	NX_SEMAPHORE				*hBufChangeSem;						//	Buffer status change semaphore ( Event )
	/*					Port Information						*/
	NX_BASEPORTTYPE				*pInputPort;						//	Input Port
	NX_BASEPORTTYPE				*pOutputPort;						//	Output Port

	/*				Video Format				*/
	OMX_VIDEO_PARAM_PORTFORMATTYPE	inputFormat;
	OMX_VIDEO_PARAM_PORTFORMATTYPE	outputFormat;

	OMX_BUFFERHEADERTYPE		*pInputBuffers[NX_OMX_MAX_BUF];		//	Input Buffers

	// Management Output Buffer
	OMX_BUFFERHEADERTYPE		*pOutputBuffers[NX_OMX_MAX_BUF];	//	Output Buffer Pool ( NULL is Empty )
	OMX_S32						outBufferUseFlag[NX_OMX_MAX_BUF];	//	Output Buffer Use Flag( Flag for Decoding )
	OMX_S32						outUsableBuffers;					//	Max Allocated Buffers or Max Usable Buffers
	OMX_S32						curOutBuffers;						//	Currently Queued Buffer Counter
	OMX_S32						minRequiredFrameBuffer;				//	Minimum H/W Required FrameBuffer( Sequence Output )
	OMX_S32						outBufferable;						//	Display Buffers
	struct OutBufferTimeInfo	outTimeStamp[NX_OMX_MAX_BUF];		//	Output Timestamp

	OMX_U32						outBufferAllocSize;					//	Native Buffer Mode vs ThumbnailMode
	OMX_U32						numOutBuffers;						//

	NX_QUEUE					*pInputPortQueue;
	NX_QUEUE					*pOutputPortQueue;

	OMX_BOOL					isOutIdr;

	union {
		OMX_VIDEO_PARAM_AVCTYPE avcType;
		OMX_VIDEO_PARAM_MPEG2TYPE mpeg2Type;
		OMX_VIDEO_PARAM_MPEG4TYPE mpeg4Type;
		OMX_VIDEO_PARAM_H263TYPE h263Type;
		OMX_VIDEO_PARAM_WMVTYPE wmvType;
	} codecType;

	//	for decoding
	OMX_BOOL					bFlush;
	NX_VID_DEC_HANDLE			hVpuCodec;
	OMX_S32						videoCodecId;
	OMX_BOOL					bInitialized;
	OMX_BOOL					bNeedKey;
	OMX_BOOL					bStartEoS;
	OMX_BOOL					frameDelay;
	OMX_U16						rvFrameCnt;

	int							(*DecodeFrame)(NX_VIDDEC_VIDEO_COMP_TYPE *, NX_QUEUE *, NX_QUEUE *);

	//	Decoder Temporary Buffer
	OMX_U8						tmpInputBuffer[VID_TEMP_IN_BUF_SIZE];

	OMX_S32						width, height;

	OMX_BOOL					bUseNativeBuffer;
	OMX_BOOL					bEnableThumbNailMode;

	//	Extra Informations &
	OMX_U8						codecSpecificData[MAX_DEC_SPECIFIC_DATA];
	OMX_S32						codecSpecificDataSize;

	//	FFMPEG Parser Data
	OMX_U8						*pExtraData;
	OMX_S32						nExtraDataSize;
	OMX_S32						codecTag;

	//	Native WindowBuffer
	NX_VID_MEMORY_INFO			vidFrameBuf[MAX_DEC_FRAME_BUFFERS];	//	Video Buffer Info
	NX_VID_MEMORY_HANDLE		hVidFrameBuf[MAX_DEC_FRAME_BUFFERS];

	//	for Debugging
	OMX_S32						inFrameCount;
	OMX_S32						outFrameCount;
};


void InitVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
void PushVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_TICKS timestamp, OMX_U32 flag );
int PopVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_TICKS *timestamp, OMX_U32 *flag );
int flushVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
int openVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
void closeVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);

#endif	//	__NX_OMXVideoDecoder_h__
