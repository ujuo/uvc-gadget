/**
 * Copyright(c) 2020 I4VINE Inc.,
 *
 *  @file  RGBDClass.cpp
 *  @brief RGBD uvc gadget test application.
 *  @author JuYoung Ryu <jyryu@i4vine.com>
 * 
 * Based on test_main.cpp by Seungwoo Kim <ksw@i4vine.com> 
*/ 
 
 
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/signal.h>
#include <stdlib.h>

//#include <apis.hpp>
#include <capis.h> 
#include "RGBDClass.hpp"

//TRGBDClass rgbd;
//static int exitRequested=false;

#if 0
/**
 *  @brief  "C" Terminate the program
 *  @param[in] sig   signals(SIGINT, SIGTERM, SIGABRT)
 *  @return none
*/
void signal_handler(int sig)
{
	stop_video_capture(MODULE_RGB);
	stop_video_capture(MODULE_DEPTH);	
	rgbd.g_video_done = 1;
	rgbd.g_depth_done = 1;
	rgbd.g_uvc_done = 1;	
	pthread_join(rgbd.rgb_capture_thr, (void**)&rgbd.thread_ret);
	pthread_join(rgbd.usb_device_thr, (void**)&rgbd.thread_ret1);	
//close thread fd
//	close(thd.fd);
//
	uninit_video_device(MODULE_RGB);
	close_video_device(MODULE_RGB);
	
	uninit_video_device(MODULE_DEPTH);
	close_video_device(MODULE_DEPTH);	
	close_uvc_gadget_device();
	exitRequested = true;
	printf("Closed rgbd & uvc device.\n");
	exit(0);				
}
#endif


/**
 *  @brief Constructor of RGBDSensor class
 *  @return none
*/

TRGBDClass::TRGBDClass(int num_buf) 
{
	thr_data.num_of_buffer = num_buf;
	thr_data.g_video_done = 0;
	thr_data.g_depth_done = 0;
	thr_data.g_uvc_done = 0;		
	exit_requested = 0;
	CB_Func = NULL;
}


/**
 *  @brief  Destructor of RGBDClass class
 *  @return none
*/
TRGBDClass::~TRGBDClass()
{
	
}

/**
 *  @brief Register callback functions
 *  @param[in] func    callback function, must be func(void *data) type.
 *  @return none
 *  @see   CALLBACK Function definition.
*/
int TRGBDClass::RegisterCallback(void *func)
{
	CB_Func = (cb_func_type)func;

	return 0;
}



/**
 *  @brief called before runner called.
 *         if data should be modified before running, do on this function.
 *  @return none

 *  @see   Application::Run(), runner
*/

void TRGBDClass::preRun(void *data)
{
	/* Do nothing for this implementation */
}


/**
 *  @brief actual work functions for do something
 *         
 *  @return none
*/
void TRGBDClass::runner(void *data)
{
	int idx;
	struct v4l2_buffer *buf = (struct v4l2_buffer *)data;		
	dequeue_and_capture(MODULE_DEPTH, buf, &thr_data.depth[0]);
	/* Copy data into some where */
	idx = (thr_data.rgb_index - 1) & 31;
	if (!FIFO_FULL(thr_data.rgbd_data_q)) {
		struct fifo_mem_t *fmem;

		fmem = DQUE_FIFO_HEAD(thr_data.rgbd_data_q);
		fmem->rgb_stamp = thr_data.rgb_stamps[idx];
		fmem->depth_stamp = buf->timestamp;
		memcpy(&fmem->rgb[0], &thr_data.rgb[idx][0], thr_data.rgb_size);
		memcpy(&fmem->depth[0], &thr_data.depth[0], buf->bytesused);
		
		if (CB_Func) {
			CB_Func(&fmem->rgb[0]);
		}
		/* The we need to call the callback func */
		QUE_FIFO_HEAD(thr_data.rgbd_data_q);
		//DBGINFO("fifo que head=%d tail=%d\n", thr_data.rgbd_data_q->head, thr_data.rgbd_data_q->tail);
	}
	/* call calback User calc functions */
	/* thd->callback((void *)thd); */
	/* We need synchronize with RGB */
	queue_capture(MODULE_DEPTH, buf);		


}






