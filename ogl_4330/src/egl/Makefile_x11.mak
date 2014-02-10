#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2010, 2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#
# Included from src/egl/Makefile

ifneq ($(call is-feature-enabled,x11),)
	EGL_BACKEND = x11
endif

# Setup X11 header include directories and X11 library directories
# (This file is relative to the driver root directory. Not this file's dir.)
include Makefile_x11.mak

X11_LIBS = -lX11 -ldrm -lXfixes -lXext -lxcb -lXau -lXdmcp

ifeq ($(EGL_BACKEND),x11)
	EGL_SUPPORT_LIBS += -Wl,-rpath=$(X11_LIB_DIR)
	EGL_EXTRA_LINK_OPTIONS += -L$(X11_LIB_DIR) $(X11_LIBS)

	TARGET_LDFLAGS += $(EGL_SUPPORT_LIBS)
	TARGET_LDFLAGS += $(EGL_EXTRA_LINK_OPTIONS)

	EGL_SRCS += $(EGL_DIR)/x11/libdri2/libdri2.c
	CPPFLAGS += -DEGL_PLATFORM_PIXMAP_GET_MALI_MEMORY_SUPPORTED=1
	CPPFLAGS += $(X11_DRM_INC_DIR)
	CPPFLAGS += $(X11_INC_DIR)
endif
