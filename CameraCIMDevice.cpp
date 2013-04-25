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

#define LOG_TAG "CameraCIMDevice"
#define DEBUG_CIM 0

#include "CameraCIMDevice.h"
#include "JZCameraParameters.h"

#define PMEMDEVICE "/dev/pmem_camera"

namespace android {

     const char CameraCIMDevice::path[] = "/dev/cim";
     Mutex CameraCIMDevice::sLock;
     CameraCIMDevice* CameraCIMDevice::sInstance = NULL;

     CameraCIMDevice* CameraCIMDevice::getInstance() {
		  ALOGE_IF(DEBUG_CIM,"%s", __FUNCTION__);
		  Mutex::Autolock _l(sLock);
		  CameraCIMDevice* instance = sInstance;
		  if (instance == NULL) {
			   instance = new CameraCIMDevice();
			   sInstance = instance;
		  }
		  return instance;
     }

     CameraCIMDevice::CameraCIMDevice()
		  :CameraDeviceCommon(),
		   mlock("CameraCIMDevice::lock"),
		   device_fd(-1),
           pmem_device_fd(-1),
           mPmemTotalSize(0),
		   cimDeviceState(DEVICE_UNINIT),
		   currentId(-1),
           mChangedBuffer(false),
		   mPreviewFrameSize(0),
		   mCaptureFrameSize(0),
		   mcapture_frame(0),
		   mpreviewFormat(PIXEL_FORMAT_YUV420P),
		   mcaptureFormat(PIXEL_FORMAT_YUV422I) {

		  memset(&mglobal_info, 0, sizeof(struct global_info));
		  memset(&preview_buffer, 0, sizeof(struct camera_buffer));
		  memset(&capture_buffer, 0, sizeof(struct camera_buffer));
		  initGlobalInfo();
     }

     CameraCIMDevice::~CameraCIMDevice() {
     }


