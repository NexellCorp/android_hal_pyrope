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

#include <ion/ion.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <linux/videodev2_nxp_media.h>

#include <ion-private.h>
#include <nxp-v4l2.h>

#include <cutils/log.h>

#define DEFAULT_WIDTH  800
#define DEFAULT_HEIGHT 600

#define MAX_BUFFER_COUNT 4

#define CHECK_COMMAND(command) do { \
        int ret = command; \
        if (ret < 0) { \
            ALOGD("line %d error!!!\n", __LINE__); \
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

    ALOGD("Allocation Buffer: count(%d), plane(%d)\n", count, plane_num);
    for (i = 0; i < count; i++) {
        buffer = &bufs[i];
        ALOGD("[Buffer %d] --->\n", i);
        for (j = 0; j < plane_num; j++) {
            buffer->sizes[j] = get_size(format, j, width, height);
            ret = ion_alloc_fd(ion_fd, buffer->sizes[j], 0, ION_HEAP_NXP_CONTIG_MASK, 0, &buffer->fds[j]);
            if (ret < 0) {
                ALOGD("failed to ion_alloc_fd()\n");
                return ret;
            }
            buffer->virt[j] = (char *)mmap(NULL, buffer->sizes[j], PROT_READ | PROT_WRITE, MAP_SHARED, buffer->fds[j], 0);
            if (!buffer->virt[j]) {
                ALOGD("failed to mmap\n");
                return ret;
            }
            ret = ion_get_phys(ion_fd, buffer->fds[j], &buffer->phys[j]);
            if (ret < 0) {
                ALOGD("failed to get phys\n");
                return ret;
            }
            buffer->plane_num = plane_num;
            ALOGD("\tplane %d: fd(%d), size(%d), phys(0x%lx), virt(%p)\n",
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
        ALOGD("can't open ion!!!\n");
        return -EINVAL;
    }

    width = DEFAULT_WIDTH;
    height = DEFAULT_HEIGHT;

    struct V4l2UsageScheme s;
    memset(&s, 0, sizeof(s));
    s.useMlc0Video = true;

    CHECK_COMMAND(v4l2_init(&s));

    CHECK_COMMAND(v4l2_set_ctrl(video_id, V4L2_CID_MLC_VID_PRIORITY, 0));
    CHECK_COMMAND(v4l2_set_ctrl(video_id, V4L2_CID_MLC_VID_COLORKEY, 0x0));
    CHECK_COMMAND(v4l2_reqbuf(video_id, MAX_BUFFER_COUNT));

    ALOGD("alloc video\n");
    struct nxp_vid_buffer bufs[MAX_BUFFER_COUNT];
    CHECK_COMMAND(alloc_buffers(ion_fd, MAX_BUFFER_COUNT, bufs, width, height, format));
    ALOGD("vid_buf: %p, %p, %p, %p\n", bufs[0].virt[0], bufs[1].virt[0], bufs[2].virt[0], bufs[3].virt[0]);

    unsigned char *readbuf = (unsigned char *)malloc(width*height);
    if (!readbuf) {
        ALOGD("can't malloc for buf\n");
        return -1;
    }
    int fd = open("/data/temp/Ydata.raw", O_RDONLY);
    if (fd < 0) {
        ALOGD("can't open Ydata.raw\n");
        return -1;
    }
    read(fd, readbuf, width*height);
    memcpy(bufs[0].virt[0], readbuf, width*height);
    close(fd);

    fd = open("/data/temp/Cbdata.raw", O_RDONLY);
    if (fd < 0) {
        ALOGD("can't open Cbdata.raw\n");
        return -1;
    }
    read(fd, readbuf, width*height);
    int i;
    unsigned char *psrc = readbuf;
    unsigned char *pdst = (unsigned char *)bufs[0].virt[1];
    for (i = 0; i < height/2; i++) {
        memcpy(pdst, psrc, width >> 1);
        psrc += width;
        pdst += width >> 1;
    }
    close(fd);

    fd = open("/data/temp/Crdata.raw", O_RDONLY);
    if (fd < 0) {
        ALOGD("can't open Crdata.raw\n");
        return -1;
    }
    read(fd, readbuf, width*height);
    psrc = readbuf;
    pdst = (unsigned char *)bufs[0].virt[2];
    for (i = 0; i < height/2; i++) {
        memcpy(pdst, psrc, width >> 1);
        psrc += width;
        pdst += width >> 1;
    }
    close(fd);
    free(readbuf);

    struct nxp_vid_buffer *buf = &bufs[0];
    CHECK_COMMAND(v4l2_qbuf(video_id, buf->plane_num, 0, buf, -1, NULL));
    CHECK_COMMAND(v4l2_streamon(video_id));

    getchar();
#if 0
    if (out_q_count >= MAX_BUFFER_COUNT) {
        CHECK_COMMAND(v4l2_dqbuf(video_id, buf->plane_num, &out_dq_index, NULL));
        out_q_count--;
    }
#endif

    CHECK_COMMAND(v4l2_streamoff(video_id));

    v4l2_exit();
    close(ion_fd);

    return 0;
}
