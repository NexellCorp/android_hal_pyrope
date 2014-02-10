#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2005, 2007-2012 ARM Limited
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

EGL_DIR ?= .

EGL_SRCS = \
	$(EGL_DIR)/egl_config.c \
	$(EGL_DIR)/egl_convert.c \
	$(EGL_DIR)/egl_context.c \
	$(EGL_DIR)/egl_display.c \
	$(EGL_DIR)/egl_entrypoints.c \
	$(EGL_DIR)/egl_main.c \
	$(EGL_DIR)/egl_misc.c \
	$(EGL_DIR)/egl_mali.c \
	$(EGL_DIR)/egl_extensions.c \
	$(EGL_DIR)/egl_surface.c \
	$(EGL_DIR)/egl_thread.c \
	$(EGL_DIR)/egl_handle.c \
	$(EGL_DIR)/egl_image.c \
	$(EGL_DIR)/egl_image_pixmap.c \
	$(EGL_DIR)/egl_image_gles.c \
	$(EGL_DIR)/egl_image_vg.c \
	$(EGL_DIR)/egl_sync.c \
	$(EGL_DIR)/mali_egl_image.c \
	$(EGL_DIR)/egl_lock_surface.c \
	$(EGL_DIR)/__egl_build_info.c \
	$(EGL_DIR)/egl_common_memory.c 

ifeq ($(MALI_EXTERNAL_SYNC),1)
EGL_SRCS += $(EGL_DIR)/mali_fence_sync.c
endif

EGL_LINKER ?= dl


EGL_SRCS_SHIM = $(EGL_DIR)/egl_entrypoints_shim.c


#need to be in fast mode to support non-interworking callees.
$(call set-file-flavour,fast,$(EGL_DIR)/egl_api_interface.c)

CPPFLAGS += -DMALI_EGL_MAJOR_VERSION=$(MALI_EGL_MAJOR_VERSION) -DMALI_EGL_MINOR_VERSION=$(MALI_EGL_MINOR_VERSION)

# vsync
#EGL_SIMULATE_VSYNC ?= 1

EGL_BACKEND ?= fbdev

# first try to figure out what kind of backend
# to use based on platform

ifneq ($(call is-feature-enabled,dummy),)
	EGL_BACKEND = dummy
else
	ifneq ($(call is-feature-enabled,fbdev),)
		EGL_BACKEND = fbdev
	else
		ifneq ($(call is-feature-enabled,no_mali),)
			EGL_BACKEND = dummy
		else
			ifeq ($(UDD_OS),linux)
				ifeq ($(MALI_ANDROID),1)
					EGL_BACKEND = android
				else
					EGL_BACKEND = fbdev
				endif
			endif
		endif
	endif
endif

# non-android back-end
ifeq ($(MALI_ANDROID_API),)
EGL_SRCS += $(wildcard $(EGL_DIR)/egl_platform_$(EGL_BACKEND).c*)
else
# beeswax 
  ifeq ($(MALI_ANDROID_API), 13)
    EGL_SRCS += $(wildcard $(EGL_DIR)/egl_platform_android_v2.cpp)
  else
    ANDROID_BACKEND_V2 = $(shell [ $(MALI_ANDROID_API) -ge 14 ] && echo true )
# ics, jb, jb mr1
    ifeq ($(ANDROID_BACKEND_V2), true)
      EGL_SRCS += $(wildcard $(EGL_DIR)/egl_platform_android_v2.cpp) \
                  $(wildcard $(EGL_DIR)/egl_recordable_android.cpp)
    else
# old android versions
      EGL_SRCS += $(wildcard $(EGL_DIR)/egl_platform_$(EGL_BACKEND).c*) 
    endif
  endif
endif

EGL_SRCS += $(wildcard $(EGL_DIR)/egl_image_$(EGL_BACKEND).c*)
# To enable async swapbuffers, set the following value to 1. To disable: 0.
# Async swapbuffers enables EGL to perform the buffer swap in a separate thread,
# which prevents blocking the mali worker thread while waiting for vsync.
ifeq ($(EGL_BACKEND),android)
	EGL_ASYNC_SWAPBUFFERS = 1
	CPPFLAGS += -DEGL_EXTENDED_CONFIGS
endif

ifeq ($(EGL_ASYNC_SWAPBUFFERS), 1)
	CPPFLAGS += -DEGL_ASYNC_SWAPBUFFERS
	EGL_WORKER_THREAD_REQUIRED = 1
endif

CPPFLAGS += -I$(DRIVER_DIR)/src/egl/android/gralloc
CPPFLAGS += -I$(DRIVER_DIR)/include/EGL/platform_$(EGL_BACKEND)
CPPFLAGS += -DEGL_BACKEND_$(shell echo $(EGL_BACKEND) | tr '[:lower:]' '[:upper:]')

# Include possible platform/backend-specific makefiles
-include src/egl/Makefile_*.mak

