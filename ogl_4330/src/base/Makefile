#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2007-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

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

TOOLCHAIN_SPECIFIC_BASE_SRCS=

BASE_DIR ?= .

BASE_FRONTEND_SRCS = \
	$(BASE_DIR)/frontend/mali_runtime.c \
	$(BASE_DIR)/common/base_common_sync_handle.c \
	$(BASE_DIR)/common/mem/base_common_mem.c \
	$(BASE_DIR)/common/pp/base_common_pp_job.c \
	$(BASE_DIR)/common/base_common_context.c \
	$(BASE_DIR)/common/dependency_system/base_common_ds.c \
	$(BASE_DIR)/common/gp/base_common_gp_job.c \
	$(BASE_DIR)/common/gp/base_common_gp_cmdlist.c \
	$(BASE_DIR)/frontend/mali_runtime_stdarg.c \
	$(BASE_DIR)/common/base_common_byteorder.c \
	$(BASE_DIR)/common/base_common_worker.c \
	$(BASE_DIR)/__base_build_info.c

ifneq ($(call is-feature-enabled,gcc_tracing),)
BASE_FRONTEND_SRCS += $(BASE_DIR)/frontend/mali_function_tracing.c
endif

ifeq ($(MALI_INSTRUMENTED),TRUE)
BASE_FRONTEND_SRCS += \
	$(BASE_DIR)/common/instrumented/base_common_pp_instrumented.c \
	$(BASE_DIR)/frontend/instrumented/mali_pp_instrumented_frontend.c \
	$(BASE_DIR)/common/instrumented/base_common_gp_instrumented.c \
	$(BASE_DIR)/frontend/instrumented/mali_gp_instrumented_frontend.c \
	$(BASE_DIR)/common/dump/base_common_dump_mem.c \
	$(BASE_DIR)/common/dump/base_common_dump_jobs.c \
	$(BASE_DIR)/common/dump/base_common_dump_pp.c \
	$(BASE_DIR)/common/dump/base_common_dump_gp.c
endif

ifneq ($(call is-feature-enabled,test),)
BASE_FRONTEND_SRCS += \
	$(BASE_DIR)/test/mali_mem_test.c \
	$(BASE_DIR)/test/mali_test_core.c
endif

ifneq ($(call is-feature-enabled,no_mali),)

	BASE_ARCH_SRCS = \
		$(BASE_DIR)/arch/arch_999_no_mali/base_arch_runtime.c \
		$(BASE_DIR)/arch/arch_999_no_mali/base_arch_mem.c \
		$(BASE_DIR)/arch/arch_999_no_mali/base_arch_pp.c \
		$(BASE_DIR)/arch/arch_999_no_mali/base_arch_profiling.c \
		$(BASE_DIR)/arch/common/base_runtime_suv3.c \
		$(BASE_DIR)/arch/common/base_runtime_pthreads.c \
		$(BASE_DIR)/arch/arch_999_no_mali/base_arch_gp.c \
		$(BASE_DIR)/os/linux/mali_osu_thread.c

	CPPFLAGS += -I$(BASE_DIR)/platform/linux/gcc-i386
	CPPFLAGS += -I$(BASE_DIR)/platform/common/gcc-i386
	CPPFLAGS += -I$(BASE_DIR)/platform/linux/common
	CPPFLAGS += -I$(BASE_DIR)/platform/common/neon
	CPPFLAGS += -I$(BASE_DIR)/platform/common/common
	CPPFLAGS += -I$(BASE_DIR)/os
	CPPFLAGS += -I$(BASE_DIR)/os/linux

	CPPFLAGS += -I$(MALI_DD_INCLUDE_DIR)/linux/mali

