#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2005, 2007-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

TARGET_TOOLCHAIN=arm-linux-gcc-vfp
#CONFIG = debug
CONFIG = release

USE_UMPV2=0
#Magic to find the right directory to build from
ifndef FIND_BUILD_DIR_MAGIC
FIND_BUILD_DIR_MAGIC := 1
ifeq ($(wildcard topleveldir),)
.PHONY: $(MAKECMDGOALS) recurse_to_parent_and_build
$(MAKECMDGOALS): recurse_to_parent_and_build
recurse_to_parent_and_build:
	$(MAKE) -C .. $(MAKECMDGOALS)
endif
endif

# Location of the include folder for the Mali device driver header files.
# (<os name>/mali should be sub-folders of this directory)
MALI_DD_INCLUDE_DIR ?= $(DRIVER_DIR)/src/devicedrv/mali/include

# Store away any useful initial environment variables, forcing evaluation now.
## NOTE: if more configuration variables are added, then some files must be updated in sympathy,
## so that we store in the libraries all the information required to rebuild
## the drivers in exactly the same way.
##
## These files are:
## * version_info.mak
## * internal/regression_server/testbench_build
INITIAL_CPPFLAGS:=$(CPPFLAGS)

clean_targets := clean distclean

# First, figure out if the user mixed clean targets and build targets. If so, invoke ourselves recursively with the new information
ifneq ($(filter $(clean_targets),$(MAKECMDGOALS)),)
ifneq ($(filter-out $(clean_targets),$(MAKECMDGOALS)),)
#yes, mix

#dummy rules for everything
.PHONY: $(MAKECMDGOALS)

define make-recursive-rule
$1:
	$(MAKE) $1
endef

EVAL_FOR_SIDE_EFFECTS := $(foreach goal,$(MAKECMDGOALS),$(eval $(call make-recursive-rule,$(goal))))

# Leave processing to above invocation of make and terminate the current one.
skip-makefile := 1
endif
endif

# We process the rest of the Makefile if this is the final invocation of make.
ifeq ($(skip-makefile),)

ifndef DRIVER_DIR
	DRIVER_DIR ?= .
endif

SRC_DIR=$(DRIVER_DIR)/src

.PHONY: all real_all lib all-presets select_default

all: select_default

LIB_SYMLINK := lib
BIN_SYMLINK := bin
DEVDRV_SYMLINK := devdrv
ifndef LIB_DIR
	LIB_DIR := $(LIB_SYMLINK)
endif
ifndef BIN_DIR
	BIN_DIR := $(BIN_SYMLINK)
endif
ifndef DEVDRV_DIR
	DEVDRV_DIR := $(LIB_SYMLINK)
endif
TMP_PRODUCT_DIR := tmp_product_dir

# Detect the presence of which APIs are installed before everything else
# to make the correct choice in customer releases
ifneq ($(wildcard $(SRC_DIR)/opengles/Makefile),)
	MALI_GLES_FOUND=gles
	MALI_GLES_PRESET=-$(MALI_GLES_FOUND)
endif
ifneq ($(wildcard $(SRC_DIR)/openvg/Makefile),)
	MALI_VG_FOUND=vg
	MALI_VG_PRESET=-$(MALI_VG_FOUND)
endif

# Logic to determine the default preset
# make with no other arguments will build a malix00-<gles>-<vg> for available APIs
# 	mali200 wins over mali400 if both are present
# MALI_ALL_PRESETS is a list of presets from both mali200 and mali400
PRESETS_DIR := presets
ifndef VARIANT
	ifneq ($(wildcard $(PRESETS_DIR)/mali200.mak),)
		PRESET?=mali200$(MALI_GLES_PRESET)$(MALI_VG_PRESET)
	else
		ifneq ($(wildcard $(PRESETS_DIR)/mali400.mak),)
			PRESET?=mali400$(MALI_GLES_PRESET)$(MALI_VG_PRESET)
		endif
	endif
	ifneq ($(wildcard $(PRESETS_DIR)/mali200.mak),)
		MALI_ALL_PRESETS=mali200$(MALI_GLES_PRESET)$(MALI_VG_PRESET) mali200$(MALI_VG_PRESET)
	endif
	ifneq ($(wildcard $(PRESETS_DIR)/mali400.mak),)
		MALI_ALL_PRESETS+=mali400$(MALI_GLES_PRESET)$(MALI_VG_PRESET) mali400$(MALI_VG_PRESET)
	endif
	-include $(PRESETS_DIR)/$(PRESET).mak
endif

## NOTE: if more configuration variables are added, then some files must be updated in sympathy,
## so that we store in the libraries all the information required to rebuild
## the drivers in exactly the same way.
##
## These files are:
## * version_info.mak
## * internal/regression_server/testbench_build

# Set to 1 to enable opengles tracing
USE_TRACING ?= 0

make-preset-%::
	VARIANT= PRESET=`echo $@ | sed s/^make-preset-//` $(MAKE)
all-presets:: $(patsubst %, make-preset-%,$(MALI_ALL_PRESETS))

CONFIG ?= debug
CUSTOMER ?= internal
BUILD_ROOT ?= build
DEFAULT_FILE_FLAVOUR ?= small

ifeq ($(CONFIG),release)
HOSTLIB ?= direct
else
HOSTLIB ?= indirect
endif

TARGET_PLATFORM ?= default_7a
#TARGET_PLATFORM ?= default_5te
# Enable SMP safety by default
MALI_SMP ?= 1
MALI_INSTRUMENTED := FALSE
MALI_INSTRUMENTED_PLUGIN_ENABLED = 0
MALI_TIMELINE_PROFILING_ENABLED = 0
MALI_TIMELINE_PROFILING_FUNC_ENABLED = 0
MALI_PERFORMANCE_COUNTERS_ENABLED = 0
MALI_SW_COUNTERS_ENABLED = 0
MALI_FRAMEBUFFER_DUMP_ENABLED = 0
MALI_EXTRA_ENVIRONMENT_FILE =

