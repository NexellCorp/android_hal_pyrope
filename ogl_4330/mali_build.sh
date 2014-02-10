#!/bin/bash

##############################################################################
##			mali library build
##############################################################################
#MALI_CONFIG=debug
#MALI_CONFIG=release
MALI_HOST_GCC=gcc
MALI_TARGET_PLATFORM=nxp4330
MALI_TARGET_GCC_PRE=arm-linux-
MALI_TARGET_GCC="$MALI_TARGET_GCC_PRE"gcc
#MALI_VARIANT=mali400-r1p1-gles11-gles20-linux-rgb_is_xrgb
#MALI_VARIANT=mali400-r1p1-gles11-gles20-linux
MALI_VARIANT=mali400-r1p1-gles11-gles20-dma_buf
#MALI_VARIANT=mali400-r1p1-gles11-gles20-linux-drawmerge
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

##############################################################################
##			mali testbenches build
##############################################################################
echo Do you want to build the mali testbenches"(y/n)"?
read BUILD_TESTBENCH
if [ $BUILD_TESTBENCH == "y" ]; then
	for TEST_SUITE in "platform_test_suite" "egl_api_main_suite" "egl_api_main_suite_11" "egl_api_main_suite_20" "gles2_api_suite" "vg_api_tests"
	do
		echo "  analyzing code trees..."
		HOST_TOOLCHAIN=$MALI_HOST_GCC TARGET_PLATFORM=$MALI_TARGET_PLATFORM TARGET_TOOLCHAIN=$MALI_TARGET_GCC CONFIG=$MALI_CONFIG VARIANT=$MALI_VARIANT make bin/$TEST_SUITE -j7
	done
fi

##############################################################################
##			mali device driver build
##############################################################################
#echo Do you want to build the mali device driver"(y/n)"?
#read BUILD_DRV

#if [ $BUILD_DRV == "y" ]; then
#	cd src/devicedrv/mali
#	if [ $CLEAR == "y" ]; then
#        	USING_MMU=1 USING_PMM=0 USING_OS_MEMORY=0 USING_UMP=0 TARGET_PLATFORM=$MALI_TARGET_PLATFORM BUILD=$MALI_CONFIG CONFIG=nxp4330-dtk-m400-2 KDIR=$MALI_KERNEL_DIR CROSS_COMPILE=arm-generic-linux-gnueabi- make clean
#	fi
#	echo "  analyzing mali code trees..."
#    USING_MMU=1 USING_PMM=0 USING_OS_MEMORY=0 USING_UMP=0 TARGET_PLATFORM=$MALI_TARGET_PLATFORM BUILD=$MALI_CONFIG CONFIG=nxp4330-dtk-m400-2 KDIR=$MALI_KERNEL_DIR CROSS_COMPILE=arm-generic-linux-gnueabi- make -j7
#fi
