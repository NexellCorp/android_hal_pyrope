#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2001-2002, 2007-2012 ARM Limited
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

TESTBENCH_API_TESTS_DIR ?= .

EGL_API_TESTS_INTEROPERABILITY := 0

ifdef MALI_ANDROID
HELPER_EXT := cpp
else
HELPER_EXT := c
endif

EGL_HELPER_BACKEND := $(EGL_BACKEND)

ifeq ($(EGL_BACKEND),dummy)
	EGL_HELPER_BACKEND := fbdev
endif

ifdef USE_OPENVG
	TESTBENCH_OPENVG_API_TESTS_DIR=$(TESTBENCH_API_TESTS_DIR)/openvg
	-include $(TESTBENCH_OPENVG_API_TESTS_DIR)/Makefile

	TESTBENCH_OPENVG_SMP_API_TESTS_DIR=$(TESTBENCH_API_TESTS_DIR)/openvg_smp
	-include $(TESTBENCH_OPENVG_SMP_API_TESTS_DIR)/Makefile

	# Force OpenGL ES 2 tests if both USE_OPENGLES_1 and USE_OPENGLES_2 are defined
	ifdef USE_OPENGLES_2
		EGL_API_TESTS_INTEROPERABILITY=2
	else
		ifdef USE_OPENGLES_1
			EGL_API_TESTS_INTEROPERABILITY=1
		endif
	endif
endif

# Force OpenGL ES 2 tests if both USE_OPENGLES_1 and USE_OPENGLES_2 are defined
ifdef USE_OPENGLES_2
	CPPFLAGS += -DMALI_EGL_GLES_MAJOR_VERSION=2
else
	ifdef USE_OPENGLES_1
		CPPFLAGS += -DMALI_EGL_GLES_MAJOR_VERSION=1
	endif
endif


#Add a flag to not test gles1 in gles2 if (USE_OPENGLES_1_AND_2 NAND MALI_MONOLITHIC)
ifndef USE_OPENGLES_1_AND_2
	CPPFLAGS += -DNO_GLES1_IN_GLES2_TESTS
endif
ifndef MALI_MONOLITHIC
	CPPFLAGS += -DNO_GLES1_IN_GLES2_TESTS
endif

# Define Platform-specific UMP pixmap support
UMP_PIXMAP_SUPPORT_SRCS=

-include $(TESTBENCH_API_TESTS_DIR)/opengles2/shaders/Makefile
-include $(TESTBENCH_API_TESTS_DIR)/egl/shaders/Makefile

EGL_HELPER_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers_pixmap.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers_$(EGL_HELPER_BACKEND).$(HELPER_EXT)

ifdef ARM_INTERNAL_BUILD
# Not needed for test in cusomer builds.
EGL_HELPER_SRCS += $(TESTBENCH_API_TESTS_DIR)/../base/base_api_tests/memory/mali_base_apitest_umm_telib.c
endif

API_TESTS_EGL_API_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_framework.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_test_parameters.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS)

ifdef ARM_INTERNAL_BUILD
ifeq ($(EGL_API_TESTS_INTEROPERABILITY), 0)
API_TESTS_EGL_API_SUITE_SRCS := $(wildcard $(TESTBENCH_API_TESTS_DIR)/egl/A*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/ST*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/P*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/B*/*.c)
endif
ifeq ($(EGL_API_TESTS_INTEROPERABILITY), 1)
API_TESTS_EGL_API_SUITE_SRCS := $(wildcard $(TESTBENCH_API_TESTS_DIR)/egl/A*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/INT_GLES11/*.c $(TESTBENCH_API_TESTS_DIR)/egl/ST*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/P*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/B*/*.c)
endif
ifeq ($(EGL_API_TESTS_INTEROPERABILITY), 2)
API_TESTS_EGL_API_SUITE_SRCS := $(wildcard $(TESTBENCH_API_TESTS_DIR)/egl/A*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/ST*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/P*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/B*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/INT/*.c)
endif       # ($(EGL_API_TESTS_INTEROPERABILITY), 2)

else        # ARM_INTERNAL_BUILD

ifeq ($(EGL_API_TESTS_INTEROPERABILITY), 0)
API_TESTS_EGL_API_SUITE_SRCS := $(wildcard $(TESTBENCH_API_TESTS_DIR)/egl/A*/*.c)
endif

