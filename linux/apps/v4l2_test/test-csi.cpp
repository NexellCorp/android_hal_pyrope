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

#include <nxp-v4l2.h>

#define DEFAULT_WIDTH  640
#define DEFAULT_HEIGHT 480
//#define DEFAULT_WIDTH  800
//#define DEFAULT_HEIGHT 600

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
    int format = V4L2_PIX_FMT_YUV420M; // yuv420
    // int format = V4L2_PIX_FMT_YUV422P; // yuv422

    if (ion_fd < 0) {
        fprintf(stderr, "can't open ion!!!\n");
        return -EINVAL;
    }

    struct V4l2UsageScheme s;
    memset(&s, 0, sizeof(s));

    s.useClipper1 = true;
    s.useDecimator1 = true;
    s.useMlc0Video = true;
    // s.useMlc1Video = true;

    if (argc >= 3) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    } else {
        width = DEFAULT_WIDTH;
        height = DEFAULT_HEIGHT;
    }
    printf("width %d, height %d\n", width, height);

    CHECK_COMMAND(v4l2_init(&s));

    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_clipper1, width, height, format));
    CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_clipper1, 0, 0, width, height));
    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_sensor1, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_mipicsi, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_mlc0_video, width, height, format));

    if (width > 1280 || height > 800)
        CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_mlc0_video, 0, 0, 1280, 800));
    else
        CHECK_COMMAND(v4l2_set_crop(nxp_v4l2_mlc0_video, 0, 0, width, height));

    CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_VID_PRIORITY, 0));
    CHECK_COMMAND(v4l2_set_ctrl(nxp_v4l2_mlc0_video, V4L2_CID_MLC_VID_COLORKEY, 0x0));

    CHECK_COMMAND(v4l2_reqbuf(nxp_v4l2_clipper1, MAX_BUFFER_COUNT));
    CHECK_COMMAND(v4l2_reqbuf(nxp_v4l2_mlc0_video, MAX_BUFFER_COUNT));

    printf("alloc video\n");
    struct nxp_vid_buffer bufs[MAX_BUFFER_COUNT];
    CHECK_COMMAND(alloc_buffers(ion_fd, MAX_BUFFER_COUNT, bufs, width, height, format));
    // CHECK_COMMAND(alloc_buffers(ion_fd, MAX_BUFFER_COUNT, bufs, width, height, V4L2_PIX_FMT_YUV444));
    // HACK allocate width * 2
    // CHECK_COMMAND(alloc_buffers(ion_fd, MAX_BUFFER_COUNT, bufs, width * 2, height, V4L2_PIX_FMT_YUV444));
    printf("vid_buf: %p, %p, %p, %p\n", bufs[0].virt[0], bufs[1].virt[0], bufs[2].virt[0], bufs[3].virt[0]);

    int i;
    for (i = 0; i < MAX_BUFFER_COUNT; i++) {
        struct nxp_vid_buffer *buf = &bufs[i];
        printf("buf plane num: %d\n", buf->plane_num);
        CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_clipper1, buf->plane_num, i, buf, -1, NULL));
    }

    CHECK_COMMAND(v4l2_streamon(nxp_v4l2_clipper1));

    int out_index = 0;
    int out_dq_index = 0;
    int out_q_count = 0;
    bool started_out = false;
    int j;
    unsigned short *prgb_data;
    struct nxp_vid_buffer *rgb_buf;
    int capture_index = 0;
    int count = 0;
    if (argc >= 4)
        count = atoi(argv[3]);
    while (count >= 0) {
        struct nxp_vid_buffer *buf = &bufs[capture_index];
        CHECK_COMMAND(v4l2_dqbuf(nxp_v4l2_clipper1, buf->plane_num, &capture_index, NULL));
        CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_mlc0_video, buf->plane_num, out_index, buf, -1, NULL));

        out_q_count++;
        out_index++;
        out_index %= MAX_BUFFER_COUNT;

        if (!started_out) {
            CHECK_COMMAND(v4l2_streamon(nxp_v4l2_mlc0_video));
            started_out = true;
        }

        if (out_q_count >= MAX_BUFFER_COUNT) {
            CHECK_COMMAND(v4l2_dqbuf(nxp_v4l2_mlc0_video, buf->plane_num, &out_dq_index, NULL));
            out_q_count--;
        }

        CHECK_COMMAND(v4l2_qbuf(nxp_v4l2_clipper1, buf->plane_num, capture_index, buf, -1, NULL));
        count--;
    }

    CHECK_COMMAND(v4l2_streamoff(nxp_v4l2_mlc0_video));
    CHECK_COMMAND(v4l2_streamoff(nxp_v4l2_clipper1));

    // v4l2_exit();
    close(ion_fd);

    return 0;
}
