LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	main.cpp

LOCAL_STATIC_LIBRARIES := \
	libstagefright_color_conversion

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder \
	libui \
	libgui \
	liblog \
	libstagefright \
	libstagefright_foundation

LOCAL_C_INCLUDES := \
	frameworks/native/include/media/openmax \
	frameworks/native/libs/ui/include/ui \
	frameworks/av/include/media/stagefright \
	frameworks/av/media/libstagefright

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 24 && echo 0), 0)
LOCAL_CFLAGS := -DANDROID7
LOCAL_C_INCLUDES += \
	system/core/include/system
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo 0), 0)
LOCAL_CFLAGS := -DANDROID8
LOCAL_C_INCLUDES += \
	frameworks/native/libs/nativewindow/include/system
endif

LOCAL_MODULE := showYUV

LOCAL_MODULE_TAGS := tests
  
include $(BUILD_EXECUTABLE)
