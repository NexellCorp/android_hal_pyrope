# -*- mode: makefile -*-
#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2011-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

# Figure out OS 
ifneq ($(call is-feature-enabled,android),)
   	TPI_OS = android
   	CPPFLAGS += -DTPI_OS_ANDORID=1
else ifneq ($(call is-feature-enabled,linux),)
	TPI_OS = linux
	CPPFLAGS += -DTPI_OS_LINUX=1
else ifneq ($(call is-feature-enabled,windowsce),)
	TPI_OS = windows 
	CPPFLAGS += -DTPI_OS_WINDOWS=1
endif

ifeq ($(TPI_OS),)
	TPI_OS = linux
	CPPFLAGS += -DTPI_OS_LINUX=1
endif

# Figure out backend used for tpi if OS is linux
ifeq ($(TPI_OS),linux)
    ifneq ($(call is-feature-enabled,dummy),)
    	TPI_BACKEND = dummy
    	CPPFLAGS += -DTPI_BACKEND_DUMMY=1
    else
    	ifneq ($(call is-feature-enabled,fbdev),)
    		TPI_BACKEND = fbdev
    		CPPFLAGS += -DTPI_BACKEND_FBDEV=1
    	else
    		ifneq ($(call is-feature-enabled,no_mali),)
    			TPI_BACKEND = dummy
    			CPPFLAGS += -DTPI_BACKEND_DUMMY=1
    		else
    			ifneq ($(call is-feature-enabled,x11),)
    				TPI_BACKEND = x11
    				CPPFLAGS += -DTPI_BACKEND_X11=1
    			endif
    		endif
    		
    	endif
    endif
endif
	
ifeq ($(TPI_BACKEND),)
	TPI_BACKEND ?= fbdev
	CPPFLAGS += -DTPI_BACKEND_FBDEV=1
endif


TPI_PLATFORM_ARM_ARCH ?= 7
TPI_DIR ?= .
TPI_SRC_DIR = $(TPI_DIR)/src
TPI_EXAMPLE_DIR = $(TPI_DIR)/example
TPI_UNIT_TEST_DIR = $(TPI_DIR)/unit_tests
TPI_INCLUDE_DIR = $(TPI_DIR)/include
TPI_SRCS ?= 
TPI_LIB ?= $(TMP_PRODUCT_DIR)/libtpi$(LIB_EXT)
TPI_EGL_LIB_DIR ?= $(TMP_PRODUCT_DIR)/libtpi_egl$(LIB_EXT)

CPPFLAGS += -I$(TPI_DIR) -I$(TPI_INCLUDE_DIR) -DEGL_FBDEV=1 -D_GNU_SOURCE -DTPI_PLATFORM_ARM_ARCH=$(TPI_PLATFORM_ARM_ARCH)

# we need malisw headers
ifneq ($(wildcard $(DRIVER_DIR)/malisw),)
	CPPFLAGS += -I$(DRIVER_DIR)
endif

TPI_SUPPORT_LIBS ?=
TPI_EXTRA_LINK_OPTIONS ?=

include $(TPI_SRC_DIR)/Makefile

ifneq ($(wildcard $(TPI_EXAMPLE_DIR)/Makefile),)
include $(TPI_EXAMPLE_DIR)/Makefile
endif

ifneq ($(wildcard $(TPI_UNIT_TEST_DIR)/src/Makefile),)
include $(TPI_UNIT_TEST_DIR)/src/Makefile
endif
