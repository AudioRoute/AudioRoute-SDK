# Note that NDK_MODULE_PATH must contain the audioroute parent directory. The
# makefile in AudioroutePcmSample implicitly takes care of this.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := demo_opensl_synth
LOCAL_LDLIBS := -llog
LOCAL_SRC_FILES := demo_opensl_synth.c
LOCAL_STATIC_LIBRARIES := audiomodule
include $(BUILD_SHARED_LIBRARY)
$(call import-module,Audioroute/jni)
