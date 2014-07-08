#include <OMX_Core.h>
#include <OMX_Component.h>
#include <NX_OMXQueue.h>
#include <NX_OMXBaseComponent.h>
#include <NX_OMXDebugMsg.h>
#include <NX_OMXMem.h>

#ifndef UNUSED_PARAM
#define	UNUSED_PARAM(X)		X=X
#endif

OMX_ERRORTYPE NX_BaseComponentInit (OMX_COMPONENTTYPE *hComponent)
{
	NX_BASE_COMPNENT *pBaseComp = (NX_BASE_COMPNENT *)(hComponent->pComponentPrivate);
	pBaseComp->hComp = hComponent;
	pBaseComp->nNumPort = 0;
	pBaseComp->pCallbacks = NxMalloc( sizeof(OMX_CALLBACKTYPE) );
	if( pBaseComp->pCallbacks == NULL ){
		return OMX_ErrorInsufficientResources;
	}
	pBaseComp->pCallbacks->EventHandler = NULL;
	pBaseComp->pCallbacks->EmptyBufferDone = NULL;
	pBaseComp->pCallbacks->FillBufferDone = NULL;
	pBaseComp->eCurState = OMX_StateLoaded;
	pBaseComp->eNewState = OMX_StateLoaded;
	return OMX_ErrorNone;
}

