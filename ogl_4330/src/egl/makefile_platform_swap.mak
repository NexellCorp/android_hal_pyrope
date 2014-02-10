#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2010-2011 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#
# Included from src/egl/Makefile

ifneq ($(call is-feature-enabled,vsync),)
    ifeq ($(TARGET_PLATFORM),orion)
        EGL_SRCS += $(EGL_DIR)/egl_platform_backend_swap_orion.c
    else
        EGL_SRCS += $(EGL_DIR)/egl_platform_backend_swap_common.c
    endif
else
        EGL_SRCS += $(EGL_DIR)/egl_platform_backend_swap_common.c
endif


