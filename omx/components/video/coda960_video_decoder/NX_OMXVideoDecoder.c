#define	LOG_TAG				"NX_VIDDEC"

#include <assert.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <NX_OMXBasePort.h>
#include <NX_OMXBaseComponent.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include <stdbool.h>
#include <ion/ion.h>
// psw0523 add for gralloc new
#include <ion-private.h>
#include <nexell_format.h>

#include "NX_OMXVideoDecoder.h"


//	Default Recomanded Functions for Implementation Components
static OMX_ERRORTYPE NX_VidDec_GetConfig(OMX_HANDLETYPE hComp, OMX_INDEXTYPE nConfigIndex, OMX_PTR pComponentConfigStructure);
static OMX_ERRORTYPE NX_VidDec_GetParameter (OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex,OMX_PTR ComponentParamStruct);
static OMX_ERRORTYPE NX_VidDec_SetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR ComponentParamStruct);
static OMX_ERRORTYPE NX_VidDec_UseBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer);
static OMX_ERRORTYPE NX_VidDec_AllocateBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** pBuffer, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes);
static OMX_ERRORTYPE NX_VidDec_FreeBuffer (OMX_HANDLETYPE hComponent, OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE NX_VidDec_EmptyThisBuffer (OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE NX_VidDec_FillThisBuffer(OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE NX_VidDec_ComponentDeInit(OMX_HANDLETYPE hComponent);
static void          NX_VidDec_CommandThread( void *arg );
static void          NX_VidDec_BufferMgmtThread( void *arg );


int flushVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
int openVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
void closeVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
int decodeVideoFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);

static OMX_S32		gstNumInstance = 0;
static OMX_S32		gstMaxInstance = 2;


#ifdef NX_DYNAMIC_COMPONENTS
//	This Function need for dynamic registration
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComponent)
#else
//	static registration
OMX_ERRORTYPE NX_VideoDecoder_ComponentInit (OMX_HANDLETYPE hComponent)
#endif
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_U32 i=0;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp;

	FUNC_IN;

	if( gstNumInstance >= gstMaxInstance )
	{
		ErrMsg( "%s() : Instance Creation Failed = %ld\n", __func__, gstNumInstance );
		return OMX_ErrorInsufficientResources;
	}

	pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)NxMalloc(sizeof(NX_VIDDEC_VIDEO_COMP_TYPE));
	if( pDecComp == NULL ){
		return OMX_ErrorInsufficientResources;
	}

	///////////////////////////////////////////////////////////////////
	//					Component configuration
	NxMemset( pDecComp, 0, sizeof(NX_VIDDEC_VIDEO_COMP_TYPE) );
	pComp->pComponentPrivate = pDecComp;

	//	Initialize Base Component Informations
	if( OMX_ErrorNone != (eError=NX_BaseComponentInit( pComp )) ){
		NxFree( pDecComp );
		return eError;
	}
	NX_OMXSetVersion( &pComp->nVersion );
	//					End Component configuration
	///////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////
	//						Port configurations
	//	Create ports
	for( i=0; i<VPUDEC_VID_NUM_PORT ; i++ )
	{
		pDecComp->pPort[i] = (OMX_PTR)NxMalloc(sizeof(NX_BASEPORTTYPE));
		NxMemset( pDecComp->pPort[i], 0, sizeof(NX_BASEPORTTYPE) );
		pDecComp->pBufQueue[i] = (OMX_PTR)NxMalloc(sizeof(NX_QUEUE));
	}
	pDecComp->nNumPort = VPUDEC_VID_NUM_PORT;
	//	Input port configuration
	pDecComp->pInputPort = (NX_BASEPORTTYPE *)pDecComp->pPort[VPUDEC_VID_INPORT_INDEX];
	pDecComp->pInputPortQueue = (NX_QUEUE *)pDecComp->pBufQueue[VPUDEC_VID_INPORT_INDEX];
	NX_InitQueue(pDecComp->pInputPortQueue, NX_OMX_MAX_BUF);
	pPort = pDecComp->pInputPort;

	NX_OMXSetVersion( &pPort->stdPortDef.nVersion );
	NX_InitOMXPort( &pPort->stdPortDef, VPUDEC_VID_INPORT_INDEX, OMX_DirInput, OMX_TRUE, OMX_PortDomainVideo );
	pPort->stdPortDef.nBufferCountActual = VID_INPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferCountMin    = VID_INPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferSize        = VID_INPORT_MIN_BUF_SIZE;

	//	Output port configuration
	pDecComp->pOutputPort = (NX_BASEPORTTYPE *)pDecComp->pPort[VPUDEC_VID_OUTPORT_INDEX];
	pDecComp->pOutputPortQueue = (NX_QUEUE *)pDecComp->pBufQueue[VPUDEC_VID_OUTPORT_INDEX];
	NX_InitQueue(pDecComp->pOutputPortQueue, NX_OMX_MAX_BUF);
	pPort = pDecComp->pOutputPort;
	NX_OMXSetVersion( &pPort->stdPortDef.nVersion );
	NX_InitOMXPort( &pPort->stdPortDef, VPUDEC_VID_OUTPORT_INDEX, OMX_DirOutput, OMX_TRUE, OMX_PortDomainVideo );
	pPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferSize        = VID_OUTPORT_MIN_BUF_SIZE;
	//					End Port configurations
	///////////////////////////////////////////////////////////////////

	//	Base overrided functions
	pComp->GetComponentVersion    = NX_BaseGetComponentVersion    ;
	pComp->SendCommand            = NX_BaseSendCommand            ;
	pComp->SetConfig              = NX_BaseSetConfig              ;
	pComp->GetExtensionIndex      = NX_BaseGetExtensionIndex      ;
	pComp->GetState               = NX_BaseGetState               ;
	pComp->ComponentTunnelRequest = NX_BaseComponentTunnelRequest ;
	pComp->SetCallbacks           = NX_BaseSetCallbacks           ;
	pComp->UseEGLImage            = NX_BaseUseEGLImage            ;
	pComp->ComponentRoleEnum      = NX_BaseComponentRoleEnum      ;


	//	Specific implemented functions
	pComp->GetConfig              = NX_VidDec_GetConfig           ;
	pComp->GetParameter           = NX_VidDec_GetParameter        ;
	pComp->SetParameter           = NX_VidDec_SetParameter        ;
	pComp->UseBuffer              = NX_VidDec_UseBuffer           ;
	pComp->AllocateBuffer         = NX_VidDec_AllocateBuffer      ;
	pComp->FreeBuffer             = NX_VidDec_FreeBuffer          ;
	pComp->EmptyThisBuffer        = NX_VidDec_EmptyThisBuffer     ;
	pComp->FillThisBuffer         = NX_VidDec_FillThisBuffer      ;
	pComp->ComponentDeInit        = NX_VidDec_ComponentDeInit     ;

	//	Create command thread
	NX_InitQueue( &pDecComp->cmdQueue, NX_MAX_QUEUE_ELEMENT );
	pDecComp->hSemCmd = NX_CreateSem( 0, NX_MAX_QUEUE_ELEMENT );
	pDecComp->hSemCmdWait = NX_CreateSem( 0, 1 );
	pDecComp->eCmdThreadCmd = NX_THREAD_CMD_RUN;
	pthread_create( &pDecComp->hCmdThread, NULL, (void*)&NX_VidDec_CommandThread , pDecComp ) ;
	NX_PendSem( pDecComp->hSemCmdWait );

	pDecComp->compName = strdup("OMX.NX.VIDEO_DECODER");
	pDecComp->compRole = strdup("video_decoder");			//	Default Component Role

	//	Buffer
	pthread_mutex_init( &pDecComp->hBufMutex, NULL );
	pDecComp->hBufAllocSem = NX_CreateSem(0, 16);

	pDecComp->hVpuCodec = NULL;		//	Initialize Video Process Unit Handler
	pDecComp->videoCodecId = -1;
	pDecComp->bInitialized = OMX_FALSE;
	pDecComp->bNeedKey = OMX_FALSE;
	pDecComp->codecSpecificDataSize = 0;
	pDecComp->outBufferable = 1;
	pDecComp->outUsableBuffers = 0;
	pDecComp->curOutBuffers = 0;
	pDecComp->minRequiredFrameBuffer = 1;


	//	Set Video Output Port Information
	pDecComp->outputFormat.eColorFormat = OMX_COLOR_FormatYUV420Planar;
	pDecComp->bUseNativeBuffer = OMX_FALSE;
	pDecComp->bEnableThumbNailMode = OMX_FALSE;

	pDecComp->outBufferAllocSize = 0;
	pDecComp->numOutBuffers = 0;


	gstNumInstance ++;

	FUNC_OUT;

	return OMX_ErrorNone;
}