# Number of base worker threads. Minimum value 1. Alternate value on dual cores: 2.
MALI_BASE_WORK_THREAD_NUMBER ?= 1
# Set to 0 to remove the worker thread affinity setting. Value 1 indicates that affinity is set.
# It should be 0, if the MALI_BASE_WORK_THREAD_NUMBER is 1. And it might be 1, if there are more worker threads.
MALI_WORKER_THREAD_AFFINITY ?= 0

# Use the cleanup thread for deferred frame resets.
MALI_USE_DEFERRED_FRAME_RESET ?= 1

BUILD_SYSTEM_DIR=$(DRIVER_DIR)/build_system

ifndef TARGET_TOOLCHAIN
	ifdef TOOLCHAIN
		TARGET_TOOLCHAIN = $(TOOLCHAIN)
	else
		TARGET_TOOLCHAIN = arm-linux-gcc
	endif
endif

TOOLCHAIN ?= gcc
HOST_TOOLCHAIN ?= $(TOOLCHAIN)
V ?= 0

ifeq ($(USE_UMPV2),0)
UMP_DIR ?= $(DRIVER_DIR)/src/ump
UMP_LIB ?=
UMP_INCLUDE_DIR ?= $(UMP_DIR)/include
else
UMP_DIR ?= $(DRIVER_DIR)/src/umpv2
UMP_LIB ?=
UMP_INCLUDE_DIR ?= $(UMP_DIR)
endif

ifeq ($(V),1)
	quiet=
	Q=
else
	ifeq ($(VERBOSE),1)
		quiet=
		Q=
	else
		ifneq ($(findstring s,$(MAKEFLAGS)),)
			quiet=silent-
		else
			quiet=quiet-
		endif
		Q=@
	endif
endif

# This conditional makefile exports the global definition ARM_INTERNAL_BUILD. Customer releases will not include arm_internal.mak
# Allows for command line setting of ARM_INTERNAL_BUILD only for ARM internal use.
ifeq ($(CUSTOMER),internal)
	-include arm_internal.mak
endif

# Platform-specific settings before makefile.rules, this means that the build macros defined in it will not be available.
include $(BUILD_SYSTEM_DIR)/platform/$(TARGET_PLATFORM).mak

# The following definition is forwarded by $(TARGET_PLATFORM).mak
CPPFLAGS += -DMALI_PLATFORM_ARM_ARCH=$(MALI_TARGET_PLATFORM_ARM_ARCH)

# Hash Build system dirs, to shorten path length
HASH_BUILD_DIR?=0

include $(BUILD_SYSTEM_DIR)/Makefile.rules

include version_info.mak

include $(BUILD_SYSTEM_DIR)/config/$(CONFIG_FILE_TO_USE).mak

CPPFLAGS += -DMALI_BASE_WORK_THREAD_NUMBER=$(MALI_BASE_WORK_THREAD_NUMBER)
CPPFLAGS += -DMALI_WORKER_THREAD_AFFINITY=$(MALI_WORKER_THREAD_AFFINITY)
CPPFLAGS += -DMALI_USE_DEFERRED_FRAME_RESET=$(MALI_USE_DEFERRED_FRAME_RESET)

CPPFLAGS += -DMALI_INSTRUMENTED_PLUGIN=$(MALI_INSTRUMENTED_PLUGIN)
CPPFLAGS += -DMALI_TIMELINE_PROFILING_ENABLED=$(MALI_TIMELINE_PROFILING_ENABLED)
CPPFLAGS += -DMALI_TIMELINE_PROFILING_FUNC_ENABLED=$(MALI_TIMELINE_PROFILING_FUNC_ENABLED)
CPPFLAGS += -DMALI_FRAMEBUFFER_DUMP_ENABLED=$(MALI_FRAMEBUFFER_DUMP_ENABLED)
CPPFLAGS += -DMALI_PERFORMANCE_COUNTERS_ENABLED=$(MALI_PERFORMANCE_COUNTERS_ENABLED)
ifneq ($(MALI_EXTRA_ENVIRONMENT_FILE),)
	CPPFLAGS += -DMALI_EXTRA_ENVIRONMENT_FILE=$(MALI_EXTRA_ENVIRONMENT_FILE)
endif

ifneq ($(call is-feature-enabled,gp_cache_alloc),)
	MALI_BUILD_ANDROID_MONOLITHIC=1
	CPPFLAGS += -DMALI_FORCE_GP_L2_ALLOC=1
else
	CPPFLAGS += -DMALI_FORCE_GP_L2_ALLOC=0
endif

ifeq ($(MALI_TIMELINE_PROFILING_ENABLED),1)
	MALI_INSTRUMENTED_PLUGIN_ENABLED=1
endif
ifeq ($(MALI_PERFORMANCE_COUNTERS_ENABLED),1)
	MALI_INSTRUMENTED_PLUGIN_ENABLED=1
endif

CPPFLAGS += -DMALI_INSTRUMENTED_PLUGIN_ENABLED=$(MALI_INSTRUMENTED_PLUGIN_ENABLED)

MALI_VSYNC_EVENT_REPORT_ENABLE ?= 0
CPPFLAGS += -DMALI_VSYNC_EVENT_REPORT_ENABLE=$(MALI_VSYNC_EVENT_REPORT_ENABLE)

# Enable SW counter capture and reporting.
CPPFLAGS += -DMALI_SW_COUNTERS_ENABLED=$(MALI_SW_COUNTERS_ENABLED)

