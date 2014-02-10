#!/bin/bash

BUILD_NAME=lynx

TOP=`pwd`
 
UBOOT_DIR=$TOP/u-boot
KERNEL_DIR=$TOP/kernel
MODULES_DIR=$TOP/modules

APPLICATION_DIR=$TOP/apps
LIBRARY_DIR=$TOP/library

FILESYSTEM_DIR=$TOP/fs
TOOLS_DIR=$TOP/tools
RESULT_DIR=$TOP/result
FASTBOOT_DIR=$TOOLS_DIR/bin

# byte
BOOT_PARTITION_SIZE=67108864

# Kbyte default:16384, 24576
RAMDISK_SIZE=32768

RAMDISK_FILE=$FILESYSTEM_DIR/buildroot/out/ramdisk.gz

NSIH_FILE=$TOOLS_DIR/boot/NSIH.txt
SECONDBOOT_FILE=$TOOLS_DIR/boot/PYROPE_SPI_800MT-MP2_800CH4_550B2P2-3D_295.bin
SECONDBOOT_OUT_FILE=$RESULT_DIR/2ndboot.bin



CMD_V_BUILD_NUM=

CMD_V_UBOOT=no
CMD_V_UBOOT_CLEAN=no
UBOOT_CONFIG_NAME=nxp4330q_${BUILD_NAME}

CMD_V_KERNEL=no
CMD_V_KERNEL_CLEAN=no
CMD_V_KERNEL_MODULE=no

CMD_V_KERNEL_PROJECT_MENUCONFIG=no
CMD_V_KERNEL_PROJECT_MENUCONFIG_COMPILE=no
KERNEL_CONFIG_NAME=nxp4330_${BUILD_NAME}_android

CMD_V_APPLICATION=no
CMD_V_FILESYSTEM=no

CMD_V_SDCARD_PACKAGING=no
CMD_V_SDCARD_SELECT_DEV=
CMD_V_EMMC_PACKAGING=no
CMD_V_EMMC_PACKAGING_2NDBOOT=no
CMD_V_EMMC_PACKAGING_UBOOT=no
CMD_V_EMMC_PACKAGING_BOOT=no

CMD_V_BASE_PORTING=no
CMD_V_NEW_BOARD=

CMD_V_BUILD_ERROR=no
CMD_V_BUILD_SEL=Not

TEMP_UBOOT_TEXT=
TEMP_KERNEL_TEXT=



YEAR=0000
MON=00
DAY=00
HOUR=00
MIN=00
SEC=00

function check_result()
{
	if [ $? -ne 0 ]; then
		echo "[Error]"
		exit
	fi
}

function currentTime()
{
	YEAR=`date +%Y`
	MON=`date +%m`
	DAY=`date +%d`
	HOUR=`date +%H`
	MIN=`date +%M`
	SEC=`date +%S`
}


function build_uboot_source()
{

	if [ ${CMD_V_UBOOT_CLEAN} = "yes" ]; then
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# build u-boot clean '
		echo '#'
		echo '#########################################################'

		sleep 1.5

		pushd . > /dev/null
		cd $UBOOT_DIR
		make ARCH=arm clean -j8
		popd > /dev/null
	fi


	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo "# build uboot "
	echo '#'
	echo '#########################################################'

	if [ -f $RESULT_DIR/build.nxp4330.uboot ]; then
		rm -f $RESULT_DIR/build.nxp4330.uboot
	fi
	echo "${UBOOT_CONFIG_NAME}_config" > $RESULT_DIR/build.nxp4330.uboot ##.build_${UBOOT_CONFIG_NAME}

	sleep 1.5
	pushd . > /dev/null

	cd $UBOOT_DIR
	make distclean
	make ${UBOOT_CONFIG_NAME}_config
	make -j8

	popd > /dev/null
}


function build_kernel_source()
{
	if [ ${CMD_V_KERNEL_CLEAN} = "yes" ]; then
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# build kernel clean '
		echo '#'
		echo '#########################################################'

		sleep 1.5

		pushd . > /dev/null
		cd $KERNEL_DIR
		#make distclean
		make ARCH=arm clean -j8
		popd > /dev/null
	fi

	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo "# build kernel "
	echo '#'
	echo '#########################################################'

	sleep 1.5

	pushd . > /dev/null
	cd $KERNEL_DIR

	if [ -f .config ]; then
		echo ""
	else
		echo "${KERNEL_CONFIG_NAME}_defconfig" > $RESULT_DIR/build.nxp4330.kernel ##.build_${UBOOT_CONFIG_NAME}	
		make ARCH=arm ${KERNEL_CONFIG_NAME}_defconfig
	fi

	make ARCH=arm Image -j8
	popd > /dev/null

	if [ -d $RESULT_DIR/boot ]; then
		echo ""
	else
		mkdir -p $RESULT_DIR/boot
	fi
}

