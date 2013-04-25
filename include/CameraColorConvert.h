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

#ifndef CAMERA_COLORCONVERT_H_
#define CAMERA_COLORCONVERT_H_

#include "CameraDeviceCommon.h"

namespace android {

     class CameraColorConvert {

     public:

	  CameraColorConvert ();
	  virtual ~CameraColorConvert ();

     public:
	  void convert_yuv420p_to_rgb565(CameraYUVMeta *yuvMeta, uint8_t *dstAttr);
	  void cimyu420b_to_ipuyuv420b(CameraYUVMeta* yuvMeta);

	  void tile420_to_rgb565(CameraYUVMeta* yuvMeta, uint8_t* dst);

	  void cimyuv420b_to_tile420(CameraYUVMeta* yuvMeta);

	  void cimyuv420b_to_tile420(CameraYUVMeta* yuvMeta,uint8_t* dest_frame);

	  void cimyuv420b_to_yuv420p(CameraYUVMeta* yuvMeta, uint8_t* dstAddr);
	  
	  void yuyv_to_rgb24 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,
			      int prgbstride, int width, int height);

	  void yuyv_to_rgb32 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,
			      int prgbstride, int width, int height);

	  void yuyv_to_bgr24 (uint8_t *pyuv, int pyuvstride, uint8_t *pbgr, 
			      int pbgrstride, int width, int height);

	  void yuyv_to_bgr32 (uint8_t *pyuv, int pyuvstride, uint8_t *pbgr, 
			      int pbgrstride, int width, int height);

	  void yuyv_to_rgb565 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,
			       int prgbstride, int width, int height);

	  void yuv420tile_to_yuv420sp(CameraYUVMeta* yuvMeta, uint8_t* dest);

	  void yuyv_to_yuv420sp (uint8_t* src_frame , uint8_t* dst_frame , int width ,
				 int height);


	  void yuyv_to_yvu422p(uint8_t *dst,int dstStride, int dstHeight, 
			       uint8_t *src, int srcStride, int width, int height);  

	  void yuyv_to_yvu420p(uint8_t *dst,int dstStride, int dstHeight,
			       uint8_t *src, int srcStride, int width, int height);


	  void yuyv_to_yvu420sp(uint8_t *dst,int dstStride, int dstHeight, 
				uint8_t *src, int srcStride, int width, int height);

	  void tile420_to_yuv420p(CameraYUVMeta* yuvMeta, uint8_t* dest);

	  void yuyv_to_yuv420p(uint8_t *dst,int dstStride, int dstHeight, 
			       uint8_t *src, int srcStride, int width, int height);

	  void uyvy_to_yuyv (uint8_t *dst,int dstStride, 
			     uint8_t *src, int srcStride, int width, int height);

	  void yvyu_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, 
			     int srcStride, int width, int height);

	  void yyuv_to_yuyv (uint8_t *dst,int dstStride, 
			     uint8_t *src, int srcStride, int width, int height);

	  void yuv420_to_yuyv (uint8_t *dst,int dstStride, 
			       uint8_t *src, int width, int height);

	  void yvu420_to_yuyv (uint8_t *dst,int dstStride, 
			       uint8_t *src, int width, int height);

	  void nv12_to_yuyv (uint8_t *dst,int dstStride, 
			     uint8_t *src, int width, int height);

	  void nv21_to_yuyv (uint8_t *dst,int dstStride, 
			     uint8_t *src, int width, int height);

	  void nv16_to_yuyv (uint8_t *dst,int dstStride, 
			     uint8_t *src, int width, int height);

	  void grey_to_yuyv (uint8_t *dst,int dstStride, 
			     uint8_t *src, int srcStride, int width, int height);

	  void y16_to_yuyv (uint8_t *dst,int dstStride, 
			    uint8_t *src, int srcStride, int width, int height);

	  void y41p_to_yuyv (uint8_t *dst,int dstStride, 
			     uint8_t *src, int width, int height);

	  void rgb_to_yuyv(uint8_t *pyuv, int dstStride, uint8_t *prgb, 
			   int srcStride, int width, int height);

	  void bgr_to_yuyv(uint8_t *pyuv, int dstStride, uint8_t *pbgr, 
			   int srcStride, int width, int height);

	  void nv61_to_yuyv (uint8_t *dst,int dstStride, 
			     uint8_t *src, int width, int height);

	  void yuyv_mirror (uint8_t* src_frame , int width , int height);

	  void yuyv_upturn (uint8_t* src_frame , int width , int height);

	  void yuyv_negative (uint8_t* src_frame , int width , int height);

	  void yuyv_monochrome (uint8_t* src_frame , int width , int height);

	  void yuyv_pieces (uint8_t* src_frame , int width , int height ,
			    int piece_size);

	  void yvu422sp_to_yuyv (uint8_t *src_frame , uint8_t *dst_frame , int width ,
				 int height);

	  void yuv422sp_to_yuv420p(uint8_t* dest, uint8_t* src_frame, int width, int height);

	  void yuv422sp_to_yuv420sp(uint8_t* dest, uint8_t* src_frame, int width, int height);

	  void yvu420sp_to_yuyv (uint8_t* src_frame , uint8_t* dst_frame , int width ,
				 int height);

	  void yuyv_to_yuv422sp (uint8_t* src_frame , uint8_t* dst_frame , int width ,
				 int height);

	  void yuv420b_64u_64v_to_rgb565(CameraYUVMeta* yuvMeta, uint8_t* dst,
					 int rgbwidth, int rgbheight, int rgbstride, int destFmt);		 
	
	  void yuv420p_to_tile420(CameraYUVMeta* yuvMeta, char *yuv420t);

	  void yuv420p_to_yuv420sp(uint8_t* src_frame, uint8_t* dest_frame,
				   int width, int height);

	  void yuv420p_to_yuv422sp(uint8_t* src_frame, uint8_t* dest_frame,
				   int width, int height);


	  void yuv420p_to_rgb565 (uint8_t* src_frame , uint8_t* dst_frame , int width ,
				  int height);

	  void yuv420sp_to_yuv420p(uint8_t* src_frame, uint8_t* dst_frame,
				   int width, int height);

	  void yuv420sp_to_rgb565 (uint8_t* src_frame , uint8_t* dst_frame , int width ,
				   int height);


	  void yuv420sp_to_argb8888 (uint8_t* src_frame , uint8_t* dst_frame ,
				     int width , int height);

     private:

	  void initClip (void);

     private:

	  const signed kClipMin;
	  const signed kClipMax;
	  int mtmp_uv_size;

	  uint8_t *mClip;
	  uint8_t* mtmp_uv;
	  uint8_t* mtmp_sweap;
	  uint8_t* msrc;
     };

};

#endif /* CAMERACOLORCONVERT_H_ */