     int CameraCIMDevice::allocateStream(BufferType type,camera_request_memory get_memory,
										 uint32_t width,
										 uint32_t height,
										 int format) {

		  status_t res = NO_ERROR;
		  struct camera_buffer* buf = NULL;
		  int tmp_size = 0;

		  switch(type) {

		  case PREVIEW_BUFFER:
		  {
			   buf = &preview_buffer;
               tmp_size = width * height << 1;

              if ((buf->common != NULL) && (buf->common->data != NULL)) {
                   if (buf->size != tmp_size || buf->yuvMeta[0].format != format) {
                       mChangedBuffer = true;
                       freeStream(PREVIEW_BUFFER);
                       mChangedBuffer = false;
                   } else {
                       mChangedBuffer = false;
                       goto setdriver;
                   }
               }

			   buf->nr = mglobal_info.preview_buf_nr;
			   buf->size = tmp_size;
			   mPreviewFrameSize = (unsigned int)(buf->size);

               tmp_size = mPreviewFrameSize * mglobal_info.preview_buf_nr + mCaptureFrameSize * mglobal_info.capture_buf_nr;
               if ((mPmemTotalSize > 0) && (tmp_size >= mPmemTotalSize)) {
                   mChangedBuffer = true;
                   freeStream(CAPTURE_BUFFER);
                   mCaptureFrameSize = 0;
                   mChangedBuffer = false;
               }

			   break;
		  }

		  case CAPTURE_BUFFER:
		  {
			   buf = &capture_buffer;
               tmp_size = width * height << 1;

               if ((buf->common != NULL) && (buf->common->data != NULL)) {
                   if (buf->size != tmp_size || buf->yuvMeta[0].format != format) {
                       mChangedBuffer = true;
                       freeStream(CAPTURE_BUFFER);
                       mChangedBuffer = false;
                   } else {
                       mChangedBuffer = false;
                       goto setdriver;
                   }
               }

               buf->nr = mglobal_info.capture_buf_nr;
			   buf->size = tmp_size;
			   mCaptureFrameSize = (unsigned int)(buf->size);

               tmp_size = mPreviewFrameSize * mglobal_info.preview_buf_nr + mCaptureFrameSize * mglobal_info.capture_buf_nr;
               if ((mPmemTotalSize > 0) && (tmp_size >= mPmemTotalSize)) {
                   mChangedBuffer = true;
                   freeStream(PREVIEW_BUFFER);
                   mPreviewFrameSize = 0;
                   mChangedBuffer = false;
               }

			   break;
		  }

		  }

		  ALOGE_IF(DEBUG_CIM,"nr = %d size = %d, size=%dx%d",buf->nr ,buf->size,width, height);

		  buf->fd = pmem_device_fd;
		  if (buf->fd < 0) {
			  ALOGE("%s: open %s fail err = %s", 
					__FUNCTION__, PMEMDEVICE, strerror(errno));
		  }

		  buf->common = get_memory(buf->fd,buf->size, buf->nr,NULL);
		  if (buf->common != NULL && buf->common->data != NULL) {

              if (pmem_device_fd > 0) {
                  struct pmem_region region;

                  if (mPmemTotalSize == 0) {
                      ::ioctl(buf->fd, PMEM_GET_TOTAL_SIZE, &region);
                      mPmemTotalSize = region.len;
                      memset(&region, 0, sizeof(struct pmem_region));
                  }

                  ::ioctl(buf->fd, PMEM_GET_PHYS, &region);
                  buf->paddr = region.offset;
                  ALOGE_IF(DEBUG_CIM,"%s: open %s, paddr = %x, fd = %d", 
                           __FUNCTION__, PMEMDEVICE, buf->paddr, buf->fd);
              }

			   buf->dmmu_info.vaddr = buf->common->data;
			   buf->dmmu_info.size = buf->common->size;

			   {
					for (int i = 0; i < (int)(buf->common->size); i += 0x1000) {
						 ((uint8_t*)(buf->common->data))[i] = 0xff;
					}
					((uint8_t*)(buf->common->data))[buf->common->size - 1] = 0xff;
			   }

			   dmmu_map_user_memory(&(buf->dmmu_info));

			   for (int i= 0; i < buf->nr; ++i) {
					buf->yuvMeta[i].index = i;
					buf->yuvMeta[i].width = width;
					buf->yuvMeta[i].height = height;
					buf->yuvMeta[i].format = format;
					buf->yuvMeta[i].count = buf->nr;
					buf->yuvMeta[i].yAddr = (int32_t)buf->common->data + (buf->size) * i;
					buf->yuvMeta[i].yPhy = buf->paddr + i * (buf->size);
					if ((buf->yuvMeta[i].format == HAL_PIXEL_FORMAT_JZ_YUV_420_P)
						|| (buf->yuvMeta[i].format == HAL_PIXEL_FORMAT_YV12)) {
						 tmp_size = width*height;
						 buf->yuvMeta[i].uAddr = (buf->yuvMeta[i].yAddr + tmp_size + 15) & (-16);
						 buf->yuvMeta[i].vAddr = buf->yuvMeta[i].uAddr + (tmp_size>>2);
						 buf->yuvMeta[i].uPhy = (buf->yuvMeta[i].yPhy + tmp_size + 15) & (-16);
						 buf->yuvMeta[i].vPhy = buf->yuvMeta[i].uPhy + (tmp_size>>2);
						 buf->yuvMeta[i].yStride = (buf->yuvMeta[i].width + 15) & (-16);
						 buf->yuvMeta[i].uStride = ((buf->yuvMeta[i].yStride >> 1) + 15) & (-16);
						 buf->yuvMeta[i].vStride = buf->yuvMeta[i].uStride;
						 ALOGE_IF(DEBUG_CIM,"alloc yuv 420p, i = %d, yaddr = 0x%x",
								  i, buf->yuvMeta[i].yAddr);
					} else if (buf->yuvMeta[i].format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
						 tmp_size = width*height*12/8;
						 buf->yuvMeta[i].uAddr = buf->yuvMeta[i].yAddr + tmp_size;
						 // + (width*height) + (0x1000-1)) & (~(0x1000-1));
						 buf->yuvMeta[i].vAddr = buf->yuvMeta[i].uAddr;
						 buf->yuvMeta[i].uPhy = buf->yuvMeta[i].yPhy + tmp_size;
						 //+ (width*height) + (0x1000-1)) & (~(0x1000-1));
						 buf->yuvMeta[i].vPhy = buf->yuvMeta[i].uPhy;
						 buf->yuvMeta[i].yStride = buf->yuvMeta[i].width<<4;
						 buf->yuvMeta[i].uStride = (buf->yuvMeta[i].yStride>>1);
						 buf->yuvMeta[i].vStride = buf->yuvMeta[i].uStride;
						 ALOGE_IF(DEBUG_CIM,"alloc yuv420b i = %d, yaddr = 0x%x", i, buf->yuvMeta[i].yAddr);
					} else if (buf->yuvMeta[i].format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
						 buf->yuvMeta[i].uAddr = buf->yuvMeta[i].yAddr;
						 buf->yuvMeta[i].vAddr = buf->yuvMeta[i].uAddr;
						 buf->yuvMeta[i].uPhy = buf->yuvMeta[i].yPhy;
						 buf->yuvMeta[i].vPhy = buf->yuvMeta[i].uPhy;
						 buf->yuvMeta[i].yStride = buf->yuvMeta[i].width<<1;
						 buf->yuvMeta[i].uStride = buf->yuvMeta[i].yStride;
						 buf->yuvMeta[i].vStride = buf->yuvMeta[i].yStride;
						 ALOGE_IF(DEBUG_CIM,"alloc yuv 422i i = %d, yaddr = 0x%x", i, buf->yuvMeta[i].yAddr);
					}
			   }
		  }

     setdriver:

          if (device_fd > 0) {
              switch(type)
                  {

                  case PREVIEW_BUFFER:
                      {
                          ALOGE_IF(1," preview buffer size = %fM",
                                   buf->size*buf->nr/(1024.0*1024.0));
			   
                          ::ioctl(device_fd, CIMIO_SET_PREVIEW_MEM, (unsigned long)(buf->yuvMeta));
                          if (pmem_device_fd < 0) {
                              ::ioctl(device_fd, CIMIO_SET_TLB_BASE,mtlb_base);
                          }
                          break;
                      }

                  case CAPTURE_BUFFER:
                      {
                          ALOGE_IF(1," capture buffer size = %fM",
                                   buf->size*buf->nr/(1024.0*1024.0));

                          ::ioctl(device_fd, CIMIO_SET_CAPTURE_MEM, (unsigned long)(buf->yuvMeta));
                          break;
                      }

                  }
          }

		  return res;
     }