static OMX_ERRORTYPE NX_VidDec_ComponentDeInit(OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pComp->pComponentPrivate;
	OMX_U32 i=0;
	FUNC_IN;

	//	prevent duplacation
	if( NULL == pComp->pComponentPrivate )
		return OMX_ErrorNone;

	// Destroy command thread
	pDecComp->eCmdThreadCmd = NX_THREAD_CMD_EXIT;
	NX_PostSem( pDecComp->hSemCmdWait );
	NX_PostSem( pDecComp->hSemCmd );
	pthread_join( pDecComp->hCmdThread, NULL );
	NX_DeinitQueue( &pDecComp->cmdQueue );
	//	Destroy Semaphore
	NX_DestroySem( pDecComp->hSemCmdWait );
	NX_DestroySem( pDecComp->hSemCmd );

	//	Destroy port resource
	for( i=0; i<VPUDEC_VID_NUM_PORT ; i++ ){
		if( pDecComp->pPort[i] ){
			NxFree(pDecComp->pPort[i]);
		}
		if( pDecComp->pBufQueue[i] ){
			//	Deinit Queue
			NX_DeinitQueue( pDecComp->pBufQueue[i] );
			NxFree( pDecComp->pBufQueue[i] );
		}
	}

	NX_BaseComponentDeInit( hComponent );

	//	Buffer
	pthread_mutex_destroy( &pDecComp->hBufMutex );
	NX_DestroySem(pDecComp->hBufAllocSem);

	//
	if( pDecComp->compName )
		free(pDecComp->compName);
	if( pDecComp->compRole )
		free(pDecComp->compRole);

	if( pDecComp ){
		NxFree(pDecComp);
		pComp->pComponentPrivate = NULL;
	}

	gstNumInstance --;
	FUNC_OUT;
	return OMX_ErrorNone;
}


static OMX_ERRORTYPE NX_VidDec_GetConfig(OMX_HANDLETYPE hComp, OMX_INDEXTYPE nConfigIndex, OMX_PTR pComponentConfigStructure)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComp;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	TRACE("%s(0x%08x) In\n", __FUNCTION__, nConfigIndex);

	switch ( nConfigIndex )
	{
		case OMX_IndexConfigCommonOutputCrop:
		{
			OMX_CONFIG_RECTTYPE *pRect = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
		    if ((pRect->nPortIndex != 0) && (pRect->nPortIndex != 1))
			{
		        return OMX_ErrorBadPortIndex;
		    }
		    pRect->nTop = 0;
		    pRect->nLeft = 0;
			pRect->nWidth = pDecComp->pInputPort->stdPortDef.format.video.nFrameWidth;
			pRect->nHeight = pDecComp->pInputPort->stdPortDef.format.video.nFrameHeight;
			TRACE("%s() : width(%ld), height(%ld)\n ", __func__, pRect->nWidth, pRect->nHeight );
			break;
		}
		default:
		    return NX_BaseGetConfig(hComp, nConfigIndex, pComponentConfigStructure);
	}
	return OMX_ErrorNone;
}


static OMX_ERRORTYPE NX_VidDec_GetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex,OMX_PTR ComponentParamStruct)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComp;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	TRACE("%s(0x%08x) In\n", __FUNCTION__, nParamIndex);
	switch( nParamIndex )
	{
		case OMX_IndexParamVideoPortFormat:
		{
			OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParamStruct;
			TRACE("%s() : OMX_IndexParamVideoPortFormat : port Index = %ld, format index = %ld\n", __FUNCTION__, pVideoFormat->nPortIndex, pVideoFormat->nIndex );
			if( pVideoFormat->nIndex != 0 ){
				return OMX_ErrorNoMore;
			}
			if( pVideoFormat->nPortIndex == 0 )	//	Input Information
			{
				//	Set from component Role
				memcpy( pVideoFormat, &pDecComp->inputFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE) );
			}
			else
			{
				//	Output
				memcpy( pVideoFormat, &pDecComp->outputFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE) );
				pVideoFormat->eColorFormat = OMX_COLOR_FormatYUV420Planar;
			}
			break;
		}
		case OMX_IndexAndroidNativeBufferUsage:
		{
			struct GetAndroidNativeBufferUsageParams *pBufUsage = (struct GetAndroidNativeBufferUsageParams *)ComponentParamStruct;
			if( pBufUsage->nPortIndex != 1 )
				return OMX_ErrorBadPortIndex;
			pBufUsage->nUsage |= (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);	//	Enable Native Buffer Usage
			break;
		}
		case OMX_IndexParamPortDefinition:
		{
			OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParamStruct;
			OMX_ERRORTYPE error = OMX_ErrorNone;
			error = NX_BaseGetParameter( hComp, nParamIndex, ComponentParamStruct );
			if( error != OMX_ErrorNone )
			{
				DbgMsg("Error NX_BaseGetParameter() failed !!! for OMX_IndexParamPortDefinition \n");
				return error;
			}
			if( pPortDef->nPortIndex == 1 )
			{
				pPortDef->format.video.eColorFormat = HAL_PIXEL_FORMAT_YV12;
			}
			break;
		}
		default :
			return NX_BaseGetParameter( hComp, nParamIndex, ComponentParamStruct );
	}
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE NX_VidDec_SetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR ComponentParamStruct)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComp;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;

	TRACE("%s(0x%08x) In\n", __FUNCTION__, nParamIndex);

	switch( nParamIndex )
	{
		case OMX_IndexParamStandardComponentRole:
		{
			OMX_PARAM_COMPONENTROLETYPE *pInRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParamStruct;
			if( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.avc"  )  )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingAVC;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= 0;
				pDecComp->videoCodecId = NX_AVC_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.mpeg4") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= 0;
				pDecComp->videoCodecId = NX_MP4_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.mpeg2") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= 0;
				pDecComp->videoCodecId = NX_MP2_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.h263") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingH263;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= 0;
				pDecComp->videoCodecId = NX_H263_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.x-ms-wmv") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingWMV;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= 0;
				pDecComp->videoCodecId = NX_VC1_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.rv") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingRV;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= 0;
				pDecComp->videoCodecId = NX_RV_DEC;
			}
			else
			{
				//	Error
				NX_ErrMsg("Error: %s(): in role = %s\n", __FUNCTION__, (OMX_STRING)pInRole->cRole );
				return OMX_ErrorBadParameter;
			}

			//	Set Output Fomrmat
			pDecComp->outputFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
			pDecComp->outputFormat.eColorFormat = OMX_COLOR_FormatYUV420Planar;
			pDecComp->outputFormat.nPortIndex= 1;

			if( pDecComp->compRole ){
				free ( pDecComp->compRole );
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
			}
			DbgMsg("%s(): Set new role : in role = %s, comp role = %s\n", __FUNCTION__, (OMX_STRING)pInRole->cRole, pDecComp->compRole );
			break;
		}

		case OMX_IndexParamVideoPortFormat:
		{
			OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParamStruct;
			if( pVideoFormat->nPortIndex == 0 ){
				memcpy( &pDecComp->inputFormat, pVideoFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE) );
			}else{
				memcpy( &pDecComp->outputFormat, pVideoFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE) );
			}
			break;
		}

		case OMX_IndexParamVideoAvc:
		{
			OMX_VIDEO_PARAM_AVCTYPE *pAvcParam = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParamStruct;
			TRACE("%s(): In, (nParamIndex=0x%08x)OMX_IndexParamVideoAvc\n", __FUNCTION__, nParamIndex );
			if( pAvcParam->eProfile > OMX_VIDEO_AVCProfileHigh )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoAvc failed!! Cannot support profile(%d)\n", pAvcParam->eProfile);
				return OMX_ErrorPortsNotCompatible;
			}
			if( pAvcParam->eLevel > OMX_VIDEO_AVCLevel51 )		//	???
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoAvc failed!! Cannot support profile(%d)\n", pAvcParam->eLevel);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexParamVideoMpeg2:
		{
			OMX_VIDEO_PARAM_MPEG2TYPE *pMpeg2Param = (OMX_VIDEO_PARAM_MPEG2TYPE *)ComponentParamStruct;
			TRACE("%s(): In, (nParamIndex=0x%08x)OMX_IndexParamVideoMpeg2\n", __FUNCTION__, nParamIndex );
			if( pMpeg2Param->eProfile != OMX_VIDEO_MPEG2ProfileSimple || pMpeg2Param->eProfile != OMX_VIDEO_MPEG2ProfileMain || pMpeg2Param->eProfile != OMX_VIDEO_MPEG2ProfileHigh )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoMpeg2 failed!! Cannot support profile(%d)\n", pMpeg2Param->eProfile);
				return OMX_ErrorPortsNotCompatible;
			}
			if( pMpeg2Param->eLevel != OMX_VIDEO_MPEG2LevelLL || pMpeg2Param->eLevel != OMX_VIDEO_MPEG2LevelML || pMpeg2Param->eLevel != OMX_VIDEO_MPEG2LevelHL )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoMpeg2 failed!! Cannot support level(%d)\n", pMpeg2Param->eLevel);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexParamVideoMpeg4:
		{
			OMX_VIDEO_PARAM_MPEG4TYPE *pMpeg4Param = (OMX_VIDEO_PARAM_MPEG4TYPE *)ComponentParamStruct;
			TRACE("%s(): In, (nParamIndex=0x%08x)OMX_VIDEO_PARAM_MPEG4TYPE\n", __FUNCTION__, nParamIndex );
			if( pMpeg4Param->eProfile >  OMX_VIDEO_MPEG4ProfileAdvancedSimple )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoMpeg4 failed!! Cannot support profile(%d)\n", pMpeg4Param->eProfile);
				return OMX_ErrorPortsNotCompatible;
			}
			if( pMpeg4Param->eLevel > OMX_VIDEO_MPEG4Level5 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoMpeg4 failed!! Cannot support level(%d)\n", pMpeg4Param->eLevel);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexParamVideoH263:
		{
			OMX_VIDEO_PARAM_H263TYPE *pH263Param = (OMX_VIDEO_PARAM_H263TYPE *)ComponentParamStruct;
			TRACE("%s(): In, (nParamIndex=0x%08x)OMX_VIDEO_PARAM_H263TYPE\n", __FUNCTION__, nParamIndex );
			if( pH263Param->eProfile > OMX_VIDEO_H263ProfileISWV2 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoH263 failed!! Cannot support profile(%d)\n", pH263Param->eProfile);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexParamVideoWmv:
		{
			OMX_VIDEO_PARAM_WMVTYPE *pWmvParam = (OMX_VIDEO_PARAM_WMVTYPE *)ComponentParamStruct;
			DbgMsg("%s(): In, (nParamIndex=0x%08x)OMX_VIDEO_PARAM_WMVTYPE\n", __FUNCTION__, nParamIndex );
			if( pWmvParam->eFormat > OMX_VIDEO_WMVFormat9 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoWmv failed!! Cannot support format(%d)\n", pWmvParam->eFormat);
				return OMX_ErrorPortsNotCompatible;
			}
			memcpy( &pDecComp->codecType.wmvType, pWmvParam, sizeof(OMX_VIDEO_PARAM_WMVTYPE));
			break;
		}

		case OMX_IndexParamVideoRv:
		{
			OMX_VIDEO_PARAM_RVTYPE *pRvParam = (OMX_VIDEO_PARAM_RVTYPE *)ComponentParamStruct;
			TRACE("%s(): In, (nParamIndex=0x%08x)OMX_VIDEO_PARAM_RVTYPE\n", __FUNCTION__, nParamIndex );
			if( pRvParam->eFormat > OMX_VIDEO_RVFormatG2 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoRv failed!! Cannot support format(%d)\n", pRvParam->eFormat);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexEnableAndroidNativeBuffers:
		{
			struct EnableAndroidNativeBuffersParams *pEnNativeBuf = (struct EnableAndroidNativeBuffersParams *)ComponentParamStruct;
			if( pEnNativeBuf->nPortIndex != 1 )
				return OMX_ErrorBadPortIndex;
			pDecComp->bUseNativeBuffer = pEnNativeBuf->enable;
			DbgMsg("Native buffer flag is %s!!", (pDecComp->bUseNativeBuffer==OMX_TRUE)?"Enabled":"Disabled");
			break;
		}
		case OMX_IndexVideoDecoderThumbnailMode:
		{
			//	Thumbnail Mode
			pDecComp->bEnableThumbNailMode = OMX_TRUE;

			//	Reconfigure Output Buffer Information for Thubmail Mode.
			pDecComp->pOutputPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT_THUMB;
			pDecComp->pOutputPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT_THUMB;
			pDecComp->pOutputPort->stdPortDef.nBufferSize        = pDecComp->width*pDecComp->height*3/2;
			break;
		}
		case OMX_IndexVideoDecoderExtradata:
		{
			OMX_U8 *pExtraData = (OMX_U8 *)ComponentParamStruct;
			OMX_S32 extraSize = *((OMX_S32*)pExtraData);

			if( pDecComp->pExtraData )
			{
				free(pDecComp->pExtraData);
				pDecComp->pExtraData = NULL;
				pDecComp->nExtraDataSize = 0;
			}
			pDecComp->nExtraDataSize = extraSize;

			pDecComp->pExtraData = (OMX_U8*)malloc( extraSize );
			memcpy( pDecComp->pExtraData, pExtraData+4, extraSize );
			break;
		}
		case OMX_IndexVideoDecoderCodecTag:
		{
			OMX_S32 *codecTag = (OMX_S32 *)ComponentParamStruct;
			pDecComp->codecTag = *codecTag;
			break;
		}
		case OMX_IndexParamPortDefinition:
		{
			OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParamStruct;
			OMX_ERRORTYPE error = OMX_ErrorNone;
			error = NX_BaseSetParameter( hComp, nParamIndex, ComponentParamStruct );
			if( error != OMX_ErrorNone )
			{
				DbgMsg("Error NX_BaseSetParameter() failed !!! for OMX_IndexParamPortDefinition \n");
				return error;
			}
			if( pPortDef->nPortIndex == 0 )
			{
				//	Set Input Width & Height
				pDecComp->width = pPortDef->format.video.nFrameWidth;
				pDecComp->height = pPortDef->format.video.nFrameWidth;
			}
			break;
		}
		default :
			return NX_BaseSetParameter( hComp, nParamIndex, ComponentParamStruct );
	}
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE NX_VidDec_UseBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_BUFFERHEADERTYPE         **pPortBuf = NULL;
	OMX_U32 i=0;

	DbgBuffer( "%s() In.(PortNo=%ld)\n", __FUNCTION__, nPortIndex );

	if( nPortIndex >= pDecComp->nNumPort ){
		return OMX_ErrorBadPortIndex;
	}

	if( OMX_StateLoaded != pDecComp->eCurState || OMX_StateIdle != pDecComp->eNewState )
		return OMX_ErrorIncorrectStateTransition;

	if( 0 ==nPortIndex ){
		pPort = pDecComp->pInputPort;
		pPortBuf = pDecComp->pInputBuffers;
	}else{
		pPort = pDecComp->pOutputPort;
		pPortBuf = pDecComp->pOutputBuffers;
	}

	DbgBuffer( "%s() : pPort->stdPortDef.nBufferSize = %ld\n", __FUNCTION__, pPort->stdPortDef.nBufferSize);

	if( pPort->stdPortDef.nBufferSize > nSizeBytes )
		return OMX_ErrorBadParameter;

	for( i=0 ; i<pPort->stdPortDef.nBufferCountActual ; i++ ){
		if( NULL == pPortBuf[i] )
		{
			//	Allocate Actual Data
			pPortBuf[i] = (OMX_BUFFERHEADERTYPE*)NxMalloc( sizeof(OMX_BUFFERHEADERTYPE) );
			if( NULL == pPortBuf[i] )
				return OMX_ErrorInsufficientResources;
			NxMemset( pPortBuf[i], 0, sizeof(OMX_BUFFERHEADERTYPE) );
			pPortBuf[i]->nSize = sizeof(OMX_BUFFERHEADERTYPE);
			NX_OMXSetVersion( &pPortBuf[i]->nVersion );
			pPortBuf[i]->pBuffer = pBuffer;
			pPortBuf[i]->nAllocLen = nSizeBytes;
			pPortBuf[i]->pAppPrivate = pAppPrivate;
			pPortBuf[i]->pPlatformPrivate = pStdComp;
			if( OMX_DirInput == pPort->stdPortDef.eDir ){
				pPortBuf[i]->nInputPortIndex = pPort->stdPortDef.nPortIndex;
			}else{
				pPortBuf[i]->nOutputPortIndex = pPort->stdPortDef.nPortIndex;
				pDecComp->outUsableBuffers ++;
				DbgBuffer( "%s() : outUsableBuffers= %ld, pPort->nAllocatedBuf = %ld\n", __FUNCTION__, pDecComp->outUsableBuffers, pPort->nAllocatedBuf);
			}
			pPort->nAllocatedBuf ++;
			if( pPort->nAllocatedBuf == pPort->stdPortDef.nBufferCountActual ){
				pPort->stdPortDef.bPopulated = OMX_TRUE;
				pPort->eBufferType = NX_BUFHEADER_TYPE_USEBUFFER;
				NX_PostSem(pDecComp->hBufAllocSem);
			}
			*ppBufferHdr = pPortBuf[i];
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorInsufficientResources;
}
static OMX_ERRORTYPE NX_VidDec_AllocateBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** pBuffer, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_BUFFERHEADERTYPE         **pPortBuf = NULL;
	OMX_U32 i=0;

	if( nPortIndex >= pDecComp->nNumPort )
		return OMX_ErrorBadPortIndex;

	if( OMX_StateLoaded != pDecComp->eCurState || OMX_StateIdle != pDecComp->eNewState )
		return OMX_ErrorIncorrectStateTransition;

	if( 0 ==nPortIndex ){
		pPort = pDecComp->pInputPort;
		pPortBuf = pDecComp->pInputBuffers;
	}else{
		pPort = pDecComp->pOutputPort;
		pPortBuf = pDecComp->pOutputBuffers;
	}

	if( pPort->stdPortDef.nBufferSize > nSizeBytes )
		return OMX_ErrorBadParameter;

	for( i=0 ; i<pPort->stdPortDef.nBufferCountActual ; i++ ){
		if( NULL == pPortBuf[i] ){
			//	Allocate Actual Data
			pPortBuf[i] = NxMalloc( sizeof(OMX_BUFFERHEADERTYPE) );
			if( NULL == pPortBuf[i] )
				return OMX_ErrorInsufficientResources;
			NxMemset( pPortBuf[i], 0, sizeof(OMX_BUFFERHEADERTYPE) );
			pPortBuf[i]->nSize = sizeof(OMX_BUFFERHEADERTYPE);
			NX_OMXSetVersion( &pPortBuf[i]->nVersion );
			pPortBuf[i]->pBuffer = NxMalloc( nSizeBytes );
			if( NULL == pPortBuf[i]->pBuffer )
				return OMX_ErrorInsufficientResources;
			pPortBuf[i]->nAllocLen = nSizeBytes;
			pPortBuf[i]->pAppPrivate = pAppPrivate;
			pPortBuf[i]->pPlatformPrivate = pStdComp;
			if( OMX_DirInput == pPort->stdPortDef.eDir ){
				pPortBuf[i]->nInputPortIndex = pPort->stdPortDef.nPortIndex;
			}else{
				pPortBuf[i]->nOutputPortIndex = pPort->stdPortDef.nPortIndex;
			}
			pPort->nAllocatedBuf ++;
			if( pPort->nAllocatedBuf == pPort->stdPortDef.nBufferCountActual ){
				pPort->stdPortDef.bPopulated = OMX_TRUE;
				pPort->eBufferType = NX_BUFHEADER_TYPE_ALLOCATED;
				TRACE("%s(): port:%ld, %ld buffer allocated\n", __FUNCTION__, nPortIndex, pPort->nAllocatedBuf);
				NX_PostSem(pDecComp->hBufAllocSem);
			}
			*pBuffer = pPortBuf[i];
			return OMX_ErrorNone;
		}
	}
	pPort->stdPortDef.bPopulated = OMX_TRUE;
	*pBuffer = pPortBuf[0];
	return OMX_ErrorInsufficientResources;
}
static OMX_ERRORTYPE NX_VidDec_FreeBuffer (OMX_HANDLETYPE hComponent, OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_BUFFERHEADERTYPE **pPortBuf = NULL;
	OMX_U32 i=0;

	if( nPortIndex >= pDecComp->nNumPort )
		return OMX_ErrorBadPortIndex;

	if( OMX_StateLoaded != pDecComp->eNewState )
		return OMX_ErrorIncorrectStateTransition;

	if( 0 == nPortIndex ){
		pPort = pDecComp->pInputPort;
		pPortBuf = pDecComp->pInputBuffers;
	}else{
		pPort = pDecComp->pOutputPort;
		pPortBuf = pDecComp->pOutputBuffers;
	}

	//	OMX_StateLoaded로 transition하는 경우 아닐 경우 ????
	if( NULL == pBuffer ){
		return OMX_ErrorBadParameter;
	}

	for( i=0 ; i<pPort->stdPortDef.nBufferCountActual ; i++ ){
		if( NULL != pBuffer && pBuffer == pPortBuf[i] ){
			if( pPort->eBufferType == NX_BUFHEADER_TYPE_ALLOCATED ){
				NxFree(pPortBuf[i]->pBuffer);
				pPortBuf[i]->pBuffer = NULL;
			}
			NxFree(pPortBuf[i]);
			pPortBuf[i] = NULL;
			pPort->nAllocatedBuf --;
			if( 0 == pPort->nAllocatedBuf ){
				pPort->stdPortDef.bPopulated = OMX_FALSE;
				NX_PostSem(pDecComp->hBufAllocSem);
			}
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorInsufficientResources;
}
static OMX_ERRORTYPE NX_VidDec_EmptyThisBuffer (OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer)
{
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)((OMX_COMPONENTTYPE *)hComp)->pComponentPrivate;
	FUNC_IN;
	//	Push data to command buffer
	assert( NULL != pDecComp->pInputPort );
	assert( NULL != pDecComp->pInputPortQueue );
	if( pDecComp->eCurState == pDecComp->eNewState ){
		if( !(OMX_StateIdle == pDecComp->eCurState || OMX_StatePause == pDecComp->eCurState || OMX_StateExecuting == pDecComp->eCurState) ){
			return OMX_ErrorIncorrectStateOperation;
		}
	}else{
		if( (OMX_StateIdle==pDecComp->eNewState) && (OMX_StateExecuting==pDecComp->eCurState || OMX_StatePause==pDecComp->eCurState) ){
			return OMX_ErrorIncorrectStateOperation;
		}
	}

	DbgBuffer("%s() : pBuffer = %p", __FUNCTION__, pBuffer);

	if( 0 != NX_PushQueue( pDecComp->pInputPortQueue, pBuffer ) )
	{
		return OMX_ErrorUndefined;
	}
	DbgBuffer("%s() : pBuffer = %p --", __FUNCTION__, pBuffer);
	NX_PostSem( pDecComp->hBufChangeSem );
	FUNC_OUT;
	return OMX_ErrorNone;
}
static OMX_ERRORTYPE NX_VidDec_FillThisBuffer(OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_S32 foundBuffer=1, i;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)((OMX_COMPONENTTYPE *)hComp)->pComponentPrivate;

//	DbgMsg("FillBuffer ++ %p\n", pBuffer);
	pthread_mutex_lock( &pDecComp->hBufMutex );

	FUNC_IN;

	//	Push data to command buffer
	assert( NULL != pDecComp->pOutputPort );
	assert( NULL != pDecComp->pOutputPortQueue );

	if( pDecComp->eCurState == pDecComp->eNewState ){
		if( !(OMX_StateIdle == pDecComp->eCurState || OMX_StatePause == pDecComp->eCurState || OMX_StateExecuting == pDecComp->eCurState) ){
			pthread_mutex_unlock( &pDecComp->hBufMutex );
			return OMX_ErrorIncorrectStateOperation;
		}
	}else{
		if( (OMX_StateIdle==pDecComp->eNewState) && (OMX_StateExecuting==pDecComp->eCurState || OMX_StatePause==pDecComp->eCurState) ){
			pthread_mutex_unlock( &pDecComp->hBufMutex );
			return OMX_ErrorIncorrectStateOperation;
		}
	}

	if( pDecComp->bEnableThumbNailMode == OMX_TRUE )
	{
		//	Normal Output Memory Mode
		if( 0 != NX_PushQueue( pDecComp->pOutputPortQueue, pBuffer ) ){
			pthread_mutex_unlock( &pDecComp->hBufMutex );
			NX_PostSem( pDecComp->hBufChangeSem );		//	Send Buffer Change Event
			return OMX_ErrorUndefined;
		}
		NX_PostSem( pDecComp->hBufChangeSem );		//	Send Buffer Change Event
	}
	else
	{
		//	Native Window Buffer Mode
		for( i=0 ; i<NX_OMX_MAX_BUF ; i++ )
		{
			//	Find Matching Buffer Pointer
			if( pDecComp->pOutputBuffers[i] == pBuffer )
			{
				NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, i );

				if( pDecComp->outBufferUseFlag[i] )
				{
					DbgMsg("===========> outBufferUseFlag Already We Have a Output Buffer!!!\n");
				}
				pDecComp->outBufferUseFlag[i] = 1;		//	Check
				pDecComp->curOutBuffers ++;
				foundBuffer = 1;
			}
		}

		if( !foundBuffer )
		{
			DbgMsg("Discard Buffer = %p\n", pBuffer);
			pBuffer->nFilledLen = 0;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pBuffer);
		}
		NX_PostSem( pDecComp->hBufChangeSem );		//	Send Buffer Change Event
	}
	FUNC_OUT;
	pthread_mutex_unlock( &pDecComp->hBufMutex );
	return OMX_ErrorNone;
}



//////////////////////////////////////////////////////////////////////////////
//
//						Command Execution Thread
//
static OMX_ERRORTYPE NX_VidDec_StateTransition( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_STATETYPE eCurState, OMX_STATETYPE eNewState )
{
	OMX_U32 i=0;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE *pPort = NULL;
	NX_QUEUE *pQueue = NULL;
	OMX_BUFFERHEADERTYPE *pBufHdr = NULL;

	TRACE( "%s() In : eCurState %d -> eNewState %d \n", __FUNCTION__, eCurState, eNewState );

	//	Check basic errors
	if( eCurState == eNewState ){
		NX_ErrMsg("%s:line(%d) : %s() same state\n", __FILE__, __LINE__, __FUNCTION__ );
		return OMX_ErrorSameState;
	}
	if( OMX_StateInvalid==eCurState || OMX_StateInvalid==eNewState ){
		NX_ErrMsg("%s:line(%d) : %s() Invalid state\n", __FILE__, __LINE__, __FUNCTION__ );
		return OMX_ErrorInvalidState;
	}

	if( OMX_StateLoaded == eCurState ){
		switch( eNewState )
		{
			//	CHECKME
			case OMX_StateIdle:
				//	Wait buffer allocation event
				for( i=0 ; i<pDecComp->nNumPort ; i++ ){
					pPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pDecComp->pPort[i];
					if( OMX_TRUE == pPort->bEnabled ){
						//	Note :
						//	원래는 Loaded Idle로 넘어가는 과정에서 Buffer Allocation이 이전에 반드시 이루어 져야만한다.
						//	하지만 Android에서는 이 과정은 거치지 않는 것으로 보인다.
						//	원래 Standard되로 된다면 이과정에서 Buffer Allocation이 이루어질 때가지 Waiting해주어야만 한다.
						NX_PendSem(pDecComp->hBufAllocSem);
					}
					//
					//	TODO : Need exit check.
					//
				}
				//	Start buffer management thread
				pDecComp->eCmdBufThread = NX_THREAD_CMD_PAUSE;
				//	Create buffer control semaphore
				pDecComp->hBufCtrlSem = NX_CreateSem(0, 1);
				//	Create buffer mangement semaphore
				pDecComp->hBufChangeSem = NX_CreateSem(0, VPUDEC_VID_NUM_PORT*1024);
				//	Create buffer management thread
				pDecComp->eCmdBufThread = NX_THREAD_CMD_PAUSE;
				//	Create buffer management thread
				pthread_create( &pDecComp->hBufThread, NULL, (void*)&NX_VidDec_BufferMgmtThread , pDecComp );

				//	Open FFMPEG Video Decoder
				TRACE("Wait BufferCtrlSem");
				NX_PendSem(pDecComp->hBufCtrlSem);
				openVideoCodec( pDecComp );

				//	Wait thread creation
				pDecComp->eCurState = eNewState;

				TRACE("OMX_StateLoaded --> OMX_StateIdle");

				break;
			case OMX_StateWaitForResources:
			case OMX_StateExecuting:
			case OMX_StatePause:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
		}
	}else if( OMX_StateIdle == eCurState ){
		switch( eNewState )
		{
			case OMX_StateLoaded:
			{
				//	Exit buffer management thread
				pDecComp->eCmdBufThread = NX_THREAD_CMD_EXIT;
				NX_PostSem( pDecComp->hBufChangeSem );
				NX_PostSem( pDecComp->hBufCtrlSem );
				pthread_join( pDecComp->hBufThread, NULL );
				NX_DestroySem( pDecComp->hBufChangeSem );
				NX_DestroySem( pDecComp->hBufCtrlSem );
				pDecComp->hBufChangeSem = NULL;
				pDecComp->hBufCtrlSem = NULL;

				//	Wait buffer free
				for( i=0 ; i<pDecComp->nNumPort ; i++ ){
					pPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pDecComp->pPort[i];
					if( OMX_TRUE == pPort->bEnabled ){
						NX_PendSem(pDecComp->hBufAllocSem);
					}
					//
					//	TODO : Need exit check.
					//
				}

				closeVideoCodec( pDecComp );

				pDecComp->eCurState = eNewState;
				break;
			}
			case OMX_StateExecuting:
				//	Step 1. Check in/out buffer queue.
				//	Step 2. If buffer is not ready in the queue, return error.
				//	Step 3. Craete buffer management thread.
				//	Step 4. Start buffer processing.
				pDecComp->eCmdBufThread = NX_THREAD_CMD_RUN;
				NX_PostSem( pDecComp->hBufCtrlSem );
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StatePause:
				//	Step 1. Check in/out buffer queue.
				//	Step 2. If buffer is not ready in the queue, return error.
				//	Step 3. Craete buffer management thread.
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateWaitForResources:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
		}
	}else if( OMX_StateExecuting == eCurState ){
		switch( eNewState )
		{
			case OMX_StateIdle:
				//	Step 1. Stop buffer processing
				pDecComp->eCmdBufThread = NX_THREAD_CMD_PAUSE;
				//	Write dummy
				NX_PostSem( pDecComp->hBufChangeSem );
				//	Step 2. Flushing buffers.
				//	Return buffer to supplier.
				pthread_mutex_lock( &pDecComp->hBufMutex );
				for( i=0 ; i<pDecComp->nNumPort ; i++ ){
					pPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pDecComp->pPort[i];
					pQueue = (NX_QUEUE*)pDecComp->pBufQueue[i];
					if( OMX_DirInput == pPort->eDir )
					{
						do{
							pBufHdr = NULL;
							if( 0 == NX_PopQueue( pQueue, (void**)&pBufHdr ) )
							{
								pDecComp->pCallbacks->EmptyBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pBufHdr );
							}
							else
							{
								break;
							}
						}while(1);
					}
					else if( OMX_DirOutput == pPort->eDir )
					{
						//	Thumbnail Mode
						if( pDecComp->bEnableThumbNailMode == OMX_TRUE )
						{
							do{
								pBufHdr = NULL;
								if( 0 == NX_PopQueue( pQueue, (void**)&pBufHdr ) )
								{
									pDecComp->pCallbacks->FillBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pBufHdr );
								}
								else
								{
									break;
								}
							}while(1);
						}
						else
						{
							//	Native Windowbuffer Mode
							for( i=0 ; i<NX_OMX_MAX_BUF ; i++ )
							{
								//	Find Matching Buffer Pointer
								if( pDecComp->outBufferUseFlag[i] )
								{
									pDecComp->pCallbacks->FillBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pDecComp->pOutputBuffers[i] );
									pDecComp->outBufferUseFlag[i] = 0;
									pDecComp->curOutBuffers --;
								}
							}
						}
					}
				}
				pthread_mutex_unlock( &pDecComp->hBufMutex );
				//	Step 3. Exit buffer management thread.
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StatePause:
				//	Step 1. Stop buffer processing using buffer management semaphore
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateLoaded:
			case OMX_StateWaitForResources:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
		}
	}else if( OMX_StatePause==eCurState ){
		switch( eNewState )
		{
			case OMX_StateIdle:
				//	Step 1. Flushing buffers.
				//	Step 2. Exit buffer management thread.
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateExecuting:
				//	Step 1. Start buffer processing.
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateLoaded:
			case OMX_StateWaitForResources:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
		}
	}else if( OMX_StateWaitForResources==eCurState ){
		switch( eNewState )
		{
			case OMX_StateLoaded:
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateIdle:
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateExecuting:
			case OMX_StatePause:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
		}
	}else{
		//	Error
		return OMX_ErrorUndefined;
	}
	return eError;
}

