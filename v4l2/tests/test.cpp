#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <ion.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <linux/videodev2_nxp_media.h>

#include "../include/nxp-v4l2.h"

//#define DEFAULT_WIDTH  640
//#define DEFAULT_HEIGHT 480
#define DEFAULT_WIDTH  800
#define DEFAULT_HEIGHT 600

#define MAX_BUFFER_COUNT 4

#define CHECK_COMMAND(command) do { \
        int ret = command; \
        if (ret < 0) { \
            fprintf(stderr, "line %d error!!!\n", __LINE__); \
            return ret; \
        } \
    } while (0)


unsigned int get_size(int format, int num, int width, int height)
{
    int size;

    switch (format) {
    case V4L2_PIX_FMT_YUYV:
        if (num > 0) return 0;
        size = (width * height) * 2;
        break;
    case V4L2_PIX_FMT_YUV420M:
        if (num == 0) {
            size = width * height;
        } else {
            size = (width * height) >> 2;
        }
        break;
    case V4L2_PIX_FMT_YUV422P:
        if (num == 0) {
            size = width * height;
        } else {
            size = (width * height) >> 1;
        }
        break;
    case V4L2_PIX_FMT_YUV444:
        size = width * height;
        break;
    default:
        size = width * height * 2;
        break;
    }

    return size;
}

int alloc_buffers(int ion_fd, int count, struct nxp_vid_buffer *bufs, int width, int height, int format)
{
    int ret;
    int size;
    int i, j;
    int y_size = width * height;
    int cb_size;
    int cr_size;
    struct nxp_vid_buffer *buffer;
    int plane_num;

    if (format == V4L2_PIX_FMT_YUYV || format == V4L2_PIX_FMT_RGB565) {
        plane_num = 1;
    } else {
        plane_num = 3;
    }

    printf("Allocation Buffer: count(%d), plane(%d)\n", count, plane_num);
    for (i = 0; i < count; i++) {
        buffer = &bufs[i];
        printf("[Buffer %d] --->\n", i);
        for (j = 0; j < plane_num; j++) {
            buffer->sizes[j] = get_size(format, j, width, height);
            ret = ion_alloc_fd(ion_fd, buffer->sizes[j], 0, ION_HEAP_NXP_CONTIG_MASK, 0, &buffer->fds[j]);
            if (ret < 0) {
                fprintf(stderr, "failed to ion_alloc_fd()\n");
                return ret;
            }
            buffer->virt[j] = (char *)mmap(NULL, buffer->sizes[j], PROT_READ | PROT_WRITE, MAP_SHARED, buffer->fds[j], 0);
            if (!buffer->virt[j]) {
                fprintf(stderr, "failed to mmap\n");
                return ret;
            }
            ret = ion_get_phys(ion_fd, buffer->fds[j], &buffer->phys[j]);
            if (ret < 0) {
                fprintf(stderr, "failed to get phys\n");
                return ret;
            }
            buffer->plane_num = plane_num;
            printf("\tplane %d: fd(%d), size(%d), phys(0x%x), virt(0x%x)\n",
                    j, buffer->fds[j], buffer->sizes[j], buffer->phys[j], buffer->virt[j]);
        }
    }

    return 0;
}

#define V4L2_CID_CAMERA_MODE_CHANGE     (V4L2_CTRL_CLASS_CAMERA | 0x1003)

