#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <gst/gst.h>
#include <glib.h>
#include "NX_DbgMsg.h"
#include "NX_MoviePlay.h"

typedef struct MOVIE_TYPE
{
	GstElement *pipeline;

	GstElement *input_src;

	GstElement *audio_queue;
	GstElement *video_queue;

	GstElement *demuxer;

	GstElement *audio_decoder;
	GstElement *video_decoder;

	GstElement *volume;
	GstElement *typefind;

	GstElement *dec_queue;

	GstElement *audio_sink;
	GstElement *video_sink;

	GMainLoop *loop;

	GThread *thread;

	GstBus *bus;

	gboolean pipeline_is_linked;

	gint contents_type;
	gint uri_type;
	gchar *uri;

	gint is_buffering_paused;
	gint seek_state;	
	gint pause_state;
	gint play_state;	
	gint stop_state;

	void (*callback)(void*, guint, guint, guint );
	void *owner;

}MOVIE_TYPE;

//	uri type
static const char *URITypeString[] =
{
	"URI_TYPE_FILE",
	"URI_TYPE_URL"
};

static const char *ContentsTypeString[] =
{
	"CONTENTS_TYPE_MP4",
	"CONTENTS_TYPE_H264",
	"CONTENTS_TYPE_H263",
	"CONTENTS_TYPE_FLV",
	"CONTENTS_TYPE_RMVB",
	"CONTENTS_TYPE_DIVX",
	"CONTENTS_TYPE_WMV",
	"CONTENTS_TYPE_MKV",
	"CONTENTS_TYPE_THEROA",
	"CONTENTS_TYPE_MP2",
};


static gpointer loop_func (gpointer data)
{
	FUNC_IN();

	MP_HANDLE handle = (MP_HANDLE )data;	
	g_main_loop_run (handle->loop); 
	
	FUNC_OUT();

	return NULL;
}

static void start_loop_thread (MP_HANDLE handle)
{
	FUNC_IN();

	handle->loop = g_main_loop_new (NULL, FALSE);
	handle->thread = g_thread_create (loop_func, handle, TRUE, NULL);

	FUNC_OUT();
}


static void stop_my_thread (MP_HANDLE handle)
{
	FUNC_IN();

	g_main_loop_quit (handle->loop);
	g_thread_join (handle->thread);
	g_main_loop_unref (handle->loop);

	FUNC_OUT();
}

static gboolean bus_callback( GstBus *bus, GstMessage *msg, MP_HANDLE handle )
{
	gchar  *debug;
	GError *err;
	FUNC_IN();

	switch (GST_MESSAGE_TYPE (msg))
	{
		case GST_MESSAGE_EOS:
			g_print ("End of stream(%p)\n", handle->callback);
			if( handle->callback )
			{
				g_print("Callback ++\n");
				handle->callback(handle->owner, CALLBACK_MSG_EOS, 0, 0);
				g_print("Callback --\n");
			}
			break;

		case GST_MESSAGE_WARNING:
			gst_message_parse_warning(msg, &err, &debug);
			g_warning("GST warning: %s", err->message);
			g_error_free(err);
			g_free(debug);
			break;

		case GST_MESSAGE_STATE_CHANGED:
		{
			GstState old_state, new_state;
			gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
			break;
		}
		case GST_MESSAGE_ERROR:
			gst_message_parse_error (msg, &err, &debug);
			g_free (debug);
			ErrMsg ("Error: domain=%d message:%s\n", err->domain, err->message);
			g_error_free (err);
			break;
		default:
#if DEB_MESSAGE
			DbgMsg("default : message type:0x%08x(%d)\n", GST_MESSAGE_TYPE (msg), GST_MESSAGE_TYPE (msg));
#endif
			break;
	}

	FUNC_OUT();

	return TRUE;
}