function build_kernel_module()
{

	echo ''
	echo '#########################################################'
	echo '# build kernel modules'
	echo '#########################################################'

	sleep 1.5

	pushd . > /dev/null
	cd $MODULES_DIR/coda960
	make ARCH=arm -j4

	popd > /dev/null
}

function build_application()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# application '
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5

	pushd . > /dev/null

	echo ''
	echo '#########################################################'
	echo '# movie_player_app '
	echo '#########################################################'
	cd $APPLICATION_DIR/movie_player_app
	make

	echo ''
	echo '#########################################################'
	echo '# transcoding_example '
	echo '#########################################################'
	cd $APPLICATION_DIR/transcoding_example
	make

	echo ''
	echo '#########################################################'
	echo '# v4l2_test '
	echo '#########################################################'
	cd $APPLICATION_DIR/v4l2_test
	make

	echo ''
	echo '#########################################################'
	echo '# vpu_test '
	echo '#########################################################'
	cd $APPLICATION_DIR/vpu_test
	make


	echo ''
	echo '#########################################################'
	echo '# libion '
	echo '#########################################################'
	cd $LIBRARY_DIR/src/libion
	make;	make install

	echo ''
	echo '#########################################################'
	echo '# libnxgraphictools '
	echo '#########################################################'
	cd $LIBRARY_DIR/src/libnxgraphictools
	make;	make install


	echo ''
	echo '#########################################################'
	echo '# libnxmovieplayer '
	echo '#########################################################'
	cd $LIBRARY_DIR/src/libnxmovieplayer
	make;	make install

	echo ''
	echo '#########################################################'
	echo '# libnxscaler '
	echo '#########################################################'
	cd $LIBRARY_DIR/src/libnxscaler
	make;	make install

	echo ''
	echo '#########################################################'
	echo '# libnxv4l2 '
	echo '#########################################################'
	cd $LIBRARY_DIR/src/libnxv4l2
	make;	make install

	echo ''
	echo '#########################################################'
	echo '# libnxvpu '
	echo '#########################################################'
	cd $LIBRARY_DIR/src/libnxvpu
	make;	make install

	popd > /dev/null
}

