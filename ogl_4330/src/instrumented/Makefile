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

INSTRUMENTED_DIR ?= .

ifeq ($(MALI_INSTRUMENTED),TRUE)

	INSTRUMENTED_SRCS = \
		$(INSTRUMENTED_DIR)/mali_instrumented_main.c \
		$(INSTRUMENTED_DIR)/mali_counters.c \
		$(INSTRUMENTED_DIR)/mali_instrumented_context.c \
		$(INSTRUMENTED_DIR)/mali_instrumented_frame.c \
		$(INSTRUMENTED_DIR)/mali_log.c \
		$(INSTRUMENTED_DIR)/mali_counters_dump.c \
		$(INSTRUMENTED_DIR)/mali_frame_dump.c \
		$(INSTRUMENTED_DIR)/sw_profiling.c \
		$(INSTRUMENTED_DIR)/egl_instrumented.c \
		$(INSTRUMENTED_DIR)/memory/mali_memory_instrumented.c

	ifeq ($(MALI_USING_MRI),TRUE)
		INSTRUMENTED_SRCS += $(INSTRUMENTED_DIR)/mri_interface.cpp \
			$(INSTRUMENTED_DIR)/mri_remote_functions.cpp \
			$(INSTRUMENTED_DIR)/libmri/mrpmessagedecoder.cpp \
			$(INSTRUMENTED_DIR)/libmri/mrpmessageelement.cpp \
			$(INSTRUMENTED_DIR)/libmri/mrpmessageencoder.cpp \
			$(INSTRUMENTED_DIR)/libmri/mrpnetio.cpp \
			$(INSTRUMENTED_DIR)/libmri/mrpserver.cpp \
			$(INSTRUMENTED_DIR)/libmri/mrpstring.cpp \
			$(INSTRUMENTED_DIR)/libmri/mrperror.cpp
	else
		INSTRUMENTED_SRCS += $(INSTRUMENTED_DIR)/mri_interface_stub.c
	endif

	ifdef USE_OPENVG
		INSTRUMENTED_SRCS += $(INSTRUMENTED_DIR)/vg/mali_vg_instrumentation.c
	endif

	ifdef USE_OPENGLES
		INSTRUMENTED_SRCS += $(INSTRUMENTED_DIR)/gles/gles_instrumentation.c
	endif

	INSTRUMENTED_SRCS += $(INSTRUMENTED_DIR)/hw/mali200/mali_pp_instrumented.c $(INSTRUMENTED_DIR)/hw/maligp2/mali_gp_instrumented.c

endif

ifeq ($(MALI_INSTRUMENTED_PLUGIN_ENABLED),1)
	INSTRUMENTED_SRCS += $(INSTRUMENTED_DIR)/mali_instrumented_plugin.c
endif

ifeq ($(MALI_API_TRACE),TRUE)
	INSTRUMENTED_SRCS += $(INSTRUMENTED_DIR)/apitrace/mali_trace.c
endif

ifeq ($(MALI_OPROFILE_SAMPLING),TRUE)
	INSTRUMENTED_SRCS += $(INSTRUMENTED_DIR)/mali_oprofile.c
endif

MALI_PLUGIN_LIB=libmaliinstr$(DYNLIB_EXT)

MALI_PLUGIN_SRCS = $(INSTRUMENTED_DIR)/plugin/mali_plugin.c

$(call make-target-dynamic-library,$(LIB_DIR)/$(MALI_PLUGIN_LIB), $(MALI_PLUGIN_SRCS) $(MALI_LIB), MULTITHREADED)