     void CameraCIMDevice::freeStream(BufferType type) {

		  struct camera_buffer* buf = NULL;

          if (device_fd < 0 || (mChangedBuffer == true)) {

              switch(type) {

              case PREVIEW_BUFFER:
                  ALOGE_IF(1,"free preview buffer");
                  buf = &preview_buffer;
                  break;

              case CAPTURE_BUFFER:
                  ALOGE_IF(1,"free capture buffer");
                  buf = &capture_buffer;
                  break;
              }

              dmmu_unmap_user_memory(&(buf->dmmu_info));
              memset(buf->yuvMeta, 0, buf->nr * sizeof (CameraYUVMeta));
              buf->size = 0;
              buf->nr = 0;
              buf->paddr = 0;

              if((buf->common != NULL) && (buf->common->data != NULL)) {

                  if (buf->fd > 0) {
                      munmap(buf->common->data,buf->common->size);
                  }

                  buf->common->release(buf->common);
                  buf->common->data = NULL;
                  buf->common = NULL;
              }

              if (buf->fd > 0) {
                  buf->fd = -1;
              }
          }

		  return;
     }

     void CameraCIMDevice::flushCache(void* buffer) {

		  uint32_t addr = 0;
		  int size = 0;
		  int flag = 1;

		  if (preview_buffer.yuvMeta[0].yAddr > 0) {
			   addr = preview_buffer.yuvMeta[0].yAddr;
			   size = preview_buffer.size * preview_buffer.nr;
		  } else if (capture_buffer.yuvMeta[0].yAddr > 0) {
			   addr = capture_buffer.yuvMeta[0].yAddr;
			   size = capture_buffer.size * preview_buffer.nr;
		  }

          if (pmem_device_fd < 0) {
              cacheflush((long int)addr, (long int)((unsigned int)addr+size), flag);
          }

     }

