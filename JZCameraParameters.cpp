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


#define LOG_TAG "JZCameraParams"
#define DEBUG_JZPARAM 0
#define DEBUG_JZPARAM_ALL 0

#include "JZCameraParameters.h"
#include <media/MediaProfiles.h>

#define MIN_WIDTH 640
#define MIN_HEIGHT 480
#define MAX_FACE 3

namespace android {

     const char JZCameraParameters::KEY_LUMA_ADAPTATION[] = "luma-adaptation"; 
     const char JZCameraParameters::KEY_NIGHTSHOT_MODE[]  = "nightshot-mode";
     const char JZCameraParameters::KEY_ORIENTATION[]     = "orientation";
     const char JZCameraParameters::PIXEL_FORMAT_JZ__YUV420T[] = "jzyuv420t"; // ingenic yuv420tile
     const char JZCameraParameters::PIXEL_FORMAT_JZ__YUV420P[] = "jzyuv420p"; // ingenic yuv420p

     const mode_map_t JZCameraParameters::wb_map[] = {
          { CameraParameters::WHITE_BALANCE_AUTO,             WHITE_BALANCE_AUTO },
          { CameraParameters::WHITE_BALANCE_INCANDESCENT,     WHITE_BALANCE_INCANDESCENT },
          { CameraParameters::WHITE_BALANCE_FLUORESCENT,      WHITE_BALANCE_FLUORESCENT },
          { CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT, WHITE_BALANCE_WARM_FLUORESCENT },
          { CameraParameters::WHITE_BALANCE_DAYLIGHT,         WHITE_BALANCE_DAYLIGHT },
          { CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT,  WHITE_BALANCE_CLOUDY_DAYLIGHT },
          { CameraParameters::WHITE_BALANCE_TWILIGHT,         WHITE_BALANCE_TWILIGHT },
          { CameraParameters::WHITE_BALANCE_SHADE,            WHITE_BALANCE_SHADE }
     };

     const  mode_map_t JZCameraParameters::effect_map[] = {
          { CameraParameters::EFFECT_NONE,       EFFECT_NONE },
          { CameraParameters::EFFECT_MONO,       EFFECT_MONO },
          { CameraParameters::EFFECT_NEGATIVE,   EFFECT_NEGATIVE },
          { CameraParameters::EFFECT_SOLARIZE,   EFFECT_SOLARIZE },
          { CameraParameters::EFFECT_SEPIA,      EFFECT_SEPIA },
          { CameraParameters::EFFECT_POSTERIZE,  EFFECT_POSTERIZE },
          { CameraParameters::EFFECT_WHITEBOARD, EFFECT_WHITEBOARD },
          { CameraParameters::EFFECT_BLACKBOARD, EFFECT_BLACKBOARD },
          { CameraParameters::EFFECT_AQUA,       EFFECT_AQUA }
          //{ "pastel", EFFECT_PASTEL },
          //{ "mosaic", EFFECT_MOSAIC },
          //{ "resize", EFFECT_RESIZE }
     };

     const mode_map_t JZCameraParameters::antibanding_map[] = {
          { CameraParameters::ANTIBANDING_AUTO, ANTIBANDING_AUTO },
          { CameraParameters::ANTIBANDING_50HZ, ANTIBANDING_50HZ },
          { CameraParameters::ANTIBANDING_60HZ, ANTIBANDING_60HZ },
          { CameraParameters::ANTIBANDING_OFF,  ANTIBANDING_OFF }
     };

     const mode_map_t JZCameraParameters::flash_map[] = {
          { CameraParameters::FLASH_MODE_OFF,     FLASH_MODE_OFF },
          { CameraParameters::FLASH_MODE_AUTO,    FLASH_MODE_AUTO },
          { CameraParameters::FLASH_MODE_ON,      FLASH_MODE_ON },
          { CameraParameters::FLASH_MODE_RED_EYE, FLASH_MODE_RED_EYE },
          { CameraParameters::FLASH_MODE_TORCH,   FLASH_MODE_TORCH }
     };

     const mode_map_t JZCameraParameters::scene_map[] = {
          { CameraParameters::SCENE_MODE_AUTO,          SCENE_MODE_AUTO },
          { CameraParameters::SCENE_MODE_ACTION,        SCENE_MODE_ACTION },
          { CameraParameters::SCENE_MODE_PORTRAIT,      SCENE_MODE_PORTRAIT },
          { CameraParameters::SCENE_MODE_LANDSCAPE,     SCENE_MODE_LANDSCAPE },
          { CameraParameters::SCENE_MODE_NIGHT,         SCENE_MODE_NIGHT},
          { CameraParameters::SCENE_MODE_NIGHT_PORTRAIT,SCENE_MODE_NIGHT_PORTRAIT},
          { CameraParameters::SCENE_MODE_THEATRE,       SCENE_MODE_THEATRE },
          { CameraParameters::SCENE_MODE_BEACH,         SCENE_MODE_BEACH},
          { CameraParameters::SCENE_MODE_SNOW,          SCENE_MODE_SNOW },
          { CameraParameters::SCENE_MODE_SUNSET,        SCENE_MODE_SUNSET},
          { CameraParameters::SCENE_MODE_STEADYPHOTO,   SCENE_MODE_STEADYPHOTO },
          { CameraParameters::SCENE_MODE_FIREWORKS,     SCENE_MODE_FIREWORKS},
          { CameraParameters::SCENE_MODE_SPORTS,        SCENE_MODE_SPORTS},
          { CameraParameters::SCENE_MODE_PARTY,         SCENE_MODE_PARTY},
          { CameraParameters::SCENE_MODE_CANDLELIGHT,   SCENE_MODE_CANDLELIGHT}
     };

     const mode_map_t JZCameraParameters::focus_map[] = {
          { CameraParameters::FOCUS_MODE_FIXED,    FOCUS_MODE_FIXED },
          { CameraParameters::FOCUS_MODE_AUTO,     FOCUS_MODE_AUTO },
          { CameraParameters::FOCUS_MODE_INFINITY, FOCUS_MODE_INFINITY },
          { CameraParameters::FOCUS_MODE_MACRO,    FOCUS_MODE_MACRO }
     };

     const mode_map_t JZCameraParameters::pix_format_map[] = {
          { CameraParameters::PIXEL_FORMAT_YUV422I,  PIXEL_FORMAT_YUV422I},
          { CameraParameters::PIXEL_FORMAT_YUV420SP, PIXEL_FORMAT_YUV420SP},
          { CameraParameters::PIXEL_FORMAT_YUV422SP, PIXEL_FORMAT_YUV422SP},
          { CameraParameters::PIXEL_FORMAT_YUV420P,  PIXEL_FORMAT_YUV420P},
          { JZCameraParameters::PIXEL_FORMAT_JZ__YUV420T, PIXEL_FORMAT_JZ_YUV420T},
          { JZCameraParameters::PIXEL_FORMAT_JZ__YUV420P, PIXEL_FORMAT_JZ_YUV420P}
     };

     JZCameraParameters::JZCameraParameters(CameraDeviceCommon* cdc, int id)
          :mParameters(),
           mCameraDevice(cdc),
           mMutex("JZCameraParameters::Mutex"),
           mCameraId(id),
           maxpreview_width(0),
           maxpreview_height(0),
           maxcapture_width(0),
           maxcapture_height(0),
           isPreviewSizeChange(false),
           isPictureSizeChange(false),
           isVideoSizeChange(false) {
     }

