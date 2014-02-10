#!/bin/bash
#
# build file systems

# command-line settable variables

FS_ROOT_PATH=
FS_COPY_PATH=./

FS_SIZE=16384
FS_LOOP=loop1
FS_NAME=ramdisk
FS_FORMAT=ext2
FS_MOUNT_PATH=mnt

FS_MAKE_DEVNODE=n
MKDEV_EXT_SH_PATH=

#########################
# Get build options
#########################
function usage()
{
	echo "usage: `basename $0`"
	echo "  -r rootfs path to build ramdisk			"
	echo "  -c copy to directory (default .)		"
	echo "  -n ramdisk name      (default ramdisk)	"
	echo "  -s ramdisk size      (default 16384)	"
#	echo "  -l set loop device   (default loop1) 	"
#	echo "  -f ramdisk format    (default ext2)		"
	echo "  -d build device node (default no)	 	"
	echo "  -e set external mknode shell path	 	"
	echo "  clean rm *.gz			 			    "
}

while getopts 'hr:c:n:s:l:f:de:' opt
do
	case $opt in
	r) FS_ROOT_PATH=$OPTARG ;;
	c) FS_COPY_PATH=$OPTARG ;;
	n) FS_NAME=$OPTARG ;;
	s) FS_SIZE=$OPTARG ;;
	l) FS_LOOP=$OPTARG ;;
	f) FS_FORMAT=$OPTARG ;;
	d) FS_MAKE_DEVNODE=y ;;
	e) MKDEV_EXT_SH_PATH=$OPTARG ;;
	h | *)
		usage
		exit 1;;
		esac
done

# no input parameter
if [ -z "$1" ]; then usage; exit 1; fi


# clean
if [ "clean" = "$1" ]; then
	echo "make clean, rm *.img"
	rm -f *.img
	exit 1;
fi

#################################################################
# functions
#################################################################

#
# get sudo permission
#
# return "sudo"
#
function sudo_permission()
{
	_user_=$(id | sed 's/^uid=//;s/(.*$//')
    if [ 0 != $_user_ ]; then
    	echo " Require root permission"
        _sudo_=$1
        eval "$_sudo_='sudo'"	# return sudo
        # test
        sudo losetup -a >> /dev/null
	fi
}
sudo_permission _SUDO_

#
# check rootfs path's permission
#
# input parameters
# $1	= path
#
# return
# - exit when not write permission
#
function check_permission_w()
{
	path=$1
	if [ -z $path ] || [ ! -d $path ]; then
		echo ""
		echo -e " - check path: $path ...."
		exit 1;
	fi

	if [ ! -w "$path" ]; then
		echo ""
		echo -e " You do not have write permission"
		echo -e " Check permission: '$path'"
		exit 1;
	fi
}

#
# make device node files
#
# input parameters
# $1	= device path
# $2 	= mknode shell path when use external shell
#
function build_mkdev()
{
	root_path=$1
	ext_mk_sh=$2

	if [ -n "$ext_mk_sh" ];
	then
		# use external shell program
		#
		if [ -x "$ext_mk_sh" ]; then
			$ext_mk_sh $root_path
			echo -e "\t[Done]"
		else
			echo ""
			echo -e " - check mknode shell: $MKDEV_EXT_SH_PATH ...."
		fi
	else
		echo ""
		echo -n " [ Make Device Node: '$root_path'..."
		###
		# make device nodes in /dev
		###
		dev_dir=$root_path/dev
		check_permission_w $root_path

		# miscellaneous one-of-a-kind stuff
		[ ! -c $dev_dir/console   ] && $_SUDO_ mknod 	$dev_dir/console 	c 5 1;
		[ ! -c "$dev_dir/full"    ] && $_SUDO_ mknod 	$dev_dir/full 		c 1 7;
		[ ! -c "$dev_dir/kmem"    ] && $_SUDO_ mknod 	$dev_dir/kmem 		c 1 2;
		[ ! -c "$dev_dir/mem"     ] && $_SUDO_ mknod 	$dev_dir/mem 		c 1 1;
		[ ! -c "$dev_dir/null"    ] && $_SUDO_ mknod 	$dev_dir/null 		c 1 3;
		[ ! -c "$dev_dir/port"    ] && $_SUDO_ mknod 	$dev_dir/port 		c 1 4;
		[ ! -c "$dev_dir/random"  ] && $_SUDO_ mknod 	$dev_dir/random 	c 1 8;
		[ ! -c "$dev_dir/urandom" ] && $_SUDO_ mknod  	$dev_dir/urandom 	c 1 9;
		[ ! -c "$dev_dir/zero"    ] && $_SUDO_ mknod  	$dev_dir/zero 		c 1 5;
		[ ! -c "$dev_dir/tty"     ] && $_SUDO_ mknod 	$dev_dir/tty 		c 5 0
		[ ! -h "$dev_dir/core"    ] && ln -s /proc/kcore	$dev_dir/core;

		# loop devs
		for i in `seq 0 7`; do
			[ ! -b "$dev_dir/loop$i" ] && $_SUDO_ mknod $dev_dir/loop$i 	b 7 $i;
		done

		# ram devs
		for i in `seq 0 9`; do
			[ ! -b "$dev_dir/ram$i" ] && $_SUDO_ mknod $dev_dir/ram$i 	b 1 $i
		done

		# ttys
		for i in `seq 0 9`; do
			[ ! -c "$dev_dir/tty$i" ] && $_SUDO_ mknod $dev_dir/tty$i 	c 4 $i
		done
		echo -e "\t Done]"
	fi
}

