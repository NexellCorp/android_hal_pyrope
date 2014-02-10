#!/bin/bash

#defalut value
BUILD=debug
#BUILD=release

#===================================
# ump environment config for user
#===================================
CONFIG=nxp4330-dtk
KDIR=~/devel/build/nxp4330/linux_ln/

#===================================
# ump device driver build
#===================================
make clean CONFIG=$CONFIG KDIR=$KDIR BUILD=$BUILD