     JZCameraParameters::~JZCameraParameters() {
          ALOGE_IF(DEBUG_JZPARAM,"Enter %s : line=%d",__FUNCTION__, __LINE__);
     }

     int JZCameraParameters::getPropertyValue(const char* property) {

          char prop[16];

          memset(prop, 0, 16);
          if (property_get(property, prop, NULL) > 0) {
               char *prop_end = prop;
               int val = strtol(prop, &prop_end, 10);
               if (*prop_end == '\0') {
                    return val;
               }
          }
          ALOGE_IF(DEBUG_JZPARAM,"%s is not a number : %s .",property, (strlen(prop) == 0) ? "NULL" : prop);
          return BAD_VALUE;
     }

     int JZCameraParameters::getPropertyPictureSize(int* width, int* height) {

          int ret = BAD_VALUE;
          int tmpWidth = 0, tmpHeight = 0;

          if (if_need_picture_upscale()) {

               tmpWidth = getPropertyValue("ro.board.camera.sensor_w");
               tmpHeight = getPropertyValue("ro.board.camera.sensor_h");
               if ((tmpWidth > 0) && (tmpHeight > 0))
               {
                    *width = tmpWidth;
                    *height = tmpHeight;
                    ret = NO_ERROR;
               }
          }

          return ret;
     }

     PixelFormat JZCameraParameters::getFormat(int32_t format) {

          PixelFormat tmp = HAL_PIXEL_FORMAT_YCbCr_422_I;
          switch(format)
          {
          case PIXEL_FORMAT_YUV422I:
               break;
          case PIXEL_FORMAT_YUV420SP:
          case HAL_PIXEL_FORMAT_YCrCb_420_SP:
               tmp =  HAL_PIXEL_FORMAT_YCrCb_420_SP;
               break;
          case PIXEL_FORMAT_YUV422SP:
          case HAL_PIXEL_FORMAT_YCbCr_422_SP:
               tmp = HAL_PIXEL_FORMAT_YCbCr_422_SP;
               break;
          case PIXEL_FORMAT_RGB565:
               tmp = HAL_PIXEL_FORMAT_RGB_565;
               break;
          case PIXEL_FORMAT_JZ_YUV420T:
               tmp = HAL_PIXEL_FORMAT_YV12;
               break;
          case PIXEL_FORMAT_JZ_YUV420P:
               tmp = HAL_PIXEL_FORMAT_YV12;
               break;
          }
          return tmp;
     }
      
     void JZCameraParameters::setPreviewFormat(int* previewformat, const char* valstr,bool isVideo) {

          if (isVideo) {
               *previewformat = PIXEL_FORMAT_JZ_YUV420T;
               return;
          }

          if (strcmp(valstr, JZCameraParameters::PIXEL_FORMAT_JZ__YUV420T) == 0) {
               *previewformat = PIXEL_FORMAT_JZ_YUV420T;
          } else if (strcmp(valstr, JZCameraParameters::PIXEL_FORMAT_JZ__YUV420P) == 0) {
               *previewformat = PIXEL_FORMAT_JZ_YUV420P;
          } else if (strcmp(valstr, CameraParameters::PIXEL_FORMAT_YUV420P) == 0) {
               *previewformat = PIXEL_FORMAT_JZ_YUV420P;
          } else if (strcmp(valstr, CameraParameters::PIXEL_FORMAT_YUV422I) == 0) {
               *previewformat = PIXEL_FORMAT_YUV422I;
          } else {
               *previewformat = PIXEL_FORMAT_JZ_YUV420P;
          }

          // if (strcmp(valstr, CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
          //      *previewformat = PIXEL_FORMAT_YUV420SP;
          // } else if (strcmp(valstr, CameraParameters::PIXEL_FORMAT_YUV422SP) == 0) {
          //      *previewformat = PIXEL_FORMAT_YUV422SP;
          // } else if (strcmp(valstr, CameraParameters::PIXEL_FORMAT_YUV422I) == 0) {
          //      *previewformat = PIXEL_FORMAT_YUV422I;
          // }
     }

