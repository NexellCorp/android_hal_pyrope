#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#define MAX_STREAM_BUFFERS          4  /* max num of v4l2 buffer */
#define MAX_NUM_FRAMES              16 /* max num of native handle per stream */

#define MAX_STREAM                  8

/* stream id : used in Android Framework */
enum {
    STREAM_ID_INVALID = 0,
    STREAM_ID_PREVIEW,
    STREAM_ID_CAPTURE,
    STREAM_ID_RECORD,
    STREAM_ID_CALLBACK,
    STREAM_ID_ZSL,
    STREAM_ID_REPROCESS,
    STREAM_ID_MAX
};

#define DEFAULT_PIXEL_FORMAT        HAL_PIXEL_FORMAT_YV12 /* yuv420 3plane */

#define MAX_THREAD_NAME             64

#define MAX_PREVIEW_ZOOM_BUFFER     4
#define MAX_CAPTURE_ZOOM_BUFFER     1
#define MAX_RECORD_ZOOM_BUFFER      4

#define MAX_PREVIEW_INTERNAL_BUFFER 4 // for CSC(YV12 to NV21) must be same to MAX_PREVIEW_ZOOM_BUFFER

#define DEFAULT_ZOOM_FACTOR         4.0

#endif
