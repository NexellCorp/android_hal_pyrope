ifeq ($(TARGET_BOARD_PLATFORM),pyrope)

LOCAL_PATH := $(call my-dir)
include $(call first-makefiles-under,$(LOCAL_PATH))

endif