     int JZCameraParameters::setParameters(String8 params) {

          status_t ret = NO_ERROR;

          const char* valstr = NULL;
          const char* valstr2 = NULL; 
          int valint = 0;
          float valfloat = 0.0;
          PixelFormat previewformat = PIXEL_FORMAT_JZ_YUV420P;
          PixelFormat captureformat = PIXEL_FORMAT_YUV422I;

          if(mCameraDevice == NULL) {

               ALOGE("%s: camera device not open", __FUNCTION__);
               return NO_MEMORY;
          }

          CameraParameters tempParam(params);   

          valstr = tempParam.getPreviewFormat();
          if((valstr != NULL)
             && isParameterValid(valstr,mParameters.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS))) {

               const char* s1 = mParameters.getPreviewFormat();
               if (strcmp(valstr,s1) != 0) {
                    mParameters.setPreviewFormat(valstr);
               }
               setPreviewFormat(&previewformat, valstr);
          } else {
               ALOGE("setParameters: supported preview format = %s, error format = %s",
                     mParameters.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS), 
                     tempParam.getPreviewFormat());
               return BAD_VALUE;
          }

          ALOGE_IF(DEBUG_JZPARAM,"set parameters: set preview format = %s", mParameters.getPreviewFormat());

          const int preview_frame_rate = tempParam.getPreviewFrameRate();
          valstr = tempParam.get(CameraParameters::KEY_PREVIEW_FRAME_RATE);
          if(isParameterValid(valstr,mParameters
                      .get(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES))) {
               if(mParameters.getPreviewFrameRate() != preview_frame_rate)
                    mParameters.setPreviewFrameRate(preview_frame_rate);
          } else {
               ALOGE("%s: support preview frame rates = %s, error frame rate = %s",__FUNCTION__,
                     mParameters.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES), valstr);
               return BAD_VALUE;
          }

          ALOGE_IF(DEBUG_JZPARAM,"%s: preivew frame rate = %d", 
                  __FUNCTION__,mParameters.getPreviewFrameRate());

          int min_fps, max_fps;
          int old_min_fps, old_max_fps;
          char tmp_fps_range[16];
          int offset_range = 0;
          memset(tmp_fps_range, 0, 16);
          tempParam.getPreviewFpsRange(&min_fps, &max_fps);
          mParameters.getPreviewFpsRange(&old_min_fps, &old_max_fps);
          offset_range = snprintf(tmp_fps_range, 16, "(%d,%d)", min_fps, max_fps);
          tmp_fps_range[offset_range+1] = '\0';
          if (!isParameterValid(tmp_fps_range, 
                    mParameters.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE))) {
               ALOGE("%s: support fps range = %s, error fps_range = %s",__FUNCTION__,
                     mParameters.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE), tmp_fps_range);
               return BAD_VALUE;
          } else if (old_min_fps != min_fps || old_max_fps != max_fps) {
               char tt_fps_range[16];
               int offset = 0;
               memset(tt_fps_range, 0, 16);
               offset = snprintf(tt_fps_range, 16, "%d,%d",min_fps, max_fps);
               tt_fps_range[offset+1] = '\0';
               mParameters.set(CameraParameters::KEY_PREVIEW_FPS_RANGE,tt_fps_range);
          }

          ALOGE_IF(DEBUG_JZPARAM,"%s: set preview fps range = %s",
                  __FUNCTION__,mParameters.get(CameraParameters::KEY_PREVIEW_FPS_RANGE));

          valstr = tempParam.get(CameraParameters::KEY_RECORDING_HINT);
          if (valstr != NULL &&
              strcmp(mParameters.get(CameraParameters::KEY_RECORDING_HINT), valstr)) {
               mParameters.set(CameraParameters::KEY_RECORDING_HINT,valstr);
          }
          
          valstr = mParameters.get(CameraParameters::KEY_RECORDING_HINT);
          ALOGE_IF(DEBUG_JZPARAM,"setParameters: set video hint = %s", valstr);

          if (strcmp(valstr, "true") == 0) {
               valstr = tempParam.get(CameraParameters::KEY_VIDEO_FRAME_FORMAT);
               if (valstr != NULL &&
                    isParameterValid(valstr, 
                         mParameters.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS))) {
                    const char* s1 = mParameters.get(CameraParameters::KEY_VIDEO_FRAME_FORMAT);
                    if (strcmp(valstr,s1) != 0) {
                         mParameters.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT,valstr);
                    }
                    setPreviewFormat(&previewformat, valstr, true);
               } else {
                    ALOGE("setParameters: Unsupported format '%s' for recording",
                           tempParam.get(CameraParameters::KEY_VIDEO_FRAME_FORMAT));
                    return BAD_VALUE;
               }
               ALOGE_IF(DEBUG_JZPARAM,"setParameters: set video frame format = %s", 
                         mParameters.get(CameraParameters::KEY_VIDEO_FRAME_FORMAT));
          }

          valstr = tempParam.get(CameraParameters::KEY_WHITE_BALANCE);
          if ((valstr != NULL) 
                    && strcmp(valstr, mParameters.get(CameraParameters::KEY_WHITE_BALANCE)) != 0) {
               mParameters.set(CameraParameters::KEY_WHITE_BALANCE,valstr);
               int num_wb = sizeof(JZCameraParameters::wb_map)/ sizeof(mode_map_t);
               ret = mCameraDevice->setCommonMode(WHITE_BALANCE,string_to_mode(valstr,JZCameraParameters::wb_map, num_wb));
          }

          valstr = tempParam.get(CameraParameters::KEY_EFFECT);
          if ((valstr != NULL) && strcmp(valstr, mParameters.get(CameraParameters::KEY_EFFECT)) != 0) {
               mParameters.set(CameraParameters::KEY_EFFECT,valstr);
               int num_ef = sizeof(JZCameraParameters::effect_map) / sizeof(mode_map_t);
               ret = mCameraDevice->setCommonMode(EFFECT_MODE,string_to_mode(valstr,JZCameraParameters::effect_map,num_ef));
          }

          valstr = tempParam.get(CameraParameters::KEY_FOCUS_MODE);
          if ((valstr != NULL) && strcmp(valstr, mParameters.get(CameraParameters::KEY_FOCUS_MODE)) != 0) {
               mParameters.set(CameraParameters::KEY_FOCUS_MODE,valstr);
               int num_fo = sizeof(JZCameraParameters::focus_map) / sizeof(mode_map_t);
               ret = mCameraDevice->setCommonMode(FOCUS_MODE,string_to_mode(valstr,JZCameraParameters::focus_map,num_fo));
          }

          valstr = tempParam.get(CameraParameters::KEY_FLASH_MODE);
          if ((valstr != NULL) && strcmp(valstr, mParameters.get(CameraParameters::KEY_FLASH_MODE)) != 0) {
               mParameters.set(CameraParameters::KEY_FLASH_MODE,valstr);
               int num_fl = sizeof(JZCameraParameters::flash_map) / sizeof(mode_map_t);
               ret = mCameraDevice->setCommonMode(FLASH_MODE,string_to_mode(valstr, JZCameraParameters::flash_map,num_fl));
          }

          valstr = tempParam.get(CameraParameters::KEY_SCENE_MODE);
          if ((valstr != NULL) && strcmp(valstr, mParameters.get(CameraParameters::KEY_SCENE_MODE)) != 0) {
               mParameters.set(CameraParameters::KEY_SCENE_MODE,valstr);
               int num_sc = sizeof(JZCameraParameters::scene_map) / sizeof(mode_map_t);
               ret = mCameraDevice->setCommonMode(SCENE_MODE,string_to_mode(valstr,JZCameraParameters::scene_map,num_sc));
          }

          valstr = tempParam.get(CameraParameters::KEY_ANTIBANDING);
          if ((valstr != NULL) && strcmp(valstr,mParameters.get(CameraParameters::KEY_ANTIBANDING)) != 0) {
               mParameters.set(CameraParameters::KEY_ANTIBANDING,valstr);
               int num_an = sizeof(JZCameraParameters::antibanding_map) / sizeof(mode_map_t);
               ret = mCameraDevice->setCommonMode(ANTIBAND_MODE,
                                                  string_to_mode(valstr,JZCameraParameters::antibanding_map,num_an));
          }

          int temp_old_W = 0,temp_old_H = 0;
          int temp_preview_W = 0,temp_preview_H = 0;
          int temp_picture_W = 0,temp_picture_H = 0;
          int temp_video_W = 0,temp_video_H = 0;
          struct camera_param param;
          memset(&param, 0,sizeof(struct camera_param));
          tempParam.getPreviewSize(&temp_preview_W,&temp_preview_H);
          mParameters.getPreviewSize(&temp_old_W,&temp_old_H);

          if(temp_preview_W != temp_old_W || temp_preview_H != temp_old_H) {
               char tmp_preview_size[16];
               int offset = 0;
               memset(tmp_preview_size, 0, 16);
               offset = snprintf(tmp_preview_size, 16, "%dx%d", temp_preview_W, temp_preview_H);
               tmp_preview_size[offset+1] = '\0';
               if (!isParameterValid(tmp_preview_size, mParameters.
                         get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES))) {
                    ALOGE("%s: support preview sizes = %s, error size = %s",__FUNCTION__,
                          mParameters.get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES), tmp_preview_size);
                    return BAD_VALUE;
               }

               mParameters.setPreviewSize(temp_preview_W,temp_preview_H);
               isPreviewSizeChange = true;
               
               param.param.ptable[0].w = temp_preview_W;
               param.param.ptable[0].h = temp_preview_H;
          } else {
               mParameters.getPreviewSize((int*)&(param.param.ptable[0].w),
                    (int*)&(param.param.ptable[0].h));
               isPreviewSizeChange = false;
          }

          ALOGE_IF(DEBUG_JZPARAM,"setParameters: set preview size = %dx%d", 
               param.param.ptable[0].w, param.param.ptable[0].h);

          param.param.bpp = bitsPerPixel(getFormat(previewformat));
          param.param.format = previewformat;
          param.cmd = CPCMD_SET_PREVIEW_RESOLUTION;
          mCameraDevice->setCameraParam(param, mParameters.getPreviewFrameRate());

          if (strcmp(tempParam.getPictureFormat(), CameraParameters::PIXEL_FORMAT_JPEG)) {
               ALOGE("setParameters: Only jpeg still pictures are supported, error format = %s",
                     tempParam.getPictureFormat());
               return BAD_VALUE;
          }

          memset(&param, 0, sizeof(struct camera_param));
          tempParam.getPictureSize(&temp_picture_W,&temp_picture_H);
          mParameters.getPictureSize(&temp_old_W,&temp_old_H);
          if(temp_picture_W != temp_old_W || temp_picture_H != temp_old_H) {
               char tmp_picture_size[16];
               int offset = 0;
               memset(tmp_picture_size, 0, 16);
               offset = snprintf(tmp_picture_size, 16, "%dx%d", temp_picture_W, temp_picture_H);
               tmp_picture_size[offset+1] = '\0';
               if (!isParameterValid(tmp_picture_size, mParameters.
                         get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES))) {
                    ALOGE("%s: supported picture sizes = %s, error size = %s",__FUNCTION__,
                          mParameters.get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES), tmp_picture_size);
                    return BAD_VALUE;
               }

               mParameters.setPictureSize(temp_picture_W,temp_picture_H);
               isPictureSizeChange = true;
               param.param.ctable[0].w = temp_picture_W;
               param.param.ctable[0].h = temp_picture_H;
          } else {
               mParameters.getPictureSize((int*)&(param.param.ctable[0].w),
                         (int*)&(param.param.ctable[0].h));
               isPictureSizeChange = false;
          }

          tempParam.getVideoSize(&temp_video_W,&temp_video_H);
          mParameters.getVideoSize(&temp_old_W,&temp_old_H);
          if((temp_video_W != temp_old_W) || (temp_video_H != temp_old_H)) {
               char tmp_video_size[16];
               int offset = 0;
               memset (tmp_video_size, 0, 16);
               offset = snprintf(tmp_video_size, 16, "%dx%d", temp_video_W, temp_video_H);
               tmp_video_size[offset+1] = '\0';
               if (!isParameterValid(tmp_video_size, mParameters
                         .get(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES))) {
                    ALOGE("%s: supported video sizes  = %s, error video size = %s",__FUNCTION__,
                          mParameters.get(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES), tmp_video_size);
                    return BAD_VALUE;
               }

               mParameters.setVideoSize(temp_video_W,temp_video_H);
               isVideoSizeChange = true;
          } else {
               isVideoSizeChange = false;
          }

          mParameters.getVideoSize(&temp_old_W,&temp_old_H);
          ALOGE_IF(DEBUG_JZPARAM,"setParameters: set video size = %dx%d", temp_old_W, temp_old_H);

          getPropertyPictureSize((int*)&(param.param.ctable[0].w),(int*)&(param.param.ctable[0].h));

          ALOGE_IF(DEBUG_JZPARAM,"setParameters : set picture size = %dx%d", 
               param.param.ctable[0].w, param.param.ctable[0].h);