void on_demux_pad_added(GstElement *element, GstPad *pad, MP_HANDLE handle)
{
	GstCaps *caps;
	GstStructure *str;
	gchar *name;
	GstPadLinkReturn rc;
	GstElement *targetqueue;
	GstPad *targetsink;

	FUNC_IN();

	caps = gst_pad_get_caps(pad);
	g_assert(caps != NULL);
	name = gst_pad_get_name(pad);
	DbgMsg("-new demux pad %s\n", name);

	str = gst_caps_get_structure(caps, 0);
	g_assert(str != NULL);

	DbgMsg("-compare string %s\n", gst_structure_get_name(str));

	targetqueue = NULL;
	// TODO: is this the right way to match video/audio pads

	if (g_strrstr(gst_structure_get_name(str), "video")) {
		DbgMsg("-Linking %s to %s\n", name, gst_structure_get_name(str) );
		targetqueue = handle->video_queue;
	}
	else if (g_strrstr(gst_structure_get_name(str), "audio")) {
		DbgMsg("-Linking %s to %s\n", name, gst_structure_get_name(str) );
		targetqueue = handle->audio_queue;
	}

	if(0 != strcmp(name,"video_01"))
	{
		if (targetqueue) {
			targetsink = gst_element_get_pad(targetqueue, "sink");
			g_assert(targetsink != NULL);
			rc = gst_pad_link(pad, targetsink);
			if (rc) {
				g_critical("demux pad link failed(%s): %d\n", __func__, rc);
			}
			gst_object_unref(targetsink);
		}
	}
	g_free(name);	


	gst_caps_unref(caps);

	FUNC_OUT();

}

void on_decodebin_pad_added(GstElement *element, GstPad *pad, MP_HANDLE handle)
{
	GstCaps *caps;
	GstStructure *str;
	gchar *name;
	GstPadLinkReturn rc;
	GstElement *targetqueue;
	GstPad *targetsink;

	FUNC_IN();

	caps = gst_pad_get_caps(pad);
	g_assert(caps != NULL);
	name = gst_pad_get_name(pad);
	DbgMsg("-new decodebin pad %s\n", name);

	str = gst_caps_get_structure(caps, 0);
	g_assert(str != NULL);

	DbgMsg("-compare string %s\n", gst_structure_get_name(str));

	targetqueue = NULL;
	// TODO: is this the right way to match /audio pads

	if (g_strrstr(gst_structure_get_name(str), "audio")) {
		DbgMsg("-Linking %s to %s\n", name, gst_structure_get_name(str) );
		targetqueue = handle->volume;
	}

	g_free(name);

	if (targetqueue) {
		targetsink = gst_element_get_pad(targetqueue, "sink");
		g_assert(targetsink != NULL);
		rc = gst_pad_link(pad, targetsink);
		if (rc) {
			g_critical("decodebin pad link failed(%s): %d\n", __func__, rc);
		}
		gst_object_unref(targetsink);
	}

	gst_caps_unref(caps);

	FUNC_OUT();

}

static void cb_typefound (GstElement *typefind,
	      					guint       probability,
	      					GstCaps    *caps,
	      					gpointer    data)
{
  GMainLoop *loop = data;
  gchar *type;

  type = gst_caps_to_string (caps);
  g_print ("Media type %s found, probability %d%%\n", type, probability);
  g_free (type);

  /* since we connect to a signal in the pipeline thread context, we need
   * to set an idle handler to exit the main loop in the mainloop context.
   * Normally, your app should not need to worry about such things. */
//  g_idle_add (idle_exit_loop, loop);
}