     void* CameraCIMDevice::getCurrentFrame(bool taking) {

		  unsigned int addr = 0;
		  struct camera_buffer* buf = NULL;

          if (device_fd < 0) {
              ALOGE("%s: cim device don't open", __FUNCTION__);
              return NULL;
          }

		  if (!taking) {
			   ALOGE_IF(DEBUG_CIM,"%s: get preview frame ",__FUNCTION__);
			   addr = ::ioctl(device_fd, CIMIO_GET_FRAME);
			   buf = &preview_buffer;
		  } else if (taking && (mcapture_frame != 0)) {
			   ALOGE_IF(DEBUG_CIM,"%s: get capture frame ",__FUNCTION__);
			   addr = mcapture_frame;
			   buf = &capture_buffer;
		  }

          if (0 == addr) {
               ALOGE("%s: get frame error, addr: %d", __FUNCTION__, addr);
          }

          if (pmem_device_fd > 0) {
              buf->offset = addr - buf->yuvMeta[0].yPhy;
              for (int i = 0; i < buf->nr; ++i) {
                  if ((int32_t)addr == buf->yuvMeta[i].yPhy) {
                      buf->index = buf->yuvMeta[i].index;
                      return (void*)&(buf->yuvMeta[i]);
                  }
              }
          } else {
 
              if ((buf->common != NULL) && (buf->common->data != NULL)) {
                  buf->offset = addr - (unsigned int)(buf->common->data);
              }

              for (int i = 0; i < buf->nr; ++i) {
                  if ((int32_t)addr == buf->yuvMeta[i].yAddr) {
                      buf->index = buf->yuvMeta[i].index;
                      return (void*)&(buf->yuvMeta[i]);
                  }
              }
          }

		  return NULL;
     }

     int CameraCIMDevice::getPreviewFrameSize(void) {
		  return preview_buffer.size;
     }

     int CameraCIMDevice::getCaptureFrameSize(void) {
		  return capture_buffer.size;
     }

     int CameraCIMDevice::getNextFrame(void) {
		  usleep(21028);
		  return NO_ERROR;
     }

     bool CameraCIMDevice::usePmem(void) {
         return pmem_device_fd != -1;
     }

     camera_memory_t* CameraCIMDevice::getPreviewBufferHandle(void) {
		  return preview_buffer.common;
     }

    camera_memory_t* CameraCIMDevice::getCaptureBufferHandle(void) {
        return capture_buffer.common;
    }

     unsigned int CameraCIMDevice::getPreviewFrameIndex(void) {
		  return preview_buffer.index;
     }

     int CameraCIMDevice::getFrameOffset(void) {
		  return preview_buffer.offset;
     }

     int CameraCIMDevice::setCommonMode(CommonMode mode_type, unsigned short mode_value) {
		  status_t res = NO_ERROR;

		  if (device_fd < 0)
			   return INVALID_OPERATION;

		  switch(mode_type)
		  {

		  case WHITE_BALANCE:
			   ALOGE_IF(DEBUG_CIM,"set white_balance mode");
			   res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_BALANCE);
			   break;

		  case EFFECT_MODE:
			   ALOGE_IF(DEBUG_CIM,"set effect mode");
			   res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_EFFECT);
			   break;

