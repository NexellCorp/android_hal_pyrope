#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2011 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

# -*- mode: makefile -*-

#
# Platform definition file suitable for most Cortex-A9 systems.
#

#
# RVCT-specific flags used by the RVCT toolchain
#
# Right-value for the --cpu flag
ifneq ($(findstring 3.1,$(ARMLIB)),)
RVCT_CPU=7-A
else
RVCT_CPU=Cortex-A9
endif
# Right-value for the --cpu flag (architecture)
RVCT_ARCH=7-A
# Right-value for the --fpu flag (vfp version)
RVCT_VFP_VERSION=+vfpv3

#
# GCC-specific flags used by the GNU toolchain
#
# Right-value for the -mcpu flag
# cortex-a9 is supported by CodeSourcery GCC compiler v. >= 2008q1-102
GCC_CPU=cortex-a9
# Right-value for the -march flag
GCC_ARCH=armv7-a
# Right-value for the -mfpu flag (vfp version)
GCC_VFP_VERSION=neon

# ARM architecture for this platform
MALI_TARGET_PLATFORM_ARM_ARCH=7

# 24MHZ 32bit-counter (SYS_24MHZ @ 0x100005C see Realview User Guide)
# Should be 0 for all platforms except Realview PBs.
MALI_HAS_24MHZ_CLK_SOURCE=0