static void NX_VidDec_CommandProc( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData )
{
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	OMX_EVENTTYPE eEvent = OMX_EventCmdComplete;
	OMX_COMPONENTTYPE *pStdComp = pDecComp->hComp;
	OMX_U32 nData1 = 0, nData2 = 0;

	TRACE("%s() : In( Cmd = %d )\n", __FUNCTION__, Cmd );

	switch( Cmd )
	{
		//	If the component successfully transitions to the new state,
		//	it notifies the IL client of the new state via the OMX_EventCmdComplete event,
		//	indicating OMX_CommandStateSet for nData1 and the new state for nData2.
		case OMX_CommandStateSet:    // Change the component state
		{
			if( pDecComp->eCurState == nParam1 ){
				//
				eEvent = OMX_EventError;
				nData1 = OMX_ErrorSameState;
				break;
			}

			eError = NX_VidDec_StateTransition( pDecComp, pDecComp->eCurState, nParam1 );
			nData1 = OMX_CommandStateSet;
			nData2 = nParam1;				//	Must be set new state (OMX_STATETYPE)
			TRACE("NX_VidDec_CommandProc : OMX_CommandStateSet(nData1=%ld, nData2=%ld)\n", nData1, nData2 );
			break;
		}
		case OMX_CommandFlush:       // Flush the data queue(s) of a component
		{
			OMX_BUFFERHEADERTYPE* pBuf = NULL;
			DbgBuffer("%s() : Flush( nParam1=%ld )\n", __FUNCTION__, nParam1 );

			pthread_mutex_lock( &pDecComp->hBufMutex );
			//	Input Port Flushing
			if( nParam1 == 0 )
			{
				DbgBuffer("Current Input QueueBuffer Size = %d\n", NX_GetQueueCnt(pDecComp->pInputPortQueue));
				do{
					if( NX_GetQueueCnt(pDecComp->pInputPortQueue) > 0 ){
						//	Flush buffer
						if( 0 == NX_PopQueue( pDecComp->pInputPortQueue, (void**)&pBuf ) )
						{
							pBuf->nFilledLen = 0;
							pDecComp->pCallbacks->EmptyBufferDone(pStdComp, pStdComp->pApplicationPrivate, pBuf);
						}
					}else{
						break;
					}
				}while(1);
			}

			//	Output Port Flushing
			if( nParam1 == 1 )
			{
				if( OMX_TRUE == pDecComp->bEnableThumbNailMode )
				{
					do{
						if( NX_GetQueueCnt(pDecComp->pOutputPortQueue) > 0 ){
							//	Flush buffer
							NX_PopQueue( pDecComp->pOutputPortQueue, (void**)&pBuf );
							pDecComp->pCallbacks->FillBufferDone(pStdComp, pStdComp->pApplicationPrivate, pBuf);
						}else{
							break;
						}
					}while(1);
				}
				else
				{
					OMX_S32 i;
					DbgBuffer("++ pDecComp->curOutBuffers = %ld\n", pDecComp->curOutBuffers);
					for( i=0 ; i<NX_OMX_MAX_BUF ; i++ )
					{
						//	Find Matching Buffer Pointer
						if( pDecComp->outBufferUseFlag[i] )
						{
							pDecComp->pOutputBuffers[i]->nFilledLen = 0;
							pDecComp->pCallbacks->FillBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pDecComp->pOutputBuffers[i] );
							pDecComp->outBufferUseFlag[i] = 0;
							pDecComp->curOutBuffers -- ;
							DbgBuffer("pDecComp->pOutputBuffers[%2d] = %p\n",  i, pDecComp->pOutputBuffers[i]);
						}
					}
					DbgBuffer("-- pDecComp->curOutBuffers = %ld\n", pDecComp->curOutBuffers);
				}
			}

			pthread_mutex_unlock( &pDecComp->hBufMutex );

			nData1 = OMX_CommandFlush;
			nData2 = nParam1;

			pDecComp->bNeedKey = OMX_TRUE;
			pDecComp->bFlush = OMX_TRUE;
			break;
		}
		//	Openmax spec v1.1.2 : 3.4.4.3 Non-tunneled Port Disablement and Enablement.
		case OMX_CommandPortDisable: // Disable a port on a component.
		{
			//	Check Parameter
			if( OMX_ALL != nParam1 || VPUDEC_VID_NUM_PORT>=nParam1 ){
				//	Bad parameter
				eEvent = OMX_EventError;
				nData1 = OMX_ErrorBadPortIndex;
				NX_ErrMsg(" Errror : OMX_ErrorBadPortIndex(%d) : %d: %s", nParam1, __FILE__, __LINE__);
				break;
			}

			//	Step 1. The component shall return the buffers with a call to EmptyBufferDone/FillBufferDone,
			NX_PendSem( pDecComp->hBufCtrlSem );
			if( OMX_ALL == nParam1 ){
				//	Disable all ports
			}else{
				pDecComp->pInputPort->stdPortDef.bEnabled = OMX_FALSE;
				//	Specific port
			}
			TRACE("NX_VidDec_CommandProc : OMX_CommandPortDisable \n");
			NX_PostSem( pDecComp->hBufCtrlSem );
			break;
		}
		case OMX_CommandPortEnable:  // Enable a port on a component.
		{
			NX_PendSem( pDecComp->hBufCtrlSem );
			TRACE("NX_VidDec_CommandProc : OMX_CommandPortEnable \n");
			pDecComp->pInputPort->stdPortDef.bEnabled = OMX_FALSE;
			NX_PostSem( pDecComp->hBufCtrlSem );
			break;
		}
		case OMX_CommandMarkBuffer:  // Mark a component/buffer for observation
		{
			NX_PendSem( pDecComp->hBufCtrlSem );
			TRACE("NX_VidDec_CommandProc : OMX_CommandMarkBuffer \n");
			NX_PostSem( pDecComp->hBufCtrlSem );
			break;
		}
		default:
		{
			break;
		}
	}

	pDecComp->pCallbacks->EventHandler( (OMX_HANDLETYPE)pStdComp, pDecComp->pCallbackData, eEvent, nData1, nData2, pCmdData );
}