ifeq ($(EGL_BACKEND),fbdev)
	CPPFLAGS += -DEGL_PLATFORM_PIXMAP_GET_MALI_MEMORY_SUPPORTED=1
	CPPFLAGS += -DEGL_PLATFORM_LOCKSURFACE_SUPPORTED=1
	CPPFLAGS += -DEGL_PLATFORM_PIXMAP_LOCKSURFACE_SUPPORTED=1
    -include src/egl/makefile_platform_swap.mak
endif

ifeq ($(EGL_BACKEND),x11)
	CPPFLAGS += -DEGL_PLATFORM_LOCKSURFACE_SUPPORTED=1
	CPPFLAGS += -DEGL_PLATFORM_PIXMAP_LOCKSURFACE_SUPPORTED=1
	CPPFLAGS += -DEGL_DEFERRED_UMP_CACHE_FLUSH=1
endif

ifeq ($(MALI_USE_UMPLOCK),1)
	CPPFLAGS += -DEGL_SURFACE_LOCKING_ENABLED=1
endif

ifneq ($(MALI_BUILD_ANDROID_MONOLITHIC), 1)
	EGL_SRCS += $(EGL_DIR)/egl_entrypoints_shim.c
endif


ifeq ($(USE_OPENGLES), 1)
	CPPFLAGS += -DEGL_MALI_GLES
	EGL_SRCS += $(EGL_DIR)/egl_gles.c
endif

CPPFLAGS += -DBUILD=$(CONFIG)

ifdef USE_OPENVG
	CPPFLAGS += -DEGL_MALI_VG
	EGL_SRCS += $(EGL_DIR)/egl_vg.c
endif

# include the apropriate file used to link in vg and gles
ifeq ($(EGL_LINKER),weak)
	EGL_SRCS += $(EGL_DIR)/egl_linker_weak.c
	CPPFLAGS += -DEGL_WEAK_LINKING
	KNOWN_LINKER= true
endif
ifeq ($(EGL_LINKER),dl)
	EGL_SRCS += $(EGL_DIR)/egl_linker_dl.c
	KNOWN_LINKER= true
endif

ifndef KNOWN_LINKER
$(error Unknown EGL_LINKER=$(EGL_LINKER))
endif


# This will be enabled by EGL_ASYNC_SWAPBUFFERS
EGL_WORKER_THREAD_REQUIRED ?= 0

ifeq ($(EGL_WORKER_THREAD_REQUIRED), 1)
	CPPFLAGS += -DEGL_WORKER_THREAD_ENABLED
endif

ifeq ($(EGL_SIMULATE_VSYNC), 1)
#vsync is currently non-usable without the simulation thread running
	CPPFLAGS += -DEGL_SIMULATE_VSYNC
	CPPFLAGS += -DEGL_USE_VSYNC
	EGL_SRCS += $(EGL_DIR)/egl_vsync.c
endif

ifeq ($(USE_TRACING),1)
	CPPFLAGS += -DGL_TRACE_WRAPPER
	EGL_SRCS += $(SHARED_DIR)/trace/egltrace.c
endif

ifeq ($(MALI_API_TRACE),TRUE)
	EGL_SRCS += $(EGL_DIR)/egl_api_trace.c
endif

-include $(EGL_DIR)/feature/swap_region/Makefile

CPPFLAGS += -I$(DRIVER_DIR)/include/VG

# Define Exports for the EGL_SRCS
$(call source-to-object,$(TARGET_SYSTEM),$(EGL_SRCS)) : CPPFLAGS+=-D__EGL_EXPORTS

TOOLCHAIN_SPECIFIC_EGL_SRCS=

# Create libEGL aliases to versioned library links, if required.
$(call make-library-aliases, \
	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_EGL_LIB_VERSIONED)) \
	$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(MALI_EGL_LIB_ALIASES)) \
	)

# Support libraries, which will be required if your platform does not support
# CLZ, requiring the slow 'fallback' CLZ implementation.
# Override to reduce the size of the binary marginally.
EGL_SUPPORT_LIBS += $(MATH_LIB)

ifndef MALI_MONOLITHIC
# If TRACING is enabled then link with libMaliShared.so otherwise link with
# libMaliShared.a. This is needed because the unreferenced symbols in the
# static library are pruned when linked with the shared library. For more
# informations see bugzilla entry 5051
ifeq ($(USE_TRACING),1)
$(call make-target-dynamic-library,$(MALI_EGL_LIB_VERSIONED), $(TOOLCHAIN_SPECIFIC_EGL_SRCS) $(EGL_SRCS) $(MALI_LIB) $(EGL_SUPPORT_LIBS), $(EGL_EXTRA_LINK_OPTIONS))
else
$(call make-target-dynamic-library,$(MALI_EGL_LIB_VERSIONED), $(TOOLCHAIN_SPECIFIC_EGL_SRCS) $(EGL_SRCS) $(MALI_LIB) $(EGL_SUPPORT_LIBS), $(EGL_EXTRA_LINK_OPTIONS))
endif
endif
# autogenerated build version info
$(call make-build-info-file,$(EGL_DIR)/__egl_build_info.c,egl,$(EGL_DIR),$(EGL_SRCS),EGL_PLATFORM=${PLATFORM} EGL_USE_OPENGLES=${USE_OPENGLES} EGL_USE_OPENVG=${USE_OPENVG})
