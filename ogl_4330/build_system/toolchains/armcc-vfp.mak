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
ifneq ($(findstring 3.1,$(ARMLIB)),)
RVCT_VERSION:=3
else
$(error TARGET_TOOLCHAIN=$(TARGET_TOOLCHAIN) requires RVCT3.1)
endif

ARM_LINUX_GCC := arm-none-linux-gnueabi-gcc
CONFIG_CSL_ROOT:=$(shell $(ARM_LINUX_GCC) -print-file-name=)../../../..
CONFIG_CSL_GCC_VNUM := $(shell $(ARM_LINUX_GCC) -dumpversion)
CONFIG_CSL_CRT1_PATH := $(shell $(ARM_LINUX_GCC) -print-file-name=crt1.o)

# Path on your target system where the dynamic loader will reside
DYNAMIC_LINKER ?= /lib/ld-linux.so.3

# Set our image entry point
ENTRYPOINT=_start

# Path to the small number of RVCT header files we need in preference to the GNU files
RVCTHEADERS=$(BUILD_SYSTEM_DIR)/toolchains/armcc-include

# Glibc and libstdc++ header directories to search in
GLIBC_HEADERS=$(CONFIG_CSL_ROOT)/arm-none-linux-gnueabi/libc/usr/include,$(CONFIG_CSL_ROOT)/lib/gcc/arm-none-linux-gnueabi/$(CONFIG_CSL_GCC_VNUM)/include,$(CONFIG_CSL_ROOT)/arm-none-linux-gnueabi/include/c++/$(CONFIG_CSL_GCC_VNUM),$(CONFIG_CSL_ROOT)/arm-none-linux-gnueabi/include/c++/$(CONFIG_CSL_GCC_VNUM)/arm-none-linux-gnueabi

# Path to the libraries in your CodeSourcery installation
LINUXINITPATH=$(CONFIG_CSL_ROOT)/arm-none-linux-gnueabi/libc/usr/lib
LINUXLIBPATH := $(shell $(ARM_LINUX_GCC) -print-search-dirs | sed -e /libraries/p -e d | sed 's/^libraries: =//' | sed 's/:/,/g')

KEEP_FLAGS=--keep *\(.init\) --keep *\(.fini\) --keep *\(.init_array\) --keep *\(.fini_array\)

# armcc and armlink flags to use when building for Linux
# Note that these are not all needed in every case, however they will cover
# almost all typical code and will not cause problems if left as is
LINUXCFLAGS=--fpu=softvfp$(RVCT_VFP_VERSION) --fpmode=fast --gnu --enum_is_int --wchar32 --library_interface=aeabi_glibc -Dlrintf=_ffix_r -D__align=__alignx -J$(RVCTHEADERS),$(GLIBC_HEADERS) \
	--no_hide_all --apcs /interwork --bss_threshold=0 \
	-D_POSIX_SOURCE -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=600 \
	-D_XOPEN_SOURCE_EXTENDED=1 -D_BSD_SOURCE=1 -D_SVID_SOURCE=1 \
	--preinclude=linux_rvct.h --cpu=$(RVCT_ARCH) --diag_suppress C188,C1293,C236,C111,C2523 --apcs /fpic --dwarf3
LINUXLDFLAGS=--sysv --no_startup --no_scanlib --no_ref_cpp_init --pt_arm_exidx \
	--userlibpath=$(LINUXLIBPATH) --diag_suppress 6318,6319,6765,6332 \
	$(KEEP_FLAGS) \
	--dynamiclinker $(DYNAMIC_LINKER) --linux_abitag=2.6.12 --pltgot_opts=weakrefs --fpic
LINUXASFLAGS=--no_hide_all --apcs /interwork --cpu=$(RVCT_ARCH) --dwarf3 $(MALI_ARM_RVCT_ARMASM_SUPPRESS_A1764)

# Glibc startup objects to link into your application
LINUXOBJS=$(LINUXINITPATH)/crti.o $(LINUXINITPATH)/crtn.o $(CONFIG_CSL_CRT1_PATH)

ifeq ($(RVCT_VERSION),3)
ARMCC_SUPPORT_LIB := "$(ARMLIB)/armlib/h_5f.l"
endif