#
# make ramdisk
#
# input parameters
# $1	= rootfs path
# $2	= ramdisk build name (default ramdisk -> ramdisk.gz)
# $3	= ramdisk size
# $4	= loop node
# $5	= ramdisk filesystem format (default ext2)
# $6	= ramdisk mount path
# $7	= ramdisk copy path
# $8	= enable build node
# $9 	= mknode shell path when use external shell

function build_ramdisk()
{
	root_path=$1
	disk_name=$2
	disk_size=$3
	loop_node=$4
	fs_format=$5
	mount_dir=$6
	copy_path=$7
	make_node=$8
	ext_mk_sh=$9
	root_size=$(echo $(du -s $root_path) | cut -f1 -d" ")

	# check path's permission
	check_permission_w $root_path
	check_permission_w $copy_path

	if [ -z $disk_name ]; then disk_name=ramdisk; fi
	if [ -z $fs_format ]; then fs_format=ext2; fi
	if [ -f $disk_name.gz ]; then rm -f $disk_name.gz; fi

	echo ""
	echo -e " Make ramdisk "
	echo " root from = $root_path"
	echo " disk name = $disk_name"
	echo " disk size = $disk_size"
#	echo " loop node = $loop_node"
#	echo " fs format = $fs_format"
	echo " copy to   = $copy_path"

	if [ "$root_size" -gt "$disk_size" ]; then
		echo    ""
		echo -e " FAIL: $root_path($root_size) is over ramdisk image($disk_size) \n"
		exit 1;
	fi

	# umount previos mount
	mount | grep -q $disk_name
	if [ $? -eq 0 ]; then
		$_SUDO_ umount $mount_dir
	else
		mkdir -p $mount_dir
	fi
	check_permission_w $mount_dir

	# build ramdisk
	dd if=/dev/zero of=$disk_name bs=1k count=$disk_size > /dev/null 2> /dev/null;
	yes | mke2fs  $disk_name > /dev/null 2> /dev/null;

	# mount ramdisk
	$_SUDO_ mount -o loop $disk_name $mount_dir;

	# copy files
	cp -dR $root_path/* $mount_dir/

	# build device nodes
	if [ $make_node = "y" ] || [ -n "$ext_mk_sh" ]; then
		build_mkdev $mount_dir $ext_mk_sh
	fi

	# exit ramdisk
	$_SUDO_ umount $mount_dir;
	gzip -f $disk_name
	chmod 666 $disk_name.gz
	rm -rf $mount_dir

	# copy image
	if [ ! -z $copy_path ] && [ -d $copy_path ]; then
		cp -f $disk_name.gz $copy_path
	fi
}


#################################################################
# RUN COMMAND
#################################################################

# build ramdisk
param1=$FS_ROOT_PATH
param2=$FS_NAME
param3=$FS_SIZE
param4=$FS_LOOP
param5=$FS_FORMAT
param6=$FS_MOUNT_PATH
param7=$FS_COPY_PATH
param8=$FS_MAKE_DEVNODE
param9=$MKDEV_EXT_SH_PATH

build_ramdisk $param1 $param2 $param3 $param4 $param5 $param6 $param7  $param8 $param9