ifeq ($(EGL_API_TESTS_INTEROPERABILITY), 1)
API_TESTS_EGL_API_SUITE_SRCS := $(wildcard $(TESTBENCH_API_TESTS_DIR)/egl/A*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/INT_GLES11/*.c )
endif

ifeq ($(EGL_API_TESTS_INTEROPERABILITY), 2)
API_TESTS_EGL_API_SUITE_SRCS := $(wildcard $(TESTBENCH_API_TESTS_DIR)/egl/A*/*.c $(TESTBENCH_API_TESTS_DIR)/egl/INT/*.c )
endif

endif    # ARM_INTERNAL_BUILD

API_TESTS_EGL_API_SRCS += \
	$(API_TESTS_EGL_API_SUITE_SRCS)

ifdef ARM_INTERNAL_BUILD
API_TESTS_EGL_DEFECT_SUITE_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/defects/wrong_usage.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/defects/randomized.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/defects/incorrect_mask.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/defects/pixmap.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/defects/destructor.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/defects/excessive_usage.c

# The following test only works if you build vg, gles1 and gles2

ifdef USE_OPENVG
ifdef USE_OPENGLES_1
ifdef USE_OPENGLES_2

API_TESTS_EGL_DEFECT_SUITE_SRCS += \
	$(TESTBENCH_API_TESTS_DIR)/egl/defects/gles1_gles2.c

endif
endif
endif


API_TESTS_EGL_DEFECT_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_framework.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/egl_defects_suite.c \
	$(API_TESTS_EGL_DEFECT_SUITE_SRCS)

API_TESTS_EGL_EXTENSIONS_SUITE_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG01_eglQueryString.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG02_eglGetProcAddress.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG03_eglCreateImageKHR_general.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG04_eglCreateImageKHR_pixmap.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG05_eglCreateImageKHR_vg.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG06_eglCreateImageKHR_gles.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG07_eglDestroyImageKHR.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG08_vgCreateEGLImageTargetKHR.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG09_glEGLImageTargetTexture2DOES.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG10_glEGLImageTargetRenderbufferStorageOES.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG11_VGImageGLES_overlay.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG12_GLESOpenVG_overlay.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG13_mixed_nativehardware_rendering.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG14_gles_cubemap_in_vg.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG15_image_format_promotion.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG16_vgscene_in_gles.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG19_general_stress_testing.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG20_multithread_vg_in_gles.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG21_multithread_gles_in_vg.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG22_multithread_mixed_rendering.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/NSR/NSR01_basic.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/ROB/ROB01_basic.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/pixmap.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/pixmap.h \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/extensions.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/extensions.h \
	$(TESTBENCH_API_TESTS_DIR)/helpers/egl_image_tools.c \
	$(TESTBENCH_API_TESTS_DIR)/helpers/yuv_texture_tools.$(HELPER_EXT)


# mali_egl_image not available on Android
# LCK and SYNC not working, GetClient and ExportImage uses unimplemented helpers
ifndef MALI_ANDROID
ifneq ($(call is-feature-enabled,ump),)
API_TESTS_EGL_EXTENSIONS_SUITE_SRCS += \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG28_eglGetClientNameKHR.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG29_eglExportImageKHR.c
endif

API_TESTS_EGL_EXTENSIONS_SUITE_SRCS += \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG25_eglCreateImageKHR_load_YUV_data.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/IMG/IMG27_mali_egl_image.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/LCK/LCK01_basic.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/SYNC/SYNC01_basic.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/SYNC/SYNC02_advanced.c
endif

ifdef MALI_ANDROID
API_TESTS_EGL_EXTENSIONS_SUITE_SRCS += \
	$(TESTBENCH_API_TESTS_DIR)/egl/SYNC/SYNC03_android_native_fence.cpp
endif

#Adding gl2 framework for yuv eglImage tests
ifdef USE_OPENGLES_2
	API_TESTS_EGL_EXTENSIONS_SUITE_SRCS += \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/gl2_framework.c
endif

API_TESTS_EGL_EXTENSIONS_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_framework.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_test_parameters.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/egl_extensions_suite.c \
	$(API_TESTS_EGL_EXTENSIONS_SUITE_SRCS)

API_TESTS_EGL_PERFORMANCE_SUITE_SRCS = $(shell ([ -d  $(TESTBENCH_API_TESTS_DIR)/egl/performance ]) && ls $(TESTBENCH_API_TESTS_DIR)/egl/performance/suites | sed "s,[^ ]*,$(TESTBENCH_API_TESTS_DIR)/egl/performance/suites/\0,g")

API_TESTS_EGL_PERFORMANCE_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl_performance_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_framework.c \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(EGL_HELPER_SRCS) \
	$(API_TESTS_EGL_PERFORMANCE_SUITE_SRCS)

endif

API_TESTS_GLES2_API_FRAMEWORK_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/gl2_framework_parameters.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/gl2_framework.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/gl2_framework_fixture_egl.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/gles_helpers.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/pixmap.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/gles_begin.c \
	$(TESTBENCH_API_TESTS_DIR)/helpers/egl_image_tools.c \
	$(TESTBENCH_API_TESTS_DIR)/helpers/api_tests_utils.c \
	$(TESTBENCH_API_TESTS_DIR)/helpers/yuv_texture_tools.$(HELPER_EXT)

API_TESTS_GLES1_API_FRAMEWORK_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/opengles1/gl1_framework_parameters.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles1/gl1_framework.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles1/gl1_framework_fixture_egl.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles1/gles_helpers.c \
	$(TESTBENCH_API_TESTS_DIR)/helpers/api_tests_utils.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles1/pixmap.c

# find all .c files in  ./opengles2/A*/ and fill the source file list to compile
API_TESTS_GLES2_API_SUITE_SRCS = $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/A*/*.c)

# shader texture LOD tests
API_TESTS_GLES2_API_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/LOD*/*.c)