# Glibc and libgcc library files
LINUXLIBS=libc.so.6 libc_nonshared.a libm.so.6 libgcc_s.so.1 libgcc.a $(ARMCC_SUPPORT_LIB)

# Flags to pass to GNU ld to use the GNU C library followed by the RVCT libraries
GLD_LIBS=-L$(CONFIG_CSL_ROOT)/lib/gcc/arm-none-linux-gnueabi/$(CONFIG_CSL_GCC_VNUM) \
	-lc -lgcc_s -lm -ldl -lgcc -lstdc++ -lsupc++ $(ARMCC_SUPPORT_LIB)

# Flags to use when building a shared library with each toolchain
RVCT_LIBCFLAGS=--diag_style gnu --apcs /fpic -DPIC
RVCT_LIBASFLAGS=--diag_style gnu --apcs /fpic
RVCT_LIBLDFLAGS=--diag_style gnu --fpic
GNU_LIBCFLAGS=-fPIC -DPIC
GNU_LIBLDFLAGS=-fPIC

define armcc-vfp-setup

$1_AS := armasm
$1_CC := armcc --c90
$1_VERSION := armcc | grep Build
$1_CXX := armcc --cpp
$1_DYNLIB := arm-none-linux-gnueabi-gcc -shared
$1_LD := arm-none-linux-gnueabi-gcc
$1_EDITBIN := true
$1_AR := arm-none-linux-gnueabi-ar
$1_DEP_CC := arm-none-linux-gnueabi-gcc -MM
$1_DEP_CXX := arm-none-linux-gnueabi-g++ -MM
$1_DEP_AS := armasm --MD

$1_SYSTEM := armcc-arm-vfp-linux

$1_FAST_FLAVOUR_FLAG := --arm
$1_SMALL_FLAVOUR_FLAG := --thumb
$1_CFLAGS := --diag_style gnu $(LINUXCFLAGS)
$1_CXXFLAGS := --diag_style gnu $(LINUXCFLAGS)

$1_DYNLIBS := $(ARMCC_SUPPORT_LIB) -lm -ldl -lrt
$1_LIBS := $(ARMCC_SUPPORT_LIB) -lm -ldl -lrt
$1_LDFLAGS  := -Wl,-rpath-link=$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -L$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)
$1_MULTITHREADED_LDFLAGS := -lpthread
$1_CXX_LDFLAGS := -lstdc++
# Translating absolute path references for calling the actual toolchain
$1_TRANSLATE_PATHS =$$1
$1_TRANSLATE_FILEPATH =$$1
$1_REWRITE_LIBRARY_COMMANDLINE = $$(patsubst $$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)lib%.so,-l%,$$2)
$1_DYNLIBFLAGS = -symbolic -Wl,-rpath-link=$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -L$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)

$1_ARFLAGS := -rcs
$1_ASFLAGS := $(LINUXASFLAGS) --fpu=SoftVFP$(RVCT_VFP_VERSION)

$1_ASM_OUTPUTS = -o $$1
$1_COMPILER_OUTPUTS = -o $$1
$1_LINKER_OUTPUTS = -o $$1
$1_DYNLIB_OUTPUTS = -o $$1
$1_AR_OUTPUTS =$$1
$1_DYNLIB_SONAME := --soname=
OBJ_EXT := .o
EXE_EXT :=
LIB_EXT := .a
DYNLIB_EXT := .so

ifeq ($(CONFIG),debug)
	$1_CFLAGS += -g -O0 --debug
	$1_LDFLAGS += --dynamic_debug
	$1_DYNLDFLAGS += --dynamic_debug
endif

ifeq ($(CONFIG),release)
	$1_FAST_FLAVOUR_FLAG += -Otime
	$1_SMALL_FLAVOUR_FLAG += -Ospace
endif

ifeq ($(CONFIG),oprofile)
	$1_FAST_FLAVOUR_FLAG += -Otime
	$1_SMALL_FLAVOUR_FLAG += -Ospace
	$1_CFLAGS += -g 
	$1_CXXFLAGS += -g 
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
