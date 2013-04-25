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

#define LOG_TAG "CameraV4L2Device"
#define DEBUG_V4L2 0

#include "CameraV4L2Device.h"
#include "JZCameraParameters.h"
#include <sys/stat.h>

namespace android {

     Mutex CameraV4L2Device::sLock;
     CameraV4L2Device* CameraV4L2Device::sInstance = NULL;

     CameraV4L2Device* CameraV4L2Device::getInstance() {
          ALOGE_IF(DEBUG_V4L2,"%s", __FUNCTION__);
          Mutex::Autolock _l(sLock);
          CameraV4L2Device* instance = sInstance;
          if (instance == 0) {
               instance = new CameraV4L2Device();
               sInstance = instance;
          }
          return instance;
     }

     CameraV4L2Device::CameraV4L2Device()
          :CameraDeviceCommon(),
          mlock("CameraV4L2Device::lock"),
          device_fd(-1),
          V4L2DeviceState(DEVICE_UNINIT),
          currentId(-1),
          mtlb_base(0),
          need_update(false),
          videoIn(NULL),
          nQueued(0),
          nDequeued(0),
          mframeBufferIdx(0),
          mCurrentFrameIndex(0),
          mPreviewFrameSize(0),
          mCaptureFrameSize(0) {

               videoIn = (struct vdIn*)calloc(1, sizeof(struct vdIn));
               ccc = new CameraColorConvert();
               memset(&mglobal_info, 0, sizeof(struct global_info));
               memset(device_name, 0, 256);
               memset(&preview_buffer, 0, sizeof(struct camera_buffer));
               memset(&capture_buffer, 0, sizeof(struct camera_buffer));
          
               for (int i = 0; i < NB_BUFFER; ++i) {
                    mPreviewBuffer[i] = NULL;
               }

               initGlobalInfo();
          }

     void CameraV4L2Device::setDeviceCount(int num)
     {
          mglobal_info.sensor_count = num;
     }

     void CameraV4L2Device::update_device_name(const char* deviceName, int len)
     {
          strncpy(device_name, deviceName, len);
          device_name[len+1] = '\0';
          need_update = true;
     }

     void CameraV4L2Device::initGlobalInfo(void)
     {
          
          mglobal_info.preview_buf_nr = NB_BUFFER;
          mglobal_info.capture_buf_nr = NB_BUFFER;
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return;
     }

     CameraV4L2Device::~CameraV4L2Device()
     {
          if (ccc) {
               delete ccc;
               ccc = NULL;
          }

          Close();
          free(videoIn);
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
     }


     int CameraV4L2Device::allocateStream(BufferType type,camera_request_memory get_memory,
                                          uint32_t width,
                                          uint32_t height,
                                          int format)
     {
          status_t res = NO_ERROR;

          size_t size = 0;
          int nr = 1;
          struct camera_buffer* buf = NULL;
          switch(type)
          {

          case PREVIEW_BUFFER:
          {
               nr = mglobal_info.preview_buf_nr;
               size = width * height << 1;
               mPreviewFrameSize = (unsigned int)size;
               buf = &preview_buffer;
               buf->size = (size_t)size;
               buf->common = get_memory(-1, buf->size,nr,NULL);
               if (buf->common) {
                    for (int i=0; i < nr; i++) {
                         mPreviewBuffer[i] = (uint8_t*)buf->common->data + (i * buf->size);
                    }
                    ALOGE_IF(DEBUG_V4L2, "%s: Alloc preview buffer nr = %d, size=%d, frame=%dx%d",
                             __FUNCTION__, nr , buf->size,width, height);
               }
               mCurrentFrameIndex = 0;
               mframeBufferIdx = 0;
               break;
          }

          case CAPTURE_BUFFER:
          {
               size = width*height<<1;
               buf = &capture_buffer;
               buf->size = (size_t)size;
               mCaptureFrameSize = buf->size;
               buf->common = get_memory(-1, buf->size, nr, NULL);
               if (buf->common) {
                    ALOGE_IF(DEBUG_V4L2, "%s: Alloc capture buffer nr = %d,  size = %d , frame = %dx%d",
                             __FUNCTION__, nr, buf->size, width, height);
               }
               break;
          }

          }

          if (buf->common && buf->common->data) {

              buf->dmmu_info.vaddr = buf->common->data;
              buf->dmmu_info.size = buf->common->size;

              {
                  for (int i = 0; i < (int)(buf->common->size); i += 0x1000) {
                      ((uint8_t*)(buf->common->data))[i] = 0xff;
                  }
                  ((uint8_t*)(buf->common->data))[buf->common->size - 1] = 0xff;
              }

              dmmu_map_user_memory(&(buf->dmmu_info));

          }

          allocV4L2Buffer();

          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return NO_ERROR;
     }

     void CameraV4L2Device::freeStream(BufferType type)
     {
          struct camera_buffer* buf = &preview_buffer;
          unsigned int i = 0;

          if (type == PREVIEW_BUFFER && buf->common != NULL)
          {
               dmmu_unmap_user_memory(&(buf->dmmu_info));
               buf->common->release(buf->common);
               buf->common->data = NULL;
               buf->size = 0;
               mCurrentFrameIndex = 0;
               mframeBufferIdx = 0;
          } else if (type == CAPTURE_BUFFER && capture_buffer.common != NULL)
          {
               dmmu_unmap_user_memory(&(capture_buffer.dmmu_info));
               capture_buffer.common->release(capture_buffer.common);
               capture_buffer.common->data = NULL;
               capture_buffer.common = NULL;
               capture_buffer.size = 0;
          }

          freeV4L2Buffer();

          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return;
     }

     int CameraV4L2Device::allocV4L2Buffer(void)
     {
          ALOGE_IF(DEBUG_V4L2,"Enter %s line = %d", __FUNCTION__, __LINE__);

          int ret = NO_ERROR;

          if (device_fd < 0) {
               ALOGE("%s: Device not open device_fd < 0",__FUNCTION__);
               return NO_INIT;
          }

          if (nQueued != 0) {
               ALOGE("%s: Device is allocV4L2Buffer already",__FUNCTION__);
               return ret;
          }

          memset(&videoIn->rb, 0, sizeof(videoIn->rb));
          videoIn->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          videoIn->rb.memory - V4L2_MEMORY_MMAP;
          videoIn->rb.count = NB_BUFFER;
          
          ret = ::ioctl(device_fd, VIDIOC_REQBUFS, &videoIn->rb);
          if (ret < 0) {
               ALOGE("Init: VIDIOC_REQBUFS failed: %s", strerror(errno));
               return ret;
          }
          
          if (videoIn->rb.count < 2) {
               ALOGE("Insufficient buffer memory on : %s", device_name);
               return UNKNOWN_ERROR;
          }

          for (unsigned int i = 0; i < videoIn->rb.count; ++i) {

               memset(&videoIn->buf, 0, sizeof(struct v4l2_buffer));
               videoIn->buf.index = i;
               videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               videoIn->buf.memory = V4L2_MEMORY_MMAP;
               
               ret = ::ioctl(device_fd, VIDIOC_QUERYCAP, &videoIn->buf);
               if (ret < 0) {
                    ALOGE("Init: Unable to query buffer (%s)", strerror(errno));
                    return ret;
               }
               videoIn->mem[i] = mmap(0,
                                      videoIn->buf.length,
                                      PROT_READ|PROT_WRITE,
                                      MAP_SHARED,
                                      device_fd,
                                      videoIn->buf.m.offset);
               if (videoIn->mem[i] == MAP_FAILED) {
                    ALOGE("Init: Unable to map buffer (%s)", strerror(errno));
                    return -1;
               }
               ret = ::ioctl(device_fd, VIDIOC_QBUF, &videoIn->buf);
               if (ret < 0) {
                    ALOGE("Init: VIDIOC_QBUF failed");
                    return -1;
               }
               nQueued++;
          }
          
          size_t tmpbuf_size = 0;
          switch (videoIn->format.fmt.pix.pixelformat)
          {
          case V4L2_PIX_FMT_JPEG:
          case V4L2_PIX_FMT_MJPEG:
          case V4L2_PIX_FMT_UYVY:
          case V4L2_PIX_FMT_YVYU:
          case V4L2_PIX_FMT_YYUV:
          case V4L2_PIX_FMT_YUV420: // only needs 3/2 bytes per pixel but we alloc 2 bytes per pixel
          case V4L2_PIX_FMT_YVU420: // only needs 3/2 bytes per pixel but we alloc 2 bytes per pixel
          case V4L2_PIX_FMT_Y41P:   // only needs 3/2 bytes per pixel but we alloc 2 bytes per pixel
          case V4L2_PIX_FMT_NV12:
          case V4L2_PIX_FMT_NV21:
          case V4L2_PIX_FMT_NV16:
          case V4L2_PIX_FMT_NV61:
          case V4L2_PIX_FMT_SPCA501:
          case V4L2_PIX_FMT_SPCA505:
          case V4L2_PIX_FMT_SPCA508:
          case V4L2_PIX_FMT_GREY:
          case V4L2_PIX_FMT_Y16:

          case V4L2_PIX_FMT_YUYV:
               //  YUYV doesn't need a temp buffer but we will set it if/when
               //  video processing disable control is checked (bayer processing).
               //            (logitech cameras only)
               break;
               
          case V4L2_PIX_FMT_SGBRG8: //0
          case V4L2_PIX_FMT_SGRBG8: //1
          case V4L2_PIX_FMT_SBGGR8: //2
          case V4L2_PIX_FMT_SRGGB8: //3
               // Raw 8 bit bayer
               // when grabbing use:
               //    bayer_to_rgb24(bayer_data, RGB24_data, width, height, 0..3)
               //    rgb2yuyv(RGB24_data, pFrameBuffer, width, height)

               // alloc a temp buffer for converting to YUYV
               // rgb buffer for decoding bayer data
               tmpbuf_size = videoIn->format.fmt.pix.width * videoIn->format.fmt.pix.height * 3;
               if (videoIn->tmpBuffer) {
                    free(videoIn->tmpBuffer);
               }
               videoIn->tmpBuffer = (uint8_t*)calloc(1, tmpbuf_size);
               if (!videoIn->tmpBuffer) {
                    ALOGE("Couldn't calloc %lu bytes of memory for frame buffer\n",
                          (unsigned long)tmpbuf_size);
                    return NO_MEMORY;
               }
               break;

          case V4L2_PIX_FMT_RGB24:
          case V4L2_PIX_FMT_BGR24:
               break;
          default:
               ALOGE("Should never arrive (1) exit fatal");
               return -1;
          }
          return NO_ERROR;
     }

     void CameraV4L2Device::freeV4L2Buffer(void)
     {
          int ret ;
          
          ALOGE_IF(DEBUG_V4L2,"Enter %s", __FUNCTION__);

          if (device_fd < 0) {
               ALOGE("Unable free V4L2 buffer, device_fd < 0");
               return ;
          }

          memset(&videoIn->buf, 0, sizeof(videoIn->buf));
          videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          videoIn->buf.memory = V4L2_MEMORY_MMAP;

          int DQcount = nQueued - nDequeued;

          for (int i = 0; i < DQcount-1; ++i) {
               ret = ::ioctl(device_fd, VIDIOC_QBUF, &videoIn->buf);
               ALOGE_IF(ret<0,"Uninit: VIDIOC_DQBUF Failed");
          }
          nQueued = 0;
          nDequeued = 0;

          for (int i = 0; i < NB_BUFFER; i++) {
               if (videoIn->mem[i] != NULL) {
                    ret = munmap(videoIn->mem[i], videoIn->buf.length);
                    ALOGE_IF(ret<0,"Uninit: Unmap failed");
                    videoIn->mem[i] = NULL;
               }
          }
          if (videoIn->tmpBuffer) {
               free(videoIn->tmpBuffer);
          }
          videoIn->tmpBuffer = NULL;
     }

     void* CameraV4L2Device::getCurrentFrame(bool tp)
     {
          ALOGE_IF(DEBUG_V4L2,"Enter %s : line=%d",__FUNCTION__,__LINE__);
          int ret = 0;

		  static CameraYUVMeta yuvMeta;
          uint8_t* frameBuffer = NULL;
          int frameSize = 0;

		  memset(&yuvMeta, 0, sizeof(CameraYUVMeta));
          if (!tp && preview_buffer.common) {
               frameBuffer = (uint8_t*)mPreviewBuffer[mframeBufferIdx];
               if (!frameBuffer) {
                    ALOGE("No preview buffer");
                    return NULL;
               }
               mCurrentFrameIndex = mframeBufferIdx;
               mframeBufferIdx = (mframeBufferIdx+1) % mglobal_info.preview_buf_nr;
               frameSize = preview_buffer.size;
          } else if (tp && capture_buffer.common) {
               frameBuffer = (uint8_t*)capture_buffer.common->data;
               if (!frameBuffer) {
                    ALOGE("No capture buffer");
                    return NULL;
               }
               mframeBufferIdx = 0;
               mCurrentFrameIndex = mframeBufferIdx;
               frameSize = capture_buffer.size;
          }

          memset(&videoIn->buf, 0, sizeof(videoIn->buf));
          videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          videoIn->buf.memory = V4L2_MEMORY_MMAP;
          ret = ::ioctl(device_fd , VIDIOC_DQBUF, &videoIn->buf);
          if (ret < 0) {
               ALOGE("%s: VIDIOC_DQBUF failed",__FUNCTION__);
               return NULL;
          }

          nDequeued++;

          int strideOut = videoIn->outWidth << 1;
          uint8_t* src = (uint8_t*)videoIn->mem[videoIn->buf.index];

          if (frameSize < videoIn->outFrameSize) {
               ALOGE("%s: Insufficient space in output buffer: required: %d, got %d dropping frame",
					 __FUNCTION__,videoIn->outFrameSize, frameSize);
          } else {
               switch(videoIn->format.fmt.pix.pixelformat)
               {
               case V4L2_PIX_FMT_UYVY:
                    if (ccc) {
                         ccc->uyvy_to_yuyv((uint8_t*)frameBuffer, strideOut,
                                           src, videoIn->format.fmt.pix.bytesperline, 
										   videoIn->outWidth, videoIn->outHeight);
                    }
                    break;

               case V4L2_PIX_FMT_YVYU:
                    if (ccc) {
                         ccc->yvyu_to_yuyv((uint8_t*)frameBuffer, strideOut,
                                           src, videoIn->format.fmt.pix.bytesperline, 
										   videoIn->outWidth, videoIn->outHeight);
                    }
                    break;

               case V4L2_PIX_FMT_YYUV:
                    if (ccc) {
                         ccc->yyuv_to_yuyv((uint8_t*)frameBuffer, strideOut,
                                           src, videoIn->format.fmt.pix.bytesperline, 
										   videoIn->outWidth, videoIn->outHeight);
                    }
                    break;

               case V4L2_PIX_FMT_YUV420:
                    if (ccc)
                         ccc->yuv420_to_yuyv((uint8_t*)frameBuffer, strideOut, src, 
											 videoIn->outWidth, videoIn->outHeight);
					
                    break;

               case V4L2_PIX_FMT_YVU420:
                    if (ccc)
                         ccc->yvu420_to_yuyv((uint8_t*)frameBuffer, strideOut, 
											 src, videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_NV12:
                    if (ccc)
                         ccc->nv12_to_yuyv((uint8_t*)frameBuffer, strideOut, 
										   src, videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_NV21:
                    if (ccc)
                         ccc->nv21_to_yuyv((uint8_t*)frameBuffer, strideOut, src, 
										   videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_NV16:
                    if (ccc)
                         ccc->nv16_to_yuyv((uint8_t*)frameBuffer, strideOut, src, 
										   videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_NV61:
                    if (ccc)
                         ccc->nv61_to_yuyv((uint8_t*)frameBuffer, strideOut, src, 
										   videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_Y41P:
                    if (ccc)
                         ccc->y41p_to_yuyv((uint8_t*)frameBuffer, strideOut, src, 
										   videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_GREY:
                    if (ccc)
                         ccc->grey_to_yuyv((uint8_t*)frameBuffer, strideOut,
                                           src, videoIn->format.fmt.pix.bytesperline, 
										   videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_Y16:
                    if (ccc)
                         ccc->y16_to_yuyv((uint8_t*)frameBuffer, strideOut,
                                          src, videoIn->format.fmt.pix.bytesperline, 
										  videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_YUYV:
               {
                    int h;
                    uint8_t* pdst = (uint8_t*)frameBuffer;
                    uint8_t* psrc = src;
                    int ss = videoIn->outWidth << 1;
                    for (h = 0; h < videoIn->outHeight; h++) {
                         memcpy(pdst, psrc, ss);
                         pdst += strideOut;
                         psrc += videoIn->format.fmt.pix.bytesperline;
                    }
               }
               break;


               case V4L2_PIX_FMT_RGB24:
                    if (ccc)
                         ccc->rgb_to_yuyv((uint8_t*) frameBuffer, strideOut,
                                          src, videoIn->format.fmt.pix.bytesperline, 
										  videoIn->outWidth, videoIn->outHeight);
                    break;

                    
               case V4L2_PIX_FMT_BGR24:
                    if (ccc)
                         ccc->bgr_to_yuyv((uint8_t*) frameBuffer, strideOut,
                                          src, videoIn->format.fmt.pix.bytesperline, 
										  videoIn->outWidth, videoIn->outHeight);
                    break;

               case V4L2_PIX_FMT_SGBRG8: //0
                    break;

               case V4L2_PIX_FMT_SGRBG8: //1
                    break;

               case V4L2_PIX_FMT_SBGGR8: //2
                    break;

               case V4L2_PIX_FMT_SRGGB8: //3
                    break;

               default:
                    ALOGE("error %s: unknow format: %i", __FUNCTION__,
                          videoIn->format.fmt.pix.pixelformat);
                    break;
               }
               ALOGE_IF(DEBUG_V4L2, "%s: copy frame to destination 0x%p", __FUNCTION__,frameBuffer);
          }

          ret = ::ioctl(device_fd, VIDIOC_QBUF, &videoIn->buf);
          if (ret < 0) {
               ALOGE("%s : VIDIOC_QBUF failed",__FUNCTION__);
               return NULL;
          }

          nQueued++;

          ALOGE_IF(DEBUG_V4L2, "%s: - leave Queued buffer", __FUNCTION__);

		  yuvMeta.index = mCurrentFrameIndex;
		  yuvMeta.count = 1;
		  yuvMeta.width = (int32_t)(videoIn->outWidth);
		  yuvMeta.height = (int32_t)(videoIn->outHeight);
		  yuvMeta.yAddr = (uint32_t)frameBuffer;
		  yuvMeta.vAddr = yuvMeta.yAddr;
		  yuvMeta.uAddr = yuvMeta.yAddr;
          yuvMeta.yPhy = 0;
          yuvMeta.uPhy = 0;
          yuvMeta.vPhy = 0;
		  yuvMeta.yStride = strideOut;
		  yuvMeta.uStride = yuvMeta.yStride;
		  yuvMeta.vStride = yuvMeta.yStride;
		  yuvMeta.format = HAL_PIXEL_FORMAT_YCbCr_422_I;

          // return frameBuffer;
		  return (void*)(&yuvMeta);
     }

     int CameraV4L2Device::getPreviewFrameSize(void)
     {
          return mPreviewFrameSize;
     }

     int CameraV4L2Device::getCaptureFrameSize(void)
     {
          return mCaptureFrameSize;
     }

     int CameraV4L2Device::getNextFrame(void)
     {
          ALOGE_IF(DEBUG_V4L2,"Enter %s : line=%d",__FUNCTION__,__LINE__);
          usleep(1000000L/m_BestPreviewFmt.getFps());
          return NO_ERROR;
     }

     unsigned int CameraV4L2Device::getPreviewFrameIndex(void)
     {
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          
          return mCurrentFrameIndex;
     }

     camera_memory_t* CameraV4L2Device::getPreviewBufferHandle(void)
     {
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return preview_buffer.common;
     }


     camera_memory_t* CameraV4L2Device::getCaptureBufferHandle(void)
     {
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return capture_buffer.common;
     }

     int CameraV4L2Device::getFrameOffset(void)
     {
          ALOGE_IF(DEBUG_V4L2,"Enter %s : line=%d",__FUNCTION__,__LINE__);
          int offset = 0;

          if (preview_buffer.common != NULL) {
               offset = mCurrentFrameIndex * preview_buffer.size;
          } else if (capture_buffer.common != NULL) {
               offset = mCurrentFrameIndex * capture_buffer.size;
          }
          return offset;
     }

     int CameraV4L2Device::setCommonMode(CommonMode mode_type, unsigned short mode_value)
     {
          status_t res = NO_ERROR;

          if (device_fd < 0)
               return INVALID_OPERATION;

          switch(mode_type)
          {
          case WHITE_BALANCE:
               ALOGE_IF(DEBUG_V4L2,"set white balance mode");
               if (mode_value == WHITE_BALANCE_AUTO)
                    update_ctrl_flags(V4L2_CID_AUTO_WHITE_BALANCE);
               else
                    update_ctrl_flags(V4L2_CID_WHITE_BALANCE_TEMPERATURE,mode_value);
               break;
          case EFFECT_MODE:
               ALOGE_IF(DEBUG_V4L2,"set effect mode");
               update_ctrl_flags(V4L2_CID_HUE_AUTO,mode_value);
               break;
          case FOCUS_MODE:
               ALOGE_IF(DEBUG_V4L2,"set focus mode");
               update_ctrl_flags(V4L2_CID_FOCUS_AUTO);
               break;
          case FLASH_MODE:
               ALOGE_IF(DEBUG_V4L2,"set flash mode");
               update_ctrl_flags(V4L2_CID_EXPOSURE);
               break;
          case SCENE_MODE:
               ALOGE_IF(DEBUG_V4L2,"set scene mode");
               update_ctrl_flags(V4L2_CID_COLORFX, mode_value);
               break;
          case ANTIBAND_MODE:
               ALOGE_IF(DEBUG_V4L2,"set antiband mode");
               update_ctrl_flags(V4L2_CID_SHARPNESS,mode_value);
               break;
          }
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return res;
     }

     void CameraV4L2Device::update_ctrl_flags(int id, unsigned short value) {
          ALOGE_IF(DEBUG_V4L2, "Enter %s, line=%d", __FUNCTION__,__LINE__);
     }

     int CameraV4L2Device::getFormat(int format)
     {
          int tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
          switch(format)
          {
          case V4L2_PIX_FMT_YUYV:
               break;
          case V4L2_PIX_FMT_NV21:
               tmp_format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
               break;
          case V4L2_PIX_FMT_NV16:
               tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_SP;
               break;
          case V4L2_PIX_FMT_RGB565:
               tmp_format = HAL_PIXEL_FORMAT_RGB_565;
               break;
          case V4L2_PIX_FMT_RGB24:
               tmp_format = HAL_PIXEL_FORMAT_RGB_888;
               break;
          case V4L2_PIX_FMT_RGB32:
               tmp_format = HAL_PIXEL_FORMAT_RGBA_8888;
               break;
          }
          return tmp_format;
     }

     int CameraV4L2Device::getPreviewFormat(void)
     {
          return getFormat(V4L2_PIX_FMT_YUYV);
     }

     int CameraV4L2Device::getCaptureFormat(void)
     {
          return getFormat(V4L2_PIX_FMT_YUYV);
     }
     
     int CameraV4L2Device::InitParam(int width, int height, int fps)
     {
          status_t res = NO_ERROR;

          ALOGE_IF(DEBUG_V4L2,"Enter %s line = %d",__FUNCTION__, __LINE__);

          static const struct {
               int fmt;
               int bpp;
               int isplanar;
               int allowcrop;
          }pixFmtsOrder[] = {
               {V4L2_PIX_FMT_YUYV,     2,0,1},
               {V4L2_PIX_FMT_YVYU,     2,0,1},
               {V4L2_PIX_FMT_UYVY,     2,0,1},
               {V4L2_PIX_FMT_YYUV,     2,0,1},
               {V4L2_PIX_FMT_SPCA501,  2,0,0},
               {V4L2_PIX_FMT_SPCA505,  2,0,0},
               {V4L2_PIX_FMT_SPCA508,  2,0,0},
               {V4L2_PIX_FMT_YUV420,   0,1,0},
               {V4L2_PIX_FMT_YVU420,   0,1,0},
               {V4L2_PIX_FMT_NV12,     0,1,0},
               {V4L2_PIX_FMT_NV21,     0,1,0},
               {V4L2_PIX_FMT_NV16,     0,1,0},
               {V4L2_PIX_FMT_NV61,     0,1,0},
               {V4L2_PIX_FMT_Y41P,     0,0,0},
               {V4L2_PIX_FMT_SGBRG8,   0,0,0},
               {V4L2_PIX_FMT_SGRBG8,   0,0,0},
               {V4L2_PIX_FMT_SBGGR8,   0,0,0},
               {V4L2_PIX_FMT_SRGGB8,   0,0,0},
               {V4L2_PIX_FMT_BGR24,    3,0,1},
               {V4L2_PIX_FMT_RGB24,    3,0,1},
               {V4L2_PIX_FMT_MJPEG,    0,1,0},
               {V4L2_PIX_FMT_JPEG,     0,1,0},
               {V4L2_PIX_FMT_GREY,     1,0,1},
               {V4L2_PIX_FMT_Y16,      2,0,1},
          };


          if(m_AllFmts.isEmpty()) {
               ALOGE("No video formats availabe");
               return -1;
          }

          frameInterval closest;
          unsigned int i;
          int area = width * height;
          int closestDArea = -1;
          for (i = 0; i < m_AllFmts.size(); ++i) {
               frameInterval f = m_AllFmts[i];

               if (f.getWidth() >= width && f.getHeight() >= height) {
                    closest = f;
                    closestDArea = f.getWidth()*f.getHeight() - area;
                    break;
               }
          }

          if (closestDArea == -1) {
               ALOGE("Size not available: (%dx%d)",width, height);
               return -1;
          }

          bool crop = width != closest.getWidth() || height != closest.getHeight();
          res = -1;
          for (i = 0; i < sizeof(pixFmtsOrder) / sizeof(pixFmtsOrder[0]); ++i) {

               if (!crop || pixFmtsOrder[i].allowcrop) {

                    memset(&videoIn->format, 0 , sizeof(videoIn->format));
                    videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    videoIn->format.fmt.pix.width = closest.getWidth();
                    videoIn->format.fmt.pix.height = closest.getHeight();
                    videoIn->format.fmt.pix.pixelformat = pixFmtsOrder[i].fmt;
                    
                    res = ::ioctl(device_fd, VIDIOC_TRY_FMT,&videoIn->format);
                    if (res >= 0) {
                         break;
                    }
               }
          }
          if (res < 0) {
               ALOGE("Open:VIDIOC_TRY_FMT failed: (%s)", strerror(errno));
               return res;
          }

          /* Set the format */
          memset(&videoIn->format,0,sizeof(videoIn->format));
          videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          videoIn->format.fmt.pix.width = closest.getWidth();
          videoIn->format.fmt.pix.height = closest.getHeight();
          videoIn->format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//pixFmtsOrder[i].fmt;
          videoIn->format.fmt.pix.field = V4L2_FIELD_INTERLACED;
          res = ioctl(device_fd, VIDIOC_S_FMT, &videoIn->format);
          if (res < 0) {
               ALOGE("Open: VIDIOC_S_FMT Failed: %s", strerror(errno));
               return res;
          }

          /* Query for the effective video format used */
          memset(&videoIn->format,0,sizeof(videoIn->format));
          videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          res = ioctl(device_fd, VIDIOC_G_FMT, &videoIn->format);
          if (res < 0) {
               ALOGE("Open: VIDIOC_G_FMT Failed: %s", strerror(errno));
               return res;
          }


          unsigned int min = videoIn->format.fmt.pix.width * 2;
          if (videoIn->format.fmt.pix.bytesperline < min)
               videoIn->format.fmt.pix.bytesperline = min;
          min = videoIn->format.fmt.pix.bytesperline * videoIn->format.fmt.pix.height;
          if (videoIn->format.fmt.pix.sizeimage < min)
               videoIn->format.fmt.pix.sizeimage = min;

          /* Store the pixel formats we will use */
          videoIn->outWidth           = width;
          videoIn->outHeight          = height;
          // Calculate the expected output framesize in YUYV
          videoIn->outFrameSize       = width * height << 1;
          videoIn->capBytesPerPixel   = pixFmtsOrder[i].bpp;

          /* sets video device frame rate */
          memset(&videoIn->params,0,sizeof(videoIn->params));
          videoIn->params.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          videoIn->params.parm.capture.timeperframe.numerator = 1;
          videoIn->params.parm.capture.timeperframe.denominator = closest.getFps();

          /* Set the framerate. If it fails, it wont be fatal */
          if (ioctl(device_fd,VIDIOC_S_PARM,&videoIn->params) < 0) {
               ALOGE("VIDIOC_S_PARM error: Unable to set %d fps", closest.getFps());
          }

          /* Gets video device defined frame rate (not real - consider it a maximum value) */
          if (ioctl(device_fd,VIDIOC_G_PARM,&videoIn->params) < 0) {
               ALOGE("VIDIOC_G_PARM - Unable to get timeperframe");
          }

          ALOGE_IF(DEBUG_V4L2,"Actual format: (%d x %d), Fps: %d, pixfmt: '%c%c%c%c', bytesperline: %d",
                   videoIn->format.fmt.pix.width,
                   videoIn->format.fmt.pix.height,
                   videoIn->params.parm.capture.timeperframe.denominator,
                   videoIn->format.fmt.pix.pixelformat & 0xFF, (videoIn->format.fmt.pix.pixelformat >> 8) & 0xFF,
                   (videoIn->format.fmt.pix.pixelformat >> 16) & 0xFF, (videoIn->format.fmt.pix.pixelformat >> 24) & 0xFF,
                   videoIn->format.fmt.pix.bytesperline);
          return res;
     }
     
     int CameraV4L2Device::setCameraParam(struct camera_param& params, int fps)
     {
          status_t res = NO_ERROR;
          int width, height;

          ALOGE_IF(DEBUG_V4L2,"Enter %s line = %d",__FUNCTION__, __LINE__);

         
          if (device_fd < 0)
               return INVALID_OPERATION;

          if (params.cmd == CPCMD_SET_PREVIEW_RESOLUTION) {
               width = params.param.ptable[0].w;
               height = params.param.ctable[0].h;
               res = InitParam(width, height, fps);
          }

          return res;
     }

     void CameraV4L2Device::initModeValues(struct sensor_info* s_info)
     {
          s_info->modes.balance = (WHITE_BALANCE_AUTO | WHITE_BALANCE_INCANDESCENT
                                        | WHITE_BALANCE_FLUORESCENT |WHITE_BALANCE_WARM_FLUORESCENT
                                        | WHITE_BALANCE_DAYLIGHT | WHITE_BALANCE_CLOUDY_DAYLIGHT);
          s_info->modes.effect = (EFFECT_MONO | EFFECT_NEGATIVE | EFFECT_SOLARIZE | EFFECT_SEPIA |
                                       EFFECT_POSTERIZE | EFFECT_WHITEBOARD);
          s_info->modes.antibanding = (ANTIBANDING_AUTO | ANTIBANDING_50HZ | ANTIBANDING_60HZ | ANTIBANDING_OFF);
          s_info->modes.flash_mode = (FLASH_MODE_OFF | FLASH_MODE_AUTO | FLASH_MODE_ON | FLASH_MODE_RED_EYE | FLASH_MODE_TORCH);
          s_info->modes.scene_mode = (SCENE_MODE_AUTO |
                                           SCENE_MODE_ACTION |
                                           SCENE_MODE_PORTRAIT |
                                           SCENE_MODE_LANDSCAPE |
                                           SCENE_MODE_NIGHT |
                                           SCENE_MODE_NIGHT_PORTRAIT |
                                           SCENE_MODE_THEATRE |
                                           SCENE_MODE_BEACH |
                                           SCENE_MODE_SNOW |
                                           SCENE_MODE_SUNSET |
                                           SCENE_MODE_STEADYPHOTO |
                                           SCENE_MODE_FIREWORKS |
                                           SCENE_MODE_SPORTS |
                                           SCENE_MODE_PARTY |
                                           SCENE_MODE_CANDLELIGHT);
          s_info->modes.focus_mode = ( FOCUS_MODE_FIXED |
                                            FOCUS_MODE_AUTO |
                                            FOCUS_MODE_INFINITY |
                                            FOCUS_MODE_MACRO );
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
     }

     void CameraV4L2Device::getSensorInfo(struct sensor_info* s_info,struct resolution_info* r_info )
     {
          status_t res = NO_ERROR;
          int flag = 0;
          struct v4l2_fmtdesc fmt;

          if (device_fd < 0)
               return;

          memset (&fmt, 0, sizeof(struct v4l2_fmtdesc));
          fmt.index = 0;
          fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         
          while((res = ::ioctl(device_fd, VIDIOC_ENUM_FMT, &fmt)) == 0)
          {
               fmt.index++;
               switch(fmt.pixelformat)
               {
               case V4L2_PIX_FMT_NV16:
                    break;
               case V4L2_PIX_FMT_NV21:
                    break;
               case V4L2_PIX_FMT_YUYV:
                    flag = 1;
                    break;
               case V4L2_PIX_FMT_YUV420:
                    break;
               case V4L2_PIX_FMT_JPEG:
                    break;
               }
               if (flag)
                    break;
          }

          
          if (flag == 0)
          {
               ALOGE("%s: do not support frame format YUYV, %d (%s)",__FUNCTION__,
                     errno, strerror(errno));
               return;
          }

          struct v4l2_frmsizeenum fsize;
          memset(&fsize, 0, sizeof(struct v4l2_frmsizeenum));
          fsize.index = 0;
          fsize.pixel_format = V4L2_PIX_FMT_YUYV;

          while ((res = ::ioctl(device_fd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0)
          {
               if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
               {
                    (r_info->ptable)[fsize.index].w = fsize.discrete.width;
                    (r_info->ptable)[fsize.index].h = fsize.discrete.height;

                    (r_info->ctable)[fsize.index].w = fsize.discrete.width;
                    (r_info->ctable)[fsize.index].h = fsize.discrete.height;

               } else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
               {
                    break;
               } else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
               {
                    break;
               }
               fsize.index++;
               s_info->prev_resolution_nr++;
               s_info->cap_resolution_nr++;
          }
          if (res != NO_ERROR && errno != EINVAL)
          {
               ALOGE("%s: enum frame size error, %d => (%s)",
                     __FUNCTION__, errno, strerror(errno));
          }

          initModeValues(s_info);

          return;
     }

     int CameraV4L2Device::getResolution(struct resolution_info* info)
     {
          ALOGE_IF(DEBUG_V4L2, "Enter %s", __FUNCTION__);

          info->format = getFormat(V4L2_PIX_FMT_YUYV);
          unsigned int count = m_AllFmts.size();
          if (m_AllFmts.size() > 16)
              count = 16;
          for (unsigned i = 0; i < count; ++i) {
              (info->ptable)[i].w = m_AllFmts[i].getWidth();
              (info->ptable)[i].h = m_AllFmts[i].getHeight();
                   
          }
          return NO_ERROR;
     }


     int CameraV4L2Device::getCurrentCameraId(void)
     {
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return currentId;
     }

     void CameraV4L2Device::Close(void)
     {
          if (videoIn->tmpBuffer)
               free(videoIn->tmpBuffer);
          videoIn->tmpBuffer = NULL;

          if (device_fd > 0)
               close(device_fd);
          device_fd = -1;
     }

     int CameraV4L2Device::connectDevice(int id)
     {
          status_t res = NO_ERROR;
          struct stat st;

          ALOGE_IF(DEBUG_V4L2,"%s:connect %d camera, state = %d",__FUNCTION__, id, V4L2DeviceState);

          if (device_fd > 0 && need_update) {
               Close();
               need_update = false;
          }

          if (device_fd < 0)
          {
               if (-1 == stat(device_name, &st)) {
                    ALOGE("Cannot identify '%s': %d, %s", device_name, errno, strerror(errno));
                    return NO_INIT;
               }
               
               if (!S_ISCHR(st.st_mode)) {
                    ALOGE("%s is no device", device_name);
                    return NO_INIT;
               }

               device_fd = open(device_name, O_RDWR|O_NONBLOCK);

               if (device_fd < 0) {

                    res = device_fd;
                    memset(device_name,0,256);
                    currentId = -1;
                    V4L2DeviceState = DEVICE_UNINIT;
                    ALOGE("%s: can not connect %s device", __FUNCTION__,device_name);
                    return NO_INIT;
               }
               
               memset(videoIn, 0, sizeof(struct vdIn));
               res = ::ioctl(device_fd, VIDIOC_QUERYCAP, &videoIn->cap);
               if (res != NO_ERROR) {
                    ALOGE("%s: opening device unable to query device.",__FUNCTION__);
                    return NO_INIT;
               }
               
               ALOGE_IF(DEBUG_V4L2,"driver: %s, card: %s, bus_info: %s, version:%d.%d.%d, capabilities: 0x%08x",
                        (const char*)(videoIn->cap.driver), (const char*)(videoIn->cap.card),
                        (const char*)(videoIn->cap.bus_info) ,
                        videoIn->cap.version>>16, (videoIn->cap.version>>8)&0xff, (videoIn->cap.version)&0xff, videoIn->cap.capabilities);

               if (!(videoIn->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                    ALOGE("%s: opening device: video capture not supported",__FUNCTION__);
                    return NO_INIT;
               }

               if (!(videoIn->cap.capabilities & V4L2_CAP_STREAMING))
               {
                    ALOGE("%s: not support streaming I/O",__FUNCTION__);
                    return NO_INIT;
               }

               videoIn->cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               if (0 == ::ioctl(device_fd, VIDIOC_CROPCAP, &videoIn->cropcap)) {
                    videoIn->crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    videoIn->crop.c = videoIn->cropcap.defrect;
                    if (-1 == ::ioctl(device_fd, VIDIOC_S_CROP, &videoIn->crop)) {
                         switch(errno) {
                         case EINVAL:
                              ALOGE("Cropping not supported");
                              break;
                         default:
                              break;
                         }
                    }
               }

               if (currentId != id) {
                   V4L2DeviceState = DEVICE_CONNECTED;
                   currentId = id;
                   EnumFrameFormats();
                   initDefaultControls();
                   dmmu_init();
                   dmmu_get_page_table_base_phys(&mtlb_base);
               }
          }

          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return res;
     }

     void CameraV4L2Device::disConnectDevice(void)
     {
          status_t res = NO_ERROR;

          ALOGE_IF(DEBUG_V4L2,"%s: uvc disconnect %d camera, state = %d",__FUNCTION__,currentId,V4L2DeviceState);

          if (V4L2DeviceState == DEVICE_STOPED)
          {
               if (device_fd >= 0)
               {
                    close(device_fd);
                    device_fd = -1;
               }
               V4L2DeviceState = DEVICE_UNINIT;
               currentId = -1;
               dmmu_deinit();
               mtlb_base = 0;
          }
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return ;
     }

     int CameraV4L2Device::startDevice(void)
     {
          status_t res = NO_ERROR;

          ALOGE_IF(DEBUG_V4L2,"%s: uvc device state = %d",__FUNCTION__,V4L2DeviceState);

          if (V4L2DeviceState == DEVICE_CONNECTED || (videoIn->isStreaming == false))
          {
          start_device:
               enum v4l2_buf_type type;
               type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               if ((res = ::ioctl(device_fd, VIDIOC_STREAMON, &type)) != NO_ERROR)
               {
                    ALOGE("%s: Unable to start stream",
                          __FUNCTION__);
                    return res;
               }
               V4L2DeviceState = DEVICE_STARTED;
               videoIn->isStreaming = true;
          }

          if (V4L2DeviceState == DEVICE_STARTED)
               return res;

          if (V4L2DeviceState == DEVICE_STOPED)
          {
               goto start_device;
          }
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return NO_ERROR;
     }

     int CameraV4L2Device::stopDevice(void)
     {
          status_t res = NO_ERROR;

          ALOGE_IF(DEBUG_V4L2,"%s: uvc device state = %d",__FUNCTION__,V4L2DeviceState);
          if (V4L2DeviceState == DEVICE_STARTED || videoIn->isStreaming)
          {
               enum v4l2_buf_type type;
               type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               if ((res = ::ioctl(device_fd, VIDIOC_STREAMOFF, &type)) != NO_ERROR)
               {
                    ALOGE("%s: Unable stop device",__FUNCTION__);
                    return res;
               }
          }
          Close();
          return res;
     }

     int CameraV4L2Device::getCameraModuleInfo(int camera_id, struct camera_info* info)
     {

          if (camera_id == 0)
          {
               info->facing = CAMERA_FACING_BACK;
               info->orientation = 90;
          } else if(camera_id == 1)
          {
               info->facing = CAMERA_FACING_FRONT;
               info->orientation = 270;
          }

          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return NO_ERROR;
     }

     int CameraV4L2Device::getCameraNum(void)
     {
          int nr = 0;

          nr = mglobal_info.sensor_count;

          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return nr;
     }

     int CameraV4L2Device::sendCommand(uint32_t cmd_type, uint32_t arg1, uint32_t arg2, uint32_t result)
     {
          status_t res = NO_ERROR;

          ALOGE_IF(DEBUG_V4L2,"%s: uvc device state = %d",__FUNCTION__,V4L2DeviceState);
          switch(cmd_type)
          {
          case PAUSE_FACE_DETECT:
               break;
          case START_FOCUS:
               break;
          case GET_FOCUS_STATUS:
               break;
          case START_PREVIEW:
               break;
          case STOP_PREVIEW:
               break;
          case START_ZOOM:
               break;
          case STOP_ZOOM:
               break;
          case START_FACE_DETECT:
               break;
          case STOP_FACE_DETECT:
               break;
          case TAKE_PICTURE:
          {
               int width = arg1;
               int height = arg2;

               res = connectDevice(currentId);
               if (res == NO_ERROR) {
                    InitParam(width, height, 1);
                    res = allocV4L2Buffer();
                    if (res == NO_ERROR) {
                         startDevice();
                    } else {
                         ALOGE("alloc v4l2buffer error");
                    }
               }
          }
          break;
          case STOP_PICTURE:
               break;
          }
          ALOGE_IF(DEBUG_V4L2,"Exit %s, line=%d",__FUNCTION__,__LINE__);
          return NO_ERROR;
     }

     void CameraV4L2Device::EnumFrameFormats()
     {
          ALOGE_IF(DEBUG_V4L2, "Enter %s",__FUNCTION__);
        
          struct v4l2_fmtdesc fmt;
        
          m_AllFmts.clear();

          memset(&fmt, 0, sizeof(struct v4l2_fmtdesc));
          fmt.index = 0;
          fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

          while(::ioctl(device_fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
               fmt.index++;

               if (!EnumFrameSizes(fmt.pixelformat)) {
                    ALOGE("Unable to enumerate frame sizes.");
               }
          }
        
          m_BestPreviewFmt = frameInterval();
          m_BestPictureFmt = frameInterval();

          unsigned int i;
          for (i=0; i < m_AllFmts.size(); i++) {
               frameInterval f = m_AllFmts[i];

               if ((f.getWidth() > m_BestPictureFmt.getWidth() && f.getHeight() > m_BestPictureFmt.getHeight())
                   || ((f.getWidth() == m_BestPictureFmt.getWidth() && f.getHeight() == m_BestPictureFmt.getHeight())
                       && f.getFps() < m_BestPictureFmt.getFps())) {
                    m_BestPictureFmt = f;
               }

               if ((f.getFps() > m_BestPreviewFmt.getFps()) ||
                   (f.getFps() == m_BestPreviewFmt.getFps() && 
                    (f.getWidth() > m_BestPictureFmt.getWidth() && f.getHeight() > m_BestPictureFmt.getHeight()))) {
                    m_BestPreviewFmt = f;
               }
          }
          return;
     }

     bool CameraV4L2Device::EnumFrameSizes(int pixfmt)
     {
          ALOGE_IF(DEBUG_V4L2,"Enter %s",__FUNCTION__);
          int ret = 0;
          int fsizeind = 0;
          struct v4l2_frmsizeenum fsize;

          memset(&fsize, 0, sizeof(fsize));
          fsize.index = 0;
          fsize.pixel_format = pixfmt;

          while(::ioctl(device_fd, VIDIOC_ENUM_FRAMESIZES, &fsize) >= 0) {
               fsize.index++;
               if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                    fsizeind++;
                    if (!EnumFrameIntervals(pixfmt, fsize.discrete.width, fsize.discrete.height)) {
                         ALOGE("Unable to enumerate frame indervals");
                    }
               }else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                    ALOGE("Will not enumerate frame intervals.");
               } else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                    ALOGE("Will not enumerate frame intervals.");
               } else {
                    ALOGE("fsize.type not supported: %d", fsize.type);
               }                
          }
          //176x144,320x240,352x288,640x480,720x480,720x576,800x600,1280x720
          if (fsizeind == 0) {
               static const struct {
                    int w;
                    int h;
               }defMode[] = {
                    {1280, 720},
                    {800, 600},
                    {720, 576},
                    {720, 480},
                    {640, 480},
                    {352, 288},
                    {320, 240},
                    {176, 144}
               };

               unsigned int i;
               for (i=0; i < sizeof(defMode)/sizeof(defMode[0]); ++i) {
                    fsizeind++;
                    struct v4l2_format fmt;
                    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    fmt.fmt.pix.width = defMode[i].w;
                    fmt.fmt.pix.height = defMode[i].h;
                    fmt.fmt.pix.pixelformat = pixfmt;
                    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;//V4L2_FIELD_ANY;

                    if (::ioctl(device_fd, VIDIOC_TRY_FMT, &fmt) >= 0) {
                         m_AllFmts.add(frameInterval(fmt.fmt.pix.width, fmt.fmt.pix.height, 25));
                    }
               }
          }
          return true;
     }

     bool CameraV4L2Device::EnumFrameIntervals(int pixfmt, int width, int height)
     {
          ALOGE_IF(DEBUG_V4L2, "Enter %s",__FUNCTION__);

          struct v4l2_frmivalenum fival;
          int list_fps = 0;
          memset(&fival, 0, sizeof(fival));
          fival.index = 0;
          fival.pixel_format = pixfmt;
          fival.width = width;
          fival.height = height;

          while(::ioctl(device_fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival) >= 0) {
               fival.index++;
               if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                    m_AllFmts.add(frameInterval(width, height, fival.discrete.denominator));
                    list_fps++;
               } else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
                    break;
               } else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
                    break;
               }
          }
          if (list_fps == 0) {
               m_AllFmts.add(frameInterval(width, height, 1));
          }
          return true;
     }

     void CameraV4L2Device::initDefaultControls(void)
     {
          struct VidState* s = this->s;

          if (s->control_list)
          {
               free_control_list(s->control_list);
          }
        
          s->num_controls = 0;
          s->control_list = get_control_list(device_fd, &(s->num_controls));
        
          if (!s->control_list) {
               ALOGE("Error: empty control list");
               return;
          }
          /**
           * will write in future
           */
          return;
     }

     void CameraV4L2Device::free_control_list(Control* control_list)
     {
          Control* first = control_list;
          Control* next = control_list->next;
          while(next != NULL) {
               if (first->str) free(first->str);
               if (first->menu) free(first->menu);
               free(first);
               first = next;
               next = first->next;
          }
          if (first->str) free(first->str);
          if (first) free(first);
          control_list = NULL;
     }

     Control* CameraV4L2Device::get_control_list(int hdevice, int* num_ctrls) {

          int ret = 0;
          Control* first = NULL;
          Control* current = NULL;
          Control* control = NULL;

          int n = 0;
          struct v4l2_queryctrl queryctrl;
          struct v4l2_querymenu querymenu;

          memset(&queryctrl, 0, sizeof(struct v4l2_queryctrl));
          memset(&querymenu, 0, sizeof(struct v4l2_querymenu));

          unsigned int currentctrl = 0;
          queryctrl.id = 0 | V4L2_CTRL_FLAG_NEXT_CTRL;
#ifdef V4L2_CTRL_FLAG_NEXT_CTRL
          if (0 == ::ioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl)) {
               queryctrl.id = 0;
               currentctrl = queryctrl.id;
               queryctrl.id |=V4L2_CTRL_FLAG_NEXT_CTRL;
               while ((ret = ::ioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl)), 
                      ret ? errno != EINVAL : 1) {
                    struct v4l2_querymenu *menu = NULL;

                    if (ret && queryctrl.id <= currentctrl) {
                         currentctrl++;
                         queryctrl.id = currentctrl;
                         goto next_control;
                    } else if ((queryctrl.id == V4L2_CTRL_FLAG_NEXT_CTRL) || 
                               (!ret && queryctrl.id == currentctrl)) {
                         *num_ctrls = n;
                         return first;                        
                    }
                    currentctrl = queryctrl.id;
                    if (ret) goto next_control;
                    if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
                         goto next_control;
                    }

                    if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
                         int i = 0;
                         int ret = 0;

                         for (querymenu.index = queryctrl.minimum;
                              (int)(querymenu.index) <= queryctrl.maximum;
                              querymenu.index++) {
                              querymenu.id = queryctrl.id;
                              ret = ::ioctl(hdevice, VIDIOC_QUERYMENU, &querymenu);
                              if (ret < 0) continue;
                              if (!menu)
                                   menu = (struct v4l2_querymenu *)calloc(i+1, sizeof(struct v4l2_querymenu ));
                              else
                                   menu  = (struct v4l2_querymenu *)realloc(menu, (i+1)*(sizeof(struct v4l2_querymenu )));
                              memcpy(&menu[i], &querymenu, sizeof(struct v4l2_querymenu));
                              i++;
                         }
                         if (!menu)
                              menu = (struct v4l2_querymenu*)calloc(i+1, sizeof(struct v4l2_querymenu));
                         else
                              menu  = (struct v4l2_querymenu *)realloc(menu, (i+1)*(sizeof(struct v4l2_querymenu )));
                         menu[i].id = queryctrl.id;
                         menu[i].index = queryctrl.maximum+1;
                         menu[i].name[0] = 0;
                    }
                    control = (Control*)calloc(1, sizeof(Control));
                    memcpy (&(control->control), &queryctrl, sizeof(struct v4l2_queryctrl));
                    control->ctrl_class = V4L2_CTRL_ID2CLASS(control->control.id);
                    control->menu = menu;
                    control->str = NULL;

                    if (first != NULL) {
                         current->next = control;
                         current = control;
                    } else {
                         first = control;
                         current = first;
                    }
                    n++;
               next_control:
                    queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
               }
          } else
#endif
          {
               /**
                * will write in future
                */
               int currentctrl;
               for (currentctrl = V4L2_CID_BASE; currentctrl < V4L2_CID_LASTP1; currentctrl++) {
                    struct v4l2_querymenu *menu = NULL;
                    queryctrl.id = currentctrl;
                    ret = ::ioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl);
                    if (ret || (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED))
                         continue;

                    if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
                         int i = 0;
                         int ret = 0;
                         for (querymenu.index = queryctrl.minimum;
                              (int)(querymenu.index) <= queryctrl.maximum;
                              querymenu.index++) {
                              querymenu.id = queryctrl.id;
                              ret = ::ioctl(hdevice ,VIDIOC_QUERYMENU , &querymenu);
                              if (ret < 0)
                                   break;

                              if (!menu)
                                   menu = (struct v4l2_querymenu*)calloc((i+1),sizeof(struct v4l2_querymenu));
                              else
                                   menu = (struct v4l2_querymenu*)realloc(menu, (i+1) * sizeof(struct v4l2_querymenu));
                              memcpy(&(menu[i]), &querymenu, sizeof(struct v4l2_querymenu));
                              i++;
                         }

                         if (!menu)
                              menu = (struct v4l2_querymenu*)calloc((i+1),sizeof(struct v4l2_querymenu));
                         else
                              menu = (struct v4l2_querymenu*)realloc(menu, (i+1) * sizeof(struct v4l2_querymenu));

                         menu[i].id = querymenu.id;
                         menu[i].index = queryctrl.maximum+1;
                         menu[i].name[0] = 0;
                    }
                    control = (Control*)calloc(1, sizeof(Control));
                    memcpy(&(control->control), &queryctrl, sizeof(struct v4l2_queryctrl));
                    control->ctrl_class = 0x00980000;
                    control->menu = menu;
                    
                    if (first != NULL) {
                         current->next = control;
                         current = control;
                    } else {
                         first = control;
                         current = control;
                    }
                    n++;
               }

               for (queryctrl.id = V4L2_CID_PRIVATE_BASE; ; queryctrl.id++) {
                    struct v4l2_querymenu *menu = NULL;
                    ret = ::ioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl);
                    if (ret)
                         break;
                    else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                         continue;

                    if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
                         int i = 0;
                         int ret = 0;
                         for (querymenu.index = queryctrl.minimum;
                              (int)(querymenu.index) <= queryctrl.maximum;
                              querymenu.index++) {
                              querymenu.id = queryctrl.id;
                              ret = ::ioctl(hdevice, VIDIOC_QUERYMENU, &querymenu);
                              if (ret < 0)
                                   break;
                              if (!menu)
                                   menu = (struct v4l2_querymenu*)calloc(i+1, sizeof(struct v4l2_querymenu));
                              else
                                   menu = (struct v4l2_querymenu*)realloc(menu, (i+1) * sizeof(struct v4l2_querymenu));
                              memcpy(&(menu[i]), &querymenu, sizeof( struct v4l2_querymenu));
                              i++;                                  
                         }
                         if (!menu)
                              menu = (struct v4l2_querymenu*) calloc(i+1, sizeof(struct v4l2_querymenu));
                         else
                              menu = (struct v4l2_querymenu*)realloc(menu, (i+1)*(sizeof(struct v4l2_querymenu)));
                         menu[i].id = querymenu.id;
                         menu[i].index = queryctrl.maximum+1;
                         menu[i].name[0]=0;
                    }
                    control = (Control*)calloc(1, sizeof(Control));
                    memcpy(&(control->control),&queryctrl, sizeof(struct v4l2_queryctrl));
                    control->menu = menu;

                    if (first != NULL) {
                         current->next = control;
                         current = control;
                    } else {
                         first = control;
                         current = first;
                    }
                    n++;
               }
          }
          *num_ctrls = n;
          return first;
     }
};