static void NX_VidDec_CommandThread( void *arg )
{
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)arg;
	NX_CMD_MSG_TYPE *pCmd = NULL;
	TRACE( "enter NX_VidDec_CommandThread() loop\n" );

	NX_PostSem( pDecComp->hSemCmdWait );
	while(1)
	{
		if( 0 != NX_PendSem( pDecComp->hSemCmd ) ){
			DbgMsg( "NX_VidDec_CommandThread : Exit command loop : error NX_PendSem()\n" );
			break;
		}

		//	IL Client에서 command response를 wait할 수 있으므로 반드시 command quque를 비워 주어야만 한다.
		if( NX_THREAD_CMD_RUN!=pDecComp->eCmdThreadCmd && 0==NX_GetQueueCnt(&pDecComp->cmdQueue) ){
			break;
		}

		if( 0 != NX_PopQueue( &pDecComp->cmdQueue, (void**)&pCmd ) ){
			DbgMsg( "NX_VidDec_CommandThread : Exit command loop : NX_PopQueue()\n" );
		}

		TRACE("%s() : pCmd = 0x%08x\n", __FUNCTION__, (unsigned int)pCmd);

		if( pCmd != NULL ){
			NX_VidDec_CommandProc( pDecComp, pCmd->eCmd, pCmd->nParam1, pCmd->pCmdData );
			NxFree( pCmd );
		}
	}
	TRACE( ">> exit NX_VidDec_CommandThread() loop!!\n" );
}

