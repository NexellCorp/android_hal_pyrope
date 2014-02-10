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

ifneq ($(call is-feature-enabled,no_mali),)
CPPFLAGS += -DNO_HARDWARE
endif

TESTBENCH_BASE_API_DIR_NEW ?= .

CPPFLAGS += -I$(TESTBENCH_DIR) -I$(DRIVER_DIR)/include/regs/MALI200 -I$(TESTBENCH_BASE_DIR)/common_pwrmgmt -I$(TESTBENCH_BASE_API_DIR)/pp_job

UNIT_TEST_SRCS_COMMON = 	\
	$(TMP_PRODUCT_DIR)/libunit_framework$(LIB_EXT) $(TMP_PRODUCT_DIR)/libtpi$(LIB_EXT)

API_TESTS_SRCS_COMMON := \
	$(TESTBENCH_BASE_API_DIR)/util/base_test_util.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/base_test_gp_util.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/base_test_pp_util_200.c \

API_TESTS_SRCS_BASE = \
	$(TESTBENCH_BASE_API_DIR)/atomic/at01.c \
	$(TESTBENCH_BASE_API_DIR)/atomic/at02.c \
	$(TESTBENCH_BASE_API_DIR)/atomic/at03.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf01.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf02.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf03.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf04.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf05.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf06.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf08.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf09.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf14.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf15.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf20.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mp01.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mp06.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf01.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf02.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf03.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf04.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf06.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf07.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf08.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf09.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf10.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf11.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf12.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf13.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf14.c \
	$(TESTBENCH_BASE_API_DIR)/runtime/file.c \
	$(TESTBENCH_BASE_API_DIR)/runtime/test_base_runtime_suv3.c

ifeq ($(MALI_EXTERNAL_SYNC),1)
API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/sync/stream.c
endif

API_TESTS_SRCS_STANDALONE =



# include the correct helpers
# helpers depend on the original, if they are being built

ifeq ($(MALI_ARCH),arch_999_no_mali)
API_TESTS_SRCS_COMMON += $(TESTBENCH_BASE_API_DIR)/helpers_linux_standalone.c
ifneq ($(USE_OPENGLES)$(USE_OPENVG),)
$(call source-to-object,$(TARGET_SYSTEM),$(TESTBENCH_BASE_API_DIR)/helpers_linux_standalone.c):$(TESTBENCH_API_TESTS_DIR)/egl/applications/egl_helpers_fbdev_standalone.c
endif
endif

ifeq ($(UDD_OS),linux)
API_TESTS_SRCS_COMMON += $(TESTBENCH_BASE_API_DIR)/helpers_linux_standalone.c
ifneq ($(USE_OPENGLES)$(USE_OPENVG),)
$(call source-to-object,$(TARGET_SYSTEM),$(TESTBENCH_BASE_API_DIR)/helpers_linux_standalone.c):$(TESTBENCH_API_TESTS_DIR)/egl/applications/egl_helpers_$(EGL_BACKEND)_standalone.c
endif
endif

API_TESTS_SRCS_PLATFORM = \
	$(TESTBENCH_BASE_API_DIR)/memory/mf01.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf11.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf01.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/pf02.c \


API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs01.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs02.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs03.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs04.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs05.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs06.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs07.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs08.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs09.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs10.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu01.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu02.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu03.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu04.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu05.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu06.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu08.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfc01.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfc02.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfc03.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfc04.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfc05.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfc06.c \
	$(TESTBENCH_BASE_API_DIR)/pp_and_gp/fc01.c \
	$(TESTBENCH_BASE_API_DIR)/pp_and_gp/fc02.c \
	$(TESTBENCH_BASE_API_DIR)/pp_and_gp/fc03.c \
	$(TESTBENCH_BASE_API_DIR)/dependency_system/dependency01.c \

