#!/bin/bash
#

# include PATH
#
BR_BASE_PATH=`pwd`
BR_AUTO_CONF=${BR_BASE_PATH}/output/build/buildroot-config/auto.conf

# GET PATH from include file
#
BR2_TOOLCHAIN_EXTERNAL_PATH=`cat $BR_AUTO_CONF | grep -w "BR2_TOOLCHAIN_EXTERNAL_PATH=" | cut -d'"' -f2`
BR2_TOOLCHAIN_EXTERNAL_PREFIX=`cat $BR_AUTO_CONF | grep -w "BR2_TOOLCHAIN_EXTERNAL_PREFIX=" | cut -d'"' -f2`

# Buildroot tool's sysroot path
# Refer to BR2_HOST_DIR
#
BR_SYSROOT_PATH=${BR_BASE_PATH}/output/host/usr/arm-buildroot-linux-gnueabi/sysroot

# Toolchain sysroot path
#
CT_SYSROOT_PATH=${BR2_TOOLCHAIN_EXTERNAL_PATH}/${BR2_TOOLCHAIN_EXTERNAL_PREFIX}/sysroot
CT_PKG_CFG_PATH=${CT_SYSROOT_PATH}/usr/lib/pkgconfig

# for pkconfig
CT_PKG_PREFIX=${CT_SYSROOT_PATH}/usr
CT_PKG_EXEC_PREFIX=${CT_SYSROOT_PATH}/usr
CT_MIN_DIR_DEPTH=4

# for libtool (*.la)
LT_DEPENDENCY_STR=${BR_BASE_PATH}/output/build

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
#sudo_permission _SUDO_

#
# check path validation
#
# input parameters
# $1	= path
# $2	= min depth
#
# return
# - if path is too short, exit
#

function check_path_valid()
{
	dir=$1
	min=$2

	path=$dir
	depth=1
	while [ X"$path" != X"." -a X"$path" != X"/" ]
	do
		path=`dirname "$path"`
		depth=`expr "$depth" + 1`
	done

    if [ $depth -lt $min ]; then
        echo -e "\n Path is too short: $dir"
        exit 1;
	fi
}

#
# change_permission
#
# input parameters
# $1	= path
# $2	= permission

function change_permission()
{
	path=$1
	perm=$2
	$_SUDO_ chmod -R $perm $path
}

#
# install toolchain
#
# input parameters
# $1	= build sysroot path
# $2	= crosstool chain sysroot path 	 (toolchain install dir)
# $3	= pkgconfig path (in toolchain)
# $4	= pkgconfig new prefix
# $5	= pkgconfig new exec_prefix
# $6	= libtool's la find string on "dependency_libs"

function install_toolchain()
{
	br_sr_dir=$1
	ct_sr_dir=$2
	pkcfg_dir=$3
	pk_prefix=$4
	pk_execprefix=$5
	lt_dependency=$6

	permission='u+w'

	# check directory exist
	if [ -z $br_sr_dir ] || [ ! -d $br_sr_dir ]; then
		echo -e "\n - Check path: Unkonw package $br_sr_dir ...."
		exit 1;
	fi

	# check directory exist
	if [ -z $ct_sr_dir ] || [ ! -d $ct_sr_dir ]; then
		echo -e "\n - Check path: Unkonw target $ct_sr_dir ...."
		exit 1;
	fi
	check_path_valid $ct_sr_dir $CT_MIN_DIR_DEPTH

	# check write permission
   #if [ ! -w "$ct_sr_dir" ]; then
	echo -e " You do not have write permission"
	echo -e " Change permissin with 'u+w'"
	change_permission $ct_sr_dir $permission
   #fi

	# install sysroot
   	echo -e "\n COPY ..."
	cp -dR $br_sr_dir/* $ct_sr_dir
   	echo    " DONE"
	echo 	""

	# chnage pkgconfig file path
	if [ -d $pkcfg_dir ]; then
		echo -e " CHANGE PKGCONFIG [$pkcfg_dir]"
		echo -e " prefix       to  '$pk_prefix'"
		echo -e " exec_prefix  to  '$pk_execprefix'"
		echo 	""

		for i in `ls $pkcfg_dir`
		do
			file=$pkcfg_dir/$i
			ls $file | grep -q pc
			if [ $? -eq 0 ]; then
				if [ ! -L $file ]; then
					echo " Change Prefix: $file"
					sed -i "s|prefix\=.*|prefix=$pk_prefix|g" $file
					sed -i "s|exec_prefix\=.*|exec_prefix=$pk_execprefix|g" $file
				fi
			fi
		done
	fi
   	echo    " DONE"
	echo -e "\n"

	# chnage libtool la file path
	echo -e " CHANGE LIBTOOLS la"
	echo -e " dependency_libs  to  '$ct_sr_dir/usr/lib'"
	echo -e " libdir           to  '$ct_sr_dir/usr/lib'"
	echo 	""

   	for i in `find $ct_sr_dir/usr/lib* -name "*.la"`;
    do
		echo -e " Change la: $i"
       	sed  -i "s|$br_sr_dir/usr/lib|$ct_sr_dir/usr/lib|g" $i;
       	sed  -i "s|libdir\=.*|libdir='$ct_sr_dir/usr/lib'|g" $i;

    	dependency_libs=`cat $i | grep  -w "dependency_libs="`

		if [ "$dependency_libs" ]; then
			for list in $dependency_libs
			do
				new_dependency=
				old_dependency=`echo "$list" | grep -w "$lt_dependency"`
				if [ $? -eq 0 ]; then
					sed -i "s|$old_dependency|$new_dependency|g" $i;
				fi
			done
		fi
    done
   	echo    " DONE"
	echo -e "\n"
}

#################################################################
# RUN COMMAND
#################################################################

echo -e "+++++++++++++++++++++++++++++++++++++++++++++++++++++++"
echo -e " Install Toolchain..."
echo -e " FROM [${BR_SYSROOT_PATH}]..."
echo -e " To   [${CT_SYSROOT_PATH}]..."
echo -e "+++++++++++++++++++++++++++++++++++++++++++++++++++++++"

param1=$BR_SYSROOT_PATH
param2=$CT_SYSROOT_PATH
param3=$CT_PKG_CFG_PATH
param4=$CT_PKG_PREFIX
param5=$CT_PKG_EXEC_PREFIX
param6=$LT_DEPENDENCY_STR

install_toolchain $param1 $param2 $param3 $param4 $param5 $param6

echo -e "-------------------------------------------------------"
echo -e " DONE Install ..."
echo -e "-------------------------------------------------------"


