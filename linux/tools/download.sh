#!/bin/sh

TOP=`pwd`

SERVER=git://210.219.52.221
BRANCH=jb-mr1.1

if [ -d linux ]; then
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# There is a folder of Linux already.'
	echo '#'
	echo '#########################################################'
	echo '#########################################################'
	echo ''
else
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# download linux'
	echo '#'
	echo '#########################################################'
	sleep 1.5
	git archive --remote=$SERVER/nexell/pyrope/android/platform/hardware/nexell/pyrope $BRANCH linux | tar xvf -	
	cd linux/tools
	git archive --remote=$SERVER/nexell/pyrope/android/platform/hardware/nexell/pyrope $BRANCH boot | tar xvf -
	git archive --remote=$SERVER/nexell/pyrope/android/device/nexell/tools $BRANCH make-pyrope-2ndboot-download-image.py | tar xvf -

	cd $TOP/linux
	if [ -d prototype ]; then
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# git pull prototype'
		echo '#'
		echo '#########################################################'
	else
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# download prototype'
		echo '#'
		echo '#########################################################'
		git clone $SERVER/nexell/pyrope/android/prototype
	fi
	cd prototype
	git pull origin $BRANCH
	cd $TOP/linux


	if [ -d u-boot ]; then
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# git pull u-boot'
		echo '#'
		echo '#########################################################'
	else
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# download u-boot'
		echo '#'
		echo '#########################################################'
		git clone $SERVER/nexell/pyrope/android/u-boot
	fi
	cd u-boot
	git pull origin $BRANCH
	cd $TOP/linux


	if [ -d kernel ]; then
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# git pull kernel'
		echo '#'
		echo '#########################################################'
	else
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# download kernel'
		echo '#'
		echo '#########################################################'
		git clone $SERVER/nexell/pyrope/android/kernel
	fi
	cd kernel
	git pull origin $BRANCH
	cd $TOP	
fi


