#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2005, 2007-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

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

SRC_DIR ?= .

ifdef MALI_USE_UNIFIED_MEMORY_PROVIDER
ifdef MALI_BUILD_UMP
ifeq ($(USE_UMPV2),1)
	UMP_DIR=$(SRC_DIR)/umpv2
else
	UMP_DIR=$(SRC_DIR)/ump
endif
	-include $(UMP_DIR)/mali_ump.mak
endif
endif

BASE_DIR=$(SRC_DIR)/base
-include $(BASE_DIR)/Makefile

SHARED_DIR=$(SRC_DIR)/shared
-include $(SHARED_DIR)/Makefile

ifneq ($(call is-feature-enabled,windowsce),)
	DRV_DIR=$(SRC_DIR)/devicedrv/mali
	include $(DRV_DIR)/Makefile.win
	ifdef MALI_USE_UNIFIED_MEMORY_PROVIDER
		include $(SRC_DIR)/devicedrv/ump/Makefile.win
	endif
endif

UTIL_DIR=$(SRC_DIR)/util
-include $(UTIL_DIR)/Makefile

EGL_DIR=$(SRC_DIR)/egl
-include $(EGL_DIR)/Makefile

ifdef USE_OPENGLES
	OPENGLES_DIR=$(SRC_DIR)/opengles
	-include $(OPENGLES_DIR)/Makefile
endif

ifdef USE_OPENVG
	OPENVG_DIR=$(SRC_DIR)/openvg
	-include $(OPENVG_DIR)/Makefile
endif

INSTRUMENTED_DIR=$(SRC_DIR)/instrumented
-include $(INSTRUMENTED_DIR)/Makefile

ifneq ($(wildcard $(SRC_DIR)/tpi/Makefile),)
	TPI_DIR=$(SRC_DIR)/tpi
	USE_TPI := 1
	-include $(TPI_DIR)/Makefile
endif
