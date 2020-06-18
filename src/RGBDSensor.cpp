/**
 * Copyright(c) 2019 I4VINE Inc.,
 * All right reserved by Seungwoo Kim <ksw@i4vine.com>
 *
 *  @file  RGBDSensor.cpp
 *  @brief RGBD sensor c++ api implementation
 *  @author Seungwoo Kim <ksw@i4vine.com>
 *
 *
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

#include <apis.hpp>
#include <capis.h>

#define DEF_RGB_WIDTH	1280
#define DEF_RGB_HEIGHT	960

#define RGB_DATA_SIZE	(1280 * 960 * 2)
#define DEPTH_DATA_SIZE	(224 * 173 * 2)
#define DEPTH9_DATA_SIZE	(DEPTH_DATA_SIZE * 9)

struct fifo_mem_t {
	struct timeval rgb_stamp;
	struct timeval depth_stamp;

	char rgb[RGB_DATA_SIZE];
	char depth[DEPTH9_DATA_SIZE];
};

struct fifo_t {
	uint8_t head;
	uint8_t tail;
	uint8_t size;
	uint8_t mask;
	
	struct fifo_mem_t fifo_mem[0];
} __attribute__ ((packed));

#define INIT_FIFO(t,s)	(t->head = t->tail = 0, t->size = s, t->mask = s-1)
#define FIFO_EMPTY(t)	(t->head == t->tail)
#define DQUE_FIFO_HEAD(t)	(&t->fifo_mem[t->head])
#define QUE_FIFO_HEAD(t)	(t->head++, t->head &= t->mask)
#define DQUE_FIFO_TAIL(t)	(&t->fifo_mem[t->tail])
#define QUE_FIFO_TAIL(t)	(t->tail++, t->tail &= t->mask)
#define FIFO_FULL(t)	(((t->head+1) & t->mask) == t->tail)
#define DATA_IN_FIFO(t)	((t->head >= t->tail) ? (t->head - t->tail) : (t->head + t->size - t->tail))

struct thread_data_t {
	int rgb_width;
	int rgb_height;
	int rgb_index;
	int rgb_size;

	struct timeval rgb_stamps[32];
	char rgb[32][RGB_DATA_SIZE];

	char depth[DEPTH9_DATA_SIZE];

	struct fifo_t *rgbd_data_q;
	char __fifo_data[sizeof(struct fifo_mem_t) * 4 + sizeof(struct fifo_t)];
};

struct thread_data_t thr_data;

static int g_all_done = 0;
static int g_video_done = 0;

static void signal_handler(int sig)
{
	/* Now close all the devices */
	printf("Aborted by signal %s (%d)..\n", (char*)strsignal(sig), sig);

	switch( sig )
	{
		case SIGINT :
			printf("SIGINT..\n"); 	break;
		case SIGTERM :
			printf("SIGTERM..\n");	break;
		case SIGABRT :
			printf("SIGABRT..\n");	break;
		default :
			break;
	}
	g_all_done = 0;
	g_video_done = 1;

	/* For uvc engine */
	while (1) {
		if (g_all_done == 3)
			break;
		usleep(10000);
	}
	close_uvc_gadget_device();
	printf("Closed all device.\n");
	exit(0);
}

static void register_signal_handler( void )
{
	signal( SIGINT,  signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGABRT, signal_handler );
}

/**
 *  @brief Constructor of RGBDSensor class
 *  @return none
 *  @see   Destructor and virtual functions.
*/

TRGBDSensor::TRGBDSensor(int num_rgb_buf, int num_depth_buf) : TApplication()
{
	num_of_rgb_buffer = num_rgb_buf;
	num_of_depth_buffer = num_depth_buf;
	CB_Func = NULL;
}

/**
 *  @brief  Destructor of RGBDSensor class
 *  @return none
 *  @see    TRGBDSensor
*/
TRGBDSensor::~TRGBDSensor()
{
	//~TApplication();
}

/**
 *  @brief Register callback functions
 *  @param[in] func    callback function, must be func(void *data) type.
 *  @return none
 *  @see   CALLBACK Function definition.
*/
int TRGBDSensor::RegisterCallback(void *func)
{
	CB_Func = (cb_func_type)func;

	return 0;
}

static void *rgb_capture_func(void *data)
{
	struct v4l2_buffer buf;

	init_video_device(MODULE_RGB, 640, 480, 4);
	start_video_capture(MODULE_RGB);
	while (!g_video_done) {
		dequeue_and_capture(MODULE_RGB, &buf, data);
		/* Copy data into some where */
		queue_capture(MODULE_RGB, &buf);
	}
	uninit_video_device(MODULE_RGB);
	close_video_device(MODULE_RGB);
}

