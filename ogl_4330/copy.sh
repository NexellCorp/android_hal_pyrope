#!/bin/bash
sudo cp src/devicedrv/mali/vr.ko ~/devel/nfs/linux_rootfs-nxp4330/test/
sudo cp bin/* ~/devel/nfs/linux_rootfs-nxp4330/test/
sudo cp lib/libEGL.so ~/devel/nfs/linux_rootfs-nxp4330/usr/lib/
sudo cp lib/libGLESv1_CM.so ~/devel/nfs/linux_rootfs-nxp4330/usr/lib/
sudo cp lib/libGLESv2.so ~/devel/nfs/linux_rootfs-nxp4330/usr/lib/
sudo cp lib/libVR.so ~/devel/nfs/linux_rootfs-nxp4330/usr/lib/

