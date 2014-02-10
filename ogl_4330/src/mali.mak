#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2008-2012 ARM Limited
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

SRC_DIR ?= .

ifeq ($(MALI_USING_MRI),TRUE)
LINK_STDCPP_LIB=CXX
endif

# Create libMali aliases to versioned library links, if required.
$(call make-library-aliases, \
	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_LIB_VERSIONED)) \
	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_LIB_ALIASES)) \
)

ifneq ($(call is-feature-enabled,test),)
# Define data exports for libMali
$(call source-to-object,$(TARGET_SYSTEM),$(BASE_SRCS) $(SHARED_SRCS) $(TOOLCHAIN_SPECIFIC_SHARED_SRCS) $(TOOLCHAIN_SPECIFIC_BASE_SRCS) ) : CPPFLAGS+=-D__BASE_TEST_VARS_EXPORTS
endif

ifndef MALI_MONOLITHIC
#
# non-MONOLITHIC libMali.so
#

# Create library libMali.so
$(call make-target-dynamic-library,$(MALI_LIB_VERSIONED), \
			$(SHADER_GENERATOR_LIB) \
			$(SHARED_SRCS) $(TOOLCHAIN_SPECIFIC_SHARED_SRCS) $(COMP_LIB) \
			$(BASE_SRCS) $(TOOLCHAIN_SPECIFIC_BASE_SRCS) $(MALI_UMP_LIB) \
			$(MATH_LIB) $(INSTRUMENTED_SRCS), MULTITHREADED $(LINK_STDCPP_LIB))

else # MALI_MONOLITHIC
#
# MONOLITHIC libMali.so
#

CREATE_LINKS_FOR_LIBS = 1

ifeq ($(CREATE_LINKS_FOR_LIBS), 1)

# libEGL.so.1.4 -> libMali.so
$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_EGL_LIB_VERSIONED)): $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_LIB_VERSIONED))
	$(Q)ln -sf $(<F) $@
	$(call $(quiet)cmd-echo-build,SYMLINK,$(LIB_DIR)/$(@F) -> $(MALI_LIB_VERSIONED))

ifdef USE_OPENVG
# libOpenVG -> libMali.so
$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_OPENVG_LIB_VERSIONED)): $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_LIB_VERSIONED))
	$(Q)ln -sf $(<F) $@
	$(call $(quiet)cmd-echo-build,SYMLINK,$(LIB_DIR)/$(@F) -> $(MALI_LIB_VERSIONED))

# libOpenVGU -> libMali.so
$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_OPENVGU_LIB_VERSIONED)): $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_LIB_VERSIONED))
	$(Q)ln -sf $(<F) $@
	$(call $(quiet)cmd-echo-build,SYMLINK,$(LIB_DIR)/$(@F) -> $(MALI_LIB_VERSIONED))
endif

# libGLESv2
ifdef USE_OPENGLES_2
# libGLESv2.so.2.0 -> libMali.so
$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_GLES20_LIB_VERSIONED)): $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_LIB_VERSIONED))
	$(Q)ln -sf $(<F) $@
	$(call $(quiet)cmd-echo-build,SYMLINK,$(LIB_DIR)/$(@F) -> $(MALI_LIB_VERSIONED))
endif # USE_OPENGLES_2

# libGLESv1_CM
ifdef USE_OPENGLES_1
# libGLESv1_CM.so.1.1 -> libMali.so
$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_GLES11_LIB_VERSIONED)): $(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_LIB_VERSIONED))
	$(Q)ln -sf $(<F) $@
	$(call $(quiet)cmd-echo-build,SYMLINK,$(LIB_DIR)/$(@F) -> $(MALI_LIB_VERSIONED))
endif # USE_OPENGLES_1

endif # CREATE_LINKS_FOR_LIBS

# Create Monolithic library libMali.so
$(call make-target-dynamic-library,$(MALI_LIB_VERSIONED), \
			$(OPENGLES_CORE_SRCS) $(GLES1_SRCS) $(GLES2_SRCS) $(FRAGMENT_BACKEND_SRCS) $(GB_LIB) \
			$(SHADER_GENERATOR_LIB) \
			$(SHARED_SRCS) $(TOOLCHAIN_SPECIFIC_SHARED_SRCS) $(COMP_LIB) \
			$(EGL_SRCS) \
			$(VG_SRCS) $(HAL_SRCS) \
			$(VGU_SRCS) \
			$(BASE_SRCS) $(TOOLCHAIN_SPECIFIC_BASE_SRCS) $(MALI_UMP_LIB) \
			$(INSTRUMENTED_SRCS) \
			$(MATH_LIB) $(OS_LIBS), MULTITHREADED $(EGL_EXTRA_LINK_OPTIONS) $(LINK_STDCPP_LIB))

endif # MALI_MONOLITHIC