function build_filesystem()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo "# filesystem(rootfs-ramdisk)"
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5

	if [ -f ${RAMDISK_FILE} ]; then
		rm -f ${RAMDISK_FILE}
	fi

	if [ -f ${RESULT_DIR}/boot/ramdisk.gz ]; then
		rm -f ${RESULT_DIR}/boot/ramdisk.gz
	fi

	if [ -d $FILESYSTEM_DIR/buildroot/out/rootfs ]; then
		echo '//////////////////////////'
		echo '// copy movie_player_app '
		cd $APPLICATION_DIR/movie_player_app/
		cp -v movieplayer_app $FILESYSTEM_DIR/buildroot/out/rootfs/usr/bin/

		echo ''
		echo '//////////////////////////'
		echo '// copy transcoding_example '
		cd $APPLICATION_DIR/transcoding_example/
		cp -v trans_test2 $FILESYSTEM_DIR/buildroot/out/rootfs/usr/bin/

		echo ''
		echo '//////////////////////////'
		echo '// copy v4l2_test '
		cd $APPLICATION_DIR/v4l2_test/
		cp -v camera_test csi_test decimator_test hdmi_test $FILESYSTEM_DIR/buildroot/out/rootfs/usr/bin/

		echo ''
		echo '//////////////////////////'
		echo '// copy vpu_test '
		cd $APPLICATION_DIR/vpu_test/
		cp -v dec_test enc_test jpg_test trans_test $FILESYSTEM_DIR/buildroot/out/rootfs/usr/bin/

		echo ''
		echo '//////////////////////////'
		echo '// copy gstreamer-0.10 '
		cp -vr $LIBRARY_DIR/lib/gstreamer-0.10 $FILESYSTEM_DIR/buildroot/out/rootfs/usr/lib/

		echo ''
		echo '//////////////////////////'
		echo '// copy lib '
		cp -v $LIBRARY_DIR/lib/*.so $FILESYSTEM_DIR/buildroot/out/rootfs/usr/lib/

		echo ''
		echo '//////////////////////////'
		echo '// copy coda960 '
		cp -v $MODULES_DIR/coda960/nx_vpu.ko $FILESYSTEM_DIR/buildroot/out/rootfs/root/
		echo ''

		pushd . > /dev/null
		cd $FILESYSTEM_DIR
		cp buildroot/scripts/mk_ramfs.sh buildroot/out/
		cd buildroot/out/

		if [ -d mnt ]; then
			sudo rm -rf mnt
		fi

		./mk_ramfs.sh -r rootfs -s ${RAMDISK_SIZE}


		popd > /dev/null

		echo ''
		echo ''
		echo '#########################################################'
		echo "# copy image"
		echo '#########################################################'
		cp -v ${UBOOT_DIR}/u-boot.bin ${RESULT_DIR}
		cp -v ${KERNEL_DIR}/arch/arm/boot/Image ${RESULT_DIR}/boot
		cp -v ${RAMDISK_FILE} ${RESULT_DIR}/boot/ramdisk.gz
	else
		echo '#########################################################'
		echo '# error : No "./fs/buildroot/out" folder.'
		echo '#########################################################'
	fi
}

function build_fastboot_2ndboot()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# fastboot 2ndboot'
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5
	pushd . > /dev/null
	sudo python $TOOLS_DIR/make-pyrope-2ndboot-download-image.py $NSIH_FILE $SECONDBOOT_FILE $SECONDBOOT_OUT_FILE
	sudo $FASTBOOT_DIR/fastboot flash 2ndboot $SECONDBOOT_OUT_FILE
	#./device/nexell/tools/update-2ndboot.sh
	popd > /dev/null
}

function build_fastboot_uboot()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# fastboot uboot'
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5
	pushd . > /dev/null
	sudo $FASTBOOT_DIR/fastboot flash bootloader $RESULT_DIR/u-boot.bin
	popd > /dev/null
}

function build_fastboot_boot()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# fastboot boot(Kernel)'
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5
	pushd . > /dev/null

	PATH=$TOOLS_DIR/bin:$PATH && mkuserimg.sh -s ${RESULT_DIR}/boot ${RESULT_DIR}/boot.img ext4 boot $BOOT_PARTITION_SIZE

	sudo $FASTBOOT_DIR/fastboot flash boot $RESULT_DIR/boot.img
	popd > /dev/null
}


function build_function_main()
{

	currentTime
	StartTime="${YEAR}-${MON}-${DAY} ${HOUR}:${MIN}:${SEC}"
	echo '#########################################################'
	echo "#  Build Time : "${StartTime}"                     #"
	echo '#########################################################'
	echo ""


	if [ ${CMD_V_UBOOT} = "yes" ]; then
		CMD_V_BUILD_SEL="Build u-boot"
		build_uboot_source
	fi

	if [ ${CMD_V_KERNEL} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Kernel"
		build_kernel_source
	fi

	if [ ${CMD_V_KERNEL_MODULE} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Kernel module"
		build_kernel_module
	fi

	if [ ${CMD_V_APPLICATION} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Application"
		build_application
	fi

	if [ ${CMD_V_FILESYSTEM} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Filesystem"
		build_filesystem
	fi

	if [ -d ${RESULT_DIR} ]; then
		echo ""
		echo '#########################################################'
		echo "ls -al ./result/boot"
		ls -al ${RESULT_DIR}/boot
		echo ""
		echo "ls -al ./result"
		ls -al ${RESULT_DIR}
	fi

	currentTime
	EndTime="${YEAR}-${MON}-${DAY} ${HOUR}:${MIN}:${SEC}"

	echo ""
	echo '#########################################################'
	echo "# Build Information				"
	echo "#     Build      : $CMD_V_BUILD_SEL	"
	echo "#"
	echo "#     Start Time : "${StartTime}"	"
	echo "#       End Time : "${EndTime}"	"
	echo '#########################################################'
}



################################################################
##
## main build start
##
################################################################

# export PATH=$TOP/prebuilts/gcc/linux-X86/arm/arm-eabi-4.6/bin/:$PATH
# arm-eabi-gcc -v 2> /dev/null
# check_result


chmod 755 $FASTBOOT_DIR/fastboot
chmod 755 $TOOLS_DIR/bin/mkuserimg.sh
chmod 755 $TOOLS_DIR/bin/make_ext4fs


if [ -d $RESULT_DIR ]; then
	echo ""
else
	mkdir -p $RESULT_DIR
fi

if [ -f $RESULT_DIR/build.nxp4330.uboot ]; then
	TEMP_UBOOT_TEXT=`cat $RESULT_DIR/build.nxp4330.uboot`
fi
if [ -f $RESULT_DIR/build.nxp4330.kernel ]; then
	TEMP_KERNEL_TEXT=`cat $RESULT_DIR/build.nxp4330.kernel`
fi

if [ ${BUILD_NAME} != "build_exit" ]; then
	while [ -z $CMD_V_BUILD_NUM ]
	do
		clear
		echo "******************************************************************** "
		echo "Build Menu "
		echo "  TOP DIR       : $TOP"
		echo "  Before Uboot  : ${TEMP_UBOOT_TEXT}"
		echo "  Before Kernel : ${TEMP_KERNEL_TEXT}"
		echo "  Build Name    : $BUILD_NAME"
		echo "******************************************************************** "
		echo "  1. ALL(+Compile)"
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  4. u-boot+kernel (+Build)"
		echo "     41. u-boot(+Build)		" #41c. u-boot(+Clean Build)"       
		echo "     42. kernel(+Build)		42c. kernel(+Clean Build)"       
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  5. Kernel Config(.config)"
		echo "     5mc.  ${KERNEL_CONFIG_NAME}_defconfig -> .config"
		echo "     5mcc. ${KERNEL_CONFIG_NAME}_defconfig -> .config (+Clean Build)"
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  6. Application"
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  7. File System(ramdisk)(+Build)"
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  9. eMMC packaging"
		echo "     91. fastboot 2ndBoot	"
		echo "     92. fastboot U-BOOT"
		echo "     93. fastboot Boot(kernel+rootfs)"
		echo "--------------------------------------------------------------------"
		echo "  0. exit "
		echo "--------------------------------------------------------------------"

		echo -n "     Select Menu -> "
		read CMD_V_BUILD_NUM
		case $CMD_V_BUILD_NUM in
			#------------------------------------------------------------------------------------------------
			1) CMD_V_UBOOT=yes	
			    CMD_V_KERNEL=yes 
			    CMD_V_KERNEL_MODULE=yes
			    CMD_V_APPLICATION=yes
			    CMD_V_FILESYSTEM=yes					
			    ;;

			#------------------------------------------------------------------------------------------------
			4) CMD_V_KERNEL=yes 
			    CMD_V_KERNEL_MODULE=yes
			    CMD_V_UBOOT=yes 
			    ;;
				41) CMD_V_UBOOT=yes							
				    ;;
				41c) CMD_V_UBOOT=yes
				       CMD_V_UBOOT_CLEAN=yes				
				       ;;
				42) CMD_V_KERNEL=yes 						
				     CMD_V_KERNEL_MODULE=yes
					 ;;
				42c) CMD_V_KERNEL=yes 
				       CMD_V_KERNEL_CLEAN=yes
					 CMD_V_KERNEL_MODULE=yes
				       ;;

			#------------------------------------------------------------------------------------------------
			6)	CMD_V_APPLICATION=yes					;;

			#------------------------------------------------------------------------------------------------
			7)	CMD_V_FILESYSTEM=yes					;;

			#------------------------------------------------------------------------------------------------
			91)	CMD_V_BUILD_NUM=
				build_fastboot_2ndboot						;;
			92)	CMD_V_BUILD_NUM=
				build_fastboot_uboot						;;
			93)	CMD_V_BUILD_NUM=
				build_fastboot_boot						;;

			#------------------------------------------------------------------------------------------------
			0)	CMD_V_BUILD_NUM=0
				echo ""
				exit 0										;;

			#------------------------------------------------------------------------------------------------
			*)	CMD_V_BUILD_NUM=							;;

		esac
		echo
	done

	if [ $CMD_V_BUILD_NUM != 0 ]; then
		CMD_V_LOG_FILE=$RESULT_DIR/build.log
		rm -rf CMD_V_LOG_FILE
		build_function_main 2>&1 | tee $CMD_V_LOG_FILE
	fi
fi

echo ""


