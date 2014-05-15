#!/bin/bash -e 
make clean 
make CROSS_COMPILE=arm-generic-linux-gnueabi-
cp spi_eeprom ~/devel/nfs/rootfs-dtk/root
