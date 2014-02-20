ifeq ($(BOARD_HAS_SENSOR),true)

LOCAL_PATH := $(call my-dir)
include $(call first-makefiles-under,$(LOCAL_PATH))

endif