//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//						Buffer Management Thread
//

//
//	NOTE :
//		1. EOS 가 들어왔을 경우 Input Buffer는 처리하지 않기로 함.
//
static int NX_VidDec_Transform(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	return decodeVideoFrame( pDecComp, pInQueue, pOutQueue );
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Buffer control semaphore is synchronize state of component and state of buffer thread.
//
//	Transition Action
//	1. OMX_StateLoaded   --> OMX_StateIdle      ;
//		==> Buffer management thread를 생성, initialize buf change sem & buf control sem,
//			thread state set to NX_THREAD_CMD_PAUSE
//	2. OMX_StateIdle     --> OMX_StateExecuting ;
//		==> Set state to NX_THREAD_CMD_RUN and post control semaphore
//	3. OMX_StateIdle     --> OMX_StatePause     ;
//		==> Noting.
//	4. OMX_StatePause    --> OMX_StateExecuting ;
//		==> Set state to NX_THREAD_CMD_RUN and post control semaphore
//	5. OMX_StateExcuting --> OMX_StatePause     ;
//		==> Set state to NX_THREAD_CMD_PAUSE and post dummy buf change semaphore
//	6. OMX_StateExcuting --> OMX_StateIdle      ;
//		==> Set state to NX_THREAD_CMD_PAUSE and post dummy buf change semaphore and
//			return all buffers on the each port.
//	7. OMX_StateIdle     --> OMX_Loaded         ;
//
/////////////////////////////////////////////////////////////////////////////////////////////////////
static void NX_VidDec_BufferMgmtThread( void *arg )
{
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)arg;
	TRACE( "enter %s() loop\n", __FUNCTION__ );

	NX_PostSem( pDecComp->hBufCtrlSem );					//	Thread Creation Semaphore
	while( NX_THREAD_CMD_EXIT != pDecComp->eCmdBufThread )
	{
		NX_PendSem( pDecComp->hBufCtrlSem );				//	Thread Control Semaphore
		while( NX_THREAD_CMD_RUN == pDecComp->eCmdBufThread ){
			NX_PendSem( pDecComp->hBufChangeSem );			//	wait buffer management command
			if( NX_THREAD_CMD_RUN != pDecComp->eCmdBufThread ){
				break;
			}

			if( pDecComp->bEnableThumbNailMode == OMX_TRUE )
			{
				//	check decoding
				if( NX_GetQueueCnt( pDecComp->pInputPortQueue ) > 0 && NX_GetQueueCnt( pDecComp->pOutputPortQueue ) > 0 ) {
					pthread_mutex_lock( &pDecComp->hBufMutex );
					if( 0 != NX_VidDec_Transform(pDecComp, pDecComp->pInputPortQueue, pDecComp->pOutputPortQueue) )
					{
						pthread_mutex_unlock( &pDecComp->hBufMutex );
						ErrMsg(("NX_VidDec_Transform failed!!!\n"));
						goto ERROR_EXIT;
					}
					pthread_mutex_unlock( &pDecComp->hBufMutex );
				}
			}
			else
			{
				//	check decoding
				if( NX_GetQueueCnt( pDecComp->pInputPortQueue ) > 0 && (pDecComp->curOutBuffers > pDecComp->minRequiredFrameBuffer) ) {
					pthread_mutex_lock( &pDecComp->hBufMutex );
					if( 0 != NX_VidDec_Transform(pDecComp, pDecComp->pInputPortQueue, pDecComp->pOutputPortQueue) )
					{
						pthread_mutex_unlock( &pDecComp->hBufMutex );
						ErrMsg(("NX_VidDec_Transform failed!!!\n"));
						goto ERROR_EXIT;
					}
					pthread_mutex_unlock( &pDecComp->hBufMutex );
				}
			}
		}
	}

ERROR_EXIT:
	//	Release or Return buffer's
	TRACE("exit buffer management thread.\n");
}