OMX_ERRORTYPE NX_BaseGetComponentVersion ( OMX_HANDLETYPE hComp, OMX_STRING pComponentName, OMX_VERSIONTYPE* pComponent, OMX_VERSIONTYPE* pSpecVersion, OMX_UUIDTYPE* pComponentUUID)
{
	NX_BASE_COMPNENT *pBaseComp = (NX_BASE_COMPNENT *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	strcpy( pComponentName, (OMX_STRING)pBaseComp->compName );
	*pComponent = ((OMX_COMPONENTTYPE*)hComp)->nVersion;
	*pSpecVersion = ((OMX_COMPONENTTYPE*)hComp)->nVersion;
	NxMemcpy( pComponentUUID, pBaseComp->compUUID, 128 );
	return OMX_ErrorNone;
}

//
//	openmax il spec v1.1.2 : 3.2.2.2 OMX_SendCommand
//	Description : The component normally executes the command outside the context of the call,
//				  though a solution without threading may elect to execute it in context.
//
OMX_ERRORTYPE NX_BaseSendCommand ( OMX_HANDLETYPE hComponent, OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData)
{
	NX_BASE_COMPNENT *pBaseComp = (NX_BASE_COMPNENT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	NX_CMD_MSG_TYPE *pCmdMsg = NxMalloc( sizeof(NX_CMD_MSG_TYPE) );
	pCmdMsg->id = pBaseComp->cmdId++;
	pCmdMsg->eCmd = Cmd;
	pCmdMsg->nParam1 = nParam1;
	pCmdMsg->pCmdData = pCmdData;

	//	Change new state
	if( OMX_CommandStateSet == Cmd ){
		pBaseComp->eNewState = nParam1;
	}

	NX_PushQueue( &pBaseComp->cmdQueue, pCmdMsg );
	NX_PostSem( pBaseComp->hSemCmd );
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseGetParameter (OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex,OMX_PTR ComponentParamStruct)
{
	NX_BASE_COMPNENT *pBaseComp = (NX_BASE_COMPNENT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
	switch( nParamIndex )
	{
		case OMX_IndexParamPortDefinition:
		{
			OMX_PARAM_PORTDEFINITIONTYPE *pInPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParamStruct;
			if( pInPortDef->nPortIndex >= pBaseComp->nNumPort )
				return OMX_ErrorBadPortIndex;
			pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)(pBaseComp->pPort[pInPortDef->nPortIndex]);
			NxMemcpy( ComponentParamStruct, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE) );
			break;
		}
		default:
			return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseSetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR ComponentParamStruct)
{
	NX_BASE_COMPNENT *pBaseComp = (NX_BASE_COMPNENT *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
	switch( nParamIndex )
	{
		case OMX_IndexParamPortDefinition:
		{
			OMX_PARAM_PORTDEFINITIONTYPE *pInPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParamStruct;
			if( pInPortDef->nPortIndex >= pBaseComp->nNumPort )
				return OMX_ErrorBadPortIndex;
			pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)(pBaseComp->pPort[pInPortDef->nPortIndex]);
			pPortDef->nBufferCountActual = pInPortDef->nBufferCountActual;
			switch(pPortDef->eDomain)
			{
				case OMX_PortDomainAudio:
					NxMemcpy(&pPortDef->format.audio, &pPortDef->format.audio, sizeof(OMX_AUDIO_PORTDEFINITIONTYPE));
					break;
				case OMX_PortDomainVideo:
					pPortDef->format.video.pNativeRender          = pInPortDef->format.video.pNativeRender;
					pPortDef->format.video.nFrameWidth            = pInPortDef->format.video.nFrameWidth;
					pPortDef->format.video.nFrameHeight           = pInPortDef->format.video.nFrameHeight;
					pPortDef->format.video.nStride                = pInPortDef->format.video.nStride;
					pPortDef->format.video.xFramerate             = pInPortDef->format.video.xFramerate;
					pPortDef->format.video.bFlagErrorConcealment  = pInPortDef->format.video.bFlagErrorConcealment;
					pPortDef->format.video.eCompressionFormat     = pInPortDef->format.video.eCompressionFormat;
					pPortDef->format.video.eColorFormat           = pInPortDef->format.video.eColorFormat;
					pPortDef->format.video.pNativeWindow          = pInPortDef->format.video.pNativeWindow;
					break;
				case OMX_PortDomainImage:
					pPortDef->format.image.nFrameWidth            = pInPortDef->format.image.nFrameWidth;
					pPortDef->format.image.nFrameHeight           = pInPortDef->format.image.nFrameHeight;
					pPortDef->format.image.nStride                = pInPortDef->format.image.nStride;
					pPortDef->format.image.bFlagErrorConcealment  = pInPortDef->format.image.bFlagErrorConcealment;
					pPortDef->format.image.eCompressionFormat     = pInPortDef->format.image.eCompressionFormat;
					pPortDef->format.image.eColorFormat           = pInPortDef->format.image.eColorFormat;
					pPortDef->format.image.pNativeWindow          = pInPortDef->format.image.pNativeWindow;
					break;
				case OMX_PortDomainOther:
					NxMemcpy(&pPortDef->format.other, &pInPortDef->format.other, sizeof(OMX_OTHER_PORTDEFINITIONTYPE));
					break;
				default:
					return OMX_ErrorBadParameter;
			}
			break;
		}
		default:
			return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseGetConfig (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nConfigIndex, OMX_PTR pComponentConfigStructure)
{
	UNUSED_PARAM(hComp);
	UNUSED_PARAM(nConfigIndex);
	UNUSED_PARAM(pComponentConfigStructure);
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseSetConfig (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nConfigIndex, OMX_PTR pComponentConfigStructure)
{
	UNUSED_PARAM(hComp);
	UNUSED_PARAM(nConfigIndex);
	UNUSED_PARAM(pComponentConfigStructure);
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseGetExtensionIndex(OMX_HANDLETYPE hComponent, OMX_STRING cParameterName, OMX_INDEXTYPE* pIndexType)
{
	UNUSED_PARAM(hComponent);
	//	Adnroid Extension
	if( strcmp(cParameterName, "OMX.google.android.index.getAndroidNativeBufferUsage") == 0 ){
		*pIndexType = OMX_IndexAndroidNativeBufferUsage;
		return OMX_ErrorNone;
	}
	if( strcmp(cParameterName, "OMX.google.android.index.enableAndroidNativeBuffers") == 0 ){
		*pIndexType = OMX_IndexEnableAndroidNativeBuffers;
		return OMX_ErrorNone;
	}
	if( strcmp(cParameterName, "OMX.google.android.index.useAndroidNativeBuffer2") == 0 ){
		*pIndexType = OMX_IndexUseAndroidNativeBuffer2;
		return OMX_ErrorNone;
	}
	if( strcmp(cParameterName, "OMX.google.android.index.storeMetaDataInBuffers" ) == 0 ){
		*pIndexType = OMX_IndexStoreMetaDataInBuffers;
		return OMX_ErrorNone;
	}
	if( strcmp(cParameterName, "OMX.NX.VIDEO_DECODER.ThumbnailMode" ) == 0 ){
		*pIndexType = OMX_IndexVideoDecoderThumbnailMode;
		return OMX_ErrorNone;
	}
	if( strcmp(cParameterName, "OMX.NX.VIDEO_DECODER.Extradata" ) == 0 ){
		*pIndexType = OMX_IndexVideoDecoderExtradata;
		return OMX_ErrorNone;
	}
	if( strcmp(cParameterName, "OMX.NX.VIDEO_DECODER.CodecTag" ) == 0 ){
		*pIndexType = OMX_IndexVideoDecoderCodecTag;
		return OMX_ErrorNone;
	}
	if( strcmp(cParameterName, "OMX.NX.AUDIO_DECODER.FFMPEG.Extradata" ) == 0 ){
		*pIndexType = OMX_IndexAudioDecoderFFMpegExtradata;
		return OMX_ErrorNone;
	}
	return OMX_ErrorNotImplemented;
}
OMX_ERRORTYPE NX_BaseGetState (OMX_HANDLETYPE hComp, OMX_STATETYPE* pState)
{
	NX_BASE_COMPNENT *pBaseComp = (NX_BASE_COMPNENT *)((OMX_COMPONENTTYPE *)hComp)->pComponentPrivate;
	*pState = pBaseComp->eCurState;
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseComponentTunnelRequest(OMX_HANDLETYPE hComp, OMX_U32 nPort, OMX_HANDLETYPE hTunneledComp, OMX_U32 nTunneledPort, OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	UNUSED_PARAM(hComp);
	UNUSED_PARAM(nPort);
	UNUSED_PARAM(hTunneledComp);
	UNUSED_PARAM(nTunneledPort);
	UNUSED_PARAM(pTunnelSetup);
	return OMX_ErrorNone;
} 
OMX_ERRORTYPE NX_BaseSetCallbacks(OMX_HANDLETYPE hComponent, OMX_CALLBACKTYPE* pCallbacks, OMX_PTR pAppData)
{
	OMX_COMPONENTTYPE *hComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_BASE_COMPNENT *pBaseComp = (NX_BASE_COMPNENT *)(hComp->pComponentPrivate);
	pBaseComp->pCallbacks = pCallbacks;
	pBaseComp->pCallbackData = pAppData;
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseComponentDeInit(OMX_HANDLETYPE hComponent)
{
	UNUSED_PARAM(hComponent);
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseUseEGLImage(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, void* eglImage)
{
	UNUSED_PARAM(hComponent);
	UNUSED_PARAM(ppBufferHdr);
	UNUSED_PARAM(nPortIndex);
	UNUSED_PARAM(pAppPrivate);
	UNUSED_PARAM(eglImage);
	return OMX_ErrorNone;
}
OMX_ERRORTYPE NX_BaseComponentRoleEnum(OMX_HANDLETYPE hComponent, OMX_U8 *cRole, OMX_U32 nIndex)
{
	UNUSED_PARAM(hComponent);
	UNUSED_PARAM(cRole);
	UNUSED_PARAM(nIndex);
	return OMX_ErrorNone;
}