# optionally add YUV test files, only a subset ported to android
ifdef MALI_ANDROID
API_TESTS_GLES2_API_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/Y*/YUV01_image_external.c)
API_TESTS_GLES2_API_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/Y*/YUV02_basic_yuv_texturing.c)
API_TESTS_GLES2_API_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/Y*/YUV03_multiple_yuv_textures.c)
API_TESTS_GLES2_API_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/Y*/YUV04_multiple_yuv_formats.c)
API_TESTS_GLES2_API_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/Y*/YUV05_multiple_yuv_ranges_and_spaces.c)
else
API_TESTS_GLES2_API_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/Y*/*.c)
endif

API_TESTS_GLES2_API_SUITE_SRCS += \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS)

API_TESTS_GLES2_API_SRCS = \
	$(API_TESTS_GLES2_API_FRAMEWORK_SRCS) \
	$(API_TESTS_GLES2_API_SUITE_SRCS)

#Monolithic test source
MONOLITHIC_TEST_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/MO*/*.c)
MONOLITHIC_TEST_SRCS = \
	$(API_TESTS_GLES2_API_FRAMEWORK_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/gles_monolithic_suite.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(MONOLITHIC_TEST_SUITE_SRCS)

API_TESTS_GLES2_MULTICORE_SRCS = \
	$(API_TESTS_GLES2_API_FRAMEWORK_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/gles2_multicore_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/multicore/cornercases.c \
	$(TESTBENCH_API_TESTS_DIR)/opengles2/multicore/uneven_load.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS)

API_TESTS_GLES2_DEFECT_SUITE_SRCS = $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/defects/*.c)
API_TESTS_GLES2_DEFECT_SRCS = \
	$(API_TESTS_GLES2_API_FRAMEWORK_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/gles2_defects_suite.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(API_TESTS_GLES2_DEFECT_SUITE_SRCS)

#White box tests
WHITEBOX_TEST_SUITE_SRCS += $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/whitebox/*.c)

WHITEBOX_TEST_SRCS = \
	$(API_TESTS_GLES2_API_FRAMEWORK_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/gles2_whitebox_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers_pixmap.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers_$(EGL_HELPER_BACKEND).$(HELPER_EXT) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(WHITEBOX_TEST_SUITE_SRCS)

# Stress tests suite
GLES2_STRESS_TESTS_EGL_SRCS += \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS)

GLES2_STRESS_TESTS_SUITE_SRCS = $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/stress/MT*/*.c)

