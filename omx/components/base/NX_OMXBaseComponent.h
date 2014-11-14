#ifndef __NX_OMXBaseComponent_h__
#define __NX_OMXBaseComponent_h__

//	Core & STD Header
#include <OMX_Core.h>
#include <OMX_Component.h>

//	Components Header
#include <NX_OMXCommon.h>
#include <NX_OMXBasePort.h>

//	Utils
#include <NX_OMXQueue.h>
#include <NX_OMXSemaphore.h>
#include <NX_OMXDebugMsg.h>
#include <NX_OMXMem.h>

#include <pthread.h>


#define DBG_FUNC_TRACE	1
#define DBG_CMD_MSG		1
#define DBG_BUF_MSG		1

//	OpenMax IL version 1.0.0.0
#define	NXOMX_VER_MAJOR		1
#define	NXOMX_VER_MINOR		0
#define	NXOMX_VER_REVISION	0
#define	NXOMX_VER_NSTEP		0

//	Default Recomanded Functions for Implementation Components
OMX_ERRORTYPE NX_BaseComponentInit (OMX_COMPONENTTYPE *hComponent);
OMX_ERRORTYPE NX_BaseGetComponentVersion ( OMX_HANDLETYPE hComp, OMX_STRING pComponentName, OMX_VERSIONTYPE* pComponent, OMX_VERSIONTYPE* pSpecVersion, OMX_UUIDTYPE* pComponentUUID);
OMX_ERRORTYPE NX_BaseSendCommand ( OMX_HANDLETYPE hComponent, OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData);
OMX_ERRORTYPE NX_BaseGetParameter (OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex,OMX_PTR ComponentParamStruct);
OMX_ERRORTYPE NX_BaseSetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR ComponentParamStruct);
OMX_ERRORTYPE NX_BaseGetConfig (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nConfigIndex, OMX_PTR pComponentConfigStructure);
OMX_ERRORTYPE NX_BaseSetConfig (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nConfigIndex, OMX_PTR pComponentConfigStructure);
OMX_ERRORTYPE NX_BaseGetExtensionIndex(OMX_HANDLETYPE hComponent, OMX_STRING cParameterName, OMX_INDEXTYPE* pIndexType);
OMX_ERRORTYPE NX_BaseGetState (OMX_HANDLETYPE hComp, OMX_STATETYPE* pState);
OMX_ERRORTYPE NX_BaseComponentTunnelRequest(OMX_HANDLETYPE hComp, OMX_U32 nPort, OMX_HANDLETYPE hTunneledComp, OMX_U32 nTunneledPort, OMX_TUNNELSETUPTYPE* pTunnelSetup); 
OMX_ERRORTYPE NX_BaseEmptyThisBuffer (OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE NX_BaseFillThisBuffer(OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE NX_BaseSetCallbacks(OMX_HANDLETYPE hComponent, OMX_CALLBACKTYPE* pCallbacks, OMX_PTR pAppData);
OMX_ERRORTYPE NX_BaseComponentDeInit(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NX_BaseUseEGLImage(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, void* eglImage);
OMX_ERRORTYPE NX_BaseComponentRoleEnum(OMX_HANDLETYPE hComponent, OMX_U8 *cRole, OMX_U32 nIndex);

#define	NX_OMX_MAX_PORTS		4		//	Max number of ports within components


typedef enum NX_THREAD_CMD{
	NX_THREAD_CMD_INVALID,
	NX_THREAD_CMD_RUN,
	NX_THREAD_CMD_PAUSE,
	NX_THREAD_CMD_EXIT
}NX_THREAD_CMD;


///////////////////////////////////////////////////////////////////////
//					Nexell Base Component Type
typedef struct _NX_CMD_MSG_TYPE NX_CMD_MSG_TYPE;
struct _NX_CMD_MSG_TYPE{
	OMX_U32					id;
	OMX_COMMANDTYPE			eCmd;
	OMX_U32					nParam1;
	OMX_PTR					pCmdData;
};
//
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
//					Nexell Base Component Type
#define NX_BASECOMPONENTTYPE								\
	OMX_COMPONENTTYPE		*hComp;							\
	OMX_U32					nNumPort;						\
	OMX_STRING				compName;						\
	OMX_STRING				compRole;						\
	OMX_UUIDTYPE			compUUID[128];					\
	OMX_PTR					pPort[NX_OMX_MAX_PORTS];		\
	OMX_PTR					pBufQueue[NX_OMX_MAX_PORTS];	\
	OMX_STATETYPE			eCurState;						\
	OMX_STATETYPE			eNewState;						\
	OMX_CALLBACKTYPE		*pCallbacks;					\
	OMX_PTR					pCallbackData;					\
	/*					Command Thread				*/		\
	pthread_t				hCmdThread;						\
	NX_THREAD_CMD			eCmdThreadCmd;					\
	NX_SEMAPHORE			*hSemCmd;						\
	NX_SEMAPHORE			*hSemCmdWait;					\
	NX_QUEUE				cmdQueue;						\
	OMX_U32					cmdId;							\

//	End of nexell base component type
///////////////////////////////////////////////////////////////////////


typedef struct _NX_BASE_COMPNENT NX_BASE_COMPNENT;
struct _NX_BASE_COMPNENT{
	NX_BASECOMPONENTTYPE
};

#endif	//	 __NX_OMXBaseComponent_h__