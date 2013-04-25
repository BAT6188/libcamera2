/*
 * Camera HAL for Ingenic android 4.1
 *
 * Copyright 2011 Ingenic Semiconductor LTD.
 *
 * author: 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define LOG_TAG  "CameraHal1"
#define DEBUG_HAL1 0
#define DEBUG_PREVIEW_THREAD 0
#define DEBUG_VIDEO_RECORDING 0
//#define CAMERA_USE_SOFT

#include "CameraHalSelector.h"
#include "CameraFaceDetect.h"

#ifndef PIXEL_FORMAT_YV16
#define PIXEL_FORMAT_YV16  0x36315659 /* YCrCb 4:2:2 Planar */
#endif

namespace android{

    CameraHal1::CameraHal1(int id, CameraDeviceCommon* device)
        :mlock("CameraHal1::lock"),
         mcamera_id(id),
         mirror(false),
         mDevice(device),
         ccc(NULL),
         mCameraModuleDev(NULL),
         mnotify_cb(NULL),
         mdata_cb(NULL),
         mdata_cb_timestamp(NULL),
         mget_memory(NULL),
         mcamera_interface(NULL),
         mJzParameters(NULL),
         mMesgEnabled(0),
         mPreviewWindow(NULL),
         mPreviewAfter(0),
         mRawPreviewWidth(0),
         mRawPreviewHeight(0),
         mPreviewFmt(HAL_PIXEL_FORMAT_YV12),
         mPreviewFrameSize(0),
         mPreviewHeap(NULL),
         mPreviewIndex(0),
         mPreviewEnabled(false),
         mRecFmt(HAL_PIXEL_FORMAT_JZ_YUV_420_B),
         mRecordingFrameSize(0),
         mRecordingHeap(NULL),
         mRecordingindex(0),
         mVideoRecEnabled(false),
         mTakingPicture(false),
         mPreviewWinFmt(HAL_PIXEL_FORMAT_RGB_565),
         mPreviewWinWidth(0),
         mPreviewWinHeight(0),
         mCurFrameTimestamp(0),
         mCurrentFrame(NULL),
         mFaceCount(0),
         isSoftFaceDetectStart(false),
         mipu(NULL),
         ipu_open_status(false),
         first_init_ipu(true),
         x2d_fd(-1),
         mdrop_frame(0),
         camera_state(CAMERA_NONE),
         mWorkerThread(NULL),
         mFocusThread(NULL) {

        if (NULL != mDevice) {
            ccc = new CameraColorConvert();

            mCameraModuleDev = new camera_device_t();
            if (mCameraModuleDev != NULL) {
                mCameraModuleDev->common.tag = HARDWARE_DEVICE_TAG;
                mCameraModuleDev->common.version = HARDWARE_DEVICE_API_VERSION(1,0);
                mCameraModuleDev->common.close = CameraHal1::device_close;
                memset(mCameraModuleDev->common.reserved, 0, 37 - 2);
            }
            mJzParameters = new JZCameraParameters(mDevice, mcamera_id);
            if (mJzParameters == NULL) {
                ALOGE("%s: create parameters object fail",__FUNCTION__);
            }
            mWorkerThread = new WorkThread(this);
            if (getWorkThread() == NULL) {
                ALOGE("%s: could not create work thread", __FUNCTION__);
            }
            mFocusThread = new AutoFocusThread(this);
            if (mFocusThread.get() == NULL) {
                ALOGE("%s: could not create focus thread", __FUNCTION__);
            }
            mWorkerQueue = new WorkQueue(10,false);
            if (mWorkerQueue == NULL) {
                ALOGE("%s: could not create WorkQueue",__FUNCTION__);
            }
        }
    }

    CameraHal1::~CameraHal1()
    {
        if (ccc != NULL) {
            delete ccc;
            ccc = NULL;
        }

        if (NULL != mCameraModuleDev) {
            delete mCameraModuleDev;
            mCameraModuleDev = NULL;
        }

        if (mJzParameters != NULL) {
            delete mJzParameters;
            mJzParameters = NULL;
        }

        if (getWorkThread() != NULL) {
            getWorkThread()->stopThread();
            mWorkerThread.clear();
        }
        if (mFocusThread.get() != NULL) {
            mFocusThread.get()->stopThread();
            mFocusThread.clear();
        }
          
        if (mWorkerQueue != NULL) {
            mWorkerQueue->cancel();
            mWorkerQueue->finish();
            delete mWorkerQueue;
            mWorkerQueue = NULL;
        }

        if (mPreviewHeap) {
            mPreviewHeap->release(mPreviewHeap);
            mPreviewHeap = NULL;
        }

        if (mRecordingHeap) {
            mRecordingHeap->release(mRecordingHeap);
            mRecordingHeap = NULL;
        }

        delete CameraFaceDetect::getInstance();

    }

    void CameraHal1::update_device(CameraDeviceCommon* device) {
        mDevice = device;
        ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
    }

    int CameraHal1::module_open(const hw_module_t* module, const char* id, hw_device_t** device) {
        status_t ret = NO_MEMORY;

        if ((NULL == mDevice) || (atoi(id) != mcamera_id)) {
            ALOGE("%s: create camera device fail",__FUNCTION__);
            return ret;
        }

        if (NULL != mCameraModuleDev) {
            mCameraModuleDev->common.module = const_cast<hw_module_t*>(module);
            mCameraModuleDev->ops = &(CameraHal1::mCamera1Ops);
            mCameraModuleDev->priv = this;
            *device = &(mCameraModuleDev->common);
            ret = initialize();
        }
        return ret;
    }

    int CameraHal1::get_number_cameras(void) {
        if (NULL != mDevice)
            return mDevice->getCameraNum();
        else
            return 0;
    }

    int CameraHal1::get_cameras_info(int camera_id, struct camera_info* info) {
        status_t ret = BAD_VALUE;

        if (mcamera_id != camera_id) {
            ALOGE("%s: you will get id = %d, but mcamra_id = %d",__FUNCTION__, camera_id, mcamera_id);
            return ret;
        }

        ALOGE_IF(DEBUG_HAL1,"%s: Enter",__FUNCTION__);
        info->device_version = CAMERA_MODULE_API_VERSION_1_0;
        info->static_camera_characteristics = (camera_metadata_t*)0xcafef00d;
        if (NULL != mDevice) {
            ALOGE_IF(DEBUG_HAL1,"%s: will getCameraModuleInfo id = %d, ",__FUNCTION__,camera_id);
            ret = mDevice->getCameraModuleInfo(mcamera_id, info);
            if (ret == NO_ERROR && info->facing == CAMERA_FACING_FRONT) {
                mirror = true;
            }
        }
        return ret;
    }

    status_t CameraHal1::initialize() {
        status_t ret = NO_ERROR;
        mDevice->connectDevice(mcamera_id);
        mJzParameters->initDefaultParameters(mirror?CAMERA_FACING_FRONT:CAMERA_FACING_BACK);
        return ret;
    }

    status_t CameraHal1::setPreviewWindow(struct preview_stream_ops *window) {
        status_t res = NO_ERROR;

        AutoMutex lock(mlock);

        int preview_fps = mJzParameters->getCameraParameters().getPreviewFrameRate();
        mRawPreviewWidth = mRawPreviewHeight = 0;
        mPreviewAfter = 0;

        if (window != NULL) {
            res = window->set_usage(window,GRALLOC_USAGE_SW_WRITE_OFTEN);
            if (res == NO_ERROR) {
                mPreviewAfter = 1000000000LL / preview_fps;
            } else {
                window = NULL;
                res = -res;
                ALOGE("%s: set preview window usage %d -> %s",
                      __FUNCTION__, res, strerror(res));
            }
        }
        mPreviewWindow = window;

        if ( ((camera_state == RUN_PREVIEW) || (camera_state == START_PREVIEW))  && window != 0) {
            ALOGE_IF(DEBUG_HAL1,"%s: Negotiating preview format",__FUNCTION__);
            NegotiatePreviewFormat(window);
        }
        ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        return res;
    }

    void CameraHal1::setCallbacks(camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void* user) {
        AutoMutex lock(mlock);

        mnotify_cb = notify_cb;
        mdata_cb = data_cb;
        mdata_cb_timestamp = data_cb_timestamp;
        mget_memory = get_memory;
        mcamera_interface = user;

        return;
    }

    void CameraHal1::enableMsgType(int32_t msg_type) {
        const char* valstr;
        int32_t old = 0;

        valstr = mJzParameters->getCameraParameters().get(CameraParameters::KEY_ZOOM_SUPPORTED);
        if ((NULL != valstr) && (strcmp(valstr,"false") == 0)) {
            AutoMutex lock(mlock);
            msg_type &=~CAMERA_MSG_ZOOM;
        }
        {
            AutoMutex lock(mlock);
            old = mMesgEnabled;
            mMesgEnabled |= msg_type;
        }

        if ((msg_type & CAMERA_MSG_VIDEO_FRAME) &&
            (mMesgEnabled ^ old) & CAMERA_MSG_VIDEO_FRAME && mVideoRecEnabled) {
            ALOGE_IF(DEBUG_HAL1,"You must alloc preview buffer for video recording ");
        }
    }

    void CameraHal1::disableMsgType(int32_t msg_type) {
        int32_t old = 0;

        {
            AutoMutex lock(mlock);
            old = mMesgEnabled;
            mMesgEnabled &= ~msg_type;
        }

        if ((msg_type & CAMERA_MSG_VIDEO_FRAME) &&
            (mMesgEnabled^old) & CAMERA_MSG_VIDEO_FRAME && mVideoRecEnabled) {
            ALOGE_IF(DEBUG_HAL1,"You must alloc preview buffer for video recording.");
        }

        return;
    }

    int CameraHal1::isMsgTypeEnabled(int32_t msg_type) {
        AutoMutex lock(mlock);
        int enable = (mMesgEnabled & msg_type) == msg_type;
        return enable;
    }

	status_t CameraHal1::startPreview() {

		ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);

		status_t res = NO_ERROR;
		camera_state_lock.lock();
		if ( camera_state == START_PREVIEW || camera_state == RUN_PREVIEW )
            {
                camera_state_lock.unlock();
                ALOGE("preview started... state:%d", camera_state);
                return NO_ERROR;
            }
		camera_state_lock.unlock();

		getWorkThread()->stopThread();

		camera_state_lock.lock();
		camera_state = START_PREVIEW;
		camera_state_lock.unlock();

		res = getWorkThread()->startThread(false);
        if (res != NO_ERROR) {
            ALOGE("%s: start preview thread error",__FUNCTION__);
		    camera_state = CAMERA_ERROR;
		    return UNKNOWN_ERROR;
        }