MP_RESULT NX_MPOpen( MP_HANDLE *handle_s, const char *uri, int volumem, int dspModule, int dspPort,
					void (*cb)(void *owner, unsigned int msg, unsigned int param1, unsigned int param2), void *cbPrivate )
{
	MP_HANDLE handle = NULL;
	const gchar *surfix = NULL;
	gint uri_len = 0;
	double vol = 0;
	FUNC_IN();

	if( !gst_is_initialized() ){
		gst_init(NULL, NULL);
	}

	handle = (MP_HANDLE)malloc( sizeof(MOVIE_TYPE) );
	if(NULL == handle)
		return ERROR_HANDLE;

	memset( handle, 0, sizeof(MOVIE_TYPE) );

	if( ( strncmp( uri, "http://", 7 ) == 0 ) ||
		( strncmp( uri, "https://", 8 ) == 0 ) )
	{
		handle->uri_type = URI_TYPE_URL;
	}
	else
	{
		handle->uri_type = URI_TYPE_FILE;
	}

	uri_len = strlen( uri );

	if( uri_len <= 4 )
		goto ERROR_EXIT;
	surfix = &uri[ uri_len-4 ];

	if( strncmp( ".mp4", surfix, 4 ) == 0 )	
	{
		handle->contents_type = CONTENTS_TYPE_MP4;
	}
	else if( strncmp( ".avi", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_DIVX;
	}
	else if( strncmp( ".h264", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_H264;
	}
	else if( strncmp( ".h263", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_H263;
	}
	else if( strncmp( ".flv", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_FLV;
	}
	else if( strncmp( ".rmvb", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_RMVB;
	}
	else if( strncmp( ".divx", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_DIVX;
	}
	else if( strncmp( ".wmv", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_WMV;
	}
	else if( strncmp( ".mkv", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_MKV;
	}
	else if( strncmp( ".ogg", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_THEROA;
	}
	else if( strncmp( "mp2", surfix, 4 ) == 0 )
	{
		handle->contents_type = CONTENTS_TYPE_MP2;
	}
	else
	{
		ErrMsg("%s() : Unrecognize contents : %s\n", __func__, uri );
		goto ERROR_EXIT;
	}

	handle->uri = strdup( uri );

	handle->callback = cb;
	handle->owner = cbPrivate;

	if( handle->pipeline_is_linked )
	{
		DbgMsg("%s() : Already Initialized\n",__func__);
		return ERROR_NONE;
	}

	handle->pipeline = gst_pipeline_new ("Movie Player");
	if( NULL==handle->pipeline )
	{
		g_print("NULL==handle->pipeline\n");
		goto ERROR_EXIT;
	}

	//	Add bus watch
	handle->bus = gst_pipeline_get_bus( (GstPipeline*)(handle->pipeline) );
	gst_bus_add_watch ( handle->bus, (GstBusFunc)bus_callback, handle );
	gst_object_unref ( GST_OBJECT(handle->bus) );

	/* Create gstreamer elements */
	switch( handle->uri_type )
	{
	case URI_TYPE_FILE:
		handle->input_src = gst_element_factory_make ("filesrc", "file-source");
		break;
	case URI_TYPE_URL :
		handle->input_src = gst_element_factory_make ("souphttpsrc", NULL);
		break;
	}
	if( NULL==handle->input_src )
	{
		g_print("NULL==handle->input_src\n");
	}
	// we set the input filename to the source element
	g_object_set (G_OBJECT (handle->input_src), "location", handle->uri, NULL);

	//	add audio queue,decoder,sink,volume
	handle->audio_queue = gst_element_factory_make ("queue", "AudioQueue");
	handle->audio_decoder = gst_element_factory_make ("decodebin", "AudioDec");
	handle->audio_sink  = gst_element_factory_make ("alsasink", "audio-sink");	

	g_object_set (G_OBJECT (handle->audio_sink ), "device", "plug:dmix", NULL);

	handle->volume = gst_element_factory_make ("volume", "volume"); 
	g_assert (handle->volume != NULL);
	vol = (double)volumem/100.;
	g_object_set (G_OBJECT (handle->volume), "volume", vol, NULL);

	//add video queue,decoder,sink
	handle->video_queue = gst_element_factory_make ("queue", "VideoQueue");
	handle->video_decoder = gst_element_factory_make ("nxvideodecoder", "VideoDec");
	handle->video_sink = gst_element_factory_make ("nxvideosink", "video-sink");
	g_object_set(G_OBJECT(handle->video_sink), "dsp-module", dspModule, NULL);
	g_object_set(G_OBJECT(handle->video_sink), "dsp-port", dspPort, NULL);
	g_object_set(G_OBJECT(handle->video_sink), "dsp-priority", 0, NULL);

	//	HDMI
	if(DISPLAY_PORT_LCD == dspPort )	//Lcd
		g_object_set(G_OBJECT(handle->audio_sink), "device", "default", NULL);				
	else 								//Hdmi 
		g_object_set(G_OBJECT(handle->audio_sink), "device", "default:CARD=SPDIFTranscieve", NULL);				

	//add demuxer
	switch( handle->contents_type )
	{
	case CONTENTS_TYPE_MP4:
	case CONTENTS_TYPE_H264:
	case CONTENTS_TYPE_H263:
		{
			handle->demuxer = gst_element_factory_make ("qtdemux", "demuxer" );			//h264, mp4v, h263
			break;
		}
	case CONTENTS_TYPE_FLV:
		{
			handle->demuxer = gst_element_factory_make ("flvdemux", "demuxer" );		//flv	
			break;
		}

	case CONTENTS_TYPE_RMVB:
		{
			handle->demuxer = gst_element_factory_make ("rmdemux", "demuxer" );			//rmvb
			break;
		}
	case CONTENTS_TYPE_DIVX:
		{
			handle->demuxer = gst_element_factory_make ("avidemux", "demuxer" );		//divx
			break;
		}
	case CONTENTS_TYPE_WMV:
		{
			handle->demuxer = gst_element_factory_make ("asfdemux", "demuxer" );		//vc1,wmv
			break;
		}
	case CONTENTS_TYPE_MKV:
		{
			handle->demuxer = gst_element_factory_make ("matroskademux", "demuxer" );	//.mkv 
			break;
		}
	case CONTENTS_TYPE_THEROA:
		{
			handle->demuxer = gst_element_factory_make ("oggdemux", "demuxer" );		//theroa  .ogg
			break;
		}
	case CONTENTS_TYPE_MP2:
		{
			handle->demuxer = gst_element_factory_make ("mpegpsdemux", "demuxer" );		//mpeg2
			break;
		}

	}	
	
	//mp4  :
	//		input-source | mp4demux	| queue	| videodecoder	| 			 videosink
	//								| queue	| audiodecoder	| volume    |alsasink
	gst_bin_add_many(	GST_BIN(handle->pipeline),
						handle->input_src,																	//	input_src 
						handle->demuxer,																	//	demux
						handle->video_queue, handle->video_decoder, handle->video_sink,						//	video
						handle->audio_queue, handle->audio_decoder, handle->volume, handle->audio_sink,		//	audio
						NULL	);

	//	link
	gst_element_link( handle->input_src, handle->demuxer );
	gst_element_link_many ( handle->video_queue, handle->video_decoder, handle->video_sink, NULL );
	gst_element_link( handle->audio_queue, handle->audio_decoder );
	gst_element_link( handle->volume, handle->audio_sink );

	g_signal_connect( handle->demuxer, "pad-added", G_CALLBACK(on_demux_pad_added), handle );
	g_signal_connect( handle->audio_decoder, "pad-added", G_CALLBACK(on_decodebin_pad_added), handle );

	if( GST_STATE_CHANGE_FAILURE == gst_element_set_state (handle->pipeline, GST_STATE_READY) )
	{
		DbgMsg("%s() : GST_STATE_CHANGE_FAILURE\n",__func__);
		return ERROR;
	}

	if( GST_STATE_CHANGE_FAILURE == gst_element_get_state( handle->pipeline, NULL, NULL, -1 ) )
	{
		DbgMsg("%s() : GST_STATE_CHANGE_FAILURE_1\n",__func__);
		return ERROR;
	}

	handle->pipeline_is_linked = TRUE;
	start_loop_thread( handle );

	*handle_s = handle;

	FUNC_OUT();

	return ERROR_NONE;

ERROR_EXIT:

	if( handle ){
		free( handle );
	}

	return ERROR;
}

void NX_MPClose( MP_HANDLE handle )
{
	FUNC_IN();
	if( !handle )
		return;

	if( handle->pipeline_is_linked )
	{
		stop_my_thread( handle );
		gst_element_set_state (handle->pipeline, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (handle->pipeline));
	}

	if(handle->uri)
		free( handle->uri );
	free( handle );
	FUNC_OUT();
}

MP_RESULT NX_MPGetMediaInfo( MP_HANDLE handle, int idx, MP_MEDIA_INFO *pInfo )
{
	FUNC_IN();

	FUNC_OUT();
	return	ERROR_NONE;
}

MP_RESULT NX_MPPlay( MP_HANDLE handle, float speed )
{
	GstStateChangeReturn ret;
	FUNC_IN();
	if( !handle || !handle->pipeline_is_linked )
	{
		ErrMsg("%s() : invalid state or invalid operation.(%p,%d)\n", __func__, handle, handle->pipeline_is_linked);
		return ERROR;
	}
	ret = gst_element_set_state (handle->pipeline, GST_STATE_PLAYING);
	if( GST_STATE_CHANGE_FAILURE == ret ){
		ErrMsg("%s() : Sate change failed!!!(ret=%d)\n", __func__, ret);
		return ERROR;
	}
//	handle->seek_state = 0;	
//	handle->pause_state = 0;	
	FUNC_OUT();
	return	ERROR_NONE;
}


MP_RESULT NX_MPPause(MP_HANDLE hande)
{
	GstStateChangeReturn ret;

	FUNC_IN();

	if( !hande || !hande->pipeline_is_linked )
	{
		ErrMsg("%s() : invalid state or invalid operation.(%p,%d)\n", __func__, hande, hande->pipeline_is_linked);
		return ERROR;
	}
	ret = gst_element_set_state (hande->pipeline, GST_STATE_PAUSED);
	if( GST_STATE_CHANGE_FAILURE == ret ){
		ErrMsg("%s() : Sate change failed!!!(ret=%d)\n", __func__, ret);
		return ERROR;
	}
//	handle->pause_state = 1;	
	FUNC_OUT();

	return	ERROR_NONE;
}

MP_RESULT NX_MPStop(MP_HANDLE handle)
{
	GstStateChangeReturn ret;

	FUNC_IN();

	if( !handle || !handle->pipeline_is_linked )
	{
		ErrMsg("%s() : invalid state or invalid operation.(%p,%d)\n", __func__, handle, handle->pipeline_is_linked);
		return ERROR;
	}
	ret = gst_element_set_state (handle->pipeline, GST_STATE_READY);
	if( GST_STATE_CHANGE_FAILURE == ret ){
		ErrMsg("%s() : Sate change failed!!!(ret=%d)\n", __func__, ret);
		return ERROR;
	}
	FUNC_OUT();
	return	ERROR_NONE;
}

MP_RESULT NX_MPSeek(MP_HANDLE hande, unsigned int seekTime)
{
	GstState state, pending;
	GstStateChangeReturn ret;
	unsigned int milliSeconds = 0;
	gint64 seekTimeUs = 0;
	milliSeconds = seekTime;
	seekTimeUs = (gint64)milliSeconds*1000000;
	FUNC_IN();
	if( !hande || !hande->pipeline_is_linked )
	{
		ErrMsg("%s() : invalid state or invalid operation.(%p,%d)\n", __func__, hande, hande->pipeline_is_linked);
		return ERROR;
	}

	ret = gst_element_get_state( hande->pipeline, &state, &pending, 500000000 );		//	wait 500 msec

	if( GST_STATE_CHANGE_FAILURE != ret )
	{
		if( state == GST_STATE_PLAYING || state == GST_STATE_PAUSED || state == GST_STATE_READY )
		{  

			if (!gst_element_seek_simple (hande->pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT , seekTimeUs))
			{
				g_printerr ("Seek failed (%d msec)\n", milliSeconds);
				return ERROR;
			}
		}
		else
		{
			ErrMsg("AUDP ERR STATE!!!\n");
			return ERROR;	//	Invalid state
		}
	}
	else
	{
		ErrMsg("AUDP ERR STATE!!!!\n");	
		return ERROR;
	}

//	pAppData->seek_state = 1;	
	FUNC_OUT();
	return	ERROR_NONE;
}


MP_RESULT NX_MPGetCurDuration(MP_HANDLE handle, unsigned int *duration)
{
	GstState state, pending;
	GstStateChangeReturn ret;
	unsigned int pMilliSeconds = 0;
	FUNC_IN();
	if( !handle || !handle->pipeline_is_linked )
	{
		ErrMsg("%s() : invalid state or invalid operation.(%p,%d)\n", __func__, handle, handle->pipeline_is_linked);
		return ERROR;
	}

	ret = gst_element_get_state( handle->pipeline, &state, &pending, 500000000 );		//	wait 500 msec
	if( GST_STATE_CHANGE_FAILURE != ret )
	{
		if( state == GST_STATE_PLAYING || state == GST_STATE_PAUSED || state==GST_STATE_READY )
		{
			gint64 len;
			GstFormat format = GST_FORMAT_TIME;
			if (gst_element_query_duration (handle->pipeline, &format, &len))
			{
				pMilliSeconds = (unsigned int)((len / 1000000));	
				*duration = pMilliSeconds;
				VbsMsg("Duration: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS (len));	//
			}
		}
		else
		{
			return ERROR;	//	Invalid state
		}
	}
	else
	{
		return ERROR;
	}

	FUNC_OUT();

	return	ERROR_NONE;
}


MP_RESULT NX_MPGetCurPosition(MP_HANDLE handle, unsigned int *position)
{
	GstState state, pending;
	GstStateChangeReturn ret;
	unsigned int pMilliSeconds = 0;
	FUNC_IN();
	if( !handle || !handle->pipeline_is_linked )
	{
		ErrMsg("%s() : invalid state or invalid operation.(%p,%d)\n", __func__, handle, handle->pipeline_is_linked);
		return ERROR;
	}

	ret = gst_element_get_state( handle->pipeline, &state, &pending, 500000000 );		//	wait 500 msec
	if( GST_STATE_CHANGE_FAILURE != ret )
	{
		if( state == GST_STATE_PLAYING || state == GST_STATE_PAUSED )
		{
			gint64 pos;
			GstFormat format = GST_FORMAT_TIME;
			if (gst_element_query_position(handle->pipeline, &format, &pos))
			{
				pMilliSeconds = (unsigned int)((pos / 1000000));
				*position = pMilliSeconds;
				VbsMsg("Position: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS (pos));	//
			}
		}
		else
		{
			return ERROR;	//	Invalid state
		}
	}
	else
	{
		return ERROR;
	}

	FUNC_OUT();

	return ERROR_NONE;

}


MP_RESULT NX_MPSetDspPosition(MP_HANDLE handle,	int dspModule, int dsPport, int x, int y, int width,	int height )
{
	FUNC_IN();
	if( handle && handle->video_sink )
	{
		g_object_set(G_OBJECT(handle->video_sink), "dsp-x", x, NULL);
		g_object_set(G_OBJECT(handle->video_sink), "dsp-y", y, NULL);
		g_object_set(G_OBJECT(handle->video_sink), "dsp-width", width, NULL);
		g_object_set(G_OBJECT(handle->video_sink), "dsp-height", height, NULL);
	}
	FUNC_OUT();
	return	ERROR_NONE;
}


MP_RESULT NX_MPSetVolume(MP_HANDLE handle, int volume)
{
	FUNC_IN();
	if( handle && handle->audio_sink && handle->volume )
	{
		double vol = (double)volume/100.;
		printf("NX_MPSetVolume = %f\n", vol);
		g_object_set (G_OBJECT (handle->volume), "volume", vol, NULL);		
	}
	FUNC_OUT();
	return	ERROR_NONE;
}

