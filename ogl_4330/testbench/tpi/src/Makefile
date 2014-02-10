#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2011-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

#Magic to find the right directory to build from

TPI_COMMON_DIR = $(TPI_SRC_DIR)/common

# All modules that use the tpi are going to need to use the standard rt and pthread libraries.
ifeq ($(findstring armcc-armlink, $(TARGET_TOOLCHAIN)),armcc-armlink)
else
	ifneq ($(findstring gcc, $(TARGET_TOOLCHAIN)),gcc)
$(warning Toolchain not gcc. Assuming gcc linker behavior. )
	endif
	TPI_SUPPORT_LIBS += -lrt -lm -lpthread -lc
endif

ifneq ($(call is-feature-enabled, x11),)
	CPPFLAGS += -I/scratch/andped01/image3/usr/local/include/xorg
    # Setup X11 header include directories and X11 library directories
    # (This file is relative to the driver root directory. Not this file's dir.)
    include Makefile_x11.mak
    
    X11_LIBS = -lX11 -ldrm -lXfixes -lXext -Wl,-rpath=$(X11_LIB_DIR)
    
    ifeq ($(EGL_BACKEND),x11)
    	TPI_SUPPORT_LIBS += $(X11_LIBS)
		TPI_EXTRA_LINK_OPTIONS += -L$(X11_LIB_DIR)

		TARGET_LDFLAGS += $(TPI_SUPPORT_LIBS)
		TARGET_LDFLAGS += $(TPI_EXTRA_LINK_OPTIONS)
		
    	CPPFLAGS += $(X11_DRM_INC_DIR)
    	CPPFLAGS += $(X11_INC_DIR)
	endif
endif

TPI_SUPPORT_LIBS += -lc

# include common sources
TPI_COMMON_SRC = \
	$(TPI_COMMON_DIR)/mali_tpi_utils.c \
	$(TPI_COMMON_DIR)/mali_tpi_rand.c  \
	$(TPI_COMMON_DIR)/mali_tpi_egl_utils.c \
	$(TPI_DIR)/mali_tpi_pr400.c
		
TPI_EGL_SRC = \
	$(TPI_COMMON_DIR)/mali_tpi_linux_egl_common.c \
	$(TPI_COMMON_DIR)/mali_tpi_linux_egl_fbdev.c
	
TPI_SRCS += $(TPI_COMMON_SRC)

ifneq ($(wildcard $(TPI_SRC_DIR)/$(TPI_OS)/Makefile),)
	include $(TPI_SRC_DIR)/$(TPI_OS)/Makefile
endif

# Define Exports for the EGL_SRCS
$(call source-to-object,$(TARGET_SYSTEM),$(TPI_SRCS)) : CPPFLAGS+=-D__EGL_EXPORTS

$(call make-target-static-library,$(TPI_LIB),$(TPI_SRCS) $(TPI_SUPPORT_LIBS),$(TPI_EXTRA_LINK_OPTIONS))

