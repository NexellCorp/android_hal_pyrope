ifeq ($(BOARD_HAS_HAPTIC),true)

LOCAL_PATH := $(call my-dir)

GLOBAL_INCLUDE := $(LOCAL_PATH)/../driver \
                  $(LOCAL_PATH)/../include

include $(CLEAR_VARS)

LOCAL_MODULE            := haptictransport
LOCAL_C_INCLUDES        := $(GLOBAL_INCLUDE) \
                           $(LOCAL_PATH)/../transport

LOCAL_SRC_FILES         := ../transport/atomic.c \
                           ../transport/device.c \
                           ../transport/threadmutex.c \
                           ../api/c/hapticapi.c

LOCAL_LDLIBS            := -llog

LOCAL_CFLAGS            := -D__linux__
LOCAL_PRELINK_MODULE    := false

include $(BUILD_STATIC_LIBRARY)

#---------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE            := libhapticapi
LOCAL_C_INCLUDES        := $(GLOBAL_INCLUDE)

LOCAL_SRC_FILES         := ../api/c/hapticapi.c

LOCAL_LDLIBS            := -llog

LOCAL_STATIC_LIBRARIES  := haptictransport
LOCAL_CFLAGS            := -D__linux__

include $(BUILD_SHARED_LIBRARY)

#---------------------------------------------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE            := libhapticjavaapi
LOCAL_C_INCLUDES        := $(GLOBAL_INCLUDE)

LOCAL_SRC_FILES         := ../api/java/hapticjavaapi.c

LOCAL_LDLIBS            := -L$(LOCAL_PATH)/../libs/armeabi/ -llog
LOCAL_SHARED_LIBRARIES  := libhapticapi
LOCAL_CFLAGS            := -D__linux__

include $(BUILD_SHARED_LIBRARY)

endif