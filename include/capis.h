/**
 * Copyright(c) 2019 I4VINE Inc.,
 * All right reserved by Seungwoo Kim <ksw@i4vine.com>
 *  @file  capis.h
 *  @brief  "C" video interface api functions header file.
 *  @author Seungwoo Kim <ksw@i4vine.com>
 *
 *
*/
#ifndef __CAPIS_H__
#define __CAPIS_H__

#ifdef __cplusplus
extern "C"{
#endif

#include <linux/videodev2.h>

#define DEBUG_PRINTOUT	1

/* IO methods supported */
enum io_method {
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
	IO_METHOD_DMABUF,
};

enum MODULE_INDEX {
	MODULE_RGB = 0,
	MODULE_3DDEPTH = 1,
	MODULE_DEPTH = 1,
};

#define V4L2_CID_CAMERA_INPUT_ROTATION	(V4L2_CID_PRIVATE_BASE + 0)

/* for video capture */
int  open_video_device(int module);//, int fd);
int get_fd(int module);
int  vidioc_set_ctrl(int module, int ctrl_id, int value);
void gain_ctrl(int module, int val);
void exposure_ctrl(int module, int val);
int  init_video_device(int module, int width, int height, int num_of_driverbuf);
void start_video_capture(int module);
int  dequeue_and_capture(int module, struct v4l2_buffer *buf, void *data);
int  queue_capture(int module, struct v4l2_buffer *buf);
void stop_video_capture(int module);
int  uninit_video_device(int module);
int close_video_device(int module);

/* for uvc gadget */
typedef void (* UVC_BUFFER_FILL_FUNC)(void *, int, void *, int);
typedef void (* UVC_BUFFER_RELEASE_FUNC)(void **, void *);

int  open_uvc_gadget_device(char *name);
int  init_uvc_gadget_device(void *fdata, UVC_BUFFER_FILL_FUNC fill_buf_func, UVC_BUFFER_RELEASE_FUNC release_buf_func);
int  process_uvc_gadget_device(int useconds);
void close_uvc_gadget_device();

/* for utills */
void timer_init();

extern FILE *dfp;
extern int debug_level;

#if DEBUG_PRINTOUT
	void cur_time(FILE *fp);
	#define DBGERROR(x...)	if (debug_level > 0) {cur_time(stderr);fprintf(stderr, x);}
	#define DBGPRINT(x...)	if (debug_level > 0) {cur_time(dfp);fprintf(dfp, x);}
	#define DBGINFO(x...)	if (debug_level > 1) {cur_time(dfp);fprintf(dfp, x);}
	#define DBGVERBOSE(x...) if (debug_level > 2) {cur_time(dfp);fprintf(dfp, x);}
#else
	#define DBGERROR(x...)	{cur_time(dfp);fprintf(dfp, x);}
	#define DBGPRINT(x...)	do {} while (0)
	#define DBGINFO(x...)	do {} while (0)
	#define DBGVERBOSE(x...)	do {} while (0)
#endif

/* PRIVATE ERROR DECLARATION */
enum {
	ERROR_OPEN_RGB = -1000,
	ERROR_OPEN_DEPTH = -1001,
	ERROR_OPEN_UVC = -1002,
	ERROR_SETTING_THREAD_ATTR_DETACH = -1003,
	ERROR_DESTROY_THREAD_ATTR = -1004,
	ERROR_CREATE_RGB_THREAD = -1005 ,
	ERROR_CREATE_DEPTH_THREAD = -1006,
	ERROR_CREATE_USB_DEVICE_THREAD = -1007,
	ERROR_INIT_DEPTH = -1007,
};

#ifdef __cplusplus
};
#endif

#endif
