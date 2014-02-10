#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2007-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#
# -*- mode: makefile -*-

# Use arm-linux-gnueabi- if it exists, if not fall back to arm-none-linux-gnueabi-
CROSS_COMPILE ?= $(call check_cc2, arm-linux-gnueabi-gcc, arm-linux-gnueabi-, arm-generic-linux-gnueabi-)

ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS := -march=$(GCC_ARCH) -mthumb-interwork -fpic -fno-strict-aliasing -mfpu=$(GCC_VFP_VERSION) -mfloat-abi=softfp
ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS += -Wall -Wextra -Wno-long-long -Wno-unused

ifeq ($(EXPORT_API_ONLY),1)
ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS += -fvisibility=hidden
endif

define arm-linux-gcc-vfp-setup
$1_AS := $(CROSS_COMPILE)as
$1_CC := $(CROSS_COMPILE)gcc
$1_VERSION := $(CROSS_COMPILE)gcc -v 2>&1 | grep "gcc version"
$1_CXX := $(CROSS_COMPILE)g++
$1_DYNLIB := $(CROSS_COMPILE)gcc -shared
$1_LD := $(CROSS_COMPILE)gcc
$1_NM := $(CROSS_COMPILE)nm

$1_EDITBIN := true
$1_AR := $(CROSS_COMPILE)ar
$1_DEP_CC := $(CROSS_COMPILE)gcc -MM -mfloat-abi=softfp -mfpu=neon
$1_DEP_CXX := $(CROSS_COMPILE)g++ -MM -mfloat-abi=softfp -mfpu=neon
$1_COVERAGE_TOOL := $(CROSS_COMPILE)gcov

$1_SYSTEM := gcc-arm-vfp-linux

$1_FAST_FLAVOUR_FLAG := $(call check_cc, $(CROSS_COMPILE)gcc ,-marm,-mno-thumb)

ifeq ($(MALI_TARGET_PLATFORM_ARM_ARCH),6)
# Force no thumb for ARMv6
$1_SMALL_FLAVOUR_FLAG := $(call check_cc, $(CROSS_COMPILE)gcc ,-marm,-mno-thumb)
endif

$1_CFLAGS := $(ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS) -Werror-implicit-function-declaration -std=c89
$1_CXXFLAGS := $(ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS) -fno-exceptions -fno-rtti -fpermissive
$1_LDFLAGS  := $(ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS) -Wl,-rpath-link=$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -L$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)
$1_MULTITHREADED_LDFLAGS := -lpthread
$1_CXX_LDFLAGS := -lstdc++
$1_LIBS := -lm -ldl -lrt
$1_DYNLIBS := -lm -ldl -lrt
# Translating absolute path references for calling the actual toolchain
$1_TRANSLATE_PATHS =$$1
$1_REWRITE_LIBRARY_COMMANDLINE = $$(patsubst $$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)lib%.so,-l%,$$2)
$1_CFLAGS += $(call check_cc,$(CROSS_COMPILE)gcc,-Wdeclaration-after-statement,)
$1_ARFLAGS := rcs
$1_DYNLIBFLAGS = $(ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS) -symbolic -Wl,-rpath-link=$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -L$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)
$1_ASFLAGS := -march=$(GCC_ARCH) -mfpu=$(GCC_VFP_VERSION)

$1_ASM_OUTPUTS = -o $$1
$1_COMPILER_OUTPUTS = -o $$1
$1_LINKER_OUTPUTS = -o $$1
$1_DYNLIB_OUTPUTS = -o $$1
$1_AR_OUTPUTS =$$1
$1_DYNLIB_SONAME := -Wl,-soname,
OBJ_EXT := .o
EXE_EXT :=
LIB_EXT := .a
DYNLIB_EXT := .so

ifeq ($(CONFIG),debug)
	$1_CFLAGS += -g
	$1_CXXFLAGS += -g
	$1_LDFLAGS += -g
endif

ifeq ($(CONFIG),release)
$1_FAST_FLAVOUR_FLAG += -O3
$1_SMALL_FLAVOUR_FLAG += -O3
endif

ifeq ($(CONFIG),coverage)
	$1_CFLAGS += -g -fprofile-arcs -ftest-coverage
	$1_CXXFLAGS += -g -fprofile-arcs -ftest-coverage
	$1_LDFLAGS += -g -fprofile-arcs -ftest-coverage
	$1_DYNLIBFLAGS += -g -fprofile-arcs -ftest-coverage
endif

ifeq ($(CONFIG),profiling)
	$1_CFLAGS += -pg
	$1_CXXFLAGS += -pg
	$1_LDFLAGS += -pg
	$1_DYNLIBFLAGS += -pg
endif

ifeq ($(CONFIG),oprofile)
	$1_FAST_FLAVOUR_FLAG += -O3
	$1_SMALL_FLAVOUR_FLAG += -Os
	$1_CFLAGS += -g -mapcs-frame -fno-omit-frame-pointer -fno-inline-functions-called-once
	$1_CXXFLAGS += -g -mapcs-frame -fno-omit-frame-pointer -fno-inline-functions-called-once
	$1_LDFLAGS += -g
	$1_DYNLIBFLAGS += -g
endif

ifeq ($(MALI_ARM_VFP11_DENORM_FIX_ENABLE),1)
	$1_LDFLAGS += -Wl,--vfp11-denorm-fix=vector
	$1_DYNLIBFLAGS += -Wl,--vfp11-denorm-fix=vector
else
	$1_LDFLAGS += -Wl,--vfp11-denorm-fix=none
	$1_DYNLIBFLAGS += -Wl,--vfp11-denorm-fix=none
endif

endef
