#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2007-2011 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#
# -*- mode: makefile -*-

ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS := -Wall -march=$(GCC_ARCH) -mthumb-interwork -fpic -fno-strict-aliasing -Wno-strict-aliasing -Wno-long-long -mfpu=$(GCC_VFP_VERSION) -mfloat-abi=softfp #-W -pedantic


define arm-linux-gcc-vfp-setup
$1_AS := arm-none-linux-gnueabi-as
$1_CC := arm-none-linux-gnueabi-gcc
$1_VERSION := arm-none-linux-gnueabi-gcc -v 2>&1 | grep "gcc version"
$1_CXX := arm-none-linux-gnueabi-g++
$1_DYNLIB := arm-none-linux-gnueabi-gcc -shared

$1_LD := arm-none-linux-gnueabi-gcc
$1_EDITBIN := true
$1_AR := arm-none-linux-gnueabi-ar
$1_DEP_CC := arm-none-linux-gnueabi-gcc -MM
$1_DEP_CXX := arm-none-linux-gnueabi-g++ -MM
$1_COVERAGE_TOOL := arm-none-linux-gnueabi-gcov

$1_SYSTEM := gcc-arm-vfp-linux
$1_FAST_FLAVOUR_FLAG := -mno-thumb
$1_LIBS := -lm -ldl
# disabled, does not work well
#$1_SMALL_FLAVOUR_FLAG := -mthumb

$1_CFLAGS := $(ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS) -Werror-implicit-function-declaration -std=c89
$1_CXXFLAGS := $(ARM_LINUX_GCC_VFP_COMMON_BUILDFLAGS) -fno-exceptions -fno-rtti
$1_LDFLAGS  := -Wl,-rpath-link=$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -L$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -lm -ldl
$1_MULTITHREADED_LDFLAGS := -lpthread
$1_CXX_LDFLAGS := -lstdc++
# Translating absolute path references for calling the actual toolchain
$1_TRANSLATE_PATHS =$$1
$1_REWRITE_LIBRARY_COMMANDLINE = $$(patsubst $$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)lib%.so,-l%,$$2)
$1_CFLAGS += $(call check_cc,arm-none-linux-gnueabi-gcc,-Wdeclaration-after-statement,)
$1_ARFLAGS := rcs
$1_DYNLIBFLAGS =  -symbolic -Wl,-rpath-link=$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -L$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)
$1_ASFLAGS := -march=$(GCC_ARCH)

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
	$1_DYNLIBFLAGS += -g
endif

ifeq ($(CONFIG),release)
$1_FAST_FLAVOUR_FLAG += -O3
$1_SMALL_FLAVOUR_FLAG += -Os
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


endef