GLES2_VG_SHARED_STRESS_TESTS_SUITE_SRCS = $(wildcard $(TESTBENCH_API_TESTS_DIR)/opengles2/stress/VG*/*.c)

GLES2_STRESS_TESTS_SRCS = \
	$(API_TESTS_GLES2_API_FRAMEWORK_SRCS) \
	$(GLES2_STRESS_TESTS_EGL_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/gles2_stress_suite.c \
	$(GLES2_STRESS_TESTS_SUITE_SRCS)

GLES2_VG_SHARED_STRESS_TESTS_SRCS = \
	$(API_TESTS_GLES2_API_FRAMEWORK_SRCS) \
	$(GLES2_STRESS_TESTS_EGL_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/gles2_vg_shared_stress_suite.c \
	$(GLES2_VG_SHARED_STRESS_TESTS_SUITE_SRCS)

# added directory test so that the makefile does not fail in the customer delivery
API_TESTS_GLES2_PERFORMANCE_SUITE_SRCS = $(shell ([ -d  $(TESTBENCH_API_TESTS_DIR)/opengles2/performance ]) && ls $(TESTBENCH_API_TESTS_DIR)/opengles2/performance/suites | sed "s,[^ ]*,$(TESTBENCH_API_TESTS_DIR)/opengles2/performance/suites/\0,g")

API_TESTS_GLES2_PERFORMANCE_SRCS = \
	$(API_TESTS_GLES2_API_FRAMEWORK_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/gles2_performance_suite.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(API_TESTS_GLES2_PERFORMANCE_SUITE_SRCS)

# added directory test so that the makefile does not fail in the customer delivery
API_TESTS_GLES1_PERFORMANCE_SUITE_SRCS = $(shell ([ -d  $(TESTBENCH_API_TESTS_DIR)/opengles1/performance ]) && ls $(TESTBENCH_API_TESTS_DIR)/opengles1/performance/suites | sed "s,[^ ]*,$(TESTBENCH_API_TESTS_DIR)/opengles1/performance/suites/\0,g")

API_TESTS_GLES1_PERFORMANCE_SRCS = \
	$(API_TESTS_GLES1_API_FRAMEWORK_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/gles1_performance_suite.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(API_TESTS_GLES1_PERFORMANCE_SUITE_SRCS)


API_TESTS_LINKER_LOADER = \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/linkload_testbench.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/linkload_load_data.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/linkload_block.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/linkload_testsystem.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/linkload_parameters.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/stack.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/randomization.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/no_permutations_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/rearranged_blocks_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/duplicate_blocks_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/missing_blocks_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/malsized_blocks_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/insert_junk_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/malformed_blocks_content_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/negative_symbol_location_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/functional_dbin_fins_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/linkerloader/corrupt_header_suite.c


API_TESTS_INSTRUMENTED_DEFECT_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/instrumented_defects_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/framework.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/defects/5302.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/defects/3635.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/defects/3905.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/defects/3980.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/defects/4218.c

API_TESTS_INSTRUMENTED_API_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/instrumented_api_suite.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/framework.c \
	$(EGL_HELPER_SRCS) \
	$(UMP_PIXMAP_SUPPORT_SRCS) \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/multicontext/imctx01.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/multicontext/imctx02.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/multijobs/imjobs01.c \
	$(TESTBENCH_API_TESTS_DIR)/instrumented/multicore/imcore01.c

ifdef ARM_INTERNAL_BUILD
$(TESTBENCH_API_TESTS_DIR)/gles2_performance/suite_list.c $(TESTBENCH_API_TESTS_DIR)/gles2_performance/suite_list.h : $(TESTBENCH_API_TESTS_DIR)/gles2_performance/suites
	cd $(TESTBENCH_API_TESTS_DIR)/gles2_performance/ && ./gen_suite_list.sh
endif

# some magic to build all api-test shaders in one go
ifdef USE_OPENGLES
	API_TESTS_GLES2_SHADER_SRCS := $(shell ls $(TESTBENCH_API_TESTS_DIR)/opengles2/shaders/*.glsl)
	API_TESTS_GLES2_SHADER_BINS := $(API_TESTS_GLES2_SHADER_SRCS:.glsl=.binshader)
.PHONY : gles2-api-test-shaders
gles2-api-test-shaders : $(API_TESTS_GLES2_SHADER_BINS)
endif

EGL_API_TESTS_SRCS = $(TESTBENCH_API_TESTS_DIR)/egl_main_suite.c

ifdef USE_OPENGLES_1
	API_TEST_LIBS := $(MALI_GLES11_LIB) $(MALI_EGL_LIB)
	ifdef ARM_INTERNAL_BUILD
		ifndef USE_OPENGLES_2
			ALL_TESTS += $(BIN_DIR)/gles1_performance_suite$(EXE_EXT)
		endif
	endif
endif

ifdef USE_OPENGLES_2
	API_TESTS_SRCS = $(TESTBENCH_API_TESTS_DIR)/gles2_api_main_suite.c
	API_TESTS_SRCS += $(API_TESTS_GLES2_API_SRCS)
	API_TEST_LIBS := $(MALI_GLES20_LIB) $(MALI_EGL_LIB)
	ALL_TESTS += $(BIN_DIR)/gles2_api_suite$(EXE_EXT)
ifdef ARM_INTERNAL_BUILD
	ALL_TESTS += $(BIN_DIR)/gles2_performance_suite$(EXE_EXT)
	ALL_TESTS += $(BIN_DIR)/gles2_defect_suite$(EXE_EXT)
	ALL_TESTS += $(BIN_DIR)/gles2_multicore_suite$(EXE_EXT)
	ALL_TESTS += $(BIN_DIR)/gles2_stress_suite$(EXE_EXT)
ifdef MALI_TEST_API
	ALL_TESTS += $(BIN_DIR)/gles2_whitebox_suite$(EXE_EXT)
endif
endif
endif

ifdef ARM_INTERNAL_BUILD

ifdef MALI_MONOLITHIC
ifdef USE_OPENGLES_2
ifdef USE_OPENVG
	ALL_TESTS += $(BIN_DIR)/gles2_vg_shared_stress_suite$(EXE_EXT)
endif
endif
endif

ifdef MALI_MONOLITHIC
ifdef USE_OPENGLES_1_AND_2
	ALL_TESTS += $(BIN_DIR)/gles_monolithic_suite$(EXE_EXT)
endif
endif

endif # ARM_INTERNAL_BUILD

ifdef USE_ESSL_COMPILER
#integrate main compiler test suite into the system the rest of the driver uses.
$(call get-binary-dir,$(TARGET_SYSTEM))/$(BIN_DIR)/malisc_main_suite$(EXE_EXT): $(call get-binary-dir,$(TARGET_SYSTEM))/utest
	cp $< $@

ALL_TESTS += $(BIN_DIR)/malisc_main_suite$(EXE_EXT)
endif

#EGL specific
EGL_API_TESTS_SRCS += $(API_TESTS_EGL_API_SRCS)
EGL_API_TEST_LIBS := $(MALI_EGL_LIB)
CPPFLAGS += -I$(TESTBENCH_API_TESTS_DIR)/egl/


ifdef USE_OPENVG
	API_TEST_LIBS += $(MALI_OPENVG_LIB) $(MALI_OPENVGU_LIB)
	EGL_API_TEST_LIBS += $(MALI_OPENVG_LIB) $(MALI_OPENVGU_LIB)
endif

ifdef USE_OPENGLES_1
	ifndef USE_OPENGLES_2
		ALL_TESTS += $(BIN_DIR)/egl_api_main_suite_11$(EXE_EXT)
	endif
	ifdef ARM_INTERNAL_BUILD
		ifndef USE_OPENGLES_2
			ALL_TESTS += $(BIN_DIR)/egl_extensions_suite_11$(EXE_EXT)
			ALL_TESTS += $(BIN_DIR)/egl_performance_suite_11$(EXE_EXT)
		endif
	endif
endif

ifdef USE_OPENGLES_2
	ALL_TESTS += $(BIN_DIR)/egl_api_main_suite_20$(EXE_EXT)

	ifdef ARM_INTERNAL_BUILD
		ifeq ($(MALI_INSTRUMENTED),TRUE)
			ALL_TESTS += $(BIN_DIR)/instrumented_defect_suite$(EXE_EXT) $(BIN_DIR)/instrumented_api_suite$(EXE_EXT)
		endif

		ALL_TESTS += $(BIN_DIR)/egl_extensions_suite_20$(EXE_EXT)
		ALL_TESTS += $(BIN_DIR)/egl_performance_suite_20$(EXE_EXT)
	endif
endif

ifeq ($(UDD_OS),windowsce)
	OS_TEST_LIBS = $(_PROJECTROOT)/cesysgen/sdk/lib/$(_TGTCPU)/$(WINCEDEBUG_TARGET)/ddraw.lib
	API_TEST_LIBS += $(OS_TEST_LIBS)
	EGL_API_TEST_LIBS += $(OS_TEST_LIBS)
endif

API_TEST_LIBS += $(MALI_LIB) $(MALI_UMP_LIB)

# Add libs needed by egl_helpers_android.cpp
ifdef MALI_ANDROID
ifeq ($(USE_TPI),1)
	EGL_HELPER_ANDROID_LIBS := libui.so libgui.so libhardware.so libutils.so libcutils.so
else
	EGL_HELPER_ANDROID_LIBS := libui.so libhardware.so libutils.so libcutils.so
endif
	API_TESTS_SRCS += $(TESTBENCH_API_TESTS_DIR)/egl/egl_framework.c
	API_TEST_COMMON_LIBS := $(TMP_PRODUCT_DIR)/libunit_framework$(LIB_EXT) \
	                        $(TMP_PRODUCT_DIR)/libtpi$(LIB_EXT)
	API_TEST_LIBS += $(EGL_HELPER_ANDROID_LIBS)
	EGL_API_TEST_LIBS += $(EGL_HELPER_ANDROID_LIBS)
	MALI_TPI_SEARCH_PATH = $(TESTBENCH_API_TESTS_DIR)/../tpi \
	                       $(TESTBENCH_API_TESTS_DIR)/../tpi/tpi

ifeq ($(USE_TPI),1)
	EGL_HELPER_ANDROID_LIBS := libui.so libgui.so libhardware.so libutils.so libcutils.so
	API_TEST_LIBS += $(TMP_PRODUCT_DIR)/libtpi_egl$(LIB_EXT)
	EGL_API_TEST_LIBS += $(TMP_PRODUCT_DIR)/libtpi_egl$(LIB_EXT)
endif
	TESTBENCH_OPENGLES_DEFECTS_LIBS = $(LIB_DIR)/libEGL_mali$(DYNLIB_EXT) $(LIB_DIR)/libGLESv1_CM_mali$(DYNLIB_EXT)
else
	TESTBENCH_OPENGLES_DEFECTS_LIBS =
	API_TEST_COMMON_LIBS := $(TMP_PRODUCT_DIR)/libunit_framework$(LIB_EXT) \
	                        $(TMP_PRODUCT_DIR)/libtpi$(LIB_EXT)
endif

ifdef USE_OPENGLES_1
ifdef ARM_INTERNAL_BUILD
	TESTBENCH_OPENGLES_DIR = $(TESTBENCH_API_TESTS_DIR)/opengles1
	TESTBENCH_OPENGLES_DEFECTS_DIR=$(TESTBENCH_OPENGLES_DIR)/defects
include $(TESTBENCH_OPENGLES_DEFECTS_DIR)/Makefile
endif
endif

ifdef USE_OPENGLES_1
ifndef USE_OPENGLES_2
$(call make-target-binary,$(BIN_DIR)/egl_api_main_suite_11$(EXE_EXT),$(EGL_API_TESTS_SRCS) $(EGL_API_TEST_LIBS) $(MALI_GLES11_LIB) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif
endif

ifndef USE_OPENGLES_2
ifndef USE_OPENGLES_1
ifdef USE_OPENVG
ALL_TESTS += $(BIN_DIR)/egl_api_main_suite$(EXE_EXT)

ifdef ARM_INTERNAL_BUILD
ALL_TESTS += $(BIN_DIR)/egl_extensions_suite$(EXE_EXT)
ALL_TESTS += $(BIN_DIR)/egl_performance_suite$(EXE_EXT)
endif

$(call make-target-binary,$(BIN_DIR)/egl_api_main_suite$(EXE_EXT),$(EGL_API_TESTS_SRCS) $(EGL_API_TEST_LIBS) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
$(call make-target-binary,$(BIN_DIR)/egl_extensions_suite$(EXE_EXT),$(API_TESTS_EGL_EXTENSIONS_SRCS) $(EGL_API_TEST_LIBS) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
$(call make-target-binary,$(BIN_DIR)/egl_performance_suite$(EXE_EXT),$(API_TESTS_EGL_PERFORMANCE_SRCS) $(EGL_API_TEST_LIBS) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif
endif
endif

ifdef USE_OPENGLES_2
$(call make-target-binary,$(BIN_DIR)/egl_api_main_suite_20$(EXE_EXT),$(EGL_API_TESTS_SRCS) $(EGL_API_TEST_LIBS) $(MALI_GLES20_LIB) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
$(call make-target-binary,$(BIN_DIR)/gles2_api_suite$(EXE_EXT),$(API_TESTS_SRCS) $(API_TEST_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)
ifdef ARM_INTERNAL_BUILD
$(call make-target-binary,$(BIN_DIR)/gles2_multicore_suite$(EXE_EXT),$(API_TESTS_GLES2_MULTICORE_SRCS) $(API_TEST_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)

$(call make-target-binary,$(BIN_DIR)/gles2_performance_suite$(EXE_EXT),$(API_TESTS_GLES2_PERFORMANCE_SRCS) $(API_TEST_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)
$(call make-target-binary,$(BIN_DIR)/gles2_defect_suite$(EXE_EXT),$(API_TESTS_GLES2_DEFECT_SRCS) $(API_TEST_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)

# The instrumented defects test rely on GLES2 for rendering
$(call make-target-binary,$(BIN_DIR)/instrumented_defect_suite$(EXE_EXT),$(API_TESTS_INSTRUMENTED_DEFECT_SRCS) $(API_TEST_LIBS) $(MALI_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)

# The instrumented main test rely on GLES2 for rendering
$(call make-target-binary,$(BIN_DIR)/instrumented_api_suite$(EXE_EXT),$(API_TESTS_INSTRUMENTED_API_SRCS) $(API_TEST_LIBS) $(MALI_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif
endif

ifdef MALI_MONOLITHIC
ifdef USE_OPENGLES_1_AND_2
$(call make-target-binary,$(BIN_DIR)/gles_monolithic_suite$(EXE_EXT),$(MONOLITHIC_TEST_SRCS) $(MALI_LIB) $(OS_TEST_LIBS) $(MALI_GLES11_LIB) $(MALI_GLES20_LIB) $(MALI_EGL_LIB) $(API_TEST_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif
endif

ifdef MALI_TEST_API
ifdef USE_OPENGLES_2
$(call make-target-binary,$(BIN_DIR)/gles2_whitebox_suite$(EXE_EXT),$(WHITEBOX_TEST_SRCS) $(API_TEST_LIBS) $(MALI_GLES20_LIB) $(MALI_EGL_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif
endif

ifdef USE_OPENGLES_2
#$(call make-target-binary,$(BIN_DIR)/gles2_stress_suite$(EXE_EXT),$(GLES2_STRESS_TESTS_SRCS) $(MALI_LIB) $(MALI_GLES20_LIB) $(MALI_EGL_LIB) $(OS_TEST_LIBS) $(TMP_PRODUCT_DIR)/libunit_framework$(LIB_EXT))
$(call make-target-binary,$(BIN_DIR)/gles2_stress_suite$(EXE_EXT),$(GLES2_STRESS_TESTS_SRCS) $(API_TEST_LIBS) $(GLES2_STRESS_TEST_EXTRA_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif

ifdef MALI_MONOLITHIC
ifdef USE_OPENGLES_2
ifdef USE_OPENVG
$(call make-target-binary,$(BIN_DIR)/gles2_vg_shared_stress_suite$(EXE_EXT),$(GLES2_VG_SHARED_STRESS_TESTS_SRCS) $(MALI_LIB) $(MALI_UMP_LIB) $(MALI_GLES20_LIB) $(MALI_EGL_LIB) $(MALI_OPENVG_LIB) $(MALI_OPENVGU_LIB) $(OS_TEST_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif
endif
endif

ifdef ARM_INTERNAL_BUILD
# The LINKLOADER_LIBS contains all libraries that the linkloader suite need to link to (since it doesn't link with an API lib)
LINKLOADER_LIBS=$(MALI_LIB)
ALL_TESTS += $(BIN_DIR)/linkerloader_testbench$(EXE_EXT)
$(call make-target-binary,$(BIN_DIR)/linkerloader_testbench$(EXE_EXT),$(API_TESTS_LINKER_LOADER) $(LINKLOADER_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)

ifdef USE_OPENGLES_1
$(call make-target-binary,$(BIN_DIR)/gles1_performance_suite$(EXE_EXT),$(API_TESTS_GLES1_PERFORMANCE_SRCS) $(API_TEST_LIBS) $(API_TEST_COMMON_LIBS), MULTITHREADED)
ifndef USE_OPENGLES_2
$(call make-target-binary,$(BIN_DIR)/egl_defect_suite_11$(EXE_EXT),$(API_TESTS_EGL_DEFECT_SRCS) $(EGL_API_TEST_LIBS) $(MALI_GLES11_LIB) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS))
$(call make-target-binary,$(BIN_DIR)/egl_extensions_suite_11$(EXE_EXT),$(API_TESTS_EGL_EXTENSIONS_SRCS) $(EGL_API_TEST_LIBS) $(MALI_GLES11_LIB) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
$(call make-target-binary,$(BIN_DIR)/egl_performance_suite_11$(EXE_EXT),$(API_TESTS_EGL_PERFORMANCE_SRCS) $(EGL_API_TEST_LIBS) $(MALI_GLES11_LIB) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif
endif

ifdef USE_OPENGLES_2
$(call make-target-binary,$(BIN_DIR)/egl_defect_suite_20$(EXE_EXT),$(API_TESTS_EGL_DEFECT_SRCS) $(EGL_API_TEST_LIBS) $(MALI_GLES20_LIB) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS))
$(call make-target-binary,$(BIN_DIR)/egl_extensions_suite_20$(EXE_EXT),$(API_TESTS_EGL_EXTENSIONS_SRCS) $(EGL_API_TEST_LIBS) $(MALI_GLES20_LIB) $(MALI_LIB) $(MALI_UMP_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
$(call make-target-binary,$(BIN_DIR)/egl_performance_suite_20$(EXE_EXT),$(API_TESTS_EGL_PERFORMANCE_SRCS) $(EGL_API_TEST_LIBS) $(MALI_GLES20_LIB) $(MALI_LIB) $(MALI_UMP_LIB) $(API_TEST_COMMON_LIBS), MULTITHREADED)
endif

ifeq ($(EGL_API_TESTS_INTEROPERABILITY), 1)
EGL_MULTIPLE_PROCESS_APP_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/applications/egl_multi_process_app_gles11.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/applications/egl_framework_standalone.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/applications/egl_helpers_$(EGL_HELPER_BACKEND)_standalone.c \
	$(UMP_PIXMAP_SUPPORT_SRCS)

EGL_MULTIPLE_PROCESS_APP_LIBS := $(MALI_OPENVG_LIB) $(MALI_OPENVGU_LIB) $(MALI_GLES11_LIB) $(MALI_EGL_LIB) $(OS_TEST_LIBS)
ALL_TESTS += $(BIN_DIR)/egl_multi_process_app_gles11$(EXE_EXT)

$(call make-target-binary,$(BIN_DIR)/egl_multi_process_app_gles11$(EXE_EXT), $(EGL_MULTIPLE_PROCESS_APP_SRCS) $(EGL_MULTIPLE_PROCESS_APP_LIBS) $(MALI_LIB) $(TMP_PRODUCT_DIR)/libtpi$(LIB_EXT), MULTITHREADED)
$(BIN_DIR)/egl_api_main_suite_11$(EXE_EXT): bin/egl_multi_process_app_gles11$(EXE_EXT)
endif

ifeq ($(EGL_API_TESTS_INTEROPERABILITY), 2)
EGL_MULTIPLE_PROCESS_APP_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/applications/egl_multiple_process_app.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/applications/egl_framework_standalone.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/applications/egl_helpers_$(EGL_HELPER_BACKEND)_standalone.c \
	$(UMP_PIXMAP_SUPPORT_SRCS)

EGL_MULTIPLE_PROCESS_APP_LIBS := $(MALI_OPENVG_LIB) $(MALI_OPENVGU_LIB) $(LIB_DIR)/libGLESv2$(DYNLIB_EXT) $(MALI_EGL_LIB) $(MALI_LIB) $(OS_TEST_LIBS)
ALL_TESTS += $(BIN_DIR)/egl_multiple_process_app$(EXE_EXT)

$(call make-target-binary,$(BIN_DIR)/egl_multiple_process_app$(EXE_EXT), $(EGL_MULTIPLE_PROCESS_APP_SRCS) $(EGL_MULTIPLE_PROCESS_APP_LIBS) $(TMP_PRODUCT_DIR)/libtpi$(LIB_EXT), MULTITHREADED)
$(BIN_DIR)/egl_api_main_suite_20$(EXE_EXT): bin/egl_multiple_process_app$(EXE_EXT)
endif

endif # ARM_INTERNAL_BUILD

#this is where we need to run the suite from, and we need these dirs in addition
DIRS_TO_COPY-gles2_api_suite = testbench/api_tests/opengles2/shaders
RUN_DIR-gles2_api_suite = testbench/api_tests/opengles2

ifdef ARM_INTERNAL_BUILD
DIRS_TO_COPY-egl_api_main_suite = testbench/api_tests/egl/shaders testbench/api_tests/egl/scripts testbench/api_tests/egl/scripts_gles11 testbench/api_tests/egl/applications
else
DIRS_TO_COPY-egl_api_main_suite = testbench/api_tests/egl/shaders
endif
RUN_DIR-egl_api_main_suite = testbench/api_tests/egl
RUN_DIR-egl_defect_suite = testbench/api_tests/egl

DIRS_TO_COPY-gles2_defect_suite = testbench/api_tests/opengles2/shaders
RUN_DIR-gles2_defect_suite = testbench/api_tests/opengles2

# Should include VG ref images from S0005 as well, but hard to automate
DIRS_TO_COPY-vg_api_tests = testbench/api_tests/openvg/datafiles
RUN_DIR-vg_api_tests = testbench/api_tests/openvg
DIRS_TO_COPY-vg_unit_tests = testbench/unit_tests/openvg
RUN_DIR-vg_unit_tests = testbench/unit_tests

# If the user didn't specify a location for API trace files then
# use a sensible default location.
TRACE_DIR ?= $(DRIVER_DIR)/traces

# Locate the trace-to-C tool's executable file.
TRACETOC = $(call source-dir-to-binary-dir,$(HOST_SYSTEM),$(BIN_DIR)/tracetoc$(EXE_EXT))

# Make a whitespace-separated list of trace file leaf names
TRACE_ALL = $(notdir $(wildcard $(TRACE_DIR)/*.xml) \
                     $(wildcard $(TRACE_DIR)/*.xml.*) \
                     $(wildcard $(TRACE_DIR)/*.trace) \
                     $(wildcard $(TRACE_DIR)/*.trace.*) \
                     $(wildcard $(TRACE_DIR)/*.trc) \
                     $(wildcard $(TRACE_DIR)/*.trc.*))

# Convert the input file name into a stem for use when inventing output file
# names. This is complicated by the need to deal with names that incorporate
# a number as a suffix (e.g. 'foo.trace.0001') rather than as part of the base
# name (e.g. 'foo_0001.trace'). Both of the above examples will translate to
# stem 'foo_0001'.
# * * * *  Note that TRACE is expanded within 'foreach', not here!  * * * *
TRACE_STEM = $(strip $(if $(filter %.xml %.trace %.trc, $(TRACE)), $(basename $(TRACE)), $(basename $(basename $(TRACE)))$(subst .,_,$(suffix $(TRACE)))))

# Make the partial path of the executable file (not including build directory)
# * * * *  Note that TRACE_STEM is expanded within 'foreach', not here!  * * * *
TRACE_EXE = $$(BIN_DIR)/trace_$(TRACE_STEM)$$(EXE_EXT)

# List of source files required to make each executable file
# * * * *  Note that TRACE_STEM is expanded within 'foreach', not here!  * * * *
TRACE_SRCS = \
	$$(EGL_HELPER_SRCS) \
	$$(UMP_PIXMAP_SUPPORT_SRCS) \
	$$(TRACE_DIR)/$(TRACE_STEM)_program.c

define foreachtrace
# Rule to convert trace files into C source files
$$(TRACE_DIR)/$(TRACE_STEM)_program.c $$(TRACE_DIR)/$(TRACE_STEM)_globals.c: $$(TRACE_DIR)/$(TRACE) $$(TRACETOC)
	$$(TRACETOC) --output $$(TRACE_DIR)/$(TRACE_STEM) $$(TRACE_DIR)/$(TRACE)

# Generate a rule to compile and link the source code
$$(call make-target-binary, $(TRACE_EXE), $(TRACE_SRCS) $$(API_TEST_LIBS) $$(TMP_PRODUCT_DIR)/libunit_framework$$(LIB_EXT), MULTITHREADED)

# Add the full path of the executable to the list of prerequisites for api-traces
TRACE_ALL_EXES += $$(call source-dir-to-binary-dir, $$(TARGET_SYSTEM), $(TRACE_EXE))
endef

# Execute the commands above for each API trace file
$(foreach TRACE, $(TRACE_ALL), \
	$(eval $(foreachtrace)) \
)

# Some magic to build all API traces in one go.
.PHONY: api-traces
api-traces: $(TRACE_ALL_EXES)

# Prevent Make deleting all the intermediate source files it knows about.
.SECONDARY: $(TRACE_ALL:%=$(TRACE_DIR)/%_program.c)

# The trace-to-C tool insists on outputting source files in the same directory
# as its input file, so a special cleaning step is needed.
CLEAN_FILES += $(TRACE_DIR)/*_program.c $(TRACE_DIR)/*_globals.c

# Test suite for the platform-independent EGL pixmap helper functions
EGL_HELPER_PIXMAP_TESTS_SRCS = \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers_pixmap.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers_pixmap_tests.c \
	$(TESTBENCH_API_TESTS_DIR)/egl/egl_helpers_pixmap_test_utils.c \
	$(MALI_LIB) \
	$(TMP_PRODUCT_DIR)/libunit_framework$(LIB_EXT)

$(call make-target-binary, $(BIN_DIR)/egl_helper_pixmap_tests$(EXE_EXT), $(EGL_HELPER_PIXMAP_TESTS_SRCS), MULTITHREADED)
