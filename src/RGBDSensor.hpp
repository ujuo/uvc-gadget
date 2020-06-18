/**
 * Copyright(c) 2019 I4VINE Inc.,
 * All right reserved by Seungwoo Kim <ksw@i4vine.com>
 *
 *  @file  RGBDSensor.hpp
 *  @brief RGBD sensor c++ header
 *  @author Seungwoo Kim <ksw@i4vine.com>
 *
 *
*/

#ifndef __RGBD_SENSOR_HPP__
#define __RGBD_SENSOR_HPP__

#include <application.hpp>
#include <RGBDSensor.h>

typedef void (*cb_func_type)(void *);

class TRGBDSensor : public TApplication {
private:
	int 							num_of_rgb_buffer;
	int 							num_of_depth_buffer;
	int 							rgb_width;
	int 							rgb_height;
	int 							depth_width;
	int 							depth_heigth;

	int 							fd_rgb;
	int 							fd_depth;

	/* for RGB params */
	struct v4l2_format 				rgb_fmt;
	struct v4l2_streamparm 			rgb_parm;
	struct v4l2_crop 				rgb_crop;
	
	//struct v4l2_mxc_dest_crop 		rgb_of;
	struct v4l2_frmsizeenum 		rgb_fsize;
	struct v4l2_fmtdesc 			rgb_ffmt;

	pthread_t 						rgb_capture_thr;
	pthread_t						depth_capture_thr;
	int 							rgb_capture_id;

	cb_func_type CB_Func;

protected:
	virtual int  Init();
	virtual void Uninit();
	virtual bool waitFor(void **data);
	virtual void preRun(void *data);
	virtual void runner(void *data);
	virtual void postRun(void *data);
	virtual void do_delay();
public:
	TRGBDSensor(int num_rgb_buf = 8, int num_depth_buf= 4);
	~TRGBDSensor();
	
	virtual void SetupFormatForRGB(int width, int height, int format);
	virtual int  RegisterCallback(void *func);
};

#endif