#ifdef COMPRESS_JPEG_USE_HW
          captureformat = PIXEL_FORMAT_JZ_YUV420T;
#else
          captureformat = PIXEL_FORMAT_YUV422I;
#endif
          param.cmd = CPCMD_SET_CAPTURE_RESOLUTION;
          param.param.bpp= bitsPerPixel(getFormat(captureformat));
          param.param.format = captureformat;
          ret = mCameraDevice->setCameraParam(param,mParameters.getPreviewFrameRate());

          valint = tempParam.getInt(CameraParameters::KEY_JPEG_QUALITY);
          if(mParameters.getInt(CameraParameters::KEY_JPEG_QUALITY) != valint)
               mParameters.set(CameraParameters::KEY_JPEG_QUALITY, valint);

          ALOGE_IF(DEBUG_JZPARAM,"setParametes : set jpeg quality = %d" , 
               mParameters.getInt(CameraParameters::KEY_JPEG_QUALITY));

          valint = tempParam.getInt(CameraParameters::KEY_ROTATION);
          if(mParameters.getInt(CameraParameters::KEY_ROTATION) != valint)
               mParameters.set(CameraParameters::KEY_ROTATION,valint);

          ALOGE_IF(DEBUG_JZPARAM,"%s: set picture rotation = %d",__FUNCTION__, 
               mParameters.getInt(CameraParameters::KEY_ROTATION));

          valstr = tempParam.get(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
          valstr2 = tempParam.get(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
          if (valstr != NULL && valstr2 != NULL) {

               char thumb_sizes[16];
               memset(thumb_sizes, 0, 16);
               snprintf(thumb_sizes, 16, "%sx%s",valstr,valstr2);
               if (isParameterValid(thumb_sizes,mParameters.
                         get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES))) {

                    valint = tempParam.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
                    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, valint);
                    valint = tempParam.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
                    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT,valint);
               } else {
                    ALOGE("%s: support thumbnail sizes = %s, error size = %s", __FUNCTION__, thumb_sizes,
                          mParameters.get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES));
                    return BAD_VALUE;
               }
          }

          valint = tempParam.getInt(CameraParameters::KEY_GPS_ALTITUDE);
          if(valint != 0) {
               mParameters.set(CameraParameters::KEY_GPS_ALTITUDE,valint);
          }

          valfloat = tempParam.getFloat(CameraParameters::KEY_GPS_LATITUDE);
          if(valfloat < 0.0001 && valfloat > -0.0001) {
               mParameters.setFloat(CameraParameters::KEY_GPS_LATITUDE,valfloat);
          }

          valstr = tempParam.get(CameraParameters::KEY_GPS_TIMESTAMP);
          if(valstr != NULL) {
               mParameters.set(CameraParameters::KEY_GPS_TIMESTAMP,valstr);
          }

          valfloat = tempParam.getFloat(CameraParameters::KEY_GPS_LONGITUDE);
          if((valfloat < 0.0001) && (valfloat > -0.0001)) {
               mParameters.setFloat(CameraParameters::KEY_GPS_LONGITUDE,valfloat);
          }

          valstr = tempParam.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);
          if(valstr != NULL) {
               mParameters.set(CameraParameters::KEY_GPS_PROCESSING_METHOD,valstr);
          }

          return ret;
     }
        
     status_t  JZCameraParameters::setUpEXIF(ExifElementsTable* exifTable) {

          status_t ret = NO_ERROR;
          struct timeval sTv;
          struct tm* pTime;
          const char* valStr = NULL;
          float valFloat = 0.0;
          int valInt = 0;

          ret = exifTable->insertElement(TAG_MODEL, CAMERA_INFO_MODULE);
          if (NO_ERROR == ret) {
               ret = exifTable->insertElement(TAG_MAKE,CAMERA_INFO_MANUFACTURER);
          }

          if (NO_ERROR == ret) {
               ret = exifTable->insertElement(TAG_SOFTWARE,SOFTWARE_VALUE);
          }

          valStr = mParameters.get(CameraParameters::KEY_FOCAL_LENGTH);        
          if (NULL != valStr && NO_ERROR == ret) {

               // unsigned int focalNum = 0;
               // unsigned int focalDen = 0;
               // char temp_value[256];
               // ExifElementsTable::stringToRational(valstr,
               //                                     &focalNum,
               //                                     &focalDen);
               // snprintf(temp_value,
               //          sizeof(temp_value)/sizeof(char),
               //          "%u/%u",
               //          focalNum,
               //          focalDen);
               //ret = exifTable->insertElement(TAG_FOCALLENGTH,temp_value);
               ret = exifTable->insertElement(TAG_FOCALLENGTH,"55/100");
          }

          if (NO_ERROR == ret) {

               int status = gettimeofday(&sTv, NULL);
               pTime = localtime(&sTv.tv_sec);
               char temp_value[50];
               if ((0 == status) && (NULL != pTime)) {

                    snprintf(temp_value, 50,
                             "%04d:%02d:%02d %02d:%02d:%02d",
                             pTime->tm_year + 1900,
                             pTime->tm_mon + 1,
                             pTime->tm_mday,
                             pTime->tm_hour,
                             pTime->tm_min,
                             pTime->tm_sec);
                    ret = exifTable->insertElement(TAG_DATETIME, temp_value);
               }
          }
        
          if (NO_ERROR == ret) {

               int width, height;
               mParameters.getPictureSize(&width, &height);
               char temp_value[5];
               snprintf(temp_value, sizeof(temp_value)/sizeof(char),"%d",width);
               ret = exifTable->insertElement(TAG_IMAGE_WIDTH, temp_value);
               memset(temp_value, '\0',5);
               snprintf(temp_value, 5, "%d",height);
               ret = exifTable->insertElement(TAG_IMAGE_LENGTH, temp_value);
          }
        
          valFloat = mParameters.getFloat(CameraParameters::KEY_GPS_LATITUDE);
          if (NO_ERROR == ret) {

               char temp_value[256];                
               if (valFloat < 0) valFloat = -valFloat;
               double b = (valFloat - (int)valFloat)*60;
               double c = (b - (int)b) * 60;
               snprintf(temp_value,256 -1,
                        "%d/1, %d/1, %d/1",
                        (int)valFloat, (int)b, (int)(c+0.5));
               ret = exifTable->insertElement(TAG_GPS_LAT, temp_value);
          }

          valFloat = mParameters.getFloat(CameraParameters::KEY_GPS_LATITUDE);
          if (NO_ERROR == ret) {

               if (valFloat < 0)
                    ret = exifTable->insertElement(TAG_GPS_LAT_REF,"S");
               else
                    ret = exifTable->insertElement(TAG_GPS_LAT_REF, "N");
          }

          valFloat = mParameters.getFloat(CameraParameters::KEY_GPS_LONGITUDE);
          if (NO_ERROR == ret) {

               char temp_value[256];                
               if (valFloat < 0) valFloat = -valFloat;
               double b = (valFloat - (int)valFloat)*60;
               double c = (b - (int)b) * 60;
               snprintf(temp_value,256 -1,
                        "%d/1, %d/1, %d/1",
                        (int)valFloat, (int)b, (int)(c+0.5));
               ret = exifTable->insertElement(TAG_GPS_LONG, temp_value);
          }

          valFloat = mParameters.getFloat(CameraParameters::KEY_GPS_LONGITUDE);
          if (NO_ERROR == ret) {

               if (valFloat < 0)
                    ret = exifTable->insertElement(TAG_GPS_LONG_REF,"W");
               else
                    ret = exifTable->insertElement(TAG_GPS_LONG_REF,"E");
          }

          valInt = mParameters.getInt(CameraParameters::KEY_GPS_ALTITUDE);
          if (NO_ERROR == ret) {

               char temp_value[256];
               snprintf(temp_value,
                        256 - 1,
                        "%d/%d",
                        valInt, 1);
               ret = exifTable->insertElement(TAG_GPS_ALT,temp_value);
          }

          if (NO_ERROR == ret) {

               int altitudeRef = 0;
               if (valInt < 0)
                    altitudeRef = 1;
               else
                    altitudeRef = 0;
               char temp_value[5];
               snprintf(temp_value,
                        5,
                        "%d",altitudeRef);
               ret = exifTable->insertElement(TAG_GPS_ALT_REF,temp_value);
          }

          if (NO_ERROR == ret) {

               char temp_value[256];
               memset(temp_value, 0, 256);
               const char* gps_procMethod = mParameters.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);
               memcpy(temp_value, ExifAsciiPrefix, sizeof(ExifAsciiPrefix));
               memcpy(temp_value+sizeof(ExifAsciiPrefix), gps_procMethod,256-sizeof(ExifAsciiPrefix));
               ret = exifTable->insertElement(TAG_GPS_PROCESSING_METHOD,temp_value);
          }

          valStr = mParameters.get(CameraParameters::KEY_GPS_TIMESTAMP);
          if (valStr != NULL) {

               char temp_value[256];
               memset(temp_value, 0, 256);
               long gpsTimestamp = strtol(valStr, NULL, 10);
               struct tm *timeinfo = gmtime((time_t*) & (gpsTimestamp));
               if (NULL != timeinfo) {

                    snprintf(temp_value, 256 -1, "%d/%d,%d/%d,%d/%d",
                             timeinfo->tm_hour,1, timeinfo->tm_min,1, timeinfo->tm_sec,1);
                    ret = exifTable->insertElement(TAG_GPS_TIMESTAMP,temp_value);
                    memset(temp_value, '\0',256);
                    if (NO_ERROR == ret) {
                         strftime(temp_value, 256, "%Y:%m:%d",timeinfo);
                         ret = exifTable->insertElement(TAG_GPS_DATESTAMP,temp_value);
                    }
               }                
          }

          valInt = mParameters.getInt(CameraParameters::KEY_ROTATION);
          if ((NO_ERROR == ret) && (valInt >= 0)) {

               const char* exif_orient = 
                    ExifElementsTable::degreesToExifOrientation((unsigned int)valInt);
               if (exif_orient) {

                    ret = exifTable->insertElement(TAG_ORIENTATION, exif_orient);
               }
          }

          if (NO_ERROR == ret) {

               char temp_value[2];
               temp_value[1] = '\0';
               unsigned int temp_num = 0;

               temp_value[0] = '0';
               exifTable->insertElement(TAG_WHITEBALANCE, temp_value);
                
               temp_value[0] = '2';
               exifTable->insertElement(TAG_METERING_MODE, temp_value);
                
               temp_value[0] = '3';
               exifTable->insertElement(TAG_EXPOSURE_PROGRAM, temp_value);

               temp_value[0] = '1';
               exifTable->insertElement(TAG_COLOR_SPACE, temp_value);
                
               temp_value[0] = '2';
               exifTable->insertElement(TAG_SENSING_METHOD, temp_value);

               temp_value[0] = '1';
               exifTable->insertElement(TAG_CUSTOM_RENDERED, temp_value);

               temp_num = 0x18;
               snprintf(temp_value, 2, "%u", temp_num);
               exifTable->insertElement(TAG_FLASH, temp_value);

               unsigned int lightsource = 0;
               valStr = mParameters.get(CameraParameters::KEY_WHITE_BALANCE);
               if (strcmp(valStr,CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT) == 0)
                    lightsource = 10;
               else if (strcmp(valStr,CameraParameters::WHITE_BALANCE_DAYLIGHT) == 0)
                    lightsource = 1;
               else if (strcmp(valStr,CameraParameters::WHITE_BALANCE_FLUORESCENT) == 0)
                    lightsource = 2;
               else if (strcmp(valStr,CameraParameters::WHITE_BALANCE_AUTO) == 0)
                    lightsource = 9;

               valStr = mParameters.get(CameraParameters::KEY_FOCUS_MODE);
               if (strcmp(valStr, CameraParameters::FLASH_MODE_ON) == 0)
                    lightsource = 4;
               snprintf(temp_value, 2, "%u", lightsource);
               exifTable->insertElement(TAG_LIGHT_SOURCE, temp_value);
          }

          return ret;
     }

     void JZCameraParameters::initDefaultParameters(int facing) {

          if(mCameraDevice == NULL) {

               ALOGE("%s: init default parameters fail, camera device not open", __FUNCTION__);
               return;
          }

          ALOGE_IF(DEBUG_JZPARAM,"Enter %s : line=%d",__FUNCTION__, __LINE__);

          char preview_formats[1024];
          memset(preview_formats, 0, 1024);
          snprintf(preview_formats, sizeof(preview_formats), "%s,%s,%s,%s,%s,%s",
                   CameraParameters::PIXEL_FORMAT_YUV422I,
                   CameraParameters::PIXEL_FORMAT_YUV420SP,
                   CameraParameters::PIXEL_FORMAT_YUV422SP,
                   CameraParameters::PIXEL_FORMAT_YUV420P,
                   JZCameraParameters::PIXEL_FORMAT_JZ__YUV420T,
                   JZCameraParameters::PIXEL_FORMAT_JZ__YUV420P);
          mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, preview_formats);

          mParameters.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420SP);
 
          mParameters.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT,
                              CameraParameters::PIXEL_FORMAT_YUV420P);
          //mParameters.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, 
          //                    JZCameraParameters::PIXEL_FORMAT_JZ__YUV420T);

          mParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
                              CameraParameters::PIXEL_FORMAT_JPEG);

          mParameters.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
          mParameters.set(CameraParameters::KEY_JPEG_QUALITY,75);
          mParameters.set(CameraParameters::KEY_ROTATION,0);

          mParameters.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,"176x144,320x240,160x160,0x0");
          mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, 75);
          mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, atoi(THUMBNAIL_WIDTH));
          mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT,atoi(THUMBNAIL_HEIGHT));

          mParameters.set(CameraParameters::KEY_ZOOM_SUPPORTED,"false");
          mParameters.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED,"false");
          mParameters.set(CameraParameters::KEY_ZOOM, "0");
          mParameters.set(CameraParameters::KEY_ZOOM_RATIOS, "100");
          mParameters.set(CameraParameters::KEY_MAX_ZOOM,"100");

          mParameters.set(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED,"false");
          mParameters.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED,"false");
          mParameters.set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED,"false");
          mParameters.set(CameraParameters::KEY_RECORDING_HINT,"false");

          mParameters.setFloat(CameraParameters::KEY_FOCAL_LENGTH, 0.55);
          mParameters.set(CameraParameters::KEY_PREVIEW_FPS_RANGE,"10000,30000");

          if (facing == CAMERA_FACING_BACK) {
              mParameters.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW,atoi(CAMERA_FACEDETECT));
              mParameters.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW,0);
          } else if (facing == CAMERA_FACING_FRONT) {
              mParameters.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW,atoi(CAMERA_FACEDETECT));
              mParameters.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW,0);
          }

          mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,"15,20,25,30");
          mParameters.setPreviewFrameRate(30);
          mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE,"(10000,30000)");
          mParameters.set(CameraParameters::KEY_FOCUS_AREAS,0);
          mParameters.set(CameraParameters::KEY_FOCUS_DISTANCES,"1,100,10000");
          mParameters.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS,4);
          mParameters.setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE,90.0);
          mParameters.setFloat(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, 60.000);
          mParameters.set(CameraParameters::KEY_METERING_AREAS,0);
          mParameters.set(CameraParameters::KEY_MAX_NUM_METERING_AREAS,0);
          mParameters.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, 0);          
          mParameters.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION,0);
          mParameters.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION,0);
          mParameters.setFloat(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP,0);
          mParameters.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED,"false");
          mParameters.set(KEY_LUMA_ADAPTATION,0);
          mParameters.set(KEY_NIGHTSHOT_MODE, 0);
          mParameters.set(KEY_ORIENTATION,0);
          mParameters.set(CameraParameters::KEY_FOCUS_MODE,CameraParameters::FOCUS_MODE_FIXED);
          mParameters.set(CameraParameters::KEY_ANTIBANDING,CameraParameters::ANTIBANDING_AUTO);
          mParameters.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);  
          mParameters.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_AUTO);         
          mParameters.set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);
          mParameters.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
          mParameters.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED,"true");

          int num_wb = sizeof(JZCameraParameters::wb_map)/ sizeof(mode_map_t);
          mCameraDevice->setCommonMode(WHITE_BALANCE,
                                        string_to_mode(CameraParameters::WHITE_BALANCE_AUTO,
                                             JZCameraParameters::wb_map,num_wb));

          int num_ef = sizeof(JZCameraParameters::effect_map) / sizeof(mode_map_t);
          mCameraDevice->setCommonMode(EFFECT_MODE,
                                        string_to_mode(CameraParameters::EFFECT_NONE,
                                             JZCameraParameters::effect_map,num_ef));

          int num_fo = sizeof(JZCameraParameters::focus_map) / sizeof(mode_map_t);
          mCameraDevice->setCommonMode(FOCUS_MODE,
                                        string_to_mode(CameraParameters::FOCUS_MODE_FIXED,
                                             JZCameraParameters::focus_map,num_fo));

          int num_fl = sizeof(JZCameraParameters::flash_map) / sizeof(mode_map_t);
          mCameraDevice->setCommonMode(FLASH_MODE,
                                       string_to_mode(CameraParameters::FLASH_MODE_OFF,
                                             JZCameraParameters::flash_map,num_fl));

          int num_sc = sizeof(JZCameraParameters::scene_map) / sizeof(mode_map_t);
          mCameraDevice->setCommonMode(SCENE_MODE,
                                       string_to_mode(CameraParameters::SCENE_MODE_AUTO,
                                             JZCameraParameters::scene_map,num_sc));

          int num_an = sizeof(JZCameraParameters::antibanding_map) / sizeof(mode_map_t);
          mCameraDevice->setCommonMode(ANTIBAND_MODE,
                                       string_to_mode(CameraParameters::ANTIBANDING_AUTO,
                                             JZCameraParameters::antibanding_map,num_an));

          mParameters.set(CameraParameters::KEY_GPS_TIMESTAMP,"1199145599");
          mParameters.set(CameraParameters::KEY_GPS_ALTITUDE,21);
          mParameters.setFloat(CameraParameters::KEY_GPS_LATITUDE,37.736071);
          mParameters.setFloat(CameraParameters::KEY_GPS_LONGITUDE,-122.441983);
          mParameters.set(CameraParameters::KEY_GPS_PROCESSING_METHOD,"fake");

          struct sensor_info sinfo;
          struct resolution_info rinfo;

          memset(&sinfo, 0, sizeof(struct sensor_info));
          mCameraDevice->getSensorInfo(&sinfo,&rinfo);

          char* support_modes = modes_to_string(sinfo.modes.balance,JZCameraParameters::wb_map,num_wb);
          mParameters.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, support_modes);

          support_modes = modes_to_string(sinfo.modes.effect,JZCameraParameters::effect_map,num_ef);
          mParameters.set(CameraParameters::KEY_SUPPORTED_EFFECTS,support_modes);

          support_modes = modes_to_string(sinfo.modes.focus_mode,JZCameraParameters::focus_map, num_fo);
          mParameters.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES,support_modes);

          support_modes = modes_to_string(sinfo.modes.flash_mode,JZCameraParameters::flash_map,num_fl);
          mParameters.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES,support_modes);

          support_modes = modes_to_string(sinfo.modes.scene_mode,JZCameraParameters::scene_map,num_sc);
          mParameters.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES,support_modes);

          support_modes = modes_to_string(sinfo.modes.antibanding,
                              JZCameraParameters::antibanding_map,num_an);
          mParameters.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING,support_modes);

          int preview_table_size = sinfo.prev_resolution_nr;
          int capture_table_size = sinfo.cap_resolution_nr;
          char support_preview_sizes[1024];
          char support_picture_sizes[1024];
          char support_video_sizes[1024];
          int offset1 = 0;
          int offset2 = 0;
          int offset3 = 0;
          unsigned int low_width = 0;
          unsigned int low_height = 0;
          unsigned int high_width = 0;
          unsigned int high_height = 0;

          MediaProfiles* media_profile = MediaProfiles::getInstance();

          if ( media_profile != NULL && media_profile->hasCamcorderProfile(mCameraId,
                         CAMCORDER_QUALITY_LOW)) {

               low_width = media_profile->getCamcorderProfileParamByName("vid.width",
                                                                         mCameraId,CAMCORDER_QUALITY_LOW);
               low_height = media_profile->getCamcorderProfileParamByName("vid.height",
                                                                          mCameraId,CAMCORDER_QUALITY_LOW);
          }

          if (media_profile != NULL && media_profile->hasCamcorderProfile(mCameraId,CAMCORDER_QUALITY_HIGH)) {
               high_width =
                   media_profile->getCamcorderProfileParamByName("vid.width", mCameraId,CAMCORDER_QUALITY_HIGH);
               high_height =
                   media_profile->getCamcorderProfileParamByName("vid.height", mCameraId,CAMCORDER_QUALITY_HIGH);
          }

          ALOGE_IF(DEBUG_JZPARAM,"profile support video high size = %d x %d", high_width, high_height);
          ALOGE_IF(DEBUG_JZPARAM,"profile support video low size = %d x %d", low_width, low_height);

          memset(support_picture_sizes, 0, 1024);
          memset(support_preview_sizes, 0 ,1024);
          memset(support_video_sizes,   0, 1024);

          if (preview_table_size > 0) {
               for(int i = preview_table_size-1; i >= 0; i--) {

                    if (offset1 == 0) {
                         offset1 += snprintf(support_preview_sizes, 1024, "%dx%d", 
                                   rinfo.ptable[i].w, rinfo.ptable[i].h);
                    }else {
                         offset1 += snprintf(support_preview_sizes + offset1,
                                   1024 - offset1,",%dx%d",rinfo.ptable[i].w,rinfo.ptable[i].h);
                    }

                    if (rinfo.ptable[i].w >= maxpreview_width || rinfo.ptable[i].h >= maxpreview_height) {
                         maxpreview_width = rinfo.ptable[i].w;
                         maxpreview_height = rinfo.ptable[i].h;
                    }

                    if ((rinfo.ptable[i].w >= low_width) && (rinfo.ptable[i].h >= low_height)) {

                         if (offset3 == 0) {
                              offset3 += snprintf(support_video_sizes, 1024, "%dx%d",
                                             rinfo.ptable[i].w, rinfo.ptable[i].h);
                         } else {
                              offset3 += snprintf(support_video_sizes+offset3, 1024-offset3,
                                             ",%dx%d",rinfo.ptable[i].w,rinfo.ptable[i].h);
                         }
                    }
               }
          }

          if (capture_table_size > 0) {
               for (int i = capture_table_size - 1; i>=0; i--) {

                    if (offset2 == 0) {
                         offset2 += snprintf(support_picture_sizes, 1024 , "%dx%d", 
                              rinfo.ctable[i].w, rinfo.ctable[i].h);
                    } else {
                         offset2 += snprintf(support_picture_sizes + offset2, 
                              1024 - offset2, ",%dx%d",rinfo.ctable[i].w,rinfo.ctable[i].h);
                    }

                    if (rinfo.ctable[i].w > maxcapture_width || rinfo.ctable[i].h > maxcapture_height) {
                         maxcapture_width = rinfo.ctable[i].w;
                         maxcapture_height = rinfo.ctable[i].h;
                    }
               }
          }

          ALOGE_IF(DEBUG_JZPARAM,"support preview size = %s", support_preview_sizes);
          ALOGE_IF(DEBUG_JZPARAM,"support video size = %s", support_video_sizes);

          support_preview_sizes[offset1 + 1] = '\0';
          support_video_sizes[offset3 + 1] = '\0';
          mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,support_preview_sizes);
          mParameters.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES,support_video_sizes);

          mParameters.setVideoSize(maxpreview_width, maxpreview_height);

          char preview_size[20];
          memset(preview_size, '\0', 20);

          snprintf(preview_size, 20, "%dx%d", high_width, high_height);
          if (!isParameterValid(preview_size, support_preview_sizes)) {
              mParameters.setPreviewSize(maxpreview_width, maxpreview_height);
              ALOGE_IF(DEBUG_JZPARAM,"init preview size: %dx%d", maxpreview_width, maxpreview_height);
          } else {
              mParameters.setPreviewSize(high_width, high_height);
              ALOGE_IF(DEBUG_JZPARAM,"init preview size: %s", preview_size);
          }

          char tmp_preview_for_video[20];
          int preferred_offset = 0;
          int preferred_width = 0;
          int preferred_height = 0;
          memset(tmp_preview_for_video, 0, 20);

          //bool support_video_snapshort = (strcmp(mParameters.get(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED),"true") == 0);

          if (media_profile != NULL &&
              media_profile->hasCamcorderProfile(mCameraId,CAMCORDER_QUALITY_1080P)) {
               preferred_width =
                    media_profile->getCamcorderProfileParamByName("vid.width",
                         mCameraId,CAMCORDER_QUALITY_1080P);
               preferred_height =
                    media_profile->getCamcorderProfileParamByName("vid.height",
                         mCameraId,CAMCORDER_QUALITY_1080P);
          } else if (media_profile != NULL &&
                    media_profile->hasCamcorderProfile(mCameraId,CAMCORDER_QUALITY_720P)) {
               preferred_width =
                    media_profile->getCamcorderProfileParamByName("vid.width",
                         mCameraId,CAMCORDER_QUALITY_720P);
               preferred_height =
                         media_profile->getCamcorderProfileParamByName("vid.height",
                              mCameraId,CAMCORDER_QUALITY_720P);
          } else {
               preferred_width = maxpreview_width;
               preferred_height = maxcapture_height;
          }

          preferred_offset = snprintf(tmp_preview_for_video, 20,"%dx%d",preferred_width, preferred_height);
          tmp_preview_for_video[preferred_offset+1] = '\0';

          if (!isParameterValid(tmp_preview_for_video, support_preview_sizes)) {
              preferred_offset = snprintf(tmp_preview_for_video, 20,"%dx%d",maxpreview_width, maxpreview_height);
              tmp_preview_for_video[preferred_offset+1] = '\0';
          }

          mParameters.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO,tmp_preview_for_video);

          ALOGE_IF(DEBUG_JZPARAM,"init preferred preview size for video = %s", tmp_preview_for_video);

          if (if_need_picture_upscale()) {

               int dst_w = getPropertyValue("ro.board.camera.upscale_dst_w");
               int dst_h = getPropertyValue("ro.board.camera.upscale_dst_h");
               if ( dst_w > 0 && dst_h > 0)
               {
                    offset2 += snprintf(support_picture_sizes + offset2, 1024 - offset2, ",%dx%d",
                                        dst_w, dst_h);
               }
          }

          support_picture_sizes[offset2 + 1] = '\0';
          mParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,support_picture_sizes);
          mParameters.setPictureSize(maxcapture_width, maxcapture_height);

          ALOGE_IF(DEBUG_JZPARAM,"support pict size = %s", support_picture_sizes);
          ALOGE_IF(DEBUG_JZPARAM,"init picture size = %dx%d", maxcapture_width, maxcapture_height);

          struct camera_param param;
          memset(&param, 0, sizeof(struct camera_param));
          param.cmd = CPCMD_SET_PREVIEW_RESOLUTION;
          mParameters.getPreviewSize((int*)&(param.param.ptable[0].w),(int*)&(param.param.ptable[0].h));
          param.param.bpp = 1;
          setPreviewFormat((int*)(&(param.param.format)), mParameters.getPreviewFormat());
          mCameraDevice->setCameraParam(param,mParameters.getPreviewFrameRate());


          param.cmd = CPCMD_SET_CAPTURE_RESOLUTION;
          mParameters.getPictureSize((int*)&(param.param.ctable[0].w),(int*)&(param.param.ctable[0].h));
