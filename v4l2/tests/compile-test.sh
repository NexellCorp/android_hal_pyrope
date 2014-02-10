#!/bin/sh

 arm-cortex_a9-linux-gnueabi-gcc test.cpp -I/home/swpark/work/nxp4330/include -I../ -L/home/swpark/work/nxp4330/libs -lion -L../ -lv4l2-nexell -o test
 arm-cortex_a9-linux-gnueabi-gcc test-decimator.cpp -I/home/swpark/work/nxp4330/include  -I../ -L/home/swpark/work/nxp4330/libs -lion -L../ -lv4l2-nexell -o test-decimator
 arm-cortex_a9-linux-gnueabi-gcc test-hdmi.cpp -I/home/swpark/work/nxp4330/include -I../ -L/home/swpark/work/nxp4330/libs -lion -L../ -lv4l2-nexell -o test-hdmi
 arm-cortex_a9-linux-gnueabi-gcc test-resc.cpp -I/home/swpark/work/nxp4330/include -I../ -L/home/swpark/work/nxp4330/libs -lion -L../ -lv4l2-nexell -o test-resc
 arm-cortex_a9-linux-gnueabi-gcc test-csi.cpp -I/home/swpark/work/nxp4330/include -I../ -L/home/swpark/work/nxp4330/libs -lion -L../ -lv4l2-nexell -o test-csi
