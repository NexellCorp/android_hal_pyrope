#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libnxjpeg.h"

int main(int argc, char *argv)
{
	//char *filename = "tulips_yuyv422_prog_packed_qcif.yuv";
	char *filename = "tulips_yuyv422_inter_packed_qcif.yuv";
	int fd,outfd;
	//int width = 176;
	//int height = 144;
	int width = 800;
	int height = 480;
	int depth = 2;
	int size, jpegsize;

	unsigned char *in, *out;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		printf("open error\n");
		return -1;
	}

	size = width*height*depth;
	in = (unsigned char *)malloc(size);
	out = (unsigned char *)malloc(size);
	read(fd, in, size);

	jpegsize = NX_JpegEncoding(out, size, in, width, height, 100, NX_PIXFORMAT_YUYV);
	if (jpegsize > 0) {
		printf("success!!!: %d\n", jpegsize);
	}

	outfd = open("testjpeg.jpg", O_CREAT|O_TRUNC|O_WRONLY, 0644);
	if(outfd < 0) {
		printf("error open outfd\n");
		return -1;
	}

	write(outfd, out, jpegsize);
	sync();

	free(in);
	free(out);

	close(fd);
	close(outfd);

	return 0;
}

