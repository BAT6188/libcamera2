/*
 * Camera HAL for Ingenic android 4.1
 *
 * Copyright 2012 Ingenic Semiconductor LTD.
 *
 * author: 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CAMERA_HW_HAL1_H_
#define __CAMERA_HW_HAL1_H_

#include "CameraHalCommon.h"
#include "CameraColorConvert.h"
#include "CameraFaceDetect.h"

#define X2D_NAME "/dev/x2d"
#define X2D_SCALE_FACTOR 512.0

namespace android {

    class CameraHal1 : public CameraHalCommon {

    public:

        CameraHal1(int id, CameraDeviceCommon* device);

        ~CameraHal1();

    public:
	  
        void update_device(CameraDeviceCommon* device);

        int module_open(const hw_module_t* module, const char* id, hw_device_t** device);

        int get_number_cameras(void);

        int get_cameras_info(int camera_id, struct camera_info* info);

    public:

        status_t initialize(void);
	  
        virtual status_t setPreviewWindow(struct preview_stream_ops *window);

        virtual void setCallbacks(camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void* user);

        virtual void enableMsgType(int32_t msg_type);

        virtual void disableMsgType(int32_t msg_type);

        virtual int isMsgTypeEnabled(int32_t msg_type);

        virtual status_t startPreview(void);

        virtual void stopPreview(void);

        virtual int isPreviewEnabled(void);

        virtual status_t storeMetaDataInBuffers(int enable);

        virtual status_t startRecording(void);

        virtual void stopRecording(void);

        virtual int isRecordingEnabled(void);

        virtual void releaseRecordingFrame(const void* opaque);

        virtual status_t setAutoFocus(void);

        virtual status_t cancelAutoFocus(void);

        virtual status_t takePicture(void);

        virtual status_t cancelPicture(void);

        virtual status_t setParameters(const char* parms);

        virtual char* getParameters(void);

        virtual void putParameters(char* params);

        virtual status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);

        virtual void releaseCamera(void);

        virtual status_t dumpCamera(int fd);

        virtual int deviceClose(void);

    private:
	  
        bool NegotiatePreviewFormat(struct preview_stream_ops* win);
        void initVideoHeap(int w, int h);
        void initPreviewHeap(int w, int h);

        int open_ipu_dev(void);

        int init_ipu_dev(int w, int h, int f);

        void close_ipu_dev(void);

        CameraDeviceCommon* getDevice(void) {
            return mDevice;
        }

        void ipu_convert_dataformat(CameraYUVMeta* yuvMeta,
                                    uint8_t* dst_buf, buffer_handle_t *buffer);

        void x2d_convert_dataformat(CameraYUVMeta* yuvMeta, 
                                    uint8_t* dst_buf, buffer_handle_t *buffer);

    private:

        mutable Mutex mlock;
        int mcamera_id;
        bool mirror;
        CameraDeviceCommon* mDevice;
        CameraColorConvert* ccc;
        camera_device_t* mCameraModuleDev;

        camera_notify_callback mnotify_cb;
        camera_data_callback mdata_cb ;
        camera_data_timestamp_callback mdata_cb_timestamp;
        camera_request_memory mget_memory;
        void* mcamera_interface;

        JZCameraParameters* mJzParameters;
        int32_t mMesgEnabled;
	  
        preview_stream_ops* mPreviewWindow;
        nsecs_t mPreviewAfter;
        int mRawPreviewWidth;
        int mRawPreviewHeight;

        int mPreviewFmt;
        int mDeviceDataFmt;
        int mPreviewFrameSize;
        camera_memory_t* mPreviewHeap;
        int mPreviewIndex;

        bool mPreviewEnabled;
        int mRecFmt;
        int mRecordingFrameSize;
        camera_memory_t* mRecordingHeap;
        int mRecordingindex;
        bool mVideoRecEnabled;
        bool mTakingPicture;

        int mPreviewWinFmt;
        int mPreviewWinWidth;
        int mPreviewWinHeight;

        nsecs_t mCurFrameTimestamp;
        nsecs_t mLastFrameTimestamp;
        CameraYUVMeta* mCurrentFrame;
        int mFaceCount;

        bool isSoftFaceDetectStart;
        struct ipu_image_info * mipu;
        bool ipu_open_status;
        bool first_init_ipu;
        int x2d_fd;
        int mdrop_frame;

    private:

        virtual bool thread_body_preview(void);
        virtual bool thread_body_capture(void);

        status_t freePreview();
        status_t readyToPreview();
        status_t readyToCapture();
        void cameraCapture();

        virtual void postFrameForPreview(void);
        virtual void postFrameForNotify(void);
        virtual void postJpegDataToApp(void);
        virtual void softCompressJpeg(void);
        virtual void hardCompressJpeg(void);
        virtual status_t fillCurrentFrame(uint8_t* img,buffer_handle_t* buffer);
        virtual status_t convertCurrentFrameToJpeg(camera_memory_t** jpeg_buff);
        virtual status_t softFaceDetectStart(int32_t detect_type);
        virtual status_t softFaceDetectStop(void);

	private:
        enum CAMERA_STATE {
            START_PREVIEW,
            RUN_PREVIEW,
            STOP_PREVIEW,
            FREE_PREVIEW,
            START_CAPTURE,
            RUN_CAPTURE,
            STOP_CAPTURE,
            CAPTURE_STOP_PREVIEW,
            CAPTURE_TO_PREVIEW,
            START_RECORD,
            RUN_RECORD,
            STOP_RECORD,

            CAMERA_NONE,
            CAMERA_ERROR,
        };
        CAMERA_STATE prev_camera_state;
        CAMERA_STATE camera_state;
        mutable Mutex camera_state_lock;
        Condition camera_state_cond;

        friend class WorkThread;
        class WorkThread : public Thread {

        private:
            CameraHal1* mCameraHal;
            bool mOnce;
            mutable Mutex release_recording_frame_lock;
            Condition release_recording_frame_condition;
            int mrelease_recording_frame;
            bool changed;
            nsecs_t start;
            nsecs_t timeout;

        public:
            enum ControlCmd {
                TIMEOUT,
                READY,
                EXIT_THREAD,
                THREAD_STOP,
                ERROR
            };


        public:
            inline explicit WorkThread(CameraHal1 * ch1)
                :Thread(false),
                 mCameraHal(ch1),
                 mOnce(false),
                 release_recording_frame_lock("WorkThread::lock"),
                 mrelease_recording_frame(0),
                 changed(true),
                 start(0),
                 timeout(3000000000LL)
            {
            }
	       
            inline ~WorkThread()
            {

            }

            inline status_t startThread(bool once)
            {
                mOnce = once;
                return run (NULL, ANDROID_PRIORITY_URGENT_DISPLAY, 0);
            }

            void threadResume(void) {
                AutoMutex lock(release_recording_frame_lock);
                changed = true;
                timeout = systemTime(SYSTEM_TIME_MONOTONIC) - start;
                mrelease_recording_frame--;
                release_recording_frame_condition.signal();
            }

            void threadPause(void) {
                AutoMutex lock(release_recording_frame_lock);

                if (changed) {
                    changed = false;
                    start = systemTime(SYSTEM_TIME_MONOTONIC);
                }

                while (mrelease_recording_frame == PREVIEW_BUFFER_CONUT) {
                    release_recording_frame_condition.waitRelative(release_recording_frame_lock,
                                                                   timeout);
                }

                mrelease_recording_frame++;
            }

            status_t readyToRun() {
                status_t res = NO_ERROR;
                changed = true;
                start = 0;
                timeout = 3000000000LL;
                mCameraHal->mdrop_frame = 0;

                return NO_ERROR;
            }

            status_t stopThread();

        private:
            bool threadLoop();
        };

        inline WorkThread* getWorkThread()
        {
            return mWorkerThread.get();
        }

    private:

        sp<WorkThread> mWorkerThread;

    private:

        bool startAutoFocus();
        friend class AutoFocusThread;
        class AutoFocusThread : public Thread {
        private:
            CameraHal1* mCameraHal;
               
        public:
            inline explicit AutoFocusThread(CameraHal1* ch1)
                :Thread(false)
            {
                mCameraHal = ch1;
            }

            inline status_t startThread()
            {
                return run (NULL, ANDROID_PRIORITY_URGENT_DISPLAY, 0);
            }

            inline status_t stopThread()
            {
                return requestExitAndWait();
            }

        private:
            bool threadLoop()
            {
                return mCameraHal->startAutoFocus();
            }
        };

    private:

        sp<AutoFocusThread> mFocusThread;

    private:
        friend class PostJpegUnit;
        class PostJpegUnit : public WorkQueue::WorkUnit {
	       
        private:

            CameraHal1* mhal;

        public:
            PostJpegUnit(CameraHal1* hal):
                WorkQueue::WorkUnit(),mhal(hal)
            {
            }
	       
            bool run() {
		    
                if (mhal != NULL) {
                    mhal->postJpegDataToApp();
                }
                return true;
            }
        };

    private:
        WorkQueue* mWorkerQueue;

    public:

        static camera_device_ops_t mCamera1Ops;

        /**
           Set the ANativeWindow to which preview frames are sent */
        static int set_preview_window(struct camera_device *,
                                      struct preview_stream_ops *window);

        /**
           Set the notification and data callbacks */
        static void set_callbacks(struct camera_device *,
                                  camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void *user);

        /**
         * The following three functions all take a msg_type, which is a bitmask of
         * the messages defined in include/ui/Camera.h
         */

        /**
         * Enable a message, or set of messages.
         */
        static void enable_msg_type(struct camera_device *, int32_t msg_type);

        /**
         * Disable a message, or a set of messages.
         *
         * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
         * HAL should not rely on its client to call releaseRecordingFrame() to
         * release video recording frames sent out by the cameral HAL before and
         * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
         * clients must not modify/access any video recording frame after calling
         * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
         */
        static void disable_msg_type(struct camera_device *, int32_t msg_type);

        /**
         * Query whether a message, or a set of messages, is enabled.  Note that
         * this is operates as an AND, if any of the messages queried are off, this
         * will return false.
         */
        static int msg_type_enabled(struct camera_device *, int32_t msg_type);

        /**
         * Start preview mode.
         */
        static int start_preview(struct camera_device *);

        /**
         * Stop a previously started preview.
         */
        static void stop_preview(struct camera_device *);

        /**
         * Returns true if preview is enabled.
         */
        static int preview_enabled(struct camera_device *);

        /**
         * Request the camera HAL to store meta data or real YUV data in the video
         * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
         * it is not called, the default camera HAL behavior is to store real YUV
         * data in the video buffers.
         *
         * This method should be called before startRecording() in order to be
         * effective.
         *
         * If meta data is stored in the video buffers, it is up to the receiver of
         * the video buffers to interpret the contents and to find the actual frame
         * data with the help of the meta data in the buffer. How this is done is
         * outside of the scope of this method.
         *
         * Some camera HALs may not support storing meta data in the video buffers,
         * but all camera HALs should support storing real YUV data in the video
         * buffers. If the camera HAL does not support storing the meta data in the
         * video buffers when it is requested to do do, INVALID_OPERATION must be
         * returned. It is very useful for the camera HAL to pass meta data rather
         * than the actual frame data directly to the video encoder, since the
         * amount of the uncompressed frame data can be very large if video size is
         * large.
         *
         * @param enable if true to instruct the camera HAL to store
         *        meta data in the video buffers; false to instruct
         *        the camera HAL to store real YUV data in the video
         *        buffers.
         *
         * @return OK on success.
         */
        static int store_meta_data_in_buffers(struct camera_device *, int enable);

        /**
         * Start record mode. When a record image is available, a
         * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
         * frame. Every record frame must be released by a camera HAL client via
         * releaseRecordingFrame() before the client calls
         * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
         * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
         * responsibility to manage the life-cycle of the video recording frames,
         * and the client must not modify/access any video recording frames.
         */
        static int start_recording(struct camera_device *);

        /**
         * Stop a previously started recording.
         */
        static void stop_recording(struct camera_device *);

        /**
         * Returns true if recording is enabled.
         */
        static int recording_enabled(struct camera_device *);

        /**
         * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
         *
         * It is camera HAL client's responsibility to release video recording
         * frames sent out by the camera HAL before the camera HAL receives a call
         * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
         * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
         * responsibility to manage the life-cycle of the video recording frames.
         */
        static void release_recording_frame(struct camera_device *,
                                            const void *opaque);

        /**
         * Start auto focus, the notification callback routine is called with
         * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
         * called again if another auto focus is needed.
         */
        static int auto_focus(struct camera_device *);

        /**
         * Cancels auto-focus function. If the auto-focus is still in progress,
         * this function will cancel it. Whether the auto-focus is in progress or
         * not, this function will return the focus position to the default.  If
         * the camera does not support auto-focus, this is a no-op.
         */
        static int cancel_auto_focus(struct camera_device *);

        /**
         * Take a picture.
         */
        static int take_picture(struct camera_device *);

        /**
         * Cancel a picture that was started with takePicture. Calling this method
         * when no picture is being taken is a no-op.
         */
        static int cancel_picture(struct camera_device *);

        /**
         * Set the camera parameters. This returns BAD_VALUE if any parameter is
         * invalid or not supported.
         */
        static int set_parameters(struct camera_device *, const char *parms);

        /** Retrieve the camera parameters.  The buffer returned by the camera HAL
            must be returned back to it with put_parameters, if put_parameters
            is not NULL.
        */
        static char *get_parameters(struct camera_device *);

        /** The camera HAL uses its own memory to pass us the parameters when we
            call get_parameters.  Use this function to return the memory back to
            the camera HAL, if put_parameters is not NULL.  If put_parameters
            is NULL, then you have to use free() to release the memory.
        */
        static void put_parameters(struct camera_device *, char *);

        /**
         * Send command to camera driver.
         */
        static int send_command(struct camera_device *,
                                int32_t cmd, int32_t arg1, int32_t arg2);

        /**
         * Release the hardware resources owned by this object.  Note that this is
         * *not* done in the destructor.
         */
        static void release(struct camera_device *);

        /**
         * Dump state of the camera hardware
         */
        static int dump(struct camera_device *, int fd);

        static int device_close(struct hw_device_t* device);
    };
};
#endif