else

	BASE_ARCH_SRCS =

	ifeq ($(findstring arm-linux-gcc, $(TARGET_TOOLCHAIN)),arm-linux-gcc)
	UDD_PLATFORM=gcc-arm
	else
	ifeq ($(TARGET_TOOLCHAIN),gcc32)
	UDD_PLATFORM=gcc-i386
	else
	ifeq ($(findstring armcc, $(TARGET_TOOLCHAIN)),armcc)
	UDD_PLATFORM=realview
	else
	ifeq ($(findstring mips, $(TARGET_TOOLCHAIN)),mips)
	UDD_PLATFORM=gcc-mips
	else
	ifeq ($(findstring mipsel, $(TARGET_TOOLCHAIN)),mipsel)
	UDD_PLATFORM=gcc-mips
	else
	ifeq ($(findstring msvc-ce6, $(TARGET_TOOLCHAIN)),msvc-ce6)
	UDD_PLATFORM=msvc-arm
	else
	ifeq ($(findstring msvc-ce7, $(TARGET_TOOLCHAIN)),msvc-ce7)
	UDD_PLATFORM=msvc-arm
	else
	ifeq ($(findstring mipsii-msvc-ce6, $(TARGET_TOOLCHAIN)),mipsii-msvc-ce6)
	UDD_PLATFORM=msvc-mips
	else
	ifeq ($(findstring sh4, $(TARGET_TOOLCHAIN)),sh4)
	UDD_PLATFORM=gcc-intrinsics
	else
	#$(error TARGET_TOOLCHAIN=$(TARGET_TOOLCHAIN) is not supported)
	UDD_PLATFORM=gcc-arm
	endif
	endif
	endif
	endif
	endif
	endif
	endif
	endif
	endif

	# Platform-OS specific sources
	ifeq ($(UDD_OS),linux)
	BASE_ARCH_SRCS += \
		$(wildcard $(BASE_DIR)/platform/$(UDD_OS)/$(UDD_PLATFORM)/mali_osu_atomics.S) \
		$(wildcard $(BASE_DIR)/platform/$(UDD_OS)/$(UDD_PLATFORM)/mali_osu_atomics.s) \
		$(wildcard $(BASE_DIR)/platform/$(UDD_OS)/$(UDD_PLATFORM)/mali_osu_atomics.c)
	else
	BASE_ARCH_SRCS += \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_atomics.c
	endif

	# Common NEON sources. Only if vfp and neon specified.
	ifeq ($(MALI_PLATFORM_ARM_NEON),1)
		BASE_ARCH_SRCS += \
			$(BASE_DIR)/platform/common/neon/mali_convert_intrinsics.c
		ifeq ($(findstring gcc, $(TARGET_TOOLCHAIN)),gcc)
			BASE_ARCH_SRCS += \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_8.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_16.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_24.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_32.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_24_to_32.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_premult_32.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_xrgb_32.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_downsample.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_math.S) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_vstrans_f32.S)
		else 
		ifeq ($(findstring armcc,$(TARGET_TOOLCHAIN)),armcc)
			BASE_ARCH_SRCS += \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_8.c) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_16.c) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_24.c) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_32.c) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_24_to_32.c) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_premult_32.c) \
			$(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_repack_xrgb_32.c) \
                        $(wildcard $(BASE_DIR)/platform/common/$(UDD_PLATFORM)/mali_neon_downsample.c)
		else
			$(warning "NEON repack not supported on toolchain "$(TARGET_TOOLCHAIN)" is not supported.")
		endif
		endif
	endif

	# OS Specific sources
	BASE_ARCH_SRCS += \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_locks.c \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_math.c \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_memory.c \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_misc.c \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_string.c \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_thread.c \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_file.c \
		$(BASE_DIR)/os/$(UDD_OS)/mali_osu_library.c \
		$(BASE_DIR)/os/$(UDD_OS)/mali_uku.c

ifeq ($(MALI_EXTERNAL_SYNC),1)
	BASE_ARCH_SRCS += \
		$(BASE_DIR)/os/$(UDD_OS)/sync/mali_external_sync.c \
		$(BASE_DIR)/arch/arch_011_udd/base_arch_external_sync.c
endif

	# Common sources

	BASE_ARCH_SRCS += \
		$(BASE_DIR)/arch/arch_011_udd/timer/$(MALI_TIMER_BACKEND)/base_arch_timer.c

	BASE_ARCH_SRCS += \
		$(BASE_DIR)/arch/arch_011_udd/base_arch_main.c \
		$(BASE_DIR)/arch/arch_011_udd/base_arch_mem.c \
		$(BASE_DIR)/arch/arch_011_udd/base_arch_virtual_mem.c \
		$(BASE_DIR)/arch/arch_011_udd/base_arch_pp.c \
		$(BASE_DIR)/arch/arch_011_udd/base_arch_runtime.c \
		$(BASE_DIR)/arch/arch_011_udd/base_arch_threads.c\
		$(BASE_DIR)/arch/arch_011_udd/base_arch_atomic.c \
		$(BASE_DIR)/arch/arch_011_udd/base_arch_vsync.c

# base_arch_profiling is needed both for timeline and for sw counters.
ifeq ($(MALI_TIMELINE_PROFILING_ENABLED),1)
	BASE_ARCH_SRCS += $(BASE_DIR)/arch/arch_011_udd/base_arch_profiling.c
else
ifeq ($(MALI_SW_COUNTERS_ENABLED),1)
	BASE_ARCH_SRCS += $(BASE_DIR)/arch/arch_011_udd/base_arch_profiling.c
endif
endif

ifeq ($(MALI_FRAMEBUFFER_DUMP_ENABLED),1)
	BASE_ARCH_SRCS += $(BASE_DIR)/os/$(UDD_OS)/mali_osu_annotate.c
endif

	CPPFLAGS += -I$(BASE_DIR)/os -I$(MALI_DD_INCLUDE_DIR)/$(UDD_OS)/mali
	CPPFLAGS += -I$(BASE_DIR)/os/$(UDD_OS)
	CPPFLAGS += -I$(BASE_DIR)/platform/$(UDD_OS)/$(UDD_PLATFORM)
	CPPFLAGS += -I$(BASE_DIR)/platform/common/$(UDD_PLATFORM)
	CPPFLAGS += -I$(BASE_DIR)/platform/$(UDD_OS)/common
	CPPFLAGS += -I$(BASE_DIR)/platform/common/neon
	CPPFLAGS += -I$(BASE_DIR)/platform/common/common

	BASE_ARCH_SRCS += \
			$(BASE_DIR)/arch/arch_011_udd/base_arch_gp.c

	# For WindowsCE, we initially specify a .def file.
	# This will be removed when we've made everything __declspec(dllexport/import)
	ifeq ($(UDD_OS),windowsce)
		TOOLCHAIN_SPECIFIC_BASE_SRCS += $(call source-dir-to-binary-dir, $(TARGET_SYSTEM), $(BASE_DIR)/libMali.def)
		ifeq ($(MALI_INSTRUMENTED),TRUE)
			TOOLCHAIN_SPECIFIC_BASE_SRCS += \
				$(_PROJECTROOT)/cesysgen/sdk/lib/$(_TGTCPU)/$(WINCEDEBUG_TARGET)/ws2.lib
		endif
	endif

endif

ifneq ($(call is-feature-enabled,no_mali),)

ifeq ($(findstring arm-linux-gcc, $(TARGET_TOOLCHAIN)),arm-linux-gcc)
BASE_ARCH_SRCS += \
        $(BASE_DIR)/arch/common/gcc-arm/base_runtime.S
else
ifeq ($(TARGET_TOOLCHAIN),gcc32)
BASE_ARCH_SRCS += \
        $(BASE_DIR)/arch/common/gcc-i386/base_runtime.S
else
ifeq ($(TARGET_TOOLCHAIN),solaris-gcc)
BASE_ARCH_SRCS += \
        $(BASE_DIR)/arch/common/gcc-solaris/base_runtime.c
else
ifeq ($(findstring armcc, $(TARGET_TOOLCHAIN)),armcc)
BASE_ARCH_SRCS += \
        $(BASE_DIR)/arch/common/realview/base_runtime.S
else
#$(error TARGET_TOOLCHAIN=$(TARGET_TOOLCHAIN) is not supported)
BASE_ARCH_SRCS += \
        $(BASE_DIR)/arch/common/gcc-arm/base_runtime.S
endif
endif
endif
endif

endif

# Include byteorder aware fucntions if building for big endian platform.
ifneq ($(MALI_BIG_ENDIAN),)
	CPPFLAGS += -I$(BASE_DIR)/platform/$(UDD_OS)/$(UDD_PLATFORM)
	BASE_ARCH_SRCS += \
		$(BASE_DIR)/platform/$(UDD_OS)/$(UDD_PLATFORM)/base_arch_byteorder_be.c
endif

BASE_CORE_SRCS += \
	$(BASE_DIR)/cores/MALI200/core_mali_200.c \
	$(BASE_DIR)/cores/MALIGP2/core_gp2.c

BASE_SRCS = $(BASE_FRONTEND_SRCS) $(BASE_ARCH_SRCS) $(BASE_CORE_SRCS)

# autogenerated build version info
$(call make-build-info-file,$(BASE_DIR)/__base_build_info.c,base,$(BASE_DIR),$(BASE_SRCS),BASE_MALI_ARCH="$(MALI_ARCH)")
