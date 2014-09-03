#ifndef _UPDATE_H
#define _UPDATE_H

int update_eeprom( int type, char *path );
int update_eeprom_from_mem( int type, char *mem, size_t img_size );

#endif