API_TESTS_SRCS_PLATFORM += $(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs01.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfvs04.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu04.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/base_test_gp_util.c \
	$(TESTBENCH_BASE_API_DIR)/util/base_test_util.c \
	$(TESTBENCH_BASE_API_DIR)/pp_job/base_test_pp_util_200.c

ifdef USING_MALI200
	ifeq ($(call is-feature-enabled,r0p1),)
	API_TESTS_SRCS_BASE += \
		$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu09.c \
		$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu10.c \
		$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu11.c \
		$(TESTBENCH_BASE_API_DIR)/memory/mf13.c
	endif
endif

ifdef USING_MALI400
API_TESTS_SRCS_BASE += \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu09.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu10.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu11.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf13.c
endif

ifdef USING_MALI450
API_TESTS_SRCS_BASE += \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu09.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu10.c \
	$(TESTBENCH_BASE_API_DIR)/gp_job/bgfplbu11.c \
	$(TESTBENCH_BASE_API_DIR)/memory/mf13.c
endif

ifdef MALI_USE_DMA_BUF
API_TESTS_SRCS_BASE += \
	$(TESTBENCH_BASE_API_DIR)/memory/dma_buf.c \

API_TESTS_SRCS_COMMON += $(TESTBENCH_BASE_API_DIR)/memory/mali_base_apitest_umm_telib.c
endif

ifdef MALI_USE_UNIFIED_MEMORY_PROVIDER
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump01.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump02.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump03.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump04.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump05.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump06.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump07.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump08.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump09.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump10.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump11.c

# This test is diabled for now since it requires a kernel with ION support
# (i.e. Android kernel >= 3.0)
#	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump_ion01.c
#	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump_ion02.c
#	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump_ion03.c

ifneq ($(UDD_OS),windowsce)
# Cached UMP memory supported on Linux only
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump_cached_mali01.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump_cached_mali02.c
	API_TESTS_SRCS_BASE += $(TESTBENCH_BASE_API_DIR)/memory/ump_cached_mali03.c
endif
    API_TESTS_SRCS_STANDALONE += $(TESTBENCH_BASE_API_DIR)/memory/base_api_standalone_app.c
endif


API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/performance/perf01_inlining.c

API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_test_api.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_suite.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_context.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_memory.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_job_gp.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_job_pp.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_job_pp_and_gp.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_sync.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_runtime_file.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_runtime_usleep.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_runtime_memory.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_scheduling.c
ifdef MALI_USE_UNIFIED_MEMORY_PROVIDER
	API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_fork.c
endif
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_runtime_tls.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_runtime_config.c
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/thread/base_thread_create.c

ifdef MALI_USE_DMA_BUF
API_TESTS_SRCS_PERFORMANCE += $(TESTBENCH_BASE_API_DIR)/memory/dma_buf_performance.c
endif


PMM_TESTS_SRCS_BASE += $(TESTBENCH_BASE_DIR)/common_pwrmgmt/mali_test_common_pwrmgmt.c \
	$(TESTBENCH_BASE_API_DIR)/powermgmt/power01.c \
	$(TESTBENCH_BASE_API_DIR)/powermgmt/power02.c \
	$(TESTBENCH_BASE_API_DIR)/powermgmt/power03.c \
	$(TESTBENCH_BASE_API_DIR)/powermgmt/power04.c \
	$(TESTBENCH_BASE_API_DIR)/powermgmt/power05.c \
	$(TESTBENCH_BASE_API_DIR)/powermgmt/power06.c \
	$(TESTBENCH_BASE_API_DIR)/powermgmt/power07.c \
	$(TESTBENCH_BASE_API_DIR)/powermgmt/power08.c \

ifdef ARM_INTERNAL_BUILD
ifndef MALI_ANDROID
ALL_TESTS += $(BIN_DIR)/base_api_main_suite$(EXE_EXT)
ALL_TESTS += $(BIN_DIR)/base_performance_suite$(EXE_EXT)

ifneq ($(call is-feature-enabled,linux),)
ALL_TESTS += $(BIN_DIR)/base_api_pmm_suite$(EXE_EXT)
endif

endif # MALI_ANDROID
endif # ARM_INTERNAL_BUILD

ALL_TESTS += $(BIN_DIR)/platform_test_suite$(EXE_EXT)

#* Create a standalone application for doing multiple/child process testing (E.g. UMP)
ifdef ARM_INTERNAL_BUILD

ifndef MALI_ANDROID

ifdef MALI_USE_UNIFIED_MEMORY_PROVIDER
ALL_TESTS += $(BIN_DIR)/base_api_standalone_app$(EXE_EXT)

$(call make-target-binary,$(BIN_DIR)/base_api_standalone_app$(EXE_EXT),$(API_TESTS_SRCS_STANDALONE)\
		$(MALI_LIB) $(MALI_UMP_LIB) $(OS_LIBS), MULTITHREADED)

$(BIN_DIR)/base_api_main_suite$(EXE_EXT): $(BIN_DIR)/base_api_standalone_app$(EXE_EXT)
endif

$(call make-target-binary,$(BIN_DIR)/base_api_main_suite$(EXE_EXT),$(API_TESTS_SRCS_COMMON) $(API_TESTS_SRCS_BASE) \
		$(TESTBENCH_BASE_API_DIR)/main_suite.c $(UNIT_TEST_SRCS_COMMON) $(MALI_LIB) $(MALI_UMP_LIB) $(OS_LIBS), MULTITHREADED)

$(call make-target-binary,$(BIN_DIR)/base_performance_suite$(EXE_EXT),$(API_TESTS_SRCS_COMMON) $(API_TESTS_SRCS_PERFORMANCE) \
		$(TESTBENCH_BASE_API_DIR)/performance_suite.c $(UNIT_TEST_SRCS_COMMON) $(MALI_LIB) $(MALI_UMP_LIB) $(OS_LIBS), MULTITHREADED)

$(call make-target-binary,$(BIN_DIR)/base_api_pmm_suite$(EXE_EXT),$(API_TESTS_SRCS_COMMON) $(PMM_TESTS_SRCS_BASE) \
		$(TESTBENCH_BASE_API_DIR)/pmm_suite.c $(UNIT_TEST_SRCS_COMMON) $(MALI_LIB) $(OS_LIBS), MULTITHREADED)

endif # MALI_ANDROID

endif # ARM_INTERNAL_BUILD

$(call make-target-binary,$(BIN_DIR)/platform_test_suite$(EXE_EXT),$(API_TESTS_SRCS_PLATFORM) \
		$(TESTBENCH_BASE_API_DIR)/main_suite_platform.c $(UNIT_TEST_SRCS_COMMON) $(MALI_LIB), MULTITHREADED)