#ifdef COMPRESS_JPEG_USE_HW
          param.param.bpp = 1;
          param.param.format = PIXEL_FORMAT_JZ_YUV420T;
#else
          param.param.bpp = 2;
          param.param.format = PIXEL_FORMAT_YUV422I;
#endif
          mCameraDevice->setCameraParam(param,mParameters.getPreviewFrameRate());

          ALOGE_IF(DEBUG_JZPARAM_ALL,"All params = %s", mParameters.flatten().string());
     }

     bool JZCameraParameters::if_need_picture_upscale(void) {

          const char property1[] = "ro.board.camera.picture_upscale";
          const char property2[] = "ro.board.camera.upscale_id";
          bool ret = false;
          char prop[16];

          memset(prop, 0, 16);

          if (property_get(property1, prop, NULL) > 0)
          {
               if (strcmp(prop,"true") == 0)
                    ret = true;
          }

          int id = getPropertyValue(property2);
          if (id == mCameraId)
               ret = true;

          return ret;
     }

     bool JZCameraParameters::isParameterValid(const char *param, const char *supportedParams) {

          bool ret = true;
          char *pos = NULL;

          ALOGE_IF(DEBUG_JZPARAM,"%s: will set param: %s, suppoted params: %s",__FUNCTION__, param, supportedParams);
          if ( NULL == supportedParams ) {

               ALOGE("Invalid supported parameters string");
               ret = false;
               goto exit;
          }

          if ( NULL == param ) {

               ret = false;
               goto exit;
          }
          pos = strstr(supportedParams, param);
          if ( NULL == pos ) {

               ret = false;
          } else {
               ret = true;
          }

     exit:

          return ret;
     }

     bool JZCameraParameters::is_preview_size_change() {

          bool ret = false;
     
          if (isPreviewSizeChange) {

               isPreviewSizeChange = false;
               ret = true;
          }
          return ret;
     }


     bool JZCameraParameters::is_video_size_change() {

          bool ret = false;
     
          if (isVideoSizeChange) {

               isVideoSizeChange = false;
               ret = true;
          }

          return ret;
     }

     bool JZCameraParameters::is_picture_size_change() {

          bool ret = false;
     
          if (isPictureSizeChange) {

               isPictureSizeChange = false;
               ret = true;
          }
          return ret;
     }

