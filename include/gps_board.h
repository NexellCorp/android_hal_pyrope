#ifndef _NX_GPS_H
#define _NX_GPS_H

// board dependent functions for GPS

#ifdef __cplusplus
extern "C" {
#endif
// return handle
int  board_gps_init(void);
void board_gps_deinit(int handle);
// return read size
int board_gps_read(int handle, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif
