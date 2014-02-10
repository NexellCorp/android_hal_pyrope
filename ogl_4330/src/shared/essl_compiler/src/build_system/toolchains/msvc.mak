# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2007-2010 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.

# -*- mode: makefile -*-
define msvc-setup

MSVC_PATH = "$(shell reg query HKLM\\Software\\Microsoft\\VisualStudio\\8.0\\Setup\\VC /v ProductDir | grep REG_SZ | sed "s/^.*REG_SZ\s*//" | sed 's:\\:/:g')"

VS8_ENV_DIR = $(shell reg query HKLM\\Software\\Microsoft\\VisualStudio\\8.0\\Setup\\VS /v EnvironmentDirectory | grep REG_SZ | sed "s/^.*REG_SZ\s*//" | sed 's:\\:/:g' | sed 's:IDE/:IDE:g' | sed 's/\([A-Za-z]\):\//\/cygdrive\/\1\//g')

MSVC_BIN = $$(MSVC_PATH)/bin
MSVC_LIB = $$(MSVC_PATH)/lib
MSVC_INCLUDES = $$(MSVC_PATH)/include

export LIB := $$(MSVC_LIB)

# MSVC needs to be able to access DLLs in the Visual Studio Environment Directory:
export PATH = $(VS8_ENV_DIR):${PATH}

$1_CC := $$(MSVC_BIN)/cl.exe
$1_CXX := $$(MSVC_BIN)/cl.exe
$1_DYNLIB := $$(MSVC_BIN)/lib.exe
$1_LD := $$(MSVC_BIN)/cl.exe
$1_EDITBIN := true
$1_AR := $$(MSVC_BIN)/lib.exe
$1_DEP_CC := gcc -MM
$1_DEP_CXX := g++ -MM

$1_SYSTEM := msvc-win32

# Translating absolute path references for calling the actual toolchain
# Important for win32 executables, as they don't understand cygwin-style absolute path refs
$1_TRANSLATE_PATHS = $$(shell echo '$$1' | sed 's/\/cygdrive\/\([a-z]\)\//\1:\//g')
$1_TRANSLATE_FILEPATH = $$(shell cygpath --mixed '$$1')

$1_REWRITE_LIBRARY_COMMANDLINE = $$2
$1_CFLAGS := /W3 /nologo /EHsc /GR- /I$$(MSVC_INCLUDES)
$1_CXXFLAGS := /W3 /nologo /EHsc /GR- /I$$(MSVC_INCLUDES)
$1_LDFLAGS  := /nologo
$1_ARFLAGS := /nologo
$1_DYNLIBFLAGS := /nologo
CPPFLAGS += -D_CRT_SECURE_NO_DEPRECATE

$1_ASM_OUTPUTS = /Fo$$1
$1_COMPILER_OUTPUTS = /Fo$$1
$1_LINKER_OUTPUTS = /Fe$$1
$1_DYNLIB_OUTPUTS = /OUT:$$1
$1_AR_OUTPUTS = /OUT:$$1
OBJ_EXT := .obj
EXE_EXT := .exe
LIB_EXT := .lib
DYNLIB_EXT := .dll

#/Gm
#/EHsc /RTC1 /MDd /Za /GR- /Fo"Debug\\" /Fd"Debug\vc80.pdb" /W3 /nologo /c /ZI /TP

ifeq ($(CONFIG),debug)
	$1_CFLAGS += /MTd /Od /RTC1 /Zi /DEBUG /Fdbuild/vc80.pdb
	$1_CXXFLAGS += /MTd /Od /RTC1 /Zi /DEBUG /Fdbuild/vc80.pdb
	$1_LDFLAGS += /MTd /Zi /DEBUG /Fdbuild/vc80.pdb
endif

ifeq ($(CONFIG),release)
	$1_CFLAGS += /MT /O2 /GR-
	$1_LDFLAGS += /MT
endif

ifeq ($(CONFIG),coverage)
	$1_CFLAGS +=
	$1_LDFLAGS +=
endif

ifeq ($(CONFIG),profiling)
	$1_CFLAGS +=
	$1_LDFLAGS +=
endif

ifeq ($(CONFIG),oprofile)
	$1_CFLAGS +=
	$1_LDFLAGS +=
endif



endef
