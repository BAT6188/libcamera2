ifeq ($(TARGET_BOARD_PLATFORM), xb4780)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	CameraCIMDevice.cpp \
	CameraV4L2Device.cpp \
	CameraCompressor.cpp \
	CameraColorConvert.cpp \
	CameraFaceDetect.cpp \
	CameraHalSelector.cpp \
	CameraHWModule.cpp

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/include \
	external/jpeg \
	external/jhead \
	external/skia/include/core/ \
	external/neven/FaceRecEm/common/src/b_FDSDK \
	frameworks/native/include \
	frameworks/native/include/media/hardware \
	frameworks/av/include \
	frameworks/av/services/camera/libcameraservice \
	frameworks/base/core/jni/android/graphics \
	frameworks/native/include/media/openmax \
	hardware/libhardware/include \
	hardware/ingenic/xb4780/libdmmu \
	hardware/ingenic/xb4780/libjzipu \
	hardware/ingenic/xb4780/hwcomposer-SGX540

include external/stlport/libstlport.mk

LOCAL_SHARED_LIBRARIES:= \
	libui \
	libskia \
	libandroid_runtime \
	libbinder \
	libcutils \
	libutils \
	libjpeg \
	libexif \
	liblog \
	libcamera_client \
	libstlport \
	libmedia \
	libFFTEm \
	libdmmu \
	libdl \
	libjzipu

LOCAL_CFLAGS += \
	-DCIM_CAMERA \
	-DCAMERA_INFO_MODULE=\"$(PRODUCT_MODEL)\" \
	-DCAMERA_INFO_MANUFACTURER=\"$(PRODUCT_MANUFACTURER)\"

#camera version config
ifeq ($(CAMERA_VERSION), 1)
LOCAL_SRC_FILES += \
	CameraHal1.cpp \
	JZCameraParameters.cpp \

LOCAL_CFLAGS += \
	-DCAMERA_VERSION1 \
	-DSOFTWARE_VALUE=\"Android4.1\"
endif

ifeq ($(CAMERA_VERSION), 2)
LOCAL_SRC_FILES += \
	CameraHal2.cpp \
	JZCameraParameters2.cpp
LOCAL_C_INCLUDES += \
	system/media/camera/include

LOCAL_SHARED_LIBRARIES += \
	libcamera_metadata \
	libcameraservice

LOCAL_CFLAGS += \
	-DCAMERA_VERSION2 \
	-DSOFTWARE_VALUE=\"Android4.2\"
endif

#camera face detect config
ifeq ($(FRONT_CAMERA_FACEDETECT),true)
LOCAL_CFLAGS += \
	-DCAMERA_FACEDETECT=\"3\"
else
LOCAL_CFLAGS += \
	-DCAMERA_FACEDETECT=\"0\"
endif

ifeq ($(BACK_CAMERA_FACEDETECT),true)
LOCAL_CFLAGS += \
	-DCAMERA_FACEDETECT=\"3\"
else
LOCAL_CFLAGS += \
	-DCAMERA_FACEDETECT=\"0\"
endif

# camera compress jpeg use hw config
ifeq ($(CAMERA_COMPRESS_JPEG_USE_HW), true)
LOCAL_CFLAGS += \
	-DCOMPRESS_JPEG_USE_HW \
	-DTHUMBNAIL_WIDTH=\"0\" \
	-DTHUMBNAIL_HEIGHT=\"0\"

LOCAL_SRC_FILES += \
	CameraCompressorHW.cpp
else
LOCAL_CFLAGS += \
	-DTHUMBNAIL_WIDTH=\"176\" \
	-DTHUMBNAIL_HEIGHT=\"144\"
endif

#camera recording use memcpy config
ifeq ($(CAMERA_COPY_MODE_RECORDING), true)
LOCAL_CFLAGS += \
	-DCOPY_RECORDING_MODE
endif

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

#LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/hw

LOCAL_MODULE:= camera.xb4780
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif
