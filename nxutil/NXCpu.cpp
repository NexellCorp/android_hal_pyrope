#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>

#include "NXCpu.h"

#define SYSFS_NX_CPU_VERSION "/sys/devices/platform/cpu/version"

uint32_t getNXCpuVersion()
{
    int fd = open(SYSFS_NX_CPU_VERSION, O_RDONLY);
    if (fd < 0)
        return 0;

    char temp[16] = {0, };
    int err = read(fd, temp, sizeof(temp));
    uint32_t version = atoi(temp);
    return version;
}
