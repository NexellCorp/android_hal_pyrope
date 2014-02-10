#!/bin/bash

##############################################################################
##			mali library build
##############################################################################
MALI_HOST_GCC=gcc
MALI_TARGET_PLATFORM=nxp4330
MALI_TARGET_GCC_PRE=arm-linux-
MALI_TARGET_GCC="$MALI_TARGET_GCC_PRE"gcc
#org
MALI_VARIANT=mali400-r1p1-gles11-gles20-linux-android-rgb_is_xrgb-jellybean_mr1-drawmerge-dma_buf-no_profiling
#MALI_VARIANT=mali400-r1p1-gles11-gles20-linux-android-rgb_is_xrgb-jellybean_mr1-dma_buf-no_profiling
MALI_KERNEL_DIR=$HOME/devel/build/nxp4330/linux_ln

echo Do you want to clear all the files first"(y/n)"?
read CLEAR
if [ $CLEAR == "y" ]; then
	HOST_TOOLCHAIN=$MALI_HOST_GCC TARGET_PLATFORM=$MALI_TARGET_PLATFORM TARGET_TOOLCHAIN=$MALI_TARGET_GCC CONFIG=$MALI_CONFIG VARIANT=$MALI_VARIANT make clean
	echo "  clear completed!!!"
fi

echo Do you want to build the mali libraries"(y/n)"? 
read BUILD_LIB
if [ $BUILD_LIB == "y" ]; then
	echo "  analyzing code trees..."
	HOST_TOOLCHAIN=$MALI_HOST_GCC TARGET_PLATFORM=$MALI_TARGET_PLATFORM TARGET_TOOLCHAIN=$MALI_TARGET_GCC CONFIG=$MALI_CONFIG VARIANT=$MALI_VARIANT make -j7
fi

