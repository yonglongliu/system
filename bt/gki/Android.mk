LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/common \
	$(LOCAL_PATH)/ulinux \
	$(LOCAL_PATH)/../btcore/include \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../osi/include \
	$(LOCAL_PATH)/../stack/include \
	$(LOCAL_PATH)/../utils/include \
	$(LOCAL_PATH)/../ \
	$(bdroid_C_INCLUDES)

LOCAL_CFLAGS += -Wno-error=unused-parameter $(bdroid_CFLAGS) -std=c99

ifeq ($(BOARD_HAVE_FM_BCM),true)
LOCAL_CFLAGS += \
	-DBOARD_HAVE_FM_BCM
endif

LOCAL_SRC_FILES := \
	./common/gki_buffer.c \
	./ulinux/gki_ulinux.c

LOCAL_MODULE := libbt-sprd_gki
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MULTILIB := 32

include $(BUILD_STATIC_LIBRARY)