        return NO_ERROR;
	}


    status_t CameraHal1::readyToPreview() {

        ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        status_t res = NO_ERROR;
        const char* is_video = NULL;
        bool isSupportVideoSnapShort = false;                    
        int width = 0, height = 0;
        const char* format = NULL;

        int64_t startTime = 0;
        int64_t workTime= 0;


        res = mDevice->connectDevice(mcamera_id);
        if (res != NO_ERROR) {
            return UNKNOWN_ERROR;
        }

        isSupportVideoSnapShort = (strcmp(mJzParameters->getCameraParameters()
                                          .get(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED),
                                          "true") == 0);
        is_video = mJzParameters->getCameraParameters().get(CameraParameters::KEY_RECORDING_HINT);
        if ((is_video != NULL) && (strcmp(is_video, "true") == 0)) {
            //mJzParameters->getCameraParameters().getVideoSize(&width, &height);
            mJzParameters->getCameraParameters().getPreviewSize(&width, &height);
            initVideoHeap(width, height);
        } else {
            mJzParameters->getCameraParameters().getPreviewSize(&width, &height);
            initPreviewHeap(width, height);
        }

        ALOGE_IF(DEBUG_HAL1,"%s: isVideoRecEnabled = %s", __FUNCTION__,  
                 mVideoRecEnabled ? "true" : "false");
        ALOGE_IF(DEBUG_HAL1,"mRawPrivewWidth = %d, mRawPrivewHeiht = %d", 
                 mRawPreviewWidth, mRawPreviewHeight);

        format = mJzParameters->getCameraParameters().getPreviewFormat();

        if (format == NULL) {
            ALOGE("cann't get the preiview format!");
            return  UNKNOWN_ERROR;
        }

        ALOGE_IF(DEBUG_HAL1,"allocateStream preview_buffer");
        res = mDevice->allocateStream(PREVIEW_BUFFER,mget_memory,width,height,
                                      mDevice->getPreviewFormat());
        if (res != NO_ERROR) {
            ALOGE("allcate preview mem failed");;
            return res;
        }

        ALOGE_IF(DEBUG_HAL1,"startDevice");
	  
        startTime = systemTime(SYSTEM_TIME_MONOTONIC);
        res = mDevice->startDevice();
        if (res != NO_ERROR) {
            ALOGE("startDevice failed\n");
            mDevice->freeStream(PREVIEW_BUFFER);
            return UNKNOWN_ERROR;
        }
         
        if (mPreviewWindow != 0) {
            ALOGE_IF(DEBUG_HAL1,"%s: Negotiating preview format",__FUNCTION__);
            NegotiatePreviewFormat(mPreviewWindow);
        }

        mDevice->sendCommand(FOCUS_INIT);

        if (mDevice->usePmem() == false) {
            x2d_fd = open (X2D_NAME, O_RDWR);
            if (x2d_fd < 0) {
                ALOGE("%s: open %s error, %s",
                      __FUNCTION__, X2D_NAME, strerror(errno));
                return UNKNOWN_ERROR;
            }

            if (open_ipu_dev() < 0) {
                ALOGE("%s: open ipu dev error",__FUNCTION__);
                return UNKNOWN_ERROR;
            }
        } else {
            if (open_ipu_dev() < 0) {
                ALOGE("%s: open ipu dev error",__FUNCTION__);
                return UNKNOWN_ERROR;
            }
        }

        workTime = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;

        ALOGE_IF(DEBUG_HAL1,"readyToPreview use time : %lld us", workTime/1000); 
     
        return NO_ERROR;
    }

	status_t CameraHal1::freePreview() {

		ALOGE_IF(DEBUG_HAL1,"freePreview");

		status_t ret = NO_ERROR;
		
		ret = mDevice->stopDevice();
		mPreviewIndex = 0;

		isSoftFaceDetectStart = false;
        mDevice->freeStream(PREVIEW_BUFFER);

		if (mDevice->usePmem() == false) {
			if (x2d_fd > 0)
				close(x2d_fd);
			x2d_fd = -1;
			close_ipu_dev();
		} else {
			close_ipu_dev();
		}

		return NO_ERROR;
	}

    void CameraHal1::stopPreview() {
        ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        getWorkThread()->stopThread();
        return;
    }

    int CameraHal1::isPreviewEnabled() {
        return camera_state == RUN_PREVIEW || camera_state == START_PREVIEW;
    }

    status_t CameraHal1::storeMetaDataInBuffers(int enable) {
        return enable ? INVALID_OPERATION : NO_ERROR;
    }

    status_t CameraHal1::startRecording() {

        ALOGE_IF(1,"Enter %s mVideoRecEnable=%s",__FUNCTION__,mVideoRecEnabled?"true":"false");
        if (mVideoRecEnabled == false) {
            AutoMutex lock(mlock);
            mVideoRecEnabled = true;
            mRecordingindex = 0;
            ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        }

        return NO_ERROR;
    }

    void CameraHal1::stopRecording() {


        ALOGE_IF(DEBUG_HAL1,"Enter %s mVideoRecEnable=%s",
                 __FUNCTION__,mVideoRecEnabled?"true":"false");

        if (mVideoRecEnabled) {
            getWorkThread()->threadResume();
            AutoMutex lock(mlock);
            mVideoRecEnabled = false;
            mRecordingindex = 0;
            ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        }

    }

    int CameraHal1::isRecordingEnabled() {

        ALOGE_IF(DEBUG_HAL1,"Enter %s mVideoRecEnable=%s",
                 __FUNCTION__,mVideoRecEnabled?"true":"false");
        int enabled;
        {
            AutoMutex lock(mlock);
            enabled = mVideoRecEnabled; 
        }
        return enabled;
    }

    void CameraHal1::releaseRecordingFrame(const void* opaque) {
        getWorkThread()->threadResume();
        return ;
    }

    status_t CameraHal1::setAutoFocus() {

        status_t ret = NO_ERROR;

        AutoMutex lock(mlock);
        mDevice->sendCommand(PAUSE_FACE_DETECT);
        if (mFocusThread.get() != NULL)
            ret = mFocusThread.get()->startThread();
        return ret;
    }

    status_t CameraHal1::cancelAutoFocus() {

        status_t ret = NO_ERROR;
          
        if (mFocusThread.get() != NULL)
            ret = mFocusThread.get()->stopThread();
        return ret;
    }

    bool CameraHal1::startAutoFocus() {

        status_t ret = NO_ERROR;
        if (mMesgEnabled & CAMERA_MSG_FOCUS) {

            ret = mDevice->sendCommand(START_FOCUS);
            if (ret != NO_ERROR)
                {
                    if (mMesgEnabled & CAMERA_MSG_FOCUS)
                        mnotify_cb(CAMERA_MSG_FOCUS,0,0,mcamera_interface);
                    if (mMesgEnabled & CAMERA_MSG_FOCUS_MOVE)
                        {
                            int focus_state = mDevice->sendCommand(GET_FOCUS_STATUS);
                            if (focus_state == NO_ERROR)
                                mnotify_cb(CAMERA_MSG_FOCUS_MOVE, false, 0, mcamera_interface);
                            else
                                mnotify_cb(CAMERA_MSG_FOCUS_MOVE, false, 0, mcamera_interface);
                        }
                } else  if (mMesgEnabled & CAMERA_MSG_FOCUS)                    
                mnotify_cb(CAMERA_MSG_FOCUS,1, 0, mcamera_interface);
        }

        ALOGE_IF(DEBUG_HAL1,"AutoFocus thread exit");
        return false;
    }

	status_t CameraHal1::readyToCapture() {

		ALOGE_IF(DEBUG_HAL1,"readyToCapture");
		status_t res = NO_ERROR;
		int width = 0, height = 0;

		if (mJzParameters->getPropertyPictureSize(&width, &height) < 0) {	
			mJzParameters->getCameraParameters().getPictureSize(&width, &height);
		}

		res = mDevice->allocateStream(CAPTURE_BUFFER,mget_memory,width, height,
                                      mDevice->getCaptureFormat());
		if (res != NO_ERROR) {
			ALOGE("alloca capture buffer fail");
			return res;
		}

		res = mDevice->sendCommand(TAKE_PICTURE, width, height);
		if (res != NO_ERROR) {
			ALOGE("sendCommand(TAKE_PICTURE) failed!");
			return res;
		}

		return NO_ERROR;
	}

	status_t CameraHal1::takePicture() {

		ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
		status_t res = NO_ERROR;
		camera_state_lock.lock();
		if ( camera_state == START_CAPTURE || camera_state == RUN_CAPTURE ) {
			camera_state_lock.unlock();
			return NO_ERROR;
		}

		camera_state_lock.unlock();

		stopPreview();
		camera_state_lock.lock();
		camera_state = START_CAPTURE;
		camera_state_lock.unlock();

		res = getWorkThread()->startThread(true);
		if (res != NO_ERROR) {
			ALOGE("%s: start preview thread error",__FUNCTION__);
			return UNKNOWN_ERROR;
		}

		return NO_ERROR;
    }

    status_t CameraHal1::cancelPicture() {
        ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        getWorkThread()->stopThread();
        return NO_ERROR;
    }

    status_t CameraHal1::setParameters(const char* parms) {

        status_t res = NO_ERROR;
          
        AutoMutex lock(mlock);
        String8 str_param(parms);
        res = mJzParameters->setParameters(str_param);
          
        return res;
    }

    static char noParams = '\0';
    char* CameraHal1::getParameters() {

        String8 params(mJzParameters->getCameraParameters().flatten());
        char* ret_str = reinterpret_cast<char*>(malloc(sizeof(char) * params.length()+1));
        memset(ret_str, 0, params.length()+1);
        if (ret_str != NULL)
            {
                memcpy(ret_str, params.string(), params.length()+1);
                return ret_str;
            }
        return &noParams;
    }

    void CameraHal1::putParameters(char* params) {

        if (NULL != params && params != &noParams)
            {
                free(params);
                params = NULL;
            }
    }

    status_t CameraHal1::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2) {

        status_t res = NO_ERROR;

        AutoMutex lock(mlock);

        switch(cmd)
            {
            case CAMERA_CMD_ENABLE_FOCUS_MOVE_MSG:
                bool enable = static_cast<bool>(arg1);
                AutoMutex lock(mlock);
                if (enable) {

                    mMesgEnabled |= CAMERA_MSG_FOCUS_MOVE;
                } else {
                    mMesgEnabled &= ~CAMERA_MSG_FOCUS_MOVE;
                }
                return res;                    
            }
          
        if (camera_state != RUN_PREVIEW) {
            ALOGE("%s: Preview is not running",__FUNCTION__);
            return INVALID_OPERATION;         
        }
        switch(cmd)
            {
            case CAMERA_CMD_START_SMOOTH_ZOOM:
                res = mDevice->sendCommand(START_ZOOM);
                break;
            case CAMERA_CMD_STOP_SMOOTH_ZOOM:
                res = mDevice->sendCommand(STOP_ZOOM);
                break;
            case CAMERA_CMD_START_FACE_DETECTION:
                res = softFaceDetectStart(arg1);
                // res = mDevice->sendCommand(START_FACE_DETECT);
                break;
            case CAMERA_CMD_STOP_FACE_DETECTION:
                res = softFaceDetectStop();
                //  res = mDevice->sendCommand(STOP_FACE_DETECT);
                break;
            default:
                break;
            }
        ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        return res;
    }

    void CameraHal1::releaseCamera() {

        stopPreview();
        stopRecording();
        cancelPicture();
        cancelAutoFocus();
          
        AutoMutex lock(mlock);
        mMesgEnabled = 0;
        mnotify_cb = NULL;
        mdata_cb = NULL;
        mdata_cb_timestamp = NULL;
        mget_memory = NULL;

        ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        return ;
    }

    status_t CameraHal1::dumpCamera(int fd) {

        char buffer[256];
        int offset = 0;
        String8 msg;

        memset(buffer, 0, 256);
        snprintf(buffer, 256, "status: previewEnable=%s,",mPreviewEnabled?"true":"false");
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer, 256, "mTakingPicture=%s,",mTakingPicture?"true":"false");
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer, 256, "mPreviewAfter=%lld,",mPreviewAfter);
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer,256, "mPreviewWidth=%d,mPreviewHeight=%d,",
                 mRawPreviewWidth, mRawPreviewHeight);
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer, 256, "mVideoRecordingEnable=%s,",mVideoRecEnabled?"true":"false");
        msg.append(buffer);
        msg.append(mJzParameters->getCameraParameters().flatten());
        write(fd, msg.string(),msg.length());

        ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        return NO_ERROR;
    }

    int CameraHal1::deviceClose(void) {		 

        releaseCamera();
        mDevice->disConnectDevice();
		  
        if (mPreviewHeap) {
            struct dmmu_mem_info dmmu_info;
            memset(&dmmu_info, 0, sizeof(struct dmmu_mem_info));
            mPreviewFrameSize = 0;
            dmmu_info.vaddr = mPreviewHeap->data;
            dmmu_info.size = mPreviewHeap->size;
            dmmu_unmap_user_memory(&dmmu_info);
            mPreviewHeap->release(mPreviewHeap);
            mPreviewHeap = NULL;
        }
		  
        if (mRecordingHeap) {
            struct dmmu_mem_info dmmu_info;
            memset(&dmmu_info, 0, sizeof(struct dmmu_mem_info));
            mRecordingFrameSize = 0;
            dmmu_info.vaddr = mRecordingHeap->data;
            dmmu_info.size = mRecordingHeap->size;
            dmmu_unmap_user_memory(&dmmu_info);
            mRecordingHeap->release(mRecordingHeap);
            mRecordingHeap = NULL;
        }

        return NO_ERROR;
    }

    bool CameraHal1::NegotiatePreviewFormat(struct preview_stream_ops* win) {

        int pw = 0, ph = 0;
        bool isRecVideo = false;
        bool isSupportVideoSnapShort = false;

        ALOGE_IF(DEBUG_HAL1,"%s: mVideoRecEnabed = %s, video hint = %s, (mMesg & video_frame) = %s",
                 __FUNCTION__, mVideoRecEnabled?"true":"false", 
                 mJzParameters->getCameraParameters().get(CameraParameters::KEY_RECORDING_HINT), 
                 ((mMesgEnabled & CAMERA_MSG_VIDEO_FRAME) == CAMERA_MSG_VIDEO_FRAME) ? "true" : "false");

        if ((mRawPreviewWidth == 0) || (mRawPreviewHeight == 0)) {
            isRecVideo = (strcmp(mJzParameters->getCameraParameters()
                                 .get(CameraParameters::KEY_RECORDING_HINT), "true") == 0);
            if (isRecVideo){
                //mJzParameters->getCameraParameters().getVideoSize(&pw, &ph);
                mJzParameters->getCameraParameters().getPreviewSize(&pw, &ph);
            } else {
                mJzParameters->getCameraParameters().getPreviewSize(&pw, &ph);
            }
        } else {
            pw = mRawPreviewWidth;
            ph = mRawPreviewHeight;
        }

        mPreviewWinFmt = 0;
        mPreviewWinWidth = 0;
        mPreviewWinHeight = 0;

        if (win->set_buffers_geometry(win,pw,ph,HAL_PIXEL_FORMAT_RGB_565) != NO_ERROR) {
            ALOGE("Unable to set buffer geometry");
            return false;
        }

        mPreviewWinFmt = HAL_PIXEL_FORMAT_RGB_565;
        mPreviewWinWidth = pw;
        mPreviewWinHeight = ph;

        ALOGE_IF(DEBUG_HAL1,"%s: previewWindow: %dx%d", 
                 __FUNCTION__, mPreviewWinWidth, mPreviewWinHeight);

        return true;
    }

    void CameraHal1::initVideoHeap(int width, int height) {

        int video_width = width, video_height = height;
        const char *format;
        int how_recording_big = 0;
        struct dmmu_mem_info dmmu_info;

        if (!mget_memory) {
            ALOGE("No memory allocator available");
            return;
        }

        memset(&dmmu_info, 0, sizeof(struct dmmu_mem_info));
        //mJzParameters->getCameraParameters().getVideoSize(&video_width, &video_height);
        //mJzParameters->getCameraParameters().getPreviewSize(&video_width, &video_height);
        format = mJzParameters->getCameraParameters().get(CameraParameters::KEY_VIDEO_FRAME_FORMAT);

        if (mRawPreviewWidth != video_width || mRawPreviewHeight != video_height) {
            mRawPreviewWidth = video_width;
            mRawPreviewHeight = video_height;
        }

        if(strcmp(format,JZCameraParameters::PIXEL_FORMAT_JZ__YUV420T) == 0) {
            mRecFmt = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
            how_recording_big = (video_width * video_height * 2);
        } else if (strcmp(format,JZCameraParameters::PIXEL_FORMAT_JZ__YUV420P) == 0) {
            mRecFmt = HAL_PIXEL_FORMAT_JZ_YUV_420_P;
            how_recording_big = (video_width * video_height * 12/8);
        }else if (strcmp(format ,CameraParameters::PIXEL_FORMAT_YUV422I) == 0) {
            mRecFmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
            how_recording_big = video_width * video_height << 1;
        } else if(strcmp(format,CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
            mRecFmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            how_recording_big = (video_width * video_height * 12/8);
        } else if (strcmp(format, CameraParameters::PIXEL_FORMAT_YUV422SP) == 0) {
            mRecFmt = HAL_PIXEL_FORMAT_YCbCr_422_SP;
            how_recording_big = (video_width * video_height * 3) >> 1;
        } else if (strcmp(format, CameraParameters::PIXEL_FORMAT_YUV420P) == 0) {
            mRecFmt = HAL_PIXEL_FORMAT_YV12;
            int stride = (video_width + 15) & (-16);
            int y_size = stride * video_height;
            int c_stride = ((stride >> 1) + 15) & (-16);
            int c_size = c_stride * video_height >> 1;
            int cr_offset = y_size;
            int cb_offset = y_size + c_size;
            int size = y_size + (c_size << 1);

            how_recording_big = size;
        }

        if (how_recording_big != mRecordingFrameSize) {
            mRecordingFrameSize = how_recording_big;
                
            if (mRecordingHeap) {
                dmmu_info.vaddr = mRecordingHeap->data;
                dmmu_info.size = mRecordingHeap->size;
                dmmu_unmap_user_memory(&dmmu_info);
                mRecordingHeap->release(mRecordingHeap);
                mRecordingHeap = NULL;
            }
			   
            mRecordingHeap = mget_memory(-1, mRecordingFrameSize,RECORDING_BUFFER_NUM, NULL);
            dmmu_info.vaddr = mRecordingHeap->data;
            dmmu_info.size = mRecordingHeap->size;

            {
                for (int i = 0; i < (int)(mRecordingHeap->size); i += 0x1000) {
                    ((uint8_t*)(mRecordingHeap->data))[i] = 0;
                }
                ((uint8_t*)(mRecordingHeap->data))[mRecordingHeap->size - 1] = 0;
            }

            dmmu_map_user_memory(&dmmu_info);
            ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        }
    }

    void CameraHal1::initPreviewHeap(int width, int height) {

        int preview_width = width, preview_height = height;
        int how_preview_big = 0;
        const char* format = NULL;
        struct dmmu_mem_info dmmu_info;

        if (!mget_memory) {
            ALOGE("No memory allocator available");
            return;
        }

        memset(&dmmu_info, 0, sizeof(struct dmmu_mem_info));
        mJzParameters->getCameraParameters().getPreviewSize(&preview_width, &preview_height);
        format = mJzParameters->getCameraParameters().getPreviewFormat();

        if (mRawPreviewWidth != preview_width || mRawPreviewHeight != preview_height) {
            mRawPreviewWidth = preview_width;
            mRawPreviewHeight = preview_height;
        }

        if (strcmp(format, JZCameraParameters::PIXEL_FORMAT_JZ__YUV420T) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
            how_preview_big = (preview_width * preview_height * 12/8);
        } else if (strcmp(format, JZCameraParameters::PIXEL_FORMAT_JZ__YUV420P) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_JZ_YUV_420_P;
            how_preview_big = (preview_width * preview_height * 12/8);
        }else if (strcmp(format ,CameraParameters::PIXEL_FORMAT_YUV422I) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
            how_preview_big = preview_width * preview_height << 1;
        } else if(strcmp(format,CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            how_preview_big = (preview_width * preview_height * 12/8);
        } else if (strcmp(format, CameraParameters::PIXEL_FORMAT_YUV422SP) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_YCbCr_422_SP;
            how_preview_big = (preview_width * preview_height * 12/8);
        } else if (strcmp(format, CameraParameters::PIXEL_FORMAT_YUV420P) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_YV12;
            int stride = (preview_width + 15) & (-16);
            int y_size = stride * preview_height;
            int c_stride = ((stride >> 1) + 15) & (-16);
            int c_size = c_stride * preview_height >> 1;
            int cr_offset = y_size;
            int cb_offset = y_size + c_size;
            int size = y_size + (c_size << 1);

            how_preview_big = size;
        } 

        if (how_preview_big != mPreviewFrameSize) {
            mPreviewFrameSize = how_preview_big;

            if (mPreviewHeap) {
                mPreviewIndex = 0;
                dmmu_info.vaddr = mPreviewHeap->data;
                dmmu_info.size = mPreviewHeap->size;
                dmmu_unmap_user_memory(&dmmu_info);
                mPreviewHeap->release(mPreviewHeap);
                mPreviewHeap = NULL;
            }

            mPreviewHeap = mget_memory(-1, mPreviewFrameSize,PREVIEW_BUFFER_CONUT,NULL);
            dmmu_info.vaddr = mPreviewHeap->data;
            dmmu_info.size = mPreviewHeap->size;

            {
                for (int i = 0; i < (int)(mPreviewHeap->size); i += 0x1000) {
                    ((uint8_t*)(mPreviewHeap->data))[i] = 0;
                }
                ((uint8_t*)(mPreviewHeap->data))[mPreviewHeap->size - 1] = 0;
            }
			  
            dmmu_map_user_memory(&dmmu_info);

            ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);
        }

    }

	bool CameraHal1::thread_body_capture(void) {

		ALOGE_IF(DEBUG_HAL1,"%s: line=%d",__FUNCTION__,__LINE__);

		int64_t startTime = 0;
		void *src = NULL;
		int64_t workTime;
		int64_t timeout;
	
		startTime = systemTime(SYSTEM_TIME_MONOTONIC);
		
		mDevice->flushCache(NULL);
		
		src = mDevice->getCurrentFrame(true); //40ms
		
		if (src == NULL) {
			ALOGE("%s: current frame is null",__FUNCTION__);
			return false;
		}

		mCurrentFrame = (CameraYUVMeta*)src;

		if ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) && ccc) {
			ccc->cimyuv420b_to_tile420(mCurrentFrame); //1- 4ms
		}

		mCurFrameTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);

		cameraCapture();

		workTime = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;
		timeout = 80000000 - workTime;
		if ( timeout > 1000000 )
			usleep(timeout/1000);

		ALOGE_IF(DEBUG_HAL1,"%s: fps = %d, workTime = %lld ", __FUNCTION__,
		      mJzParameters->getCameraParameters().getPreviewFrameRate(),
		      workTime);

		return true;
	}

	bool CameraHal1::thread_body_preview(void) {

		void* src = NULL;
		int64_t timeout = 0;
		int64_t startTime = 0;
		int64_t workTime = 0;

		static int frame_count = 0;

		startTime = systemTime(SYSTEM_TIME_MONOTONIC);

		mDevice->flushCache(NULL);
		
		src = mDevice->getCurrentFrame(false); //40ms

		if (src == NULL) {
            ALOGE("%s: current frame is null",__FUNCTION__);
            return false;
		}

		mCurrentFrame = (CameraYUVMeta*)src;

		if ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) && ccc) {
			ccc->cimyuv420b_to_tile420(mCurrentFrame); //1- 4ms
			//ccc->cimyu420b_to_ipuyuv420b(mCurrentFrame);
		}

		mCurFrameTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);
		
		postFrameForNotify(); // 5ms 

        if (mdrop_frame == 3) {
            postFrameForPreview(); // 5ms
        } else {
            mdrop_frame++;
        }

		workTime = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;
		
		timeout = mPreviewAfter - workTime; // 50ms - 12ms - (45 + 2) = -9ms

		if ( timeout > 0 ) {
			if ( timeout > 1000 )
				usleep(timeout/1000);
		}

		return true;
	}

    void CameraHal1::postFrameForPreview() {

        int res = NO_ERROR;
        if (mPreviewWindow == NULL)
            return;

        buffer_handle_t* buffer = NULL;
        int stride = 0;
        res = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buffer, &stride);
        if ((res != NO_ERROR) || (buffer == NULL)) {
            ALOGE("%s: dequeue buffer error",__FUNCTION__);
            return ;
        }

        res = mPreviewWindow->lock_buffer(mPreviewWindow,buffer);
        if (res != NO_ERROR) {
            mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
            return ;
        }

        void *img = NULL;
        const Rect rect(mCurrentFrame->width, mCurrentFrame->height);
        GraphicBufferMapper& mapper(GraphicBufferMapper::get());
        res = mapper.lock(*buffer, GRALLOC_USAGE_SW_WRITE_OFTEN, rect, &img);
        if (res != NO_ERROR) {
            mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
            return ;
        }

        res = fillCurrentFrame((uint8_t*)img,buffer);
        if (res == NO_ERROR) {
            mPreviewWindow->set_timestamp(mPreviewWindow, mCurFrameTimestamp);
            mPreviewWindow->enqueue_buffer(mPreviewWindow, buffer);
        } else {
            mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
        }
        mapper.unlock(*buffer);
    }

    status_t CameraHal1::fillCurrentFrame(uint8_t* img,buffer_handle_t* buffer) {

        int srcStride = mCurrentFrame->yStride;
        uint8_t*src = (uint8_t*)(mCurrentFrame->yAddr);
        int srcWidth = mCurrentFrame->width;
        int srcHeight = mCurrentFrame->height;

        int xStart = (mPreviewWinWidth - srcWidth) >> 1;
        int yStart = (mPreviewWinHeight - srcHeight) >> 1;

        IMG_native_handle_t* dst_handle = NULL;
        dst_handle = (IMG_native_handle_t*)(*buffer);
        int dest_size = dst_handle->iHeight * dst_handle->iStride * (dst_handle->uiBpp >> 3);
		  
        if (xStart < 0 || yStart < 0) {

            ALOGE_IF(DEBUG_HAL1,"Preview window is smaller than video preview size - Croppint image");

            if (xStart < 0) {
                srcWidth += (xStart<<1); //srcWidth += xStart;
                src += ((-xStart) >> 1) << 1;
                xStart = 0;
            }

            if (yStart < 0) {
                srcHeight += (yStart<<1); //srcHeight += yStart;
                src += ((-yStart)>>1) * srcStride;
                yStart = 0;
            }
        }

        int bytesPerPixel = 2;
        if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGB_565) {
            bytesPerPixel = 2;
        } else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_JZ_YUV_420_P ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_YCbCr_422_SP ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_YV12 ) {
            bytesPerPixel = 1;
        } else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGB_888) {
            bytesPerPixel = 3;
        } else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGBA_8888 ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_RGBX_8888 ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_BGRA_8888) {
            bytesPerPixel = 4;
        } else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            bytesPerPixel = 2;
        }

        int dstStride = bytesPerPixel * dst_handle->iStride;
        uint8_t* dst = ((uint8_t*)img) + (xStart*bytesPerPixel) + (dstStride*yStart);

        switch (mPreviewWinFmt) {
        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
            ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_yvu420sp (1)");
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_yvu420sp(dst, dstStride, 
                                      mPreviewWinHeight, src, srcStride, srcWidth, srcHeight);
            break;

        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_yvu420sp (2)");
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_yvu420sp(dst, dstStride, mPreviewWinHeight, 
                                      src, srcStride, srcWidth, srcHeight);
            break;

        case HAL_PIXEL_FORMAT_YV12:
            ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_yvu420p (3)");
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_yvu420p( dst, dstStride, mPreviewWinHeight, 
                                      src, srcStride, srcWidth, srcHeight);
            break;

        case PIXEL_FORMAT_YV16:
            ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_yvu422p (4)");
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_yvu422p(dst, dstStride, mPreviewWinHeight, 
                                     src, srcStride, srcWidth, srcHeight);
            break;

        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            {
                ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_yuyv (5)");
                uint8_t* pdst = dst;
                uint8_t* psrc = src;
                int h;
                for (h=0; h<srcHeight; h++) {
                    memcpy(pdst, psrc, srcWidth<<1);
                    pdst += dstStride;
                    psrc += srcStride;
                }
            }
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_rgb24 (6)");
            if (ipu_open_status) {
                ipu_convert_dataformat(mCurrentFrame,dst,buffer);
            } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                ccc->yuyv_to_rgb24(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            }
            break;

        case HAL_PIXEL_FORMAT_RGBA_8888:
            ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_rgb32 (7)");
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_rgb32(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            break;

        case HAL_PIXEL_FORMAT_RGBX_8888:
            ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_rgb32 (8)");
            if (ipu_open_status) {
                ipu_convert_dataformat(mCurrentFrame,dst,buffer);
            } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                ccc->yuyv_to_rgb32(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            }

            break;

        case HAL_PIXEL_FORMAT_BGRA_8888:
            ALOGE_IF(DEBUG_PREVIEW_THREAD,"yuyv_to_bgr32 (9)");
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                ccc->yuyv_to_bgr32(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            }
            break;

        case HAL_PIXEL_FORMAT_RGB_565:
            /**
             * ipu and x2d  convert to rgb565
             **/

#ifdef CAMERA_USE_SOFT
            if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P)
                        || (mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12))) {
                ccc->convert_yuv420p_to_rgb565(mCurrentFrame, dst);
            } else if (ccc && mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
                ccc->tile420_to_rgb565(mCurrentFrame, dst);
            } else if (ccc && mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
                ccc->yuyv_to_rgb565(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            }
#else
            if (mDevice->usePmem()) {
                ipu_convert_dataformat(mCurrentFrame,dst,buffer);
            } else {
                if (mCurrentFrame->format != HAL_PIXEL_FORMAT_YCbCr_422_I) {
                    x2d_convert_dataformat(mCurrentFrame, dst,buffer);
                } else {
                    ipu_convert_dataformat(mCurrentFrame,dst,buffer);
                }
            }
#endif
            break;

        default:
            ALOGE("Unhandled pixel format");
            goto preview_win_format_error;
        }

        if (isSoftFaceDetectStart == true && ccc) {
            if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGB_565) {
                mFaceCount = CameraFaceDetect::getInstance()->detect((uint16_t*)dst);
            } else {
                camera_memory_t* rgb565 = mget_memory(-1, (srcWidth*srcHeight*2), 1, NULL);
                if ((rgb565 != NULL) && (rgb565->data != NULL)) {
                    if (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
                        ccc->yuyv_to_rgb565(src, srcStride, (uint8_t*)(rgb565->data),
                                            mPreviewWinWidth*2, srcWidth, srcHeight);
                        mFaceCount = CameraFaceDetect::getInstance()->detect((uint16_t*)rgb565->data);
                    } else if (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
                        if (mDevice->usePmem()) {
                            ipu_convert_dataformat(mCurrentFrame,(uint8_t*)(rgb565->data), buffer);
                        } else {
                            x2d_convert_dataformat(mCurrentFrame, 
                                                   (uint8_t*)(rgb565->data), buffer);
                        }
                        //ccc->tile420_to_rgb565(mCurrentFrame, (uint8_t*)(rgb565->data));
                        mFaceCount = CameraFaceDetect::getInstance()->detect((uint16_t*)rgb565->data);
                    } else if (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) {
                        ccc->yuv420p_to_rgb565(src, (uint8_t*)(rgb565->data),srcWidth, srcHeight);
                        mFaceCount = CameraFaceDetect::getInstance()->detect((uint16_t*)rgb565->data);
                    }
                    rgb565->release(rgb565);
                    rgb565 = NULL;
                }
            }
        }
    preview_win_format_error:
        return NO_ERROR;
    }

    void CameraHal1::postFrameForNotify() {

        if ((mMesgEnabled & CAMERA_MSG_VIDEO_FRAME) && mVideoRecEnabled) {
               
            if ((NULL != mRecordingHeap) && (mRecordingHeap->data != NULL)) {
                void* dest = NULL;
                bool recFmtSupport = true;
                camera_memory_t* tmpRecordingHeapBase = mDevice->getPreviewBufferHandle();
                int index = mDevice->getPreviewFrameIndex();
                switch(mRecFmt) {
                case HAL_PIXEL_FORMAT_YCbCr_422_SP:
                    ALOGE_IF(DEBUG_HAL1,"yuyv_to_yvu420sp, (1)");
                    dest = (void*)((int)(mRecordingHeap->data)
                                   + mRecordingFrameSize * mRecordingindex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        ccc->yuyv_to_yvu420sp((uint8_t*)(dest),
                                              mRawPreviewWidth, mRawPreviewHeight,
                                              (uint8_t*)(mCurrentFrame->yAddr),
                                              mCurrentFrame->yStride,
                                              mCurrentFrame->width, mCurrentFrame->height);

                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) ||
                                       mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12)) {
                        ccc->yuv420p_to_yuv422sp((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest),
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP))) {
                        memcpy((uint8_t*)dest, (uint8_t*)(mCurrentFrame->yAddr),
                               mRecordingFrameSize);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        ccc->yuv420p_to_yuv420sp((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest), 
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else {
                        recFmtSupport = false;
                    }
                    break;

                case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                    ALOGE_IF(DEBUG_HAL1,"yuyv_to_yvu420sp, (2)");
                    dest = (void*)((int)(mRecordingHeap->data) 
                                   + mRecordingFrameSize * mRecordingindex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) { 
                        ccc->yuyv_to_yvu420sp((uint8_t*)(dest),
                                              mCurrentFrame->width,mCurrentFrame->height,
                                              (uint8_t*)(mCurrentFrame->yAddr), 
                                              mCurrentFrame->yStride, mCurrentFrame->width,
                                              mCurrentFrame->height);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP)) {
                        ccc->yuv422sp_to_yuv420sp((uint8_t*)(dest),(uint8_t*)(mCurrentFrame->yAddr),
                                                  mCurrentFrame->width,mCurrentFrame->height);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP)) {
                        memcpy((uint8_t*)dest, (uint8_t*)(mCurrentFrame->yAddr),
                               mRecordingFrameSize);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) ||
                                       mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12)) {
                        ccc->yuv420p_to_yuv420sp((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest), 
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        ccc->yuv420p_to_yuv420sp((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest), 
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else {
                        recFmtSupport = false;
                    }
                    break;

                case HAL_PIXEL_FORMAT_YV12:
                    ALOGE_IF(DEBUG_HAL1,"yuyv_to_yvu420p, (3)");
                    dest = (void*)((int)(mRecordingHeap->data) 
                                   + mRecordingFrameSize * mRecordingindex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) { 
                        ccc->yuyv_to_yuv420p((uint8_t*)(dest),
                                             mCurrentFrame->width,mCurrentFrame->height,
                                             (uint8_t*)(mCurrentFrame->yAddr),
                                             mCurrentFrame->yStride,
                                             mCurrentFrame->width, mCurrentFrame->height);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P))) {
                        memcpy(dest,(uint8_t*)(mCurrentFrame->yAddr),
                               mRecordingFrameSize);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP))) {
                        ccc->yuv420sp_to_yuv420p((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest), 
                                                 mCurrentFrame->width,mCurrentFrame->height);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                    } else {
                        recFmtSupport = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_YCbCr_422_I:
                    ALOGE_IF(DEBUG_HAL1,"yuyv_to_yuyv, (4)");
                    dest = (void*)((int)(mRecordingHeap->data) 
                                   + mRecordingFrameSize * mRecordingindex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        memcpy((uint8_t*)dest,(uint8_t*)(mCurrentFrame->yAddr),
                               mRecordingFrameSize);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)){
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        //ccc->yuv420p_to_yuyv(mCurrentFrame, (uint8_t*)(mRecordingHeap->data));
                    } else {
                        recFmtSupport = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
                    ALOGE_IF(DEBUG_HAL1,"jzyuv420tile video, (5)");
                    dest = (void*)((int)(mRecordingHeap->data) 
                                   + mRecordingFrameSize * mRecordingindex);
                    if (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
                        if (RECORDING_BUFFER_NUM == PREVIEW_BUFFER_CONUT)
                            memcpy(dest, (uint8_t*)(mCurrentFrame->yAddr), mRecordingFrameSize);
                    } else {
                        recFmtSupport = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
                    ALOGE_IF(DEBUG_HAL1,"jzyuv420p, (6)");
                    dest = (void*)((int)(mRecordingHeap->data) 
                                   + mRecordingFrameSize * mRecordingindex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                    } if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        ccc->yuyv_to_yuv420p((uint8_t*)(dest),
                                             mCurrentFrame->width,mCurrentFrame->height,
                                             (uint8_t*)(mCurrentFrame->yAddr), 
                                             mCurrentFrame->yStride,
                                             mCurrentFrame->width,mCurrentFrame->height);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) || 
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12))){
                        memcpy((uint8_t*)dest, (uint8_t*)(mCurrentFrame->yAddr),
                               mRecordingFrameSize);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP))) {
                        ccc->yuv420sp_to_yuv420p((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest), 
                                                 mCurrentFrame->width,mCurrentFrame->height);
                    } else {
                        recFmtSupport = false;
                    }
                    break;
                default:
                    ALOGE("Recording format not support : error format = %d",mRecFmt);
                    recFmtSupport = false;
                    break;
                }

                if (recFmtSupport) {

                    if (RECORDING_BUFFER_NUM == PREVIEW_BUFFER_CONUT) {
                        mdata_cb_timestamp(mCurFrameTimestamp,CAMERA_MSG_VIDEO_FRAME,
                                           mRecordingHeap, mRecordingindex, mcamera_interface);
                        mRecordingindex = (mRecordingindex+1)%RECORDING_BUFFER_NUM; 
                    } else {
                        mdata_cb_timestamp(mCurFrameTimestamp,CAMERA_MSG_VIDEO_FRAME,
                                           tmpRecordingHeapBase, mCurrentFrame->index, 
                                           mcamera_interface);
                    }

                    //getWorkThread()->threadPause();
                }
            }
        }

        if (mMesgEnabled & CAMERA_MSG_PREVIEW_FRAME) {

            int width = 0, height = 0;

            mJzParameters->getCameraParameters().getPreviewSize(&width, &height);

            int cwidth = width;
            int cheight = height;
            bool convert_result = true;

            if (cwidth > mCurrentFrame->width)
                cwidth = mCurrentFrame->width;
            if (cheight > mCurrentFrame->height)
                cheight = mCurrentFrame->height;

            ALOGE_IF(DEBUG_HAL1,"post for preview frame, preview size =%dx%d, raw size = %dx%d", 
                     width, height, mCurrentFrame->width, mCurrentFrame->height);
            if ((mPreviewHeap != NULL) && (mPreviewHeap->data != NULL)) {
                void* dest = NULL;
                switch(mPreviewFmt) {

                case HAL_PIXEL_FORMAT_YCbCr_422_SP:
                    ALOGE_IF(DEBUG_HAL1,"yuyv_to_yvu420sp (1)");
                    dest = (void*)((int)(mPreviewHeap->data) 
                                   + mPreviewFrameSize*mPreviewIndex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) { 
                        ccc->yuyv_to_yvu420sp((uint8_t*)(dest), 
                                              width, height, (uint8_t*)(mCurrentFrame->yAddr), 
                                              mCurrentFrame->yStride, cwidth, cheight);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        ccc->yuv420p_to_yuv420sp((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest), 
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP))) {
                        memcpy((uint8_t*)(dest), (uint8_t*)(mCurrentFrame->yAddr), 
                               mPreviewFrameSize);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12) || 
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P))) {
                        ccc->yuv420p_to_yuv420sp((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest),
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else {
                        convert_result = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                    ALOGE_IF(DEBUG_HAL1,"yuyv_to_yvu420sp (2)");
                    dest = (void*)((int)(mPreviewHeap->data) 
                                   + mPreviewFrameSize*mPreviewIndex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) { 
                        ccc->yuyv_to_yvu420sp((uint8_t*)(dest), 
                                              width, height, (uint8_t*)(mCurrentFrame->yAddr), 
                                              mCurrentFrame->yStride, cwidth, cheight);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        ccc->yuv420p_to_yuv420sp((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest), 
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP)) {
                        memcpy((uint8_t*)dest, (uint8_t*)(mCurrentFrame->yAddr), 
                               mPreviewFrameSize);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP)) {
                        ccc->yuv422sp_to_yuv420sp((uint8_t*)(dest),(uint8_t*)(mCurrentFrame->yAddr),
                                                  mCurrentFrame->width,mCurrentFrame->height);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12) || 
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P))) {
                        ccc->yuv420p_to_yuv420sp((uint8_t*)(mCurrentFrame->yAddr), 
                                                 (uint8_t*)(dest),
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else {
                        convert_result = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_YV12:
                    ALOGE_IF(DEBUG_HAL1,"yuyv_to_yvu420p (3)");
                    dest = (void*)((int)(mPreviewHeap->data) 
                                   + mPreviewFrameSize*mPreviewIndex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        ccc->yuyv_to_yvu420p((uint8_t*)(dest), 
                                             width, height, (uint8_t*)(mCurrentFrame->yAddr), 
                                             (mRawPreviewWidth<<1), cwidth, cheight); 
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P))) {
                        memcpy((uint8_t*)dest,(uint8_t*)(mCurrentFrame->yAddr),
                               mPreviewFrameSize);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP)) {
                        ccc->yuv420sp_to_yuv420p((uint8_t*)(mCurrentFrame->yAddr), 
                                                 (uint8_t*)(dest),
                                                 mCurrentFrame->width,
                                                 mCurrentFrame->height);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP)) {
                        ccc->yuv422sp_to_yuv420p((uint8_t*)dest, (uint8_t*)(mCurrentFrame->yAddr),
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else {
                        convert_result = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_YCbCr_422_I:
                    ALOGE_IF(DEBUG_HAL1,"yuyv_to_yuyv (4)");
                    dest = (void*)((int)(mPreviewHeap->data) 
                                   + mPreviewFrameSize*mPreviewIndex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        //ccc->yuv420p_to_yuyv(mCurrentFrame, (uint8_t*)(dest));
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        memcpy((uint8_t*)dest, (uint8_t*)(mCurrentFrame->yAddr),
                               mPreviewFrameSize);
                    } else {
                        convert_result = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
                    ALOGE_IF(DEBUG_HAL1,"jzyuv420tile_to_yuv420p (5)");
                    dest = (void*)((int)(mPreviewHeap->data) 
                                   + mPreviewFrameSize * mPreviewIndex);
                    if (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
                        memcpy((uint8_t*)dest, (uint8_t*)(mCurrentFrame->yAddr),mPreviewFrameSize);
                    }
                    break;
                case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
                    ALOGE_IF(DEBUG_HAL1,"jzyuv420p_to_yuv420p (6)");
                    dest = (void*)((int)(mPreviewHeap->data) 
                                   + mPreviewFrameSize*mPreviewIndex);
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12))) {
                        memcpy((uint8_t*)dest, (uint8_t*)(mCurrentFrame->yAddr),mPreviewFrameSize);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP))) {
                        ccc->yuv420sp_to_yuv420p((uint8_t*)(mCurrentFrame->yAddr),
                                                 (uint8_t*)(dest),
                                                 mCurrentFrame->width, mCurrentFrame->height);
                    } else {
                        convert_result = false;
                    }
                    break;
                default:
                    convert_result = false;
                    ALOGE("Unhandled pixel format");
                }
                if (convert_result) {
                    mdata_cb(CAMERA_MSG_PREVIEW_FRAME, mPreviewHeap, mPreviewIndex, 
                             NULL,mcamera_interface);
                    mPreviewIndex = (mPreviewIndex+1)%PREVIEW_BUFFER_CONUT;
                }
            }
        }

        if ((mMesgEnabled & CAMERA_MSG_PREVIEW_METADATA) && (isSoftFaceDetectStart == true)) {
            Rect **faceRect = NULL;
            camera_frame_metadata_t frame_metadata;
            int maxFaces = mJzParameters->getCameraParameters()
                .getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
            status_t ret = NO_ERROR;
            float lx = 0, ly = 0, rx = 0, ry = 0;
            float fl = 0, fr = 0, ft = 0, fb = 0;

            if (mFaceCount > 0) {
                if (mFaceCount > maxFaces)
                    mFaceCount = maxFaces;
                faceRect = new Rect*[mFaceCount];
                frame_metadata.faces = (camera_face_t*)calloc(mFaceCount, sizeof(camera_face_t));
                frame_metadata.number_of_faces = mFaceCount;
                for (int i = 0; i < mFaceCount; ++i) {
                    faceRect[i] = new Rect();
                    CameraFaceDetect::getInstance()->get_face(faceRect[i],i);
                    fl = faceRect[i]->left;
                    fr = faceRect[i]->right;
                    ft = faceRect[i]->top;
                    fb = faceRect[i]->bottom;

                    if (fl >= -1000 && fl <= 1000) {
                        ;
                    } else {
                        fl = fl - 1000;
                        fr = fr - 1000;
                        ft = ft - 1000;
                        fb = fb - 1000;
                    }

                    frame_metadata.faces[i].rect[0] = (int32_t)fl;
                    frame_metadata.faces[i].rect[1] = (int32_t)fr;
                    frame_metadata.faces[i].rect[2] = (int32_t)ft;
                    frame_metadata.faces[i].rect[3] = (int32_t)fb;

                    frame_metadata.faces[i].id = i;
                    frame_metadata.faces[i].score = CameraFaceDetect::getInstance()->get_confidence();
                    frame_metadata.faces[i].mouth[0] = -2000; frame_metadata.faces[i].mouth[1] = -2000;
                    lx = CameraFaceDetect::getInstance()->getLeftEyeX();
                    ly = CameraFaceDetect::getInstance()->getLeftEyeY();
                    rx = CameraFaceDetect::getInstance()->getRightEyeX();
                    ry = CameraFaceDetect::getInstance()->getRightEyeY();
                    if ((lx >= -1000 && lx <= 1000)) {
                        ;
                    } else {
                        lx = lx - 1000;
                        ly = ly - 1000;
                        rx = rx - 1000;
                        ry = ry - 1000;
                    }
                    frame_metadata.faces[i].left_eye[0] = (int32_t)lx;
                    frame_metadata.faces[i].left_eye[1] = (int32_t)ly;
                    frame_metadata.faces[i].right_eye[0] = (int32_t)rx;
                    frame_metadata.faces[i].right_eye[1] = (int32_t)ry;
                }

                camera_memory_t *tmpBuffer = mget_memory(-1, 1, 1, NULL);
                mdata_cb(CAMERA_MSG_PREVIEW_METADATA, tmpBuffer, 0, &frame_metadata,mcamera_interface);

                if ( NULL != tmpBuffer ) {
                    tmpBuffer->release(tmpBuffer);
                    tmpBuffer = NULL;
                }

                for (int i = 0; i < mFaceCount; ++i) {
                    delete faceRect[i];
                    faceRect[i] = NULL;
                }
                delete [] faceRect;
                faceRect = NULL;
                   
                if (frame_metadata.faces != NULL) {
                    free(frame_metadata.faces);
                    frame_metadata.faces = NULL;
                }
            }
            ALOGE_IF(DEBUG_HAL1,"%s: ret = %d", __FUNCTION__, ret);
        }
        return;
    }

	void CameraHal1::cameraCapture() {

        bool is_video = strcmp(mJzParameters->getCameraParameters().get(CameraParameters::KEY_RECORDING_HINT),"true") == 0;

		if (!is_video && (mMesgEnabled & CAMERA_MSG_SHUTTER)) {
			mnotify_cb(CAMERA_MSG_SHUTTER, 0, 0, mcamera_interface);
		}

        if (mMesgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY)
            mnotify_cb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, mcamera_interface);

        if (!is_video && (mMesgEnabled & CAMERA_MSG_RAW_IMAGE) && (mCurrentFrame->yAddr>0))
            mdata_cb(CAMERA_MSG_RAW_IMAGE, mDevice->getCaptureBufferHandle(),
                     mCurrentFrame->index, NULL, mcamera_interface);

        if (!is_video && (mMesgEnabled & CAMERA_MSG_POSTVIEW_FRAME) && (mCurrentFrame->yAddr>0))
            mdata_cb(CAMERA_MSG_RAW_IMAGE,mDevice->getCaptureBufferHandle(), 
                     mCurrentFrame->index, NULL, mcamera_interface);

		if (mMesgEnabled & CAMERA_MSG_COMPRESSED_IMAGE) {
			int64_t jpeg_s = systemTime(SYSTEM_TIME_MONOTONIC);
			int64_t jpeg_e = 0;
			postJpegDataToApp();
			//mWorkerQueue->schedule(new PostJpegUnit(this));
			jpeg_e = systemTime(SYSTEM_TIME_MONOTONIC) - jpeg_s;
			ALOGE("postJpegDataToApp() use time: %lld us", jpeg_e/1000);
		}
	}

    void CameraHal1::postJpegDataToApp(void) {

        if ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) && ccc) {
            hardCompressJpeg();
        } else if (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            softCompressJpeg();
        } else {
            ALOGE("%s: don't support other format(%d) for compress jpeg, ccc=%p",
                  __FUNCTION__,mCurrentFrame->format, ccc?ccc:0);
        }

        return;
    }

    void CameraHal1::softCompressJpeg(void) {

        ALOGE_IF(DEBUG_HAL1,"%s: Enter", __FUNCTION__);

        camera_memory_t* jpeg_buff = NULL;
        int ret = convertCurrentFrameToJpeg(&jpeg_buff);

        if (ret == NO_ERROR && jpeg_buff != NULL && jpeg_buff->data != NULL) {
            mdata_cb(CAMERA_MSG_COMPRESSED_IMAGE, jpeg_buff, 0, NULL, mcamera_interface);
            jpeg_buff->release(jpeg_buff);
        } else if (jpeg_buff != NULL && jpeg_buff->data != NULL) {
            jpeg_buff->release(jpeg_buff);
        }

        mDevice->freeStream(CAPTURE_BUFFER);

    }

    void CameraHal1::hardCompressJpeg(void) {

        ALOGE_IF(DEBUG_HAL1,"%s: Enter",__FUNCTION__);

        status_t ret = UNKNOWN_ERROR;
        int width = 0, height = 0;
        const char* is_video = NULL;

        is_video = mJzParameters->getCameraParameters().get(CameraParameters::KEY_RECORDING_HINT);
        if (is_video == NULL)
            is_video = "false";

        if (mJzParameters->getPropertyPictureSize(&width, &height) !=  NO_ERROR)
            mJzParameters->getCameraParameters().getPictureSize(&width, &height);
        int picQuality = mJzParameters->getCameraParameters().getInt(CameraParameters::KEY_JPEG_QUALITY);
        int thumQuality = mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
        if (picQuality <= 0 || picQuality == 100) picQuality = 90;
        if (thumQuality <= 0 || thumQuality == 100) thumQuality = 90;

        if (strcmp(is_video,"true") == 0) {
            //mJzParameters->getCameraParameters().getVideoSize(&width, &height);
            mJzParameters->getCameraParameters().getPreviewSize(&width, &height);
            ALOGE("start recording you want take picture %d x  %d", width, height);
        }

        int csize = mDevice->getCaptureFrameSize();
        int th_width = mJzParameters->getCameraParameters().
            getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
        int th_height =  mJzParameters->getCameraParameters().
            getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
        int ctnsize =  th_width * th_height;

        int jpeg_size = csize;
        int thumb_size = ctnsize;
        camera_memory_t* jpeg_buff = mget_memory(-1,csize+1000 ,1,NULL);
        camera_memory_t* jpeg_tn_buff = (ctnsize == 0) ? NULL : (mget_memory(-1, ctnsize+1000, 1, NULL));
        camera_memory_t* jpegMem = NULL;

#ifdef COMPRESS_JPEG_USE_HW
        CameraCompressorHW ccHW;
        compress_params_hw_t hw_cinfo;

        memset(&hw_cinfo, 0, sizeof(compress_params_hw_t));

        hw_cinfo.pictureYUV420_y = (uint8_t*)(mCurrentFrame->yAddr);
        hw_cinfo.pictureYUV420_c = (uint8_t*)(mCurrentFrame->yAddr 
                                              + mCurrentFrame->width*mCurrentFrame->height);
        hw_cinfo.pictureWidth = width;
        hw_cinfo.pictureHeight = height;
        hw_cinfo.pictureQuality = picQuality;
        hw_cinfo.thumbnailWidth = th_width;
        hw_cinfo.thumbnailHeight = th_height;
        hw_cinfo.thumbnailQuality = thumQuality;
        hw_cinfo.format = mCurrentFrame->format;
        hw_cinfo.jpeg_out = (unsigned char*)(jpeg_buff->data);
        hw_cinfo.jpeg_size = &jpeg_size;
        hw_cinfo.th_jpeg_out = (jpeg_tn_buff==NULL) ? NULL : ((unsigned char*)(jpeg_tn_buff->data));
        hw_cinfo.th_jpeg_size = &thumb_size;
        hw_cinfo.tlb_addr = mDevice->getTlbBase();
        hw_cinfo.requiredMem = mget_memory;

        ccHW.setPrameters(&hw_cinfo);
        ccHW.hw_compress_to_jpeg();
#endif
        ExifElementsTable* exif = new ExifElementsTable();
        if (NULL != exif) {
            mJzParameters->setUpEXIF(exif);
            exif->insertExifToJpeg((unsigned char*)(jpeg_buff->data),jpeg_size);
            if (NULL != jpeg_tn_buff
                && jpeg_tn_buff->data != NULL 
                && thumb_size > 0) {
                exif->insertExifThumbnailImage((const char*)(jpeg_tn_buff->data), (int)thumb_size);
            }
            Section_t* exif_section = NULL;
            exif_section = FindSection(M_EXIF);
            if (NULL != exif_section) {
                jpegMem = mget_memory(-1, (jpeg_size + exif_section->Size), 1, NULL);
                if ((NULL != jpegMem) && (jpegMem->data != NULL)) {
                    exif->saveJpeg((unsigned char*)(jpegMem->data),(jpeg_size + exif_section->Size));
                    mdata_cb(CAMERA_MSG_COMPRESSED_IMAGE, jpegMem, 0, NULL, mcamera_interface);
                    jpegMem->release(jpegMem);
                    jpegMem = NULL;
                }
            }
        }

        if (jpeg_buff != NULL) {
            jpeg_buff->release(jpeg_buff);
            jpeg_buff = NULL;
        }

        if (jpeg_tn_buff != NULL) {
            jpeg_tn_buff->release(jpeg_tn_buff);
            jpeg_tn_buff = NULL;
        }

        if (jpegMem) {
            jpegMem->release(jpegMem);
            jpegMem = NULL;
        }

        mDevice->freeStream(CAPTURE_BUFFER);
    }

    status_t CameraHal1::convertCurrentFrameToJpeg(camera_memory_t** jpeg_buff) {

        status_t ret = UNKNOWN_ERROR;
        int width = 0, height = 0;
        const char* is_video = NULL;

        is_video = mJzParameters->getCameraParameters().get(CameraParameters::KEY_RECORDING_HINT);
        if (is_video == NULL)
            is_video = "false";

        if (mJzParameters->getPropertyPictureSize(&width, &height) !=  NO_ERROR)
            mJzParameters->getCameraParameters().getPictureSize(&width, &height);
        int picQuality = mJzParameters->getCameraParameters().getInt(CameraParameters::KEY_JPEG_QUALITY);
        int thumQuality = mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
        if (picQuality <= 0 || picQuality == 100) picQuality = 75;
        if (thumQuality <= 0 || thumQuality == 100) thumQuality = 75;

        if (strcmp(is_video,"true") == 0) {
            //mJzParameters->getCameraParameters().getVideoSize(&width, &height);
            mJzParameters->getCameraParameters().getPreviewSize(&width, &height);
            ALOGE("%s: start recording you want take picture %dx%d",__FUNCTION__,
                  width, height);
        }

        compress_params_t params;
        memset(&params, 0, sizeof(compress_params_t));
        params.src = (uint8_t*)(mCurrentFrame->yAddr);
        params.pictureWidth = width;
        params.pictureHeight = height;
        params.pictureQuality = picQuality;
        params.thumbnailWidth = 
            mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
        params.thumbnailHeight = 
            mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
        params.thumbnailQuality = thumQuality;
        params.format = mCurrentFrame->format;
        params.jpegSize = 0;
        params.requiredMem = mget_memory;

        int rot = mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_ROTATION);
        CameraCompressor compressor(&params,mirror,rot);

        ExifElementsTable* exif = new ExifElementsTable();
        if (NULL != exif) {
            mJzParameters->setUpEXIF(exif);
            ret = compressor.compress_to_jpeg(exif, jpeg_buff);
        }

        return ret;
    }


    status_t CameraHal1::softFaceDetectStart(int32_t detect_type) {

        int w = mCurrentFrame->width;
        int h = mCurrentFrame->height;
        int maxFaces = 0;
        status_t res = NO_ERROR;

        switch(detect_type)
            {
            case CAMERA_FACE_DETECTION_HW:
                ALOGE("start hardware face detect");
                maxFaces = mJzParameters->getCameraParameters()
					.getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
                goto hard_detect_method;
                break;
            case CAMERA_FACE_DETECTION_SW:
                ALOGE("start Software face detection");
                maxFaces = mJzParameters->getCameraParameters()
					.getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW);
                goto soft_detect_method;
                break;
            }

    hard_detect_method:
    soft_detect_method:

        if (maxFaces == 0) {
            isSoftFaceDetectStart = false;
            return -1;
        }

        ALOGE_IF(DEBUG_HAL1,"%s: max Face = %d", __FUNCTION__,maxFaces);

        res = CameraFaceDetect::getInstance()->initialize(w, h, maxFaces);
        if (res == NO_ERROR) {
            isSoftFaceDetectStart = true;
        } else {
            isSoftFaceDetectStart = false;
        }

        return res;
    }

    status_t CameraHal1::softFaceDetectStop(void) {
        if (isSoftFaceDetectStart) {
            isSoftFaceDetectStart = false;
            CameraFaceDetect::getInstance()->deInitialize();
        }
        return NO_ERROR;
    }

    void CameraHal1::x2d_convert_dataformat(CameraYUVMeta* yuvMeta, 
                                            uint8_t* dst_buf, buffer_handle_t *buffer) {

        struct jz_x2d_config x2d_cfg;
        IMG_native_handle_t* dst_handle = NULL;

        if (x2d_fd < 0) {
            ALOGE("%s: open %s error or not open it", __FUNCTION__, X2D_NAME);
            return;
        }

        dst_handle = (IMG_native_handle_t*)(*buffer);
        struct dmmu_mem_info dst_dmmu;

        dst_dmmu.vaddr = (void*)(dst_buf);
        dst_dmmu.size = dst_handle->iHeight * dst_handle->iStride * (dst_handle->uiBpp >> 3);
        int ret = 0;
        ret = dmmu_map_user_memory(&dst_dmmu);
        if (ret != 0) {
            ALOGE("%s: dmmu map user memory ret = %d",__FUNCTION__, ret);
            return ;
        }

        /* set dst configs */
        x2d_cfg.dst_address = (int)dst_buf;
        x2d_cfg.dst_width = dst_handle->iWidth;
        x2d_cfg.dst_height = dst_handle->iHeight;
        x2d_cfg.dst_format = X2D_OUTFORMAT_RGB565;
        x2d_cfg.dst_stride = dst_handle->iStride * (dst_handle->uiBpp >> 3);

        x2d_cfg.dst_back_en = 0;
        x2d_cfg.dst_glb_alpha_en = 1;
        x2d_cfg.dst_preRGB_en = 0;
        x2d_cfg.dst_mask_en = 1;
        x2d_cfg.dst_alpha_val = 0x80;	
        x2d_cfg.dst_bcground = 0xff0ff0ff;

        x2d_cfg.tlb_base = mDevice->getTlbBase();

        /* layer num */
        x2d_cfg.layer_num = 1;

        /* src yuv address */
        if (yuvMeta->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            x2d_cfg.lay[0].addr = yuvMeta->yAddr;
            x2d_cfg.lay[0].u_addr = yuvMeta->yAddr + (yuvMeta->width*yuvMeta->height);

            x2d_cfg.lay[0].v_addr = (int)(x2d_cfg.lay[0].u_addr);
            x2d_cfg.lay[0].y_stride = yuvMeta->yStride/16;
            x2d_cfg.lay[0].v_stride = yuvMeta->vStride/16;

            /* src data format */
            x2d_cfg.lay[0].format = X2D_INFORMAT_TILE420;
        } else if (yuvMeta->format ==  HAL_PIXEL_FORMAT_JZ_YUV_420_P) {
            x2d_cfg.lay[0].addr = yuvMeta->yAddr;
            x2d_cfg.lay[0].u_addr = yuvMeta->uAddr;
            x2d_cfg.lay[0].v_addr = yuvMeta->vAddr;
            x2d_cfg.lay[0].y_stride = yuvMeta->yStride;
            x2d_cfg.lay[0].v_stride = yuvMeta->vStride;

            /* src data format */
            x2d_cfg.lay[0].format = X2D_INFORMAT_YUV420SP;
        } else {
            ALOGE("%s: preview format %d not support",__FUNCTION__, yuvMeta->format);
            return;
        }

        /* src rotation degree */	
        x2d_cfg.lay[0].transform = X2D_ROTATE_0;

        /* src input geometry && output geometry */		
        x2d_cfg.lay[0].in_width =  yuvMeta->width;
        x2d_cfg.lay[0].in_height = yuvMeta->height;
        x2d_cfg.lay[0].out_width = dst_handle->iWidth;
        x2d_cfg.lay[0].out_height = dst_handle->iHeight;
        x2d_cfg.lay[0].out_w_offset = 0;
        x2d_cfg.lay[0].out_h_offset = 0;
        x2d_cfg.lay[0].mask_en = 0;
        x2d_cfg.lay[0].msk_val = 0xffffffff;
        x2d_cfg.lay[0].glb_alpha_en = 1;
        x2d_cfg.lay[0].global_alpha_val = 0xff;
        x2d_cfg.lay[0].preRGB_en = 1;

        /* src scale ratio set */
        float v_scale, h_scale;
        switch (x2d_cfg.lay[0].transform) {
        case X2D_H_MIRROR:
        case X2D_V_MIRROR:
        case X2D_ROTATE_0:
        case X2D_ROTATE_180:
            h_scale = (float)x2d_cfg.lay[0].in_width / (float)x2d_cfg.lay[0].out_width;
            v_scale = (float)x2d_cfg.lay[0].in_height / (float)x2d_cfg.lay[0].out_height;
            x2d_cfg.lay[0].h_scale_ratio = (int)(h_scale * X2D_SCALE_FACTOR);
            x2d_cfg.lay[0].v_scale_ratio = (int)(v_scale * X2D_SCALE_FACTOR);
            break;
        case X2D_ROTATE_90:
        case X2D_ROTATE_270:
            h_scale = (float)x2d_cfg.lay[0].in_width / (float)x2d_cfg.lay[0].out_height;
            v_scale = (float)x2d_cfg.lay[0].in_height / (float)x2d_cfg.lay[0].out_width;
            x2d_cfg.lay[0].h_scale_ratio = (int)(h_scale * X2D_SCALE_FACTOR);
            x2d_cfg.lay[0].v_scale_ratio = (int)(v_scale * X2D_SCALE_FACTOR);
            break;
        default:
            ALOGE("%s %s %d:undefined rotation degree!!!!", __FILE__, __FUNCTION__, __LINE__);
            return;
        }
		 
        /* ioctl set configs */
        ret = ioctl(x2d_fd, IOCTL_X2D_SET_CONFIG, &x2d_cfg);
        if (ret < 0) {
            ALOGE("%s %s %d: IOCTL_X2D_SET_CONFIG failed", __FILE__, __FUNCTION__, __LINE__);
            return ;
        }

        /* ioctl start compose */
        ret = ioctl(x2d_fd, IOCTL_X2D_START_COMPOSE);
        if (ret < 0) {
            ALOGE("%s %s %d: IOCTL_X2D_START_COMPOSE failed", __FILE__, __FUNCTION__, __LINE__);
            return ;
        }

        ret = dmmu_unmap_user_memory(&dst_dmmu);
        if (ret < 0) {
            ALOGE("%s %s %d: dmmu_unmap_user_memory failed", __FILE__, __FUNCTION__, __LINE__);
            return;
        }
    
    }

    void CameraHal1::ipu_convert_dataformat(CameraYUVMeta* yuvMeta,
                                            uint8_t* dst_buf, buffer_handle_t *buffer) {

        struct source_data_info *srcInfo;
        struct ipu_data_buffer* srcBuf;
        struct dest_data_info* dstInfo;
        struct ipu_data_buffer* dstBuf;

        int err = 0;

        if (ipu_open_status == false) {
            ALOGE("%s: open ipu error or not open it", __FUNCTION__);
            return;
        }

        IMG_native_handle_t* dst_handle = NULL;
        dst_handle = (IMG_native_handle_t*)(*buffer);

        struct dmmu_mem_info dst_dmmu;
        dst_dmmu.vaddr = (void*)dst_buf;
        dst_dmmu.size = dst_handle->iHeight * dst_handle->iStride * (dst_handle->uiBpp >> 3);
        int ret = 0;
        ret = dmmu_map_user_memory(&dst_dmmu);
        if (ret != 0) {
            ALOGE("%s: dmmu map user memory ret = %d",__FUNCTION__, ret);
            return ;
        }

        srcInfo = &(mipu->src_info);
        srcBuf = &(mipu->src_info.srcBuf);
        memset(srcInfo, 0, sizeof(struct source_data_info));
        srcInfo->fmt = yuvMeta->format;
        srcInfo->is_virt_buf = 1;
        srcInfo->width = yuvMeta->width;
        srcInfo->height = yuvMeta->height;
        srcInfo->stlb_base = mDevice->getTlbBase();
        if (yuvMeta->format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            srcBuf->y_buf_v = (void*)yuvMeta->yAddr;
            srcBuf->u_buf_v = (void*)yuvMeta->yAddr;
            srcBuf->v_buf_v = (void*)yuvMeta->yAddr;

            if (mDevice->usePmem()) {
                srcBuf->y_buf_phys = yuvMeta->yPhy; 
                srcBuf->u_buf_phys = 0;
                srcBuf->v_buf_phys = 0;
            } else {
                srcBuf->y_buf_phys = 0;
                srcBuf->u_buf_phys = 0;
                srcBuf->v_buf_phys = 0;
            }

            srcBuf->y_stride = yuvMeta->yStride;
            srcBuf->u_stride = yuvMeta->uStride;
            srcBuf->v_stride = yuvMeta->vStride;
        } else if (yuvMeta->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            srcBuf->y_buf_v = (void*)yuvMeta->yAddr;
            srcBuf->u_buf_v = (void*)(yuvMeta->yAddr + yuvMeta->width*yuvMeta->height) ;
            srcBuf->v_buf_v = (void*)srcBuf->u_buf_v;
            srcBuf->y_stride = yuvMeta->yStride;
            srcBuf->u_stride = yuvMeta->uStride;
            srcBuf->v_stride = yuvMeta->vStride;
        } else if (yuvMeta->format ==  HAL_PIXEL_FORMAT_JZ_YUV_420_P) {
            srcBuf->y_buf_v = (void*)yuvMeta->yAddr;
            srcBuf->u_buf_v = (void*)yuvMeta->uAddr;
            srcBuf->v_buf_v = (void*)yuvMeta->vAddr;

            if (mDevice->usePmem()) {
                srcBuf->y_buf_phys = yuvMeta->yPhy;
                srcBuf->u_buf_phys = yuvMeta->uPhy;
                srcBuf->v_buf_phys = yuvMeta->vPhy;
            } else {
                srcBuf->y_buf_phys = 0;
                srcBuf->u_buf_phys = 0;
                srcBuf->v_buf_phys = 0;
            }

            srcBuf->y_stride = yuvMeta->yStride;
            srcBuf->u_stride = yuvMeta->uStride;
            srcBuf->v_stride = yuvMeta->vStride;
        } else {
            ALOGE("%s: preview format %d not support",__FUNCTION__, mCurrentFrame->format);
            return;
        }

        dstInfo = &(mipu->dst_info);
        dstBuf = &(dstInfo->dstBuf);
        memset(dstInfo, 0, sizeof(struct dest_data_info));

        dstInfo->dst_mode = IPU_OUTPUT_TO_FRAMEBUFFER | IPU_OUTPUT_BLOCK_MODE;
        dstInfo->fmt = dst_handle->iFormat;
        dstInfo->dtlb_base = mDevice->getTlbBase();

        dstInfo->left = 0;
        dstInfo->top = 0;
        dstInfo->width = dst_handle->iWidth;
        dstInfo->height = dst_handle->iHeight;
        dstInfo->out_buf_v = dst_buf;
        dstBuf->y_buf_v = (void*)(dst_buf);
        dstBuf->y_stride = dst_handle->iStride * (dst_handle->uiBpp >> 3);

        if (init_ipu_dev(yuvMeta->width, yuvMeta->height, yuvMeta->format) < 0) {
            ALOGE("%s: ipu init failed ipuHalder",__FUNCTION__);
            dmmu_unmap_user_memory(&dst_dmmu);
            return;
        }

        ipu_postBuffer(mipu);
        dmmu_unmap_user_memory(&dst_dmmu);
    }

    int CameraHal1::open_ipu_dev(void) {
        if (ipu_open(&mipu) < 0) {
            ALOGE("ipu_open() failed ipuHandler");
            ipu_close(&mipu);
            mipu = NULL;
            return -1;
        }
        ipu_open_status = true;
        first_init_ipu = true;
        return 0;
    }

    void CameraHal1::close_ipu_dev(void) {

        AutoMutex lock(mlock);

        if (ipu_open_status == false)
            return;

        int err = 0;
        err = ipu_close(&mipu);
        if (err < 0) {
            ALOGE("%s: ipu_close failed ipuHalder", __FUNCTION__);
        }
        mipu = NULL;
        ipu_open_status = false;
        first_init_ipu = true;
    }

    int CameraHal1::init_ipu_dev(int w, int h, int f) {
        static int width = 0;
        static int height = 0;
        static int format = 0;

        if (first_init_ipu) {
            first_init_ipu = false;
            width = 0;
            height = 0;
            format = 0;
        }

        if (w != width || h != height || f != format) {
            width = w;
            height = h;
            format = f;

            if (ipu_init(mipu) < 0) {
                close_ipu_dev();
                return -1;
            }
        }
        return 0;
    }

    status_t CameraHal1::WorkThread::stopThread() {

        mCameraHal->camera_state_lock.lock();

        if ( mCameraHal->camera_state == CAMERA_NONE ) {
            mCameraHal->camera_state_lock.unlock();
            requestExitAndWait();
            return NO_ERROR;
        }

        if ( mCameraHal->camera_state == START_CAPTURE ||
             mCameraHal->camera_state == RUN_CAPTURE ) {
            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = STOP_CAPTURE;
            mCameraHal->camera_state_lock.unlock();
            requestExitAndWait();
            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = CAMERA_NONE;
            return NO_ERROR;
        }

        if (mCameraHal->camera_state == START_PREVIEW ||
            mCameraHal->camera_state == RUN_PREVIEW) {

            ALOGE_IF(DEBUG_HAL1,"stopThread, stop preview");
            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = STOP_PREVIEW;
            mCameraHal->camera_state_lock.unlock();
            ALOGE_IF(DEBUG_HAL1,"requestExitandWait");
            requestExitAndWait();
            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = CAMERA_NONE;
            return NO_ERROR;
        }

        if ( mCameraHal->camera_state == STOP_PREVIEW || mCameraHal->camera_state == STOP_CAPTURE) {
            mCameraHal->camera_state_lock.unlock();
            requestExitAndWait();
            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = CAMERA_NONE;
            return NO_ERROR;
        }

        mCameraHal->camera_state_lock.unlock();
        return NO_ERROR;
    }


    bool CameraHal1::WorkThread::threadLoop() {

        status_t res = NO_ERROR;
        mCameraHal->camera_state_lock.lock();
        if ( mCameraHal->camera_state == START_CAPTURE ) {
            mCameraHal->camera_state_lock.unlock();
            res = mCameraHal->readyToCapture();
            if ( res != NO_ERROR ) {
                ALOGE("readyToCapture failed");
                mCameraHal->camera_state_lock.lock();
                mCameraHal->prev_camera_state = mCameraHal->camera_state;
                mCameraHal->camera_state = CAMERA_NONE;
                mCameraHal->camera_state_lock.unlock();
                return false;
            }
            mCameraHal->camera_state_lock.lock();
            if ( mCameraHal->camera_state == STOP_CAPTURE ) {

                mCameraHal->camera_state_lock.unlock();
                ALOGE_IF(DEBUG_HAL1,"threadLoop free capture");
                mCameraHal->getDevice()->sendCommand(STOP_PICTURE);
                mCameraHal->getDevice()->freeStream(CAPTURE_BUFFER);

                mCameraHal->camera_state_lock.lock();
                mCameraHal->prev_camera_state = mCameraHal->camera_state;
                mCameraHal->camera_state = CAMERA_NONE;
                mCameraHal->camera_state_lock.unlock();
                return false;
            }
            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = RUN_CAPTURE;
            mCameraHal->camera_state_lock.unlock();
            return true;
        }
        else if (mCameraHal->camera_state == RUN_CAPTURE) {
            mCameraHal->camera_state_lock.unlock();
            if (mCameraHal->thread_body_capture()) {

                ALOGE_IF(DEBUG_HAL1,"threadLoop free capture");
                mCameraHal->getDevice()->sendCommand(STOP_PICTURE);
                usleep(10000);
                mCameraHal->getDevice()->freeStream(CAPTURE_BUFFER);

                mCameraHal->camera_state_lock.lock();
                mCameraHal->prev_camera_state = mCameraHal->camera_state;
                mCameraHal->camera_state = STOP_CAPTURE;
                mCameraHal->camera_state_lock.unlock();

                return false;
            }
            else {
                ALOGE("capture error");
                mCameraHal->getDevice()->sendCommand(STOP_PICTURE);
                mCameraHal->getDevice()->freeStream(CAPTURE_BUFFER);

                mCameraHal->camera_state_lock.lock();
                mCameraHal->prev_camera_state = mCameraHal->camera_state;
                mCameraHal->camera_state = CAMERA_NONE;
                mCameraHal->camera_state_lock.unlock();
                return false;
            }
        } else if (mCameraHal->camera_state == START_PREVIEW) {
            mCameraHal->camera_state_lock.unlock();
            res = mCameraHal->readyToPreview();
            if ( res != NO_ERROR ) {
                ALOGE("ready to preview failed");
                return false;
            }
            mCameraHal->camera_state_lock.lock();
            if ( mCameraHal->camera_state == STOP_PREVIEW ) {
                mCameraHal->camera_state_lock.unlock();
                ALOGE_IF(DEBUG_HAL1,"threadLoop: camera_state = %d stop_preview", 
                         mCameraHal->camera_state);
                mCameraHal->freePreview();
                mCameraHal->camera_state_lock.lock();
                mCameraHal->prev_camera_state = mCameraHal->camera_state;
                mCameraHal->camera_state = CAMERA_NONE;
                mCameraHal->camera_state_lock.unlock();
                return false;
            }
            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = RUN_PREVIEW;
            mCameraHal->camera_state_lock.unlock();
            return true;
        } else if (mCameraHal->camera_state == RUN_PREVIEW ) {
            mCameraHal->camera_state_lock.unlock();
            if (mCameraHal->thread_body_preview()) {

                mCameraHal->camera_state_lock.lock();
                if ( mCameraHal->camera_state == STOP_PREVIEW ) {
                    mCameraHal->camera_state_lock.unlock();
                    ALOGE_IF(DEBUG_HAL1,"threadLoop: camera_state = %d stop_preview", 
                             mCameraHal->camera_state);
                    mCameraHal->freePreview();
                    mCameraHal->camera_state_lock.lock();
                    mCameraHal->prev_camera_state = mCameraHal->camera_state;
                    mCameraHal->camera_state = CAMERA_NONE;
                    mCameraHal->camera_state_lock.unlock();
                    return false;

                }
                mCameraHal->camera_state_lock.unlock();
                return !mOnce;
            }
            else {
                ALOGE_IF(DEBUG_HAL1,"threadLoop: camera_state = %d preview error", mCameraHal->camera_state);
                mCameraHal->freePreview();
                mCameraHal->camera_state_lock.lock();
                mCameraHal->prev_camera_state = mCameraHal->camera_state;
                mCameraHal->camera_state = CAMERA_NONE;
                mCameraHal->camera_state_lock.unlock();
                return false;
            }
        } 
        else if ( mCameraHal->camera_state == STOP_PREVIEW ) {

            if (mCameraHal->prev_camera_state == RUN_PREVIEW) {
                mCameraHal->camera_state_lock.unlock();
                mCameraHal->freePreview();
                mCameraHal->camera_state_lock.lock();
                mCameraHal->prev_camera_state = mCameraHal->camera_state;
                mCameraHal->camera_state = CAMERA_NONE;
                mCameraHal->camera_state_lock.unlock();
                return false;
            }

            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = CAMERA_NONE;
            mCameraHal->camera_state_lock.unlock();
            return false;
        }
        else if ( mCameraHal->camera_state == STOP_CAPTURE ) {

            if (mCameraHal->prev_camera_state == RUN_CAPTURE) {
                mCameraHal->camera_state_lock.unlock();
                mCameraHal->getDevice()->sendCommand(STOP_PICTURE);
                mCameraHal->getDevice()->freeStream(CAPTURE_BUFFER);
                mCameraHal->camera_state_lock.lock();
                mCameraHal->prev_camera_state = mCameraHal->camera_state;
                mCameraHal->camera_state = CAMERA_NONE;
                mCameraHal->camera_state_lock.unlock();
                return false;
            }
            mCameraHal->prev_camera_state = mCameraHal->camera_state;
            mCameraHal->camera_state = CAMERA_NONE;
            mCameraHal->camera_state_lock.unlock();
            return false;
        }
        mCameraHal->camera_state_lock.unlock();
        return false;
    }

    //------------------------------------------------------------------//

    /**
       there is call by camera service */

    camera_device_ops_t CameraHal1::mCamera1Ops = {
        CameraHal1::set_preview_window,
        CameraHal1::set_callbacks,
        CameraHal1::enable_msg_type,
        CameraHal1::disable_msg_type,
        CameraHal1::msg_type_enabled,
        CameraHal1::start_preview,
        CameraHal1::stop_preview,
        CameraHal1::preview_enabled,
        CameraHal1::store_meta_data_in_buffers,
        CameraHal1::start_recording,
        CameraHal1::stop_recording,
        CameraHal1::recording_enabled,
        CameraHal1::release_recording_frame,
        CameraHal1::auto_focus,
        CameraHal1::cancel_auto_focus,
        CameraHal1::take_picture,
        CameraHal1::cancel_picture,
        CameraHal1::set_parameters,
        CameraHal1::get_parameters,
        CameraHal1::put_parameters,
        CameraHal1::send_command,
        CameraHal1::release,
        CameraHal1::dump
    };

    int CameraHal1::set_preview_window(struct camera_device* device,
                                       struct preview_stream_ops *window)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(device->priv);
          
        if (ch1 == NULL)
            {
                ALOGE("%s: camera hal1 is null",__FUNCTION__);
                return NO_MEMORY;
            }
        return ch1->setPreviewWindow(window);
    }

    void CameraHal1::set_callbacks(
                                   struct camera_device* dev,
                                   camera_notify_callback notify_cb,
                                   camera_data_callback data_cb,
                                   camera_data_timestamp_callback data_cb_timestamp,
                                   camera_request_memory get_memory,
                                   void* user)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
    }

    void CameraHal1::enable_msg_type(struct camera_device* dev, int32_t msg_type)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->enableMsgType(msg_type);
    }

    void CameraHal1::disable_msg_type(struct camera_device* dev, int32_t msg_type)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->disableMsgType(msg_type);
    }

    int CameraHal1::msg_type_enabled(struct camera_device* dev, int32_t msg_type)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->isMsgTypeEnabled(msg_type);
    }

    int CameraHal1::start_preview(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->startPreview();
    }

    void CameraHal1::stop_preview(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->stopPreview();
    }

    int CameraHal1::preview_enabled(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->isPreviewEnabled();
    }

    int CameraHal1::store_meta_data_in_buffers(struct camera_device* dev,
                                               int enable)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->storeMetaDataInBuffers(enable);
    }

    int CameraHal1::start_recording(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->startRecording();
    }

    void CameraHal1::stop_recording(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->stopRecording();
    }

    int CameraHal1::recording_enabled(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->isRecordingEnabled();
    }

    void CameraHal1::release_recording_frame(struct camera_device* dev,
                                             const void* opaque)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->releaseRecordingFrame(opaque);
    }

    int CameraHal1::auto_focus(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->setAutoFocus();
    }

    int CameraHal1::cancel_auto_focus(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->cancelAutoFocus();
    }

    int CameraHal1::take_picture(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->takePicture();
    }

    int CameraHal1::cancel_picture(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->cancelPicture();
    }

    int CameraHal1::set_parameters(struct camera_device* dev, const char* parms)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->setParameters(parms);
    }

    char* CameraHal1::get_parameters(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NULL;
        }
        return ch1->getParameters();
    }

    void CameraHal1::put_parameters(struct camera_device* dev, char* params)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->putParameters(params);
    }

    int CameraHal1::send_command(struct camera_device* dev,
                                 int32_t cmd,
                                 int32_t arg1,
                                 int32_t arg2)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->sendCommand(cmd, arg1, arg2);
    }

    void CameraHal1::release(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->releaseCamera();
    }

    int CameraHal1::dump(struct camera_device* dev, int fd)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->dumpCamera(fd);
    }

    int CameraHal1::device_close(struct hw_device_t* device)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(reinterpret_cast<struct camera_device*>(device)->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->deviceClose();
    }
}; // end namespace
