#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2009-2010 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

# -*- mode: makefile -*-

# Defines whether this platform support SMP (1) or not (0).
# This is not a guarantee that the underlying OS is built to be SMP
MALI_TARGET_PLATFORM_SUPPORTS_SMP=0

#
# RVCT-specific flags used by the RVCT toolchain
#
# Right-value for the --cpu flag
RVCT_CPU=ARM1176JZF-S
# Right-value for the --cpu flag (architecture)
RVCT_ARCH=6
# Right-value for the --fpu flag (vfp version)
RVCT_VFP_VERSION=+vfpv2

#
# GCC-specific flags used by the GNU toolchain
#
# Right-value for the -mcpu flag
GCC_CPU=arm1176jzf-s
# Right-value for the -march flag
GCC_ARCH=armv6
# Right-value for the -mfpu flag (vfp version)
GCC_VFP_VERSION=vfp

# ARM architecture for this platform
MALI_TARGET_PLATFORM_ARM_ARCH=6

# 24MHZ 32bit-counter (SYS_24MHZ @ 0x100005C see Realview User Guide)
MALI_HAS_24MHZ_CLK_SOURCE=1
