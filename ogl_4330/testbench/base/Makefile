#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2007-2010, 2012 ARM Limited
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

TESTBENCH_BASE_DIR ?= .

TESTBENCH_BASE_API_DIR=$(TESTBENCH_BASE_DIR)/base_api_tests
-include $(TESTBENCH_BASE_API_DIR)/Makefile

ifdef ARM_INTERNAL_BUILD

ifndef MALI_ANDROID
PWRMGMT_API_DIR=$(TESTBENCH_BASE_DIR)/power_mgmt
-include $(PWRMGMT_API_DIR)/Makefile

ifdef MALI_TEST_API
	TESTBENCH_BASE_UNIT_DIR=$(TESTBENCH_BASE_DIR)/unit_tests
	-include $(TESTBENCH_BASE_UNIT_DIR)/Makefile
endif
endif

endif # ARM_INTERNAL_BUILD
