#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2010-2011 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#
# -*- mode: makefile -*-

GCC_COMMON_BUILDFLAGS := -Wall -fpic -fno-strict-aliasing -Wno-strict-aliasing -Wno-long-long # -W -pedantic
GCC_OPTS := -m32
define gcc32-setup
$1_AS := as --32
$1_CC := gcc $(GCC_OPTS)
$1_VERSION := gcc -v 2>&1 | grep "gcc version"
$1_CXX := g++ $(GCC_OPTS)
$1_DYNLIB := gcc -shared $(GCC_OPTS)
$1_LD := gcc $(GCC_OPTS)
$1_EDITBIN := true
$1_AR := ar
$1_DEP_CC := gcc -MM $(GCC_OPTS)
$1_DEP_CXX := g++ -MM $(GCC_OPTS)
$1_COVERAGE_TOOL := gcov

$1_SYSTEM := gcc32-$(shell gcc $(GCC_OPTS) -dumpmachine | sed 's/-gnu//'| sed 's/-unknown//')
$1_LIBS := -lm -ldl
$1_CFLAGS := $(GCC_COMMON_BUILDFLAGS) -Werror-implicit-function-declaration  -std=c89
$1_CXXFLAGS := $(GCC_COMMON_BUILDFLAGS)  -fno-rtti
$1_LDFLAGS  :=-Wl,-rpath-link=$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -L$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)
$1_MULTITHREADED_LDFLAGS := -lpthread
$1_CXX_LDFLAGS := -lstdc++
# Translating absolute path references for calling the actual toolchain
$1_TRANSLATE_PATHS =$$1
$1_TRANSLATE_FILEPATH =$$1
$1_REWRITE_LIBRARY_COMMANDLINE = $$(patsubst $$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)lib%.so,-l%,$$2)
$1_CFLAGS += $(call check_cc,gcc,-Wdeclaration-after-statement,)
$1_ARFLAGS := rcs
$1_DYNLIBFLAGS = -Wl,-rpath-link=$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/) -L$$(call source-dir-to-binary-dir,$$($1_SYSTEM),$(LIB_DIR)/)

$1_ASM_OUTPUTS = -o $$1
$1_COMPILER_OUTPUTS = -o $$1
$1_LINKER_OUTPUTS = -o $$1
$1_DYNLIB_OUTPUTS = -o $$1
$1_AR_OUTPUTS =$$1
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
	$1_CFLAGS += -g -fno-omit-frame-pointer -fno-inline-functions-called-once
	$1_CXXFLAGS += -g -fno-omit-frame-pointer -fno-inline-functions-called-once
	$1_LDFLAGS += -g
	$1_DYNLIBFLAGS += -g
endif



endef
