LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# OpenCV
OPENCV_CAMERA_MODULES  := off
OPENCV_INSTALL_MODULES := off

OPENCV_LIB_TYPE := STATIC

include $(OPENCV_SDK_PATH)/sdk/native/jni/OpenCV.mk

# App
LOCAL_CXXFLAGS += -std=c++11 -Wall -Werror
LOCAL_CPP_EXTENSION := .cc .cpp .cxx
LOCAL_MODULE    := res_mara
LOCAL_SRC_FILES := main.cpp

include $(BUILD_EXECUTABLE)