		  case FLASH_MODE:
			   ALOGE_IF(DEBUG_CIM,"set flash mode");
			   res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_FLASH_MODE);
			   break;

		  case FOCUS_MODE:
			   ALOGE_IF(DEBUG_CIM,"set focus mode");
			   res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_FOCUS_MODE);
			   break;

		  case SCENE_MODE:
			   ALOGE_IF(DEBUG_CIM,"set scene mode");
			   res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_SCENE_MODE);
			   break;

		  case ANTIBAND_MODE:
			   ALOGE_IF(DEBUG_CIM,"set antiband mode");
			   res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_ANTIBANDING);
			   break;

		  }
		  return res;
     }

     int CameraCIMDevice::getFormat(int format)
     {
		  int tmp_format;

		  switch(format)
		  {

		  case PIXEL_FORMAT_YUV422SP:
			   tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_SP;
			   break;

		  case PIXEL_FORMAT_YUV420SP:
			   tmp_format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
			   break;

		  case PIXEL_FORMAT_YUV422I:
			   tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
			   break;

		  case PIXEL_FORMAT_YUV420P:
			   tmp_format = HAL_PIXEL_FORMAT_YV12;
			   break;

		  case PIXEL_FORMAT_RGB565:
			   tmp_format = HAL_PIXEL_FORMAT_RGB_565;
			   break;

		  case PIXEL_FORMAT_JZ_YUV420P:
			   tmp_format = HAL_PIXEL_FORMAT_JZ_YUV_420_P;
			   break;

		  case PIXEL_FORMAT_JZ_YUV420T:
			   tmp_format = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
			   break;

		  default:
			   tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
			   break;

		  }
		  return tmp_format;
     }

     int CameraCIMDevice::getCaptureFormat() {
		  return  getFormat(mcaptureFormat);
     }

     int CameraCIMDevice::getPreviewFormat() {
		  return  getFormat(mpreviewFormat);
     }

     int CameraCIMDevice::setCameraParam(struct camera_param& param,int fps)
     {
		  status_t res = NO_ERROR;

		  if (device_fd > 0)
		  {
			   switch(param.cmd)
			   {

			   case CPCMD_SET_PREVIEW_RESOLUTION:
					mpreviewFormat = param.param.format;
					res = ::ioctl(device_fd, CIMIO_SET_PREVIEW_SIZE, &(param.param.ptable[0]));
					::ioctl(device_fd, CIMIO_SET_PREVIEW_FMT, getFormat(mpreviewFormat));
					break;

			   case CPCMD_SET_CAPTURE_RESOLUTION:
					mcaptureFormat = param.param.format;
					res = ::ioctl(device_fd, CIMIO_SET_CAPTURE_SIZE, &(param.param.ctable[0]));
					::ioctl(device_fd, CIMIO_SET_CAPTURE_FMT, getFormat(mcaptureFormat));
                    break;

			   }
		  }

		  return res;
     }

     void CameraCIMDevice::getSensorInfo(struct sensor_info* s_info,struct resolution_info* r_info ) {

         if (device_fd > 0) {

             ::ioctl(device_fd, CIMIO_GET_SENSORINFO, &ms_info);
             memcpy(s_info, &ms_info, sizeof(struct sensor_info));

             ALOGE_IF(DEBUG_CIM,"%s: sensor id = %d, name =%s, facing = %d, \
                           orig = %d, preview table number = %d, capture table nr = %d",
                      __FUNCTION__,s_info->sensor_id, 
                      s_info->name, s_info->facing, 
                      s_info->orientation, s_info->prev_resolution_nr, 
                      s_info->cap_resolution_nr);

             ::ioctl(device_fd, CIMIO_GET_SUPPORT_PSIZE, &(r_info->ptable));
             ::ioctl(device_fd, CIMIO_GET_SUPPORT_CSIZE, &(r_info->ctable));
         }

		  return;
     }

     int CameraCIMDevice::getCurrentCameraId(void) {
		  return currentId;
     }

     int CameraCIMDevice::getResolution(struct resolution_info* r_info) {
		  struct sensor_info s_info;
		  getSensorInfo(&s_info, r_info);
		  return NO_ERROR;
     }

     int CameraCIMDevice::connectDevice(int id) {
		  status_t res = NO_ERROR;

		  ALOGE_IF(DEBUG_CIM,"%s: cim device state = %d, connect camera = %d, currentId = %d",
				   __FUNCTION__,cimDeviceState, id, currentId);

		  if (cimDeviceState != DEVICE_CONNECTED) {

			   if (device_fd < 0) {
					device_fd = open(CameraCIMDevice::path,O_RDWR);
					if (device_fd < 0) {
						 ALOGE("%s: can not connect cim device",__FUNCTION__);
						 return NO_INIT;
					}
			   }

               if (pmem_device_fd < 0) {
                   pmem_device_fd = open(PMEMDEVICE, O_RDWR);
                   if (pmem_device_fd < 0) {
                       ALOGE("%s: open %s error, %s", __FUNCTION__, PMEMDEVICE, strerror(errno));
                       pmem_device_fd = -1;
                   }
               }

			   if (currentId != id) {
					currentId = id;
					::ioctl(device_fd, CIMIO_SELECT_SENSOR, currentId);
					dmmu_init();
					dmmu_get_page_table_base_phys(&mtlb_base);
					cimDeviceState = DEVICE_CONNECTED;
			   }

		  }
		  return res;
     }

     void CameraCIMDevice::disConnectDevice(void) {

		  status_t res = NO_ERROR;

		  ALOGE_IF(DEBUG_CIM,"%s:cim device state = %d",__FUNCTION__,cimDeviceState);
		  if (cimDeviceState == DEVICE_STOPED) {

			   if (device_fd > 0) {
					close(device_fd);
					device_fd = -1;
			   }

			   dmmu_deinit();
			   currentId = -1;
			   cimDeviceState = DEVICE_UNINIT;
			   mpreviewFormat = PIXEL_FORMAT_JZ_YUV420P;
			   mcaptureFormat = PIXEL_FORMAT_YUV422I;

               freeStream(PREVIEW_BUFFER);
               freeStream(CAPTURE_BUFFER);

               if (pmem_device_fd >= 0) {
                   close(pmem_device_fd);
                   pmem_device_fd = -1;
                   mPmemTotalSize = 0;
               }
		  }

		  return;
     }

     int CameraCIMDevice::startDevice(void) {

		  status_t res = UNKNOWN_ERROR;

		  ALOGE_IF(DEBUG_CIM,"%s: cim device state = %d",__FUNCTION__, cimDeviceState);

		  if (cimDeviceState == DEVICE_STARTED)
			   return NO_ERROR;
           
		  if (cimDeviceState == DEVICE_CONNECTED) {
			   cimDeviceState = DEVICE_STOPED;
		  }

		  if(cimDeviceState == DEVICE_STOPED) {
               ALOGE_IF(DEBUG_CIM,"%s: start preview",__FUNCTION__);    
			   res = ::ioctl(device_fd,CIMIO_START_PREVIEW);
			   if (res == 0) {
					cimDeviceState = DEVICE_STARTED;
			   }
		  }
		  ALOGE_IF(DEBUG_CIM,"Exit %s : line=%d",__FUNCTION__,__LINE__);
		  return res;
     }

     int CameraCIMDevice::stopDevice(void) {
		  status_t res = NO_ERROR;

		  if(cimDeviceState == DEVICE_STOPED)
			   return res;

		  ALOGE_IF(DEBUG_CIM,"%s: cim device state = %d",__FUNCTION__, cimDeviceState);
		  if (cimDeviceState == DEVICE_STARTED) {
			   res = ::ioctl(device_fd,CIMIO_SHUTDOWN);
			   if (res == 0)
					cimDeviceState = DEVICE_STOPED;
		  }

		  return res;
     }

     int CameraCIMDevice::getCameraModuleInfo(int camera_id, struct camera_info* info) {

		  status_t res = NO_ERROR;
		  unsigned int i = 0;

		  ALOGE_IF(DEBUG_CIM,"%s: id: %d",__FUNCTION__,camera_id);

          if (device_fd < 0) {
              device_fd = open(CameraCIMDevice::path,O_RDWR);
              if (device_fd < 0) {
                  ALOGE("%s: can not connect cim device",__FUNCTION__);
                  return NO_INIT;
              }
          }

		  if (device_fd > 0) {

			   ms_info.sensor_id = camera_id;
               ::ioctl(device_fd, CIMIO_SELECT_SENSOR, ms_info.sensor_id); 
			   res = ::ioctl(device_fd, CIMIO_GET_SENSORINFO, &ms_info);
			   info->facing = ms_info.facing;
			   info->orientation = ms_info.orientation;

               ALOGE_IF(DEBUG_CIM,"%s: id: %d, name: %s, facing: %d, oriention: %d",
						__FUNCTION__,ms_info.sensor_id, 
                        strlen(ms_info.name) > 0 ? ms_info.name : "NULL" , 
                        info->facing, info->orientation
                        );

			   return NO_ERROR;
		  }

		  ALOGE("%s: get camera info error id = %d", __FUNCTION__, camera_id);
		  info->facing = CAMERA_FACING_BACK;
		  info->orientation = 0;
		  return NO_ERROR;
     }

     void CameraCIMDevice::initGlobalInfo(void) {

		  status_t res = BAD_VALUE;

		  if (device_fd < 0) {

			   device_fd = open(CameraCIMDevice::path,O_RDWR);
			   if (device_fd < 0) {
					ALOGE("%s: can not connect cim device",__FUNCTION__);
					return ;
			   }

			   mglobal_info.sensor_count = ::ioctl(device_fd, CIMIO_GET_SENSOR_COUNT);
			   mglobal_info.preview_buf_nr = PREVIEW_BUFFER_CONUT;
			   mglobal_info.capture_buf_nr = CAPTURE_BUFFER_COUNT;
		  }

		  ALOGE_IF(DEBUG_CIM,"%s: device_fd: %d, num: %d", __FUNCTION__,device_fd,mglobal_info.sensor_count);
     }

     int CameraCIMDevice::getCameraNum(void) {

		  status_t res = NO_ERROR;
          
		  if (mglobal_info.sensor_count > 0)
			   return mglobal_info.sensor_count;

		  return 0; 
     }

     int CameraCIMDevice::sendCommand(uint32_t cmd_type, uint32_t arg1, uint32_t arg2, uint32_t result) {

		  status_t res = UNKNOWN_ERROR;

		  ALOGE_IF(DEBUG_CIM,"%s: cim device state = %d, cmd = %d",
				   __FUNCTION__,cimDeviceState, cmd_type);

		  if (device_fd > 0)
		  {
			   switch(cmd_type)
			   {
			   case PAUSE_FACE_DETECT:
					break;
			   case FOCUS_INIT:
					res = ::ioctl(device_fd, CIMIO_AF_INIT);
                    break;
			   case START_FOCUS:
					res = ::ioctl(device_fd, CIMIO_DO_FOCUS);
					break;
			   case GET_FOCUS_STATUS:
					//res = ::ioctl(device_fd, CIMIO_GET_FOCUS_STATE);
					break;
			   case START_PREVIEW:
					res = ::ioctl(device_fd,CIMIO_START_PREVIEW);
					break;
			   case STOP_PREVIEW:
					res = ::ioctl(device_fd,CIMIO_SHUTDOWN);
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
					mcapture_frame = ::ioctl(device_fd,CIMIO_START_CAPTURE);
					if (mcapture_frame == 0) {
						 ALOGE("%s: line = %d , take picture fail",__FUNCTION__, __LINE__);
						 res = BAD_VALUE;
					} else {
						 res = NO_ERROR;
						 ALOGE_IF(DEBUG_CIM,"%s: line = %d , take picture capture frame = 0x%x",
								  __FUNCTION__, __LINE__, mcapture_frame);
					};
					break;
			   case STOP_PICTURE:
                    ALOGE_IF(DEBUG_CIM,"%s: stop picture",__FUNCTION__);
					break;
			   }
		  }
		  return res;
     }
};