static void *depth_capture_func(void *data)
{
	struct v4l2_buffer buf;

	init_video_device(MODULE_DEPTH, 224, 172 * 9, 4);
	start_video_capture(MODULE_DEPTH);
	while (!g_video_done) {
		dequeue_and_capture(MODULE_DEPTH, &buf, data);
		/* Copy data into some where */
		/* We need synchronize with RGB */
		queue_capture(MODULE_DEPTH, &buf);
	}
	uninit_video_device(MODULE_DEPTH);
	close_video_device(MODULE_DEPTH);
}

void fill_buf_func(void *fdata, int mode, void *data, int len)
{
	/* if 3d data available, put a data into data ptr*/
	/* also fill rgb camera data */
}

void release_buf_func(void **ptr, void *data)
{
	/* we don't need release function now. */
}

/**
 *  @brief Initialize all. This is virtual function called on starting point of Run
 *         All devices are open here and all parameters are actually set here.
 *  @return \b zero for success.
 			\b Non zero for error code
 *  @see   CALLBACK Function definition.
*/
int TRGBDSensor::Init()
{
	pthread_attr_t attr;
	int ret, i;
	struct v4l2_buffer buf;
	enum v4l2_buf_type type;
	struct v4l2_requestbuffers req;

	/* 1. open video devices. if error, return ERROR CODE */
	ret = open_video_device(MODULE_RGB);
	if (ret) return ERROR_OPEN_RGB;
	ret = open_video_device(MODULE_DEPTH);
	if (ret) return ERROR_OPEN_DEPTH;

	ret = open_uvc_gadget_device("/dev/video2");
	if (ret) return ERROR_OPEN_UVC;

	/* 2. make a thread for get RGB camera */
	pthread_attr_init(&attr);
	/* stack size of thread to 512K, enough for our usage. */
	ret = pthread_attr_setstacksize(&attr, 0x800000); 
	if (ret != 0) {
		//DBGINFO("pthread attr setstacksize fail = %d\n", ret);
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
		//DBGERROR("========== detach fail\n");
		return ERROR_SETTING_THREAD_ATTR_DETACH;
	}	
	ret = pthread_create(&rgb_capture_thr, &attr, rgb_capture_func, (void *)this);
	if (ret != 0) return ERROR_CREATE_RGB_THREAD;

	/* 3. make a thread for get DEPTH camera */
	ret = pthread_create(&depth_capture_thr, &attr, depth_capture_func, (void *)this);
	if (ret != 0) return ERROR_CREATE_DEPTH_THREAD;

	/* 4. destroy thread attribute */
	if( pthread_attr_destroy(&attr) != 0 ) {
		return ERROR_DESTROY_THREAD_ATTR;
	}
	/* setup callback for uvc */
	//ret = init_uvc_gadget_device((void *)&rgbd_data, fill_buf_func, release_buf_func);

	/* Now register signal handler */
	register_signal_handler();

	return ret;
}

/**
 *  @brief Uninit all. Stop rgb capture thread.
 *         All devices are close.
 *  @return \b zero for success.
 			\b Non zero for error code
 *  @see   CALLBACK Function definition.
*/
void TRGBDSensor::Uninit()
{
	/* call after stop/RUN */

	/* stop RGB camera capture thread. */

	/* close uvc */

	close_uvc_gadget_device();
}

/**
 *  @brief wait for depth & RGB capture.
 *         capture depth, and find closest timestamp buffer from rgb data.
 *  @return \b true for depth data ready.
 			\b false for no data.
 			\b actually this function blocked and wait for data, should always be true.
 *  @see   v4l2 dqbuf and internal rgb capture_func
*/
bool TRGBDSensor::waitFor(void **data)
{


	//fwrite(buffers[buf.index].start, fmt.fmt.pix.sizeimage, 1, fd_y_file);

	/* capture video 0 also... */
	if (1 /* closest_rgb_found */) {
		/* setup data to */
	} else {
		/* just fillup latest data */
	}

	return true;
}

/**
 *  @brief called before runner called.
 *         if data should be modified before running, do on this function.
 *  @return none

 *  @see   Application::Run(), runner
*/
void TRGBDSensor::preRun(void *data)
{
	/* Do nothing for this implementation */
}

/**
 *  @brief actual work functions for do something
 *         
 *  @return none

 *  @see   Application::Run()
*/
void TRGBDSensor::runner(void *data)
{
	process_uvc_gadget_device(2000000);

	if (CB_Func) {
		CB_Func(data);
		/* push data on internal queue for uvc interface. */
	}
}

/**
 *  @brief called after runner called.
 *         usually do dqbuf only.
 *  @return none

 *  @see   Application::Run(), runner
*/
void TRGBDSensor::postRun(void *data)
{
	// Done do anything now.
}

/**
 *  @brief called between postRun and waitFor
 *         if delay should implemented, do on this function.
 *  @return none

 *  @see   Application::Run(), runner, waitFor
*/
void TRGBDSensor::do_delay( void )
{
	/* Do nothing for this implementation */
}