/**
 *  @brief   Capture thread start_routine. Fill the video buffer
 *  @param[in]  data     struct thread_data_t 
 *  @return NULL
 *  @see    main
*/
void * rgb_capture_func(void *data)
{
	struct v4l2_buffer buf;
	struct thread_data_t *thd = (struct thread_data_t *)data;	
	int ret, idx;

	ret = init_video_device(MODULE_RGB, thd->rgb_width, thd->rgb_height, thd->num_of_buffer);
	if (ret) return NULL;
	start_video_capture(MODULE_RGB);
	thd->rgb_index = 0;
	
	while (!thd->g_video_done) {
		idx = thd->rgb_index;
		dequeue_and_capture(MODULE_RGB, &buf, &thd->rgb[idx][0]);
		
		thd->rgb_stamps[idx] = buf.timestamp;
		idx++;
		idx &= 31;
		thd->rgb_index = idx;
	
		/* Copy data into some where */
		queue_capture(MODULE_RGB, &buf);
		DBGVERBOSE("rgb capture\n");
#if 0
		if (thd->rgb_index == 15) {
			FILE *fp;
			int j;
			char str[128];

			for (j=0; j<14; j++) {
				sprintf(str, "testdrgb_%02d.yuv", j);
				fp = fopen(str, "wb");
				if (fp) {
					fwrite(&thd->rgb[j][0], 1, thd->rgb_size, fp);
					fclose(fp);
				}
			}
			break;
		}
#endif

	}

//	uninit_video_device(MODULE_RGB);
//	close_video_device(MODULE_RGB);
//	g_all_done |= 1;

}
/** 
 *  @brief  Fill the uvc buffer with video data
 *  @param[in] fdt   struct thread_data_t 
 *  @param[out] data  video data
 *  @param[in] len   video data length
 *  @return none 
 *  @see   init_uvc_gadget_device. nothing to do
*/
void fill_buf_func(void *fdt, int data_mode, void *data, int len)
{
	struct thread_data_t *thd = (struct thread_data_t *)fdt;
	struct fifo_mem_t *fmem;
	/* if 3d data available, put a data into data ptr*/
	DBGINFO("buf_fuc empty=%d len=%d\n", FIFO_EMPTY(thd->rgbd_data_q), len);
	DBGVERBOSE("fifo head=%d tail=%d\n", thd->rgbd_data_q->head, thd->rgbd_data_q->tail);
#if 1
	if (!FIFO_EMPTY(thd->rgbd_data_q)) {
		fmem = DQUE_FIFO_TAIL(thd->rgbd_data_q);
		memcpy(data, &fmem->rgb[0], len);
		QUE_FIFO_TAIL(thd->rgbd_data_q);
	}
#else
	if (!flip) {
		memcpy(data, &pic0[0], len);
	} else {
		memcpy(data, &pic1[0], len);
	}
	flip = !flip;
#endif
	/* IF FIFO is empty then we don't copy at all, then data would be last data */
}


/** 
 *  @brief  Release the buffer
 *  @param[in] ptr   
 *  @param[out] data  ???????
 *  @return none 
 *  @see   init_uvc_gadget_device.
*/
void release_buf_func(void **ptr, void *data)
{
	/* we don't need release function now. */
}

/** 
 *  @brief  UVC thread start_routine
 *  @param[in] data   struct thread_data_t 
 *  @return NULL
 *  @see   main
*/
void * usb_device_func(void *data)
{
	struct thread_data_t *thd = (struct thread_data_t *)data;
	int ret;


	/* setup callback for uvc */
	ret = init_uvc_gadget_device((void *)thd, fill_buf_func, release_buf_func);
	if (ret) return NULL;

	/* Now process the uvc event */
	while (!thd->g_uvc_done) {
		process_uvc_gadget_device(2000000);
//		usleep(40000);
	}

//	close_uvc_gadget_device();

//	g_all_done |= 2;

	return NULL;
}  


/**
 *  @brief Initialize all. This is virtual function called on starting point of Run
 *         All devices are open here and all parameters are actually set here.
 *  @return \b zero for success.
 			\b Non zero for error code
 *  @see   CALLBACK Function definition.
*/
int TRGBDClass::Init()
{
//	pthread_attr_t attr;
	int ret, i;
	struct v4l2_buffer buf;

	thr_data.rgb_width = 640;
	thr_data.rgb_height = 480;
	thr_data.rgb_size = thr_data.rgb_width * thr_data.rgb_height * 2;
	thr_data.rgbd_data_q = (struct fifo_t *)&thr_data.__fifo_data[0];

	INIT_FIFO(thr_data.rgbd_data_q, thr_data.num_of_buffer);
	
	/* 1. open video devices. if error, return ERROR CODE */
	ret = open_video_device(MODULE_RGB);
	if (ret) return ERROR_OPEN_RGB;
	ret = open_video_device(MODULE_DEPTH);
	if (ret) return ERROR_OPEN_DEPTH;

	ret = open_uvc_gadget_device("/dev/video2");
	if (ret) return ERROR_OPEN_UVC;

	/* 2. make a thread for get RGB camera */
	ret = pthread_create(&rgb_capture_thr, NULL, rgb_capture_func, (void *)&thr_data);
	if (ret != 0) return ERROR_CREATE_RGB_THREAD;

	/* 3. make a thread for get uvc */
	ret = pthread_create(&usb_device_thr, NULL, usb_device_func, (void *)&thr_data);
	if (ret != 0) return ERROR_CREATE_DEPTH_THREAD;

	ret = init_video_device(MODULE_DEPTH, 224, 173 * 9, 4);
	if (ret) return ERROR_INIT_DEPTH;
	
	start_video_capture(MODULE_DEPTH);

	return ret;
}