// # ./test w h
int main(int argc, char *argv[])
{
    int ion_fd = ion_open();
    int width;
    int height;
    int module = 0;
    int clipper_id = nxp_v4l2_clipper0;
    int sensor_id = nxp_v4l2_sensor0;
    int video_id = nxp_v4l2_mlc0_video;
    int format = V4L2_PIX_FMT_YUV420M;
    // int format = V4L2_PIX_FMT_YUV422P;
    // int format = V4L2_PIX_FMT_YUV444;
    // int format = V4L2_PIX_FMT_YUYV;
    if (ion_fd < 0) {
        fprintf(stderr, "can't open ion!!!\n");
        return -EINVAL;
    }

    if (argc >= 4) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
        module = atoi(argv[3]);
    } else {
        printf("usage: ./test width height module");
        return 0;
    }

    struct V4l2UsageScheme s;
    memset(&s, 0, sizeof(s));

    if (module == 0) {
        s.useClipper0 = true;
        s.useDecimator0 = true;
        clipper_id = nxp_v4l2_clipper0;
        sensor_id = nxp_v4l2_sensor0;
    } else {
        s.useClipper1 = true;
        s.useDecimator1 = true;
        clipper_id = nxp_v4l2_clipper1;
        sensor_id = nxp_v4l2_sensor1;
    }
    s.useMlc0Video = true;

    printf("width %d, height %d, module %d\n", width, height, module);

    CHECK_COMMAND(v4l2_init(&s));
    CHECK_COMMAND(v4l2_set_format(clipper_id, width, height, format));
    CHECK_COMMAND(v4l2_set_crop(clipper_id, 0, 0, width, height));
    // for sp0838 601
    if (module == 1)
        CHECK_COMMAND(v4l2_set_format(sensor_id, 640, 480, V4L2_MBUS_FMT_YUYV8_2X8));
    else
        CHECK_COMMAND(v4l2_set_format(sensor_id, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    CHECK_COMMAND(v4l2_set_format(video_id, width, height, format));

    if (width > 1280 || height > 800)
        CHECK_COMMAND(v4l2_set_crop(video_id, 0, 0, 1280, 800));
    else
        CHECK_COMMAND(v4l2_set_crop(video_id, 0, 0, width, height));

#if 0
    if (width > 1024 || height > 768) {
        printf("set sensor mode to capture\n");
        CHECK_COMMAND(v4l2_set_ctrl(sensor_id, V4L2_CID_CAMERA_MODE_CHANGE, 1));
    } else {
        printf("set sensor mode to preview\n");
        CHECK_COMMAND(v4l2_set_ctrl(sensor_id, V4L2_CID_CAMERA_MODE_CHANGE, 0));
    }
#endif

    CHECK_COMMAND(v4l2_set_ctrl(video_id, V4L2_CID_MLC_VID_PRIORITY, 0));
    CHECK_COMMAND(v4l2_set_ctrl(video_id, V4L2_CID_MLC_VID_COLORKEY, 0x0));
    CHECK_COMMAND(v4l2_reqbuf(clipper_id, MAX_BUFFER_COUNT));
    CHECK_COMMAND(v4l2_reqbuf(video_id, MAX_BUFFER_COUNT));

    printf("alloc video\n");
    struct nxp_vid_buffer bufs[MAX_BUFFER_COUNT];
    CHECK_COMMAND(alloc_buffers(ion_fd, MAX_BUFFER_COUNT, bufs, width, height, format));
    printf("vid_buf: %p, %p, %p, %p\n", bufs[0].virt[0], bufs[1].virt[0], bufs[2].virt[0], bufs[3].virt[0]);

    int i;
    for (i = 0; i < MAX_BUFFER_COUNT; i++) {
        struct nxp_vid_buffer *buf = &bufs[i];
        printf("buf plane num: %d\n", buf->plane_num);
        CHECK_COMMAND(v4l2_qbuf(clipper_id, buf->plane_num, i, buf, -1, NULL));
    }

    CHECK_COMMAND(v4l2_streamon(clipper_id));

    int out_index = 0;
    int out_dq_index = 0;
    int out_q_count = 0;
    bool started_out = false;
    int j;
    unsigned short *prgb_data;
    struct nxp_vid_buffer *rgb_buf;
    int capture_index = 0;
    int count = 10000;
    if (argc >= 5)
        count = atoi(argv[3]);
    while (count >= 0) {
        struct nxp_vid_buffer *buf = &bufs[capture_index];
        CHECK_COMMAND(v4l2_dqbuf(clipper_id, buf->plane_num, &capture_index, NULL));
        // printf("====>capture_index: %d\n", capture_index);
        CHECK_COMMAND(v4l2_qbuf(video_id, buf->plane_num, out_index, buf, -1, NULL));

        out_q_count++;
        out_index++;
        out_index %= MAX_BUFFER_COUNT;

        if (!started_out) {
            CHECK_COMMAND(v4l2_streamon(video_id));
            started_out = true;
        }

        if (out_q_count >= MAX_BUFFER_COUNT) {
            CHECK_COMMAND(v4l2_dqbuf(video_id, buf->plane_num, &out_dq_index, NULL));
            out_q_count--;
        }

        CHECK_COMMAND(v4l2_qbuf(clipper_id, buf->plane_num, capture_index, buf, -1, NULL));
        count--;
    }

    CHECK_COMMAND(v4l2_streamoff(video_id));
    CHECK_COMMAND(v4l2_streamoff(clipper_id));

    v4l2_exit();
    close(ion_fd);

    return 0;
}
