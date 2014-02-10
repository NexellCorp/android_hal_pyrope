#ifndef __NX_MoviePlay_H__
#define __NX_MoviePlay_H__

typedef struct MOVIE_TYPE	*MP_HANDLE;
#define  MP_RESULT           int

//	uri type
enum{
	URI_TYPE_FILE,
	URI_TYPE_URL
};

//	disply port
enum{
	DISPLAY_PORT_LCD,
 	DISPLAY_PORT_HDMI
};

//	disply module
enum{
	DISPLAY_MODULE_MLC0,
	DISPLAY_MODULE_MLC1,
};

//	contents type
enum{
	CONTENTS_TYPE_MP4,
	CONTENTS_TYPE_H264,
	CONTENTS_TYPE_H263,
	CONTENTS_TYPE_FLV,
	CONTENTS_TYPE_RMVB,
	CONTENTS_TYPE_DIVX,
	CONTENTS_TYPE_WMV,
	CONTENTS_TYPE_MKV,
	CONTENTS_TYPE_THEROA,
	CONTENTS_TYPE_MP2,
};

enum{
	CALLBACK_MSG_EOS			= 0x1000,
	CALLBACK_MSG_PLAY_ERR		= 0x8001,
};

//ErrorCode
enum{
	ERROR_NONE 				                    = 0,
	ERROR										= -1,
	ERROR_HANDLE		                 		= -2,
	ERROR_NOT_SUPPORT_CONTENTS          		= -3
}; 


typedef struct MP_MEDIA_INFO {
	unsigned long		VDuration;
	unsigned long       Framerate;
	unsigned long		Width;	
	unsigned long		Height;	
	unsigned long		ADuration;	
	unsigned long		samplerate;	
	unsigned long		channels;	
	unsigned long		bitrate;	
} MP_MEDIA_INFO;


#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

MP_RESULT NX_MPOpen( MP_HANDLE *handle, const char *uri, int volumem, int dspModule, int dspPort,
						void (*cb)(void *owner, unsigned int msg, unsigned int param1, unsigned int param2), void *cbPrivate);
void NX_MPClose( MP_HANDLE handle );
MP_RESULT NX_MPGetMediaInfo( MP_HANDLE handle, int index, MP_MEDIA_INFO *pInfo );
MP_RESULT NX_MPPlay( MP_HANDLE handle, float speed );
MP_RESULT NX_MPPause(MP_HANDLE hande);
MP_RESULT NX_MPStop(MP_HANDLE hande);
MP_RESULT NX_MPSeek(MP_HANDLE hande, unsigned int seekTime);				//seekTime : msec
MP_RESULT NX_MPGetCurDuration(MP_HANDLE handle, unsigned int *duration);	//duration : msec
MP_RESULT NX_MPGetCurPosition(MP_HANDLE handle, unsigned int *position);	//position : msec	
MP_RESULT NX_MPSetDspPosition(MP_HANDLE handle,	int dspModule, int dspPort, int x, int y, int w,	int h );
MP_RESULT NX_MPSetVolume(MP_HANDLE handle, int volume);						//volume range : 0 ~ 100

#ifdef __cplusplus
}
#endif


#endif // __NX_MoviePlay_H__
