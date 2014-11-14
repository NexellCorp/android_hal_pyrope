#ifndef _NXP_ION_PRIVATE_H
#define _NXP_ION_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

int ion_get_phys(int fd, int buf_fd, unsigned long *phys);
int ion_sync_from_device(int fd, int handle_fd);

#ifdef __cplusplus
}
#endif

#endif