//-----------------------------------------------------------------

     struct integer_string_pair {
          unsigned int integer;
          const char* string;
     };

     static integer_string_pair degrees_to_exif_lut[] = {
          {0,   "1"},
          {90,  "6"},
          {180, "3"},
          {270, "8"},
     };

     const char* ExifElementsTable::degreesToExifOrientation(unsigned int degrees) {

          for (int i=0; i < 4; ++i) {

               if (degrees == degrees_to_exif_lut[i].integer)
                    return degrees_to_exif_lut[i].string;
          }
          return NULL;
     }

     void ExifElementsTable::stringToRational(const char* str, unsigned int* num, unsigned int* den) {

          int len;
          char* tmpVal = NULL;

          if (str != NULL) {

               len = strlen(str);
               tmpVal = (char*) malloc(sizeof(char) * (len + 1));
          }
        
          if (tmpVal != NULL) {

               size_t den_len;                
               char *ctx;
               unsigned int numerator = 0;
               unsigned int denominator = 0;
               char* temp = NULL;
                
               memset(tmpVal, '\0', len + 1);
               strncpy (tmpVal, str, len);
               temp = strtok_r(tmpVal, ".", &ctx);
                
               if (temp != NULL)
                    numerator = atoi(temp);
               if (!numerator)
                    numerator = 1;
                
               temp = strtok_r(NULL, ".", &ctx);
               if (temp != NULL) {

                    den_len = strlen(temp);
                    if (den_len == 255)
                         den_len = 0;
                    denominator = static_cast<unsigned int>(pow((double)10, (int)den_len));
                    numerator = numerator * denominator + atoi(temp);
               } else {
                    denominator = 1;
               }
               free(tmpVal);
               *num = numerator;
               *den = denominator;
          }
     }

     bool ExifElementsTable::isAsciiTag(const char* tag) {

          return (strcmp(tag, TAG_GPS_PROCESSING_METHOD) == 0);
     }

     void ExifElementsTable::insertExifToJpeg(unsigned char* jpeg, size_t jpeg_size) {

          ReadMode_t read_mode = (ReadMode_t)(READ_METADATA | READ_IMAGE);

          ResetJpgfile();
          if (ReadJpegSectionsFromBuffer(jpeg, jpeg_size, read_mode)) {

               jpeg_opened = true;
               create_EXIF(table, exif_tag_count, gps_tag_count, has_datatime_tag);
          }
     }

     status_t ExifElementsTable::insertExifThumbnailImage(const char* thumb, int len) {

          status_t ret = NO_ERROR;

          if ((len > 0) && jpeg_opened) {

               ret = ReplaceThumbnailFromBuffer(thumb, len);
          }
          return ret;
     }

     void ExifElementsTable::saveJpeg(unsigned char* jpeg, size_t jpeg_size) {

          if (jpeg_opened) {

               WriteJpegToBuffer(jpeg, jpeg_size);
               DiscardData();
               jpeg_opened = false;
          }
     }

     ExifElementsTable::~ExifElementsTable() {

          int num_elements = gps_tag_count + exif_tag_count;
        
          for (int i = 0; i < num_elements; i++) {

               if (table[i].Value) {

                    free(table[i].Value);
                    table[i].Value = NULL;
               }
          }

          if (jpeg_opened) {

               DiscardData();
          }
     }

     status_t ExifElementsTable::insertElement(const char* tag, const char* value) {

          unsigned int value_length = 0;
          status_t ret = NO_ERROR;

          if (!value || !tag) {
               return BAD_VALUE;
          }

          if (position >= MAX_EXIF_TAGS_SUPPORTED) {

               return NO_MEMORY;
          }

          if (isAsciiTag(tag)) {
               value_length = sizeof (ExifAsciiPrefix) + strlen(value + sizeof(ExifAsciiPrefix));
          } else {
               value_length = strlen(value);
          }

          if (IsGpsTag(tag)) {

               table[position].GpsTag = TRUE;
               table[position].Tag = GpsTagNameToValue(tag);
               gps_tag_count++;
          } else {
               table[position].GpsTag = FALSE;
               table[position].Tag = TagNameToValue(tag);
               exif_tag_count++;

               if (strcmp(tag, TAG_DATETIME) == 0)
                    has_datatime_tag = true;
          }

          table[position].DataLength = 0;
          table[position].Value = (char*) malloc(sizeof(char) * (value_length + 1));
          if (table[position].Value) {
               memcpy(table[position].Value, value, value_length+1);
               table[position].DataLength = value_length + 1;
          }
          position++;
          return ret;
     }
};