# MALI_SMP is unconditionally added to the list of defs for the pre-processor
CPPFLAGS += -DMALI_SMP=$(MALI_SMP)

#nexell block
#CPPFLAGS += -DMALI_BB_TRANSFORM

# Don't create the extra directories when we're cleaning
# Using the logic also in $(BUILD_SYSTEM_DIR)/Makefile.rules, doesn't create
# the 'nodir_target' and 'nodir_host' directories
# Required for Cygwin Make
ifeq ($(filter $(clean_targets),$(MAKECMDGOALS)),)
CREATE_VARIOUS_DIRS__EVAL_FOR_SIDE_EFFECTS := \
	$(shell mkdir -p $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(BIN_DIR)/)) \
	$(shell mkdir -p $(call source-dir-to-binary-dir,$(HOST_SYSTEM),$(BIN_DIR)/)) \
	$(shell mkdir -p $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(LIB_DIR)/)) \
	$(shell mkdir -p $(call source-dir-to-binary-dir,$(HOST_SYSTEM),$(LIB_DIR)/)) \
	$(shell mkdir -p $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(TMP_PRODUCT_DIR)/)) \
	$(shell mkdir -p $(call source-dir-to-binary-dir,$(HOST_SYSTEM),$(TMP_PRODUCT_DIR)/)) \
	$(shell mkdir -p $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(DEVDRV_DIR)/)) \
	$(shell rm -f $(LIB_SYMLINK) && ln -s $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(LIB_DIR)) $(LIB_SYMLINK)) \
	$(shell rm -f $(BIN_SYMLINK) && ln -s $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(BIN_DIR)) $(BIN_SYMLINK)) \
	$(shell rm -f $(DEVDRV_SYMLINK) && ln -s $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(DEVDRV_DIR)) $(DEVDRV_SYMLINK))
endif # ($(filter $(clean_targets),$(MAKECMDGOALS)),)

# Library variables definition
# Note: use immediate evaluation.
MALI_GLES11_LIB_NAME = libGLESv1_CM
MALI_GLES20_LIB_NAME = libGLESv2
MALI_GLES11_LIB = $(LIB_DIR)/$(MALI_GLES11_LIB_NAME)$(DYNLIB_EXT)
MALI_GLES20_LIB = $(LIB_DIR)/$(MALI_GLES20_LIB_NAME)$(DYNLIB_EXT)
MALI_EGL_LIB = $(LIB_DIR)/libEGL$(DYNLIB_EXT)
MALI_OPENVG_LIB = $(LIB_DIR)/libOpenVG$(DYNLIB_EXT)
MALI_OPENVGU_LIB = $(LIB_DIR)/libOpenVGU$(DYNLIB_EXT)

#org
#MALI_LIB = $(LIB_DIR)/libMali$(DYNLIB_EXT)
#nexell add
MALI_LIB = $(LIB_DIR)/libVR$(DYNLIB_EXT)

MALI_UMP_LIB = $(LIB_DIR)/libUMP$(DYNLIB_EXT)
# Library Versioning
MALI_EGL_MAJOR_VERSION = 1
MALI_EGL_MINOR_VERSION = 4
MALI_GLES_1_MAJOR_VERSION = 1
MALI_GLES_1_MINOR_VERSION = 1
MALI_GLES_2_MAJOR_VERSION = 2
MALI_GLES_2_MINOR_VERSION = 0
MALI_VG_MAJOR_VERSION = 1
MALI_VG_MINOR_VERSION = 1

# Libraries will be created with 'versioned' names by default. Overridden by some variants (e.g. windowsce)
VERSIONED_LIBRARIES ?=1

ifeq ($(CONFIG),release)
EXPORT_API_ONLY ?= 1
else
# Export all symbols by default for debug builds
EXPORT_API_ONLY ?= 0
endif

# Use monolithic unless no_monolithic is in VARIANT
ifeq ($(call is-feature-enabled,no_monolithic),)
	# Build system variable to force linking of objects into a unique binary
	MALI_MONOLITHIC=1
	CPPFLAGS += -DMALI_MONOLITHIC
ifneq ($(call is-feature-enabled,android),)
	MALI_BUILD_ANDROID_MONOLITHIC=1
	CPPFLAGS += -DMALI_BUILD_ANDROID_MONOLITHIC
	ifeq ($(EXPORT_API_ONLY),1)
		CPPFLAGS += -fvisibility=hidden
		LDFLAGS += -fvisibility=hidden
	endif
endif
endif

ALL_LIBS = $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_LIB))

# Enables support for external memory shared using dma_buf API
ifneq ($(call is-feature-enabled,dma_buf),)
	MALI_USE_DMA_BUF=1
	CPPFLAGS += -DMALI_USE_DMA_BUF=1
#	CPPFLAGS += -DDMA_BUF_ON_DEMAND=1
endif

# Enables connection with an external UMP driver (Unified Memory Provider)
ifneq ($(call is-feature-enabled,ump),)
	MALI_USE_UNIFIED_MEMORY_PROVIDER=1
	CPPFLAGS += -DMALI_USE_UNIFIED_MEMORY_PROVIDER=1 -I$(UMP_INCLUDE_DIR)
ifeq ($(UMP_LIB),)
	# Build included libUMP
	MALI_BUILD_UMP = 1
	ALL_LIBS += $(MALI_UMP_LIB)
	CPPFLAGS += -I$(UMP_INCLUDE_DIR)/ump
ifeq ($(USE_UMPV2),0)
	CPPFLAGS += -I./src/devicedrv/ump/linux # For ump_ioctl.h
endif
	ifneq ($(call is-feature-enabled,android),)
		MALI_UMP_LIB = libUMP
	endif
	UMP_LIB = $(MALI_UMP_LIB)
else
	# Link against externally provided libUMP
	MALI_UMP_LIB = $(UMP_LIB)
endif
else
	# UMP is disabled; don't link against it
	MALI_UMP_LIB = ""
endif

ifeq ($(MALI_USE_UNIFIED_MEMORY_PROVIDER),1)
ifneq ($(call is-feature-enabled,umplock),)
	MALI_USE_UMPLOCK=1
endif
endif


# See per-arch is-feature-enables for extra libs (e.g. Windows CE)
CONFIG_NONASSERTED := 0
ifeq ($(CONFIG),release)
	CONFIG_NONASSERTED := 1
endif
ifeq ($(CONFIG),profiling)
	CONFIG_NONASSERTED := 1
endif
ifeq ($(CONFIG),oprofile)
	CONFIG_NONASSERTED := 1
endif
ifeq ($(CONFIG_NONASSERTED),1)
	ifeq ($(call is-feature-enabled,asserted),)
		CPPFLAGS += -DNDEBUG -DMALI_DEBUG_EXTENDED_MUTEX_LOCK_CHECKING=0
		CPPFLAGS += -DMALI_DEBUG_SKIP_ASSERT -DMALI_DEBUG_SKIP_CODE -DMALI_DEBUG_SKIP_TRACE -DMALI_DEBUG_SKIP_TPRINT -DMALI_DEBUG_SKIP_PRINT -DMALI_DEBUG_SKIP_ERROR
	endif
endif

ifeq ($(CONFIG),debug)
	CPPFLAGS += -DDEBUG -DMALI_DEBUG_EXTENDED_MUTEX_LOCK_CHECKING=1
endif

ifeq ($(HOSTLIB),direct)
	CPPFLAGS += -I$(DRIVER_DIR)/include/base/hostlib/direct
	CPPFLAGS += -DMALI_HOSTLIB_DIRECT=1
endif
ifeq ($(HOSTLIB),stubs)
	CPPFLAGS += -I$(DRIVER_DIR)/include/base/hostlib/stubs
	CPPFLAGS += -DMALI_HOSTLIB_STUBS=1
endif
ifeq ($(HOSTLIB),indirect)
	CPPFLAGS += -I$(DRIVER_DIR)/include/base/hostlib/indirect
	CPPFLAGS += -DMALI_HOSTLIB_INDIRECT=1
endif

CPPFLAGS += -D_REENTRANT -DMALI_USE_THREADS -DPIC
CPPFLAGS += -D_POSIX_SOURCE

CPPFLAGS += -I$(DRIVER_DIR)/include -I$(DRIVER_DIR)/internal/include/khronos -I$(DRIVER_DIR)/src -I$(DRIVER_DIR)/3rdparty/include/khronos

# This allow for automatic validation of the builds during customer release since khronos_types.h is not included in the release
ifndef ARM_INTERNAL_BUILD
	CPPFLAGS += -I$(DRIVER_DIR)/3rdparty/include/khronos/VG
endif

# Provides release name for query purposes
CPPFLAGS += -DMALI_RELEASE_NAME=$(MALI_RELEASE_NAME)

ifneq ($(call is-feature-enabled,no_mali),)
	MALI_ARCH=arch_999_no_mali
	CPPFLAGS += -DLINUX
	CPPFLAGS += -I$(DRIVER_DIR)/include/base/timer/libc
endif

# Selects the Mali timer backend used by base_arch_timer code
ifeq ($(MALI_HAS_24MHZ_CLK_SOURCE),1)
	MALI_TIMER_BACKEND=realview
else
	MALI_TIMER_BACKEND=libc
endif

ifndef MALI_ARCH
	MALI_ARCH=arch_011_udd
	ifneq ($(call is-feature-enabled,linux),)
		CPPFLAGS += -DLINUX
		UDD_OS=linux
		ifneq ($(call is-feature-enabled,android),)
			MALI_ANDROID=1
			MALI_ANDROID_API=15
			ifneq ($(call is-feature-enabled,eclair),)
				MALI_ANDROID_API=7
			endif
			ifneq ($(call is-feature-enabled,froyo),)
				MALI_ANDROID_API=8
			endif
			ifneq ($(call is-feature-enabled,gingerbread),)
				MALI_ANDROID_API=10
			endif
			ifneq ($(call is-feature-enabled,beeswax),)
				MALI_ANDROID_API=13
			endif
			ifneq ($(call is-feature-enabled,ics),)
				MALI_ANDROID_API=14
			endif
			ifneq ($(call is-feature-enabled,jellybean),)
				MALI_ANDROID_API=15
			endif
			ifneq ($(call is-feature-enabled,jellybean_mr1),)
				MALI_ANDROID_API=17
			endif
			CPPFLAGS += -DMALI_ANDROID_API=$(MALI_ANDROID_API)
		endif
	else
$(warning "No OS specified. Assuming Linux.")
		CPPFLAGS += -DLINUX
		UDD_OS=linux
	endif

	# Enforce indirect hostlib for UDD
	ifneq ($(HOSTLIB),indirect)
	ifneq ($(UDD_OS),linux)
$(error HOSTLIB must be set to indirect for UDD)
	endif
	endif

	CPPFLAGS += -I$(DRIVER_DIR)/include/base/timer/$(MALI_TIMER_BACKEND)
endif

CPPFLAGS += -DMALI_ARCH=$(MALI_ARCH)
CPPFLAGS += -I$(DRIVER_DIR)/include/base/mem/$(MALI_ARCH)

# Use the cache efficient UORDER texture swizzling format
CPPFLAGS += -DUSE_UORDER
# Versioned Library names, for systems that can handle it
# The convention will be for the real executable has the target MALI_<lib>_LIB_VERSIONED
# A list of symlinks to create will have targets listed via MALI_<lib>_LIB_ALIASES, which contains a list of whitespace separate words.
#
# This is only done for systems that support versioning of libraries - enabled by VERSIONED_LIBRARIES
# Otherwise, systems which cannot handle multiple versions of libraries will used the unversioned name.
#
# Note that libOpenVG, libOpenVGU and libMali are not currently versioned, but could be in future.

ifneq ($(VERSIONED_LIBRARIES),)
	MALI_GLES11_LIB_VERSIONED  = $(MALI_GLES11_LIB).$(MALI_GLES_1_MAJOR_VERSION).$(MALI_GLES_1_MINOR_VERSION)
	MALI_GLES20_LIB_VERSIONED  = $(MALI_GLES20_LIB).$(MALI_GLES_2_MAJOR_VERSION).$(MALI_GLES_2_MINOR_VERSION)
	MALI_EGL_LIB_VERSIONED     = $(MALI_EGL_LIB).$(MALI_EGL_MAJOR_VERSION).$(MALI_EGL_MINOR_VERSION)
	MALI_OPENVG_LIB_VERSIONED  = $(MALI_OPENVG_LIB)
	MALI_OPENVGU_LIB_VERSIONED = $(MALI_OPENVGU_LIB)
	MALI_LIB_VERSIONED         = $(MALI_LIB)
	MALI_UMP_LIB_VERSIONED     = $(MALI_UMP_LIB)

	# Aliases of libraries
	MALI_GLES11_LIB_ALIASES  = $(MALI_GLES11_LIB).$(MALI_GLES_1_MAJOR_VERSION) $(MALI_GLES11_LIB)
	MALI_GLES20_LIB_ALIASES  = $(MALI_GLES20_LIB).$(MALI_GLES_2_MAJOR_VERSION) $(MALI_GLES20_LIB)
	MALI_EGL_LIB_ALIASES     = $(MALI_EGL_LIB).$(MALI_EGL_MAJOR_VERSION)       $(MALI_EGL_LIB)
	MALI_OPENVG_LIB_ALIASES  =
	MALI_OPENVGU_LIB_ALIASES =
	MALI_LIB_ALIASES         =
else
	MALI_GLES11_LIB_VERSIONED  = $(MALI_GLES11_LIB)
	MALI_GLES20_LIB_VERSIONED  = $(MALI_GLES20_LIB)
	MALI_EGL_LIB_VERSIONED     = $(MALI_EGL_LIB)
	MALI_OPENVG_LIB_VERSIONED  = $(MALI_OPENVG_LIB)
	MALI_OPENVGU_LIB_VERSIONED = $(MALI_OPENVGU_LIB)
	MALI_LIB_VERSIONED         = $(MALI_LIB)
	MALI_UMP_LIB_VERSIONED     = $(MALI_UMP_LIB)

	# Aliases of libraries
	MALI_GLES11_LIB_ALIASES  =
	MALI_GLES20_LIB_ALIASES  =
	MALI_EGL_LIB_ALIASES     =
	MALI_OPENVG_LIB_ALIASES  =
	MALI_OPENVGU_LIB_ALIASES =
	MALI_LIB_ALIASES         =
endif

# Only enable NEON optimizations if vfp toolchain and neon fpu are specified.
ifeq ($(GCC_VFP_VERSION),neon)
	# The device supports ARM_NEON
	ifeq ($(findstring vfp, $(TARGET_TOOLCHAIN)),vfp)
		# The toolchain supports ARM_NEON
		ifeq ($(findstring armcc, $(TARGET_TOOLCHAIN)), armcc)
			# Check RVCT version. ARM_NEON support not sufficient in RVCT 3.x
			ifeq ($(shell test $(RVCT_VERSION) -gt 3 && echo OK),OK)
				MALI_PLATFORM_ARM_NEON := 1
			else
				MALI_PLATFORM_ARM_NEON := 0
			endif
		else
			MALI_PLATFORM_ARM_NEON := 1
		endif
	else
		MALI_PLATFORM_ARM_NEON := 0
	endif
else
	MALI_PLATFORM_ARM_NEON := 0
endif
CPPFLAGS += -DMALI_PLATFORM_ARM_NEON=$(MALI_PLATFORM_ARM_NEON)


#setup cores variable
ifneq ($(call is-feature-enabled,mali450),)
	CORES += MALI450
	CPPFLAGS += -I$(DRIVER_DIR)/include/regs/MALI200

	# choose hw version based on variant string
	ifneq ($(call is-feature-enabled,r0p0),)
		MALI450_HWVER = 0x0000
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI450/r0p0
	endif

	# set default version (when no version is specified in the variant string)
	ifndef MALI450_HWVER
		MALI450_HWVER = 0x0000
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI450/r0p0
	endif

	CPPFLAGS +=-DMALI450_HWVER=$(MALI450_HWVER)
	CPPFLAGS += -I$(DRIVER_DIR)/include/regs/MALIGP2
endif

ifneq ($(call is-feature-enabled,mali400),)
	CORES += MALI400
	CPPFLAGS += -I$(DRIVER_DIR)/include/regs/MALI200

	# choose hw version based on variant string
	ifneq ($(call is-feature-enabled,r0p0),)
		MALI400_HWVER = 0x0000
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI400/r0p0
	endif
	ifneq ($(call is-feature-enabled,r0p1),)
		MALI400_HWVER = 0x0001
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI400/r0p1
	endif
	ifneq ($(call is-feature-enabled,r1p0),)
		MALI400_HWVER = 0x0100
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI400/r1p0
	endif
	ifneq ($(call is-feature-enabled,r1p1),)
		MALI400_HWVER = 0x0101
		MALI300_HWVER = 0x0000
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI400/r1p1
	endif

	# set default version (when no version is specified in the variant string)
	ifndef MALI400_HWVER
		MALI400_HWVER = 0x0101
		MALI300_HWVER = 0x0000
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI400/r1p1
	endif

	CPPFLAGS +=-DMALI400_HWVER=$(MALI400_HWVER) -DMALI300_HWVER=$(MALI300_HWVER)
	CPPFLAGS += -I$(DRIVER_DIR)/include/regs/MALIGP2
endif

ifneq ($(call is-feature-enabled,mali300),)
	CORES += MALI400
	CPPFLAGS += -I$(DRIVER_DIR)/include/regs/MALI200

	# choose hw version based on variant string
	ifneq ($(call is-feature-enabled,r0p0),)
		# Mali300 r0p0 is a single core Mali400 r1p1
		MALI300_HWVER = 0x0000
		MALI400_HWVER = 0x0101
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI400/r1p1
	endif

	# set default version (when no version is specified in the variant string)
	ifndef MALI300_HWVER
		# Mali300 r0p0 is a single core Mali400 r1p1
		MALI300_HWVER = 0x0000
		MALI400_HWVER = 0x0101
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI400/r1p1
	endif

	CPPFLAGS +=-DMALI400_HWVER=$(MALI400_HWVER) -DMALI300_HWVER=$(MALI300_HWVER)
	CPPFLAGS += -I$(DRIVER_DIR)/include/regs/MALIGP2
endif

ifneq ($(call is-feature-enabled,mali200),)
	CORES += MALI200
	CPPFLAGS += -I$(DRIVER_DIR)/include/regs/MALI200

	# choose hw version based on variant string
	ifneq ($(call is-feature-enabled,r0p1),)
		MALI200_HWVER = 0x0001
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI200/r0p1
	endif
	ifneq ($(call is-feature-enabled,r0p2),)
		MALI200_HWVER = 0x0002
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI200/r0p2
	endif
	ifneq ($(call is-feature-enabled,r0p3),)
		MALI200_HWVER = 0x0003
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI200/r0p3
	endif
	ifneq ($(call is-feature-enabled,r0p4),)
		MALI200_HWVER = 0x0004
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI200/r0p4
	endif
	ifneq ($(call is-feature-enabled,r0p5),)
		MALI200_HWVER = 0x0005
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI200/r0p5
	endif

	# set default version (when no version is specified in the variant string)
	ifndef MALI200_HWVER
		MALI200_HWVER = 0x0005
		CPPFLAGS += -I$(DRIVER_DIR)/src/base/cores/MALI200/r0p5
	endif

	CPPFLAGS +=-DMALI200_HWVER=$(MALI200_HWVER)
	CPPFLAGS += -I$(DRIVER_DIR)/include/regs/MALIGP2
endif

ifneq ($(call is-feature-enabled,mali200),)
	CPPFLAGS +=-DMALI_MAX_PP_SPLIT_COUNT=1
endif

ifneq ($(call is-feature-enabled,mali300),)
	CPPFLAGS +=-DMALI_MAX_PP_SPLIT_COUNT=1
endif

ifneq ($(call is-feature-enabled,mali400),)
	CPPFLAGS +=-DMALI_MAX_PP_SPLIT_COUNT=4
endif

ifneq ($(call is-feature-enabled,mali450),)
	CPPFLAGS +=-DMALI_MAX_PP_SPLIT_COUNT=8
endif

$(foreach core,$(CORES), $(eval USING_$(core)=1))
$(foreach core,$(CORES), $(eval CPPFLAGS += -DUSING_$(core)))

ifeq ($(call is-feature-enabled,mali200)$(call is-feature-enabled,mali300)$(call is-feature-enabled,mali400)$(call is-feature-enabled,mali450),)
$(error No Mali core set. )
endif
ifneq ($(call is-feature-enabled,gles11),)
	USE_OPENGLES := 1
	USE_OPENGLES_1 := 1
	CPPFLAGS += -DMALI_USE_GLES_1=$(USE_OPENGLES_1)

	CPPFLAGS += -DMALI_GLES_1_MINOR_VERSION=$(MALI_GLES_1_MINOR_VERSION)

	# Note that LIB_ALIASES may be empty
	ALL_LIBS += 	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_GLES11_LIB_VERSIONED) $(MALI_GLES11_LIB_ALIASES)) \
				$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_EGL_LIB_VERSIONED) $(MALI_EGL_LIB_ALIASES))

endif

ifneq ($(call is-feature-enabled,gles20),)
	USE_OPENGLES := 1
	USE_OPENGLES_2 := 1
	CPPFLAGS += -DMALI_USE_GLES_2=$(USE_OPENGLES_2)

	CPPFLAGS += -DMALI_GLES_2_MINOR_VERSION=$(MALI_GLES_2_MINOR_VERSION)

	ALL_LIBS += 	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_GLES20_LIB_VERSIONED) $(MALI_GLES20_LIB_ALIASES))\
				$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_EGL_LIB_VERSIONED) $(MALI_EGL_LIB_ALIASES))

endif

RGB_IS_XRGB := 0
DISABLE_YCCA_53 := 0
ifneq ($(call is-feature-enabled,rgb_is_xrgb),)
	RGB_IS_XRGB := 1
	CPPFLAGS += -DRGB_IS_XRGB=$(RGB_IS_XRGB)
	CPPFLAGS += -DDISABLE_YCCA_53=$(DISABLE_YCCA_53)
endif

ifneq ($(call is-feature-enabled,gcc_tracing),)
# on Android add the flags separatelly
ifneq ($(call is-feature-enabled,android),)
	GCC_TRACING_FLAGS += -DMALI_GCC_TRACING -finstrument-functions
