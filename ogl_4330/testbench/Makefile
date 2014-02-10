#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2001-2002, 2005, 2007-2012 ARM Limited
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

TESTBENCH_DIR ?= .

ALL_TESTS=

ifneq ($(wildcard $(TPI_DIR)/*),)
	TPI_PLATFORM_ARM_ACH = $(MALI_TARGET_PLATFORM_ARM_ARCH)
	TPI_DIR=$(TESTBENCH_DIR)/tpi
	-include $(TPI_DIR)/Makefile
endif

ifdef MALI_TEST_API
	TESTBENCH_UNIT_TESTS_DIR=$(TESTBENCH_DIR)/unit_tests
	-include $(TESTBENCH_UNIT_TESTS_DIR)/Makefile
endif

TESTBENCH_API_TESTS_DIR=$(TESTBENCH_DIR)/api_tests
-include $(TESTBENCH_API_TESTS_DIR)/Makefile

TESTBENCH_BASE_DIR=$(TESTBENCH_DIR)/base
-include $(TESTBENCH_BASE_DIR)/Makefile

#ifdef USE_OPENGLES_1
#	TESTBENCH_OPENGLES_DIR=$(TESTBENCH_DIR)/opengles
#	-include $(TESTBENCH_OPENGLES_DIR)/Makefile
#endif

UNIT_FRAMEWORK_DIR=$(TESTBENCH_DIR)/unit_framework
-include $(UNIT_FRAMEWORK_DIR)/Makefile

-include $(TESTBENCH_DIR)/Makefile.run

.PHONY: suites
suites:MALI_MAKE_SUITE=1
suites:$(call source-dir-to-binary-dir,$(TARGET_SYSTEM),$(ALL_TESTS))

dummy=