/**
 *  @brief Uninit all. Stop rgb capture thread.
 *         All devices are close.
 *  @return \b zero for success.
 			\b Non zero for error code
 *  @see   CALLBACK Function definition.
*/
void TRGBDClass::Uninit()
{
	/* stop RGB camera & depth sensor capture */	
	stop_video_capture(MODULE_RGB);
	stop_video_capture(MODULE_DEPTH);	

	/* destory RGB thread & uvc thread. */	
	thr_data.g_video_done = 1;
	thr_data.g_depth_done = 1;
	thr_data.g_uvc_done = 1;	
	pthread_join(rgb_capture_thr, (void**)&thread_ret);
	pthread_join(usb_device_thr, (void**)&thread_ret1);	

	/* unmap and close RGB camera & depth sensor */
	uninit_video_device(MODULE_RGB);
	close_video_device(MODULE_RGB);
	
	uninit_video_device(MODULE_DEPTH);
	close_video_device(MODULE_DEPTH);	

	/* close uvc */	
	close_uvc_gadget_device();
	
	printf("Closed rgbd & uvc device.\n");	

}

#if 0
int main(int argc, char *argv[])
{
	int ret;
	struct v4l2_buffer buf;	
//	char uvc_dev[128];	
	char* uvc_dev = "/dev/video2";

//	pthread_t 		rgb_capture_thr;
//	pthread_t		depth_capture_thr;	
//	int rgb_fd=0, depth_fd=0;
	
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);	
	dfp = stdout;
	if(argc > 1){
		uvc_dev = argv[1];	
	}
//	memset(uvc_dev, 0, 128);
//	strcpy(uvc_dev, uvcdevname);	

/*	{
		FILE *fp;
		fp = fopen("testdrgb_00.yuv", "rb");
		if (fp) {
			fread(&pic0[0], 1, 640*480*2, fp);
			fclose(fp);
		}
		fp = fopen("testdrgb_01.yuv", "rb");
		if (fp) {
			fread(&pic1[0], 1, 640*480*2, fp);
			fclose(fp);
		}
		flip = 0;
	}	
	*/
	rgbd.thr_data.rgb_width = 640;
	rgbd.thr_data.rgb_height = 480;
	rgbd.thr_data.rgb_size = rgbd.thr_data.rgb_width * rgbd.thr_data.rgb_height * 2;
	rgbd.thr_data.rgbd_data_q = (struct fifo_t *)&rgbd.thr_data.__fifo_data[0];
	INIT_FIFO(rgbd.thr_data.rgbd_data_q, 4);
	
	ret = open_video_device(MODULE_RGB);
	if (ret) return ERROR_OPEN_RGB;
	ret = open_video_device(MODULE_DEPTH);
	if (ret) return ERROR_OPEN_DEPTH;	

	ret = open_uvc_gadget_device(uvc_dev);
	if (ret) return ERROR_OPEN_UVC;	
		
//	thr_data.rgb_fd = rgb_fd;
//	thr_data.depth_fd = depth_fd;
	
	ret = pthread_create(&rgbd.rgb_capture_thr, NULL, rgbd.rgb_capture_func, (void *)&rgbd.thr_data);
	if (ret != 0) return ERROR_CREATE_RGB_THREAD;

	ret = pthread_create(&rgbd.usb_device_thr, NULL, rgbd.usb_device_func, (void *)&rgbd.thr_data);
	if (ret != 0) return ERROR_CREATE_USB_DEVICE_THREAD;
	
	ret = init_video_device(MODULE_DEPTH, 224, 173 * 9, 4);
	if (ret) return ERROR_INIT_DEPTH;
	start_video_capture(MODULE_DEPTH);
	
	while(!rgbd.g_depth_done){
		int idx;
		
		dequeue_and_capture(MODULE_DEPTH, &buf, &rgbd.thr_data.depth[0]);
		/* Copy data into some where */
		idx = (rgbd.thr_data.rgb_index - 1) & 31;
		if (!FIFO_FULL(rgbd.thr_data.rgbd_data_q)) {
			struct fifo_mem_t *fmem;

			fmem = DQUE_FIFO_HEAD(rgbd.thr_data.rgbd_data_q);
			fmem->rgb_stamp = rgbd.thr_data.rgb_stamps[idx];
			fmem->depth_stamp = buf.timestamp;
			memcpy(&fmem->rgb[0], &rgbd.thr_data.rgb[idx][0], rgbd.thr_data.rgb_size);
			memcpy(&fmem->depth[0], &rgbd.thr_data.depth[0], buf.bytesused);
			/* The we need to call the callback func */
			QUE_FIFO_HEAD(rgbd.thr_data.rgbd_data_q);
			//DBGINFO("fifo que head=%d tail=%d\n", thr_data.rgbd_data_q->head, thr_data.rgbd_data_q->tail);
		}
		/* call calback User calc functions */
		/* thd->callback((void *)thd); */
		/* We need synchronize with RGB */
		queue_capture(MODULE_DEPTH, &buf);		
	}
	
/*	while(!exitRequested){
		printf("exit main process\n");
		usleep(1000000);
	};*/

	return 0;
}
#endif