else
# on Linux, add the Mali tracing flags to CPPFLAGS
	CPPFLAGS += -DMALI_GCC_TRACING -finstrument-functions
endif

endif

ifdef USE_OPENGLES_1
	ifdef USE_OPENGLES_2
		USE_OPENGLES_1_AND_2 = 1
	endif
endif

ifneq ($(call is-feature-enabled,vg),)
	USE_OPENVG := 1
endif
ifneq ($(call is-feature-enabled,vg10),)
	USE_OPENVG := 1
endif

ifeq ($(USE_OPENVG),1)
	ALL_LIBS += 	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_OPENVG_LIB)) \
				$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_OPENVGU_LIB)) \
				$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_EGL_LIB_VERSIONED) $(MALI_EGL_LIB_ALIASES) )
	CPPFLAGS += -DMALI_USE_OPENVG
endif

ifneq ($(call is-feature-enabled,instrumented),)
MALI_INSTRUMENTED := TRUE
endif
ifeq ($(MALI_PERFORMANCE_COUNTERS_ENABLED),1)
MALI_INSTRUMENTED := TRUE
endif

ifeq ($(MALI_INSTRUMENTED),TRUE)
	CPPFLAGS += -I$(DRIVER_DIR)/include/instrumented -I$(DRIVER_DIR)/include/noninstrumented/types
	CPPFLAGS += -DMALI_INSTRUMENTED=1
	CPPFLAGS += -DMALI_DUMP_ENABLE -DMALI_MEMORY_PROFILING
	MALI_API_TRACE := TRUE
	CPPFLAGS += -DMALI_API_TRACE=1
else
	CPPFLAGS += -I$(DRIVER_DIR)/include/noninstrumented -I$(DRIVER_DIR)/include/noninstrumented/types
	CPPFLAGS += -DMALI_INSTRUMENTED=0
	ifneq ($(call is-feature-enabled,mem_profiling),)
		CPPFLAGS += -DMALI_MEMORY_PROFILING
	endif
	MALI_API_TRACE := FALSE
	CPPFLAGS += -DMALI_API_TRACE=0
endif

ifneq ($(call is-feature-enabled,oprofile_sampling),)
	MALI_OPROFILE_SAMPLING := TRUE
	CPPFLAGS += -DMALI_OPROFILE_SAMPLING=1
else
	MALI_OPROFILE_SAMPLING := FALSE
	CPPFLAGS += -DMALI_OPROFILE_SAMPLING=0
endif

ifneq ($(call is-feature-enabled,perfkit),)
	MALI_PERFORMANCE_KIT := TRUE
	CPPFLAGS += -DMALI_PERFORMANCE_KIT=1
else
	MALI_PERFORMANCE_KIT := FALSE
	CPPFLAGS += -DMALI_PERFORMANCE_KIT=0
endif

ifneq ($(call is-feature-enabled,ds_trace),)
	CPPFLAGS += -DMALI_DS_TRACE=1
else
	CPPFLAGS += -DMALI_DS_TRACE=0
endif

# Logic for the (M)ali (R)emote (I)nterface
# Only enables MRI in internal builds with instrumentation enabled
MALI_USING_MRI?=FALSE
ifdef ARM_INTERNAL_BUILD
	ifeq ($(MALI_INSTRUMENTED),TRUE)
		ifneq ($(call is-feature-enabled,mri),)
			MALI_USING_MRI:=TRUE
		endif
	endif
endif

# enable testbench only if it exists
# nexell make testbench disable
#ifneq ($(wildcard testbench/Makefile),) 
#	MALI_TEST_BENCH := 1
#else
	MALI_TEST_BENCH := 0
#endif

ifneq ($(call is-feature-enabled,test),)
	MALI_TEST_API := 1
	CPPFLAGS += -DMALI_TEST_API
endif

# Disables the shadergenerator, effectively disabling GLES1.1 support
ifneq ($(call is-feature-enabled,no_shadergen),)
	NO_SHADERGEN_SUPPORT_FEATURE :=1
endif

# Disables the online ESSL compiler, effectively disabling glCompileShader in GLES2
ifneq ($(call is-feature-enabled,no_essl),)
	NO_ESSL_SUPPORT_FEATURE := 1
endif

ifneq ($(call is-feature-enabled,eglswapregion),)
	EGL_FEATURE_SWAP_REGION_ENABLE := 1
	CPPFLAGS += -DEGL_FEATURE_SWAP_REGION_ENABLE=1
endif

# External sync/
ifneq ($(call is-feature-enabled,jellybean_mr1),)
	CPPFLAGS += -DMALI_EXTERNAL_SYNC=1
	MALI_EXTERNAL_SYNC=1
endif

lib :  $(ALL_LIBS)

ifneq ($(call is-feature-enabled,android),)
ifneq ($(MALI_TEST_BENCH),0)
CPPFLAGS += -DUSE_TPI
USE_TPI=1
TPI_SEARCH_PATH=testbench/tpi
# has to be 1 if building tests, otherwise undefined (because in makefile_builder.mak, 
# $(if) is used for checking the state of this flag)
MALI_MAKE_SUITE=1
endif
-include $(BUILD_SYSTEM_DIR)/android/makefile_builder.mak
endif

$(LIB_SYMLINK)/%: $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(LIB_DIR))/% ;
$(BIN_SYMLINK)/%: $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(BIN_DIR))/% ;
$(DEVDRV_SYMLINK)/%: $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(DEVDRV_DIR))/% ;
$(TMP_PRODUCT_DIR)/%: $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(TMP_PRODUCT_DIR))/% ;

CLEAN_FILES += $(LIB_SYMLINK) $(BIN_SYMLINK) $(DEVDRV_SYMLINK)  output.binshader $(filter /%,$(LIB_DIR) $(BIN_DIR) $(DEVDRV_DIR))
CLEAN_FILES += Android.mk egl.cfg _mali_build_info.c

include $(SRC_DIR)/Makefile
include $(SRC_DIR)/mali.mak

ifeq ($(MALI_TEST_BENCH),1)
	TESTBENCH_DIR=$(DRIVER_DIR)/testbench
	-include $(TESTBENCH_DIR)/Makefile
endif

ifeq ($(MALI_ANDROID),1)
ifeq ($(MALI_TEST_BENCH),1)
CPPFLAGS += -I$(TESTBENCH_DIR)
endif
add-define = $(eval MALI_DEFINES+=$1)
add-search-path = $(eval MALI_SEARCH_PATH+=$(patsubst -I./%,%,$1))
parse-item = $(if $(filter -I%,$1),$(call add-search-path,$1),$(if $(filter -D%,$1),$(call add-define,$1), $(if $(filter -f%,$1),$(call add-define,$1),$(error Unknown CPPFLAG $1))))
$(foreach item,$(CPPFLAGS),$(call parse-item,$(item)))

add-link-define = $(eval MALI_LINK_DEFINES+=$1)
parse-link-item = $(if $(filter -f%, $1), $(call add-link-define, $1), $(error Unknown LDFLAG $1))
$(foreach item,$(LDFLAGS),$(call parse-link-item,$(item)))
select_default : Android.mk egl.cfg
else
select_default : real_all
endif

real_all: lib $(EXTRA_ALL_DEPS)

distclean: clean

COVERAGE_DIR=$(DRIVER_DIR)/coverage
-include $(COVERAGE_DIR)/Makefile

.PHONY: docs doxygen doxygen_uddapi_only doxygen_umpapi_only doxygen_eglapi_only

# 'doxygen' target creates both ./doxygen, ./doxygen_uddapi_only, ./doxygen_umpapi_only and ./doxygen_eglapi_only
# NOTE: ./doxygen contains both source and header comments for UDD APIs, which
# may produce confusing documentation! Instead, use doxygen_uddapi_only.
#
# 'doxygen_uddapi_only' target only creates ./doxygen_uddapi_only
# 'doxygen_umpapi_only' target only creates ./doxygen_umpapi_only
# 'doxygen_pmmapi_only' target only creates ./doxygen_pmmapi_only

docs: doxygen_uddapi_only doxygen_umpapi_only doxygen_pmmapi_only doxygen_eglapi_only
	doxygen Doxyfile
doxygen: docs

DOXYGEN_UDDAPI_DIR=doxygen_uddapi_only
doxygen_uddapi_only:
	doxygen Doxyfile_uddapi_only
	@echo "Replacing $(DOXYGEN_UDDAPI_DIR)/html/index.html ..."
	rm -f "$(DOXYGEN_UDDAPI_DIR)/html/index.html"
	cp -a "$(DOXYGEN_UDDAPI_DIR)/html/modules.html" "$(DOXYGEN_UDDAPI_DIR)/html/index.html"

DOXYGEN_UMPAPI_DIR=doxygen_umpapi_only
doxygen_umpapi_only:
	doxygen Doxyfile_umpapi_only
	@echo "Replacing $(DOXYGEN_UMPAPI_DIR)/html/index.html ..."
	rm -f "$(DOXYGEN_UMPAPI_DIR)/html/index.html"
	cp -a "$(DOXYGEN_UMPAPI_DIR)/html/modules.html" "$(DOXYGEN_UMPAPI_DIR)/html/index.html"

DOXYGEN_PMMAPI_DIR=doxygen_pmmapi_only
doxygen_pmmapi_only:
	doxygen Doxyfile_pmmapi_only
	@echo "Replacing $(DOXYGEN_PMMAPI_DIR)/html/index.html ..."
	rm -f "$(DOXYGEN_PMMAPI_DIR)/html/index.html"
	cp -a "$(DOXYGEN_PMMAPI_DIR)/html/modules.html" "$(DOXYGEN_PMMAPI_DIR)/html/index.html"

DOXYGEN_EGLAPI_DIR=doxygen_eglapi_only
doxygen_eglapi_only:
	doxygen Doxyfile_eglapi_only
	@echo "Replacing $(DOXYGEN_EGLAPI_DIR)/html/index.html ..."
	rm -f "$(DOXYGEN_EGLAPI_DIR)/html/index.html"
	cp -a "$(DOXYGEN_EGLAPI_DIR)/html/modules.html" "$(DOXYGEN_EGLAPI_DIR)/html/index.html"

.PHONY: check-syntax
check-syntax: $(wildcard $(CHK_SOURCES) $(addprefix */,$(CHK_SOURCES)))
	LC_ALL=C gcc $(HOST_CFLAGS) $(CPPFLAGS) -Wextra -fsyntax-only $< 2>&1 | sed s,^$(dir $<),, 1>&2

todo:
	egrep -ri 'note|todo|fixme' * | egrep -v '~:|.svn-base:|Doxyfile:'

.PHONY: targets
targets:
	@# Grep the makefiles for the important lines, and then echo them seperately so that variable expansion takes place
	$(eval TARGET_LIST=$(shell find -name Makefile -print0 | egrep -zv '~:|.svn-base:|Doxyfile:' | xargs -0 egrep -h --only-matching "call make-[-a-zA-Z]*,[^,]*" | cut -d , -f 2))
	@echo "$(TARGET_LIST)" | sed -e "s/ /\n/g" | sort | uniq

-include internal/regression_server/submit_jobs.mak
ifneq ($(MALI_ANDROID),1)
include $(BUILD_SYSTEM_DIR)/Makefile.dep
endif

endif	# skip-makefile
