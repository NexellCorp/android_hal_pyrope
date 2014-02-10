#!/bin/bash
#
# build file systems

# Script PATH
#
ROOT_HOME_PATH=${HOME}
BR_BASE_PATH=`pwd`

BR_SCRIPT_PATH=${BR_BASE_PATH}/../scripts
RF_SCRIPT_PATH=${BR_BASE_PATH}/../scripts

# Target rootfs path
# default path BR_TARGET_RF_PATH=${BR_BASE_PATH}/../../out/rootfs
# nfs path     BR_TARGET_RF_PATH=${ROOT_HOME_PATH}/devel/nfs/buildroot

BR_TARGET_RF_PATH=${BR_BASE_PATH}/../out/rootfs

# Buildroot source rootfs path
# do not modify !!!
#
BR_SOURCE_RF_PATH=$1

# Copy options
#
BR_RM_DEF_UNUSED_FILE=y
BR_RM_LIST_PATH=${BR_SCRIPT_PATH}/rm_list.txt
BR_RM_FILES_NO_ASK=y
BR_TARGET_RF_OVERWRITE=n

echo -e "+++++++++++++++++++++++++++++++++++++++++++++++++++++++"
echo -e " MAKE ROOTFS ..."
echo -e "+++++++++++++++++++++++++++++++++++++++++++++++++++++++"
${RF_SCRIPT_PATH}/mk_rootfs.sh 	\
		-s ${BR_SOURCE_RF_PATH} 	\
		-t ${BR_TARGET_RF_PATH}		\
		-r ${BR_RM_DEF_UNUSED_FILE}	\
		-l ${BR_RM_LIST_PATH} 		\
		-a ${BR_RM_FILES_NO_ASK}	\
		-o ${BR_TARGET_RF_OVERWRITE}
echo -e "-------------------------------------------------------"
echo -e " DONE MAKE ROOTFS ..."
echo -e "-------------------------------------------------------"


