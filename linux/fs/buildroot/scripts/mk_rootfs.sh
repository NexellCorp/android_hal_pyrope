#!/bin/bash
#
# build file systems

# command-line settable variables

RF_ROOT_PATH=
RF_COPY_PATH=./out/rootfs

RF_OVERWRITE=n
RF_MAKE_DEVNODE=n
MKDEV_EXT_SH_PATH=
RM_UNUSED_FILE=y
RM_LIST_PATH=
RM_FILE_NO_ASK=n

RAMFS_RM_FILES="*.a *.la *doc include man* pkgconfig"

#########################
# Get build options
#########################
function usage()
{
	echo "usage: `basename $0`"
	echo "  -s source rootfs path to copy							"
	echo "  -t copy to path 	  (default ./rootfs)				"
	echo "  -l set path remove files list							"
	echo "  -r select remove unused files (y or n)					"
	echo "  -a remove files without ask   (y or n)					"
	echo "  -o overwrite target rootfs    (y or n)					"
	echo "  -d build device node (default no)	 					"
	echo "  -e set external mknode shell path	 					"
}

while getopts 'hs:t:l:r:a:o:e:d' opt
do
	case $opt in
	s) RF_ROOT_PATH=$OPTARG ;;
	t) RF_COPY_PATH=$OPTARG ;;
	l) RM_LIST_PATH=$OPTARG ;;
	r) RM_UNUSED_FILE=$OPTARG ;;
	a) RM_FILE_NO_ASK=$OPTARG ;;
	o) RF_OVERWRITE=$OPTARG ;;
	e) MKDEV_EXT_SH_PATH=$OPTARG ;;
	d) RF_MAKE_DEVNODE=y ;;
	h | *)
		usage
		exit 1;;
		esac
done

# no input parameter
if [ -z "$1" ]; then usage; exit 1; fi

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
[ $RF_MAKE_DEVNODE = "y" ] && sudo_permission _SUDO_

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
		echo -e "\n - check path: $path ...."
		exit 1;
	fi

	if [ ! -w "$path" ]; then
		echo -e "\n"
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
			echo -e "\n - check mknode shell: $MKDEV_EXT_SH_PATH ...."
		fi
	else
		echo -e "\n"
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
# remove unused files
#
# input parameters
# $1	= remove dir path
# $2	= select remove filse, y or n
# $3	= remove files without ask
# $4	= remove file list path

function remove_unused_f()
{
	rm_path=$1
	rm_exec=$2
	rm_nask=$3
	rm_list=$4
	rm_file=$RAMFS_RM_FILES

	check_permission_w $rm_path

	# remove files
	if [ $rm_exec = "y" ]; then
		set $rm_file
		index=1
		echo -e "\n [ Remove Files :$rm_path]"
		echo -e "\n"

		for list in "$@"
		do
			echo -n " - Remove '$list'...[y/n]= "
			if [ $rm_nask = "y" ]; then arg=y; else read arg; fi
			if [ "$arg" = "y" ]; then
				echo " - Find and remove $list"
				find $rm_path -name $list | xargs rm -rf
			fi
			let "index+=1" # next list
		done
	fi

	if [ ! -z "$rm_list" ]; then
		echo ""
		echo " [ Remove Files :$rm_path]"
		echo " [ Remove with  :$rm_list]"
		echo ""

		OLD_IFS=$IFS
		NEW_IFS=$'\n'
		IFS=$NEW_IFS

		for list in $(cat $rm_list)
		do
			echo "$list" | grep "#" > /dev/null
			if [ $? -eq 1 ]; then
				echo -n " - Remove unused '$list'...[y/n]= "
				if [ $rm_nask = "y" ]; then arg=y; else read arg; fi
				if [ "$arg" = "y" ]; then
					echo "$list" | grep "/" > /dev/null
					if [ $? -eq 0 ]; then
						echo " - Remove $rm_path$list"
						rm -rf $rm_path$list
					else
						echo " - Find and remove $list"
						find $rm_path -name $list | xargs rm -rf
					fi
				fi
			fi
		done
		IFS=$OLD_IFS
	fi
}

#
# copy rootfs to destination
#
# input parameters
# $1	= rootfs source path (buildroot's target fs)
# $2	= rootfs destination path
# $3	= overwirte rootfs on exist rootfs (y=overwrite else make new rootfs)

function copy_rootfs()
{
	root_path=$1
	copy_path=$2
	root_ovwr=$3

	# check path's permission
	check_permission_w $root_path

	# check destination path permission
	[ ! -d $copy_path ] && mkdir -p $copy_path
	check_permission_w $copy_path

	echo ""
	if [ $root_ovwr = "n" ]; then
		echo -e " - Remove    : '$copy_path' ...."
		rm -rf $copy_path
		echo -e " - Copy      : '$root_path'"
		echo -e " - To        : '$copy_path' ...."
		cp -dR $root_path $copy_path
	else
		echo -e " - Overwrite : '$root_path'"
		echo -e " - To        : '$copy_path' ...."
		if [ -d $copy_path ]; then
			cp -dR $root_path/* $copy_path/
		else
			cp -dR $root_path $copy_path
		fi
	fi
}


#################################################################
# RUN COMMAND
#################################################################

# copy rootfs
param1=$RF_ROOT_PATH
param2=$RF_COPY_PATH
param3=$RF_OVERWRITE

copy_rootfs $param1 $param2 $param3

# rm unused files
param1=$RF_COPY_PATH
param2=$RM_UNUSED_FILE
param3=$RM_FILE_NO_ASK
param4=$RM_LIST_PATH

remove_unused_f $param1 $param2 $param3 $param4

# build device nodes
if [ $RF_MAKE_DEVNODE = "y" ] || [ -n "$MKDEV_EXT_SH_PATH" ]; then
	param1=$RF_COPY_PATH
	param2=$MKDEV_EXT_SH_PATH
	build_mkdev $param1 $param2
fi

# get copyed dir size
dr_size=$(du -sh $RF_COPY_PATH)
fs_size=$(echo $dr_size | cut -f1 -d" ")
echo -e "\n"
echo -e " - Rootfs size = $fs_size \n"