//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//					FFMPEG Video Decoder Handler
//

#include "DecodeFrame.h"

void CodecTagToMp4Class( OMX_S32 codecTag, OMX_S32 *codecId, OMX_S32 *mp4Class )
{
	*mp4Class = 0;
	switch( codecTag )
	{
		case MKTAG('D','I','V','X'):case MKTAG('d','i','v','x'):
		case MKTAG('D','I','V','4'):case MKTAG('d','i','v','4'):
			*mp4Class = 5;
			break;
		case MKTAG('D','X','5','0'):case MKTAG('d','x','5','0'):
		case MKTAG('D','I','V','5'):case MKTAG('d','i','v','5'):
		case MKTAG('D','I','V','6'):case MKTAG('d','i','v','6'):
			*mp4Class = 1;
			break;
		case MKTAG('X','V','I','D'):case MKTAG('x','v','i','d'):
			*mp4Class = 2;
			break;
		case MKTAG('F','L','V','1'):case MKTAG('f','l','v','1'):
			*mp4Class = 256;
			break;
		case MKTAG('D','I','V','3'):case MKTAG('d','i','v','3'):
		case MKTAG('M','P','4','3'):case MKTAG('m','p','4','3'):
		case MKTAG('M','P','G','3'):case MKTAG('m','p','g','3'):
		case MKTAG('D','V','X','3'):case MKTAG('d','v','x','3'):
		case MKTAG('A','P','4','1'):case MKTAG('a','p','4','1'):
			*codecId = NX_DIV3_DEC;
			break;

		default:
			*mp4Class = 0;
			break;
	}
}

//	0 is OK, other Error.
int openVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	OMX_S32 mp4Class = 0;
	FUNC_IN;

	pDecComp->isOutIdr = OMX_FALSE;

	//	FIXME : Move to Port SetParameter Part
	pDecComp->width = pDecComp->pInputPort->stdPortDef.format.video.nFrameWidth;
	pDecComp->height = pDecComp->pInputPort->stdPortDef.format.video.nFrameHeight;

	CodecTagToMp4Class( pDecComp->codecTag, &pDecComp->videoCodecId, &mp4Class );

	switch( pDecComp->videoCodecId )
	{
		case NX_AVC_DEC:
			pDecComp->DecodeFrame = NX_DecodeAvcFrame;
			break;
		case NX_MP2_DEC:
			pDecComp->DecodeFrame = NX_DecodeAvcFrame;
			break;
		case NX_MP4_DEC:
		case NX_H263_DEC:
			pDecComp->DecodeFrame = NX_DecodeMpeg4Frame;
			break;
		case NX_DIV3_DEC:
			pDecComp->DecodeFrame = NX_DecodeDiv3Frame;
			break;

		case NX_VC1_DEC:
			pDecComp->DecodeFrame = NX_DecodeVC1Frame;
			break;
		case NX_RV_DEC:
			pDecComp->DecodeFrame = NX_DecodeRVFrame;
			break;
		default:
			pDecComp->DecodeFrame = NULL;
			break;
	}

	DbgMsg("CodecID = %ld, mp4Class = %ld, Tag = %c%c%c%c\n\n", pDecComp->videoCodecId, mp4Class,
		(char)(pDecComp->codecTag&0xFF),
		(char)((pDecComp->codecTag>>8)&0xFF),
		(char)((pDecComp->codecTag>>16)&0xFF),
		(char)((pDecComp->codecTag>>24)&0xFF) );
	pDecComp->hVpuCodec = NX_VidDecOpen( pDecComp->videoCodecId, mp4Class );

	if( NULL == pDecComp->hVpuCodec ){
		ErrMsg("%s(%d) NX_VidDecOpen() failed.(CodecID=%ld)\n", __FILE__, __LINE__, pDecComp->videoCodecId);
		FUNC_OUT;
		return -1;
	}
	InitVideoTimeStamp( pDecComp );

	FUNC_OUT;
	return 0;
}

void closeVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	FUNC_IN;
	if( pDecComp->hVpuCodec ){
		NX_VidDecFlush( pDecComp->hVpuCodec );
		NX_VidDecClose( pDecComp->hVpuCodec );
		pDecComp->bInitialized = OMX_FALSE;
		pDecComp->bNeedKey = OMX_TRUE;
		pDecComp->hVpuCodec = NULL;
		pDecComp->isOutIdr = OMX_FALSE;
		FUNC_OUT;
	}
	FUNC_OUT;
}

int flushVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	FUNC_IN;
	if( pDecComp->hVpuCodec )
	{
		NX_VidDecFlush( pDecComp->hVpuCodec );
		pDecComp->isOutIdr = OMX_FALSE;
		return NX_VidDecFlush( pDecComp->hVpuCodec );
	}
	FUNC_OUT;
	return 0;
}

int InitialzieCodaVpu(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, unsigned char *buf, int size)
{
	int ret = -1;
	NX_VID_SEQ_OUT seqOut;
	FUNC_IN;
	if( pDecComp->hVpuCodec )
	{
		int i;
		FUNC_OUT;

		if( pDecComp->bUseNativeBuffer )
		{
			struct private_handle_t const *handle;
			DbgMsg("Native Buffer Mode : pDecComp->outUsableBuffers=%ld, ExtraSize = %ld, MAX_DEC_FRAME_BUFFERS = %d\n", pDecComp->outUsableBuffers, pDecComp->codecSpecificDataSize, MAX_DEC_FRAME_BUFFERS );

			//	Translate Gralloc Memory Buffer Type To Nexell Video Memory Type
			for( i=0 ; i<pDecComp->outUsableBuffers ; i++ )
			{
				handle = (struct private_handle_t const *)pDecComp->pOutputBuffers[i]->pBuffer;

				pDecComp->vidFrameBuf[i].memoryMap = 0;		//	Linear
				pDecComp->vidFrameBuf[i].fourCC    = FOURCC_YV12;
				pDecComp->vidFrameBuf[i].imgWidth  = pDecComp->width;
				pDecComp->vidFrameBuf[i].imgHeight = pDecComp->height;

                // psw0523 fix for new gralloc
#if 0
				pDecComp->vidFrameBuf[i].luPhyAddr = handle->phys ;
				pDecComp->vidFrameBuf[i].cbPhyAddr = handle->phys1;
				pDecComp->vidFrameBuf[i].crPhyAddr = handle->phys2;

				pDecComp->vidFrameBuf[i].luVirAddr = handle->base ;
				pDecComp->vidFrameBuf[i].cbVirAddr = handle->base1;
				pDecComp->vidFrameBuf[i].crVirAddr = handle->base2;

				pDecComp->vidFrameBuf[i].luStride = ((pDecComp->width+31)>>5)<<5;
				pDecComp->vidFrameBuf[i].cbStride = pDecComp->vidFrameBuf[i].luStride/2;
				pDecComp->vidFrameBuf[i].crStride = pDecComp->vidFrameBuf[i].luStride/2;
#else
                int ion_fd = ion_open();
                ret = ion_get_phys(ion_fd, handle->share_fd, (long unsigned int *)&pDecComp->vidFrameBuf[i].luPhyAddr);
                if (ret != 0) {
                     ALOGE("%s: failed to ion_get_phys", __func__);
                     return ret;
                }
				pDecComp->vidFrameBuf[i].cbPhyAddr = pDecComp->vidFrameBuf[i].luPhyAddr + (handle->stride * ALIGN(handle->height, 16));
				pDecComp->vidFrameBuf[i].crPhyAddr =
                    pDecComp->vidFrameBuf[i].cbPhyAddr + (ALIGN(handle->stride >> 1, 16) * ALIGN(handle->height >> 1, 16));

				pDecComp->vidFrameBuf[i].luStride = handle->stride;
				pDecComp->vidFrameBuf[i].cbStride = pDecComp->vidFrameBuf[i].crStride = ALIGN(handle->stride >> 1, 16);
                close(ion_fd);
#endif

				//{
				//	OMX_S32 strideY, strideUV;
				//	strideY  = pDecComp->vidFrameBuf[i].luStride;
				//	strideUV = pDecComp->vidFrameBuf[i].cbStride;
				//	DbgMsg("===== Image Stride %ld, %ld\n", strideY, strideUV );
				//}

				TRACE("===== Physical Address = 0x%08x, 0x%08x, 0x%08x\n", handle->phys , handle->phys1 , handle->phys2 );
				pDecComp->hVidFrameBuf[i] = &pDecComp->vidFrameBuf[i];
			}

			ret = NX_VidDecInitWidthBuffer( pDecComp->hVpuCodec, buf, size, pDecComp->width, pDecComp->height, pDecComp->hVidFrameBuf, pDecComp->outUsableBuffers, &seqOut );

			//	Initialize Number of Bufferable Buffer's Counter
			pDecComp->outBufferable = seqOut.numBuffers - seqOut.nimBuffers;
			//	Update Read Required Frame Buffer
			pDecComp->minRequiredFrameBuffer = seqOut.nimBuffers;
			DbgMsg("<<<<<<<<<< call NX_VidDecInitWidthBuffer(Min=%ld) >>>>>>>>>>\n", pDecComp->minRequiredFrameBuffer );
		}
		else
		{
			ret = NX_VidDecInit( pDecComp->hVpuCodec, buf, size, pDecComp->width, pDecComp->height, &seqOut );
			pDecComp->outBufferable = seqOut.numBuffers - seqOut.nimBuffers;
		}
	}
	FUNC_OUT;
	return ret;
}

void InitVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	OMX_S32 i;
	for( i=0 ;i<NX_OMX_MAX_BUF; i++ )
	{
		pDecComp->outTimeStamp[i].flag = (OMX_U32)-1;
	}
}

void PushVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_TICKS timestamp, OMX_U32 flag )
{
	OMX_S32 i=0;
	for( i=0 ;i<NX_OMX_MAX_BUF; i++ )
	{
		if( pDecComp->outTimeStamp[i].flag == (OMX_U32)-1 )
		{
			pDecComp->outTimeStamp[i].timestamp = timestamp;
			pDecComp->outTimeStamp[i].flag = flag;
			break;
		}
	}
}

int PopVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_TICKS *timestamp, OMX_U32 *flag )
{
	OMX_S32 i=0;
	OMX_TICKS minTime = 0x7FFFFFFFFFFFFFFFll;
	OMX_S32 minIdx = -1;
	for( i=0 ;i<NX_OMX_MAX_BUF; i++ )
	{
		if( pDecComp->outTimeStamp[i].flag != (OMX_U32)-1 )
		{
			if( minTime > pDecComp->outTimeStamp[i].timestamp )
			{
				minTime = pDecComp->outTimeStamp[i].timestamp;
				minIdx = i;
			}
		}
	}
	if( minIdx != -1 )
	{
		*timestamp = pDecComp->outTimeStamp[minIdx].timestamp;
		*flag      = pDecComp->outTimeStamp[minIdx].flag;
		pDecComp->outTimeStamp[minIdx].flag = (OMX_U32)-1;
		return 0;
	}
	else
	{
		DbgMsg("Cannot Found Time Stamp!!!");
		return -1;
	}
}

static int isIdrFrame( unsigned char *buf, int size )
{
	int i;
	int isIdr=0;

	for( i=0 ; i<size-4; i++ )
	{
		if( buf[i]==0 && buf[i+1]==0 && ((buf[i+2]&0x1F)==0x05) )	//	Check Nal Start Code & Nal Type
		{
			isIdr = 1;
			break;
		}
	}

	return isIdr;
}


//	0 is OK, other Error.
int decodeVideoFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	if( pDecComp->DecodeFrame )
	{
		return pDecComp->DecodeFrame( pDecComp, pInQueue, pOutQueue );
	}
	else{
		return 0;
	}
}

//
//////////////////////////////////////////////////////////////////////////////
