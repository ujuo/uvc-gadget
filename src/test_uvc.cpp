/*
 * Copyright(c) 2020 I4VINE Inc.,
 *
 *  @file  test_uvc.cpp
 *  @brief UVC gadget test application using dummy test pattern. 
 *  @author JuYoung Ryu <jyryu@i4vine.com>
 *
 * Based on test_main.cpp by Seungwoo Kim <ksw@i4vine.com>
*/  

#include <unistd.h>          
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/signal.h>
#include <stdlib.h>

#include "capis.h"


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
static int g_uvc_done = 0;
pthread_t 	usb_device_thr;
int thread_ret1;  
  

void signal_handler(int sig)
{
//	stop_video_capture(MODULE_RGB);
//	stop_video_capture(MODULE_DEPTH);	

	g_uvc_done = 1;

	pthread_join(usb_device_thr, (void**)&thread_ret1);
		
//close thread fd
//	close(thd.fd);
//
//	uninit_video_device(MODULE_RGB);
//	close_video_device(MODULE_RGB);
	
//	uninit_video_device(MODULE_DEPTH);
//	close_video_device(MODULE_DEPTH);	
	close_uvc_gadget_device();
	printf("Closed uvc device.\n");
	exit(0);		
}
  

static int flip = 0;
static char pic0[640*550*2];
static char pic1[640*550*2];

void fill_buf_func(void *fdt, int data_mode, void *data, int len)
{
	struct thread_data_t *thd = (struct thread_data_t *)fdt;
	struct fifo_mem_t *fmem;
	/* if 3d data available, put a data into data ptr*/
	DBGINFO("buf_fuc empty=%d len=%d\n", FIFO_EMPTY(thd->rgbd_data_q), len);
	DBGVERBOSE("fifo head=%d tail=%d\n", thd->rgbd_data_q->head, thd->rgbd_data_q->tail);
#if 0
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

void release_buf_func(void **ptr, void *data)
{
	/* we don't need release function now. */
}

static void *usb_device_func(void *data)
{
	struct thread_data_t *thd = (struct thread_data_t *)data;
	int ret;


	/* setup callback for uvc */
	ret = init_uvc_gadget_device((void *)thd, fill_buf_func, release_buf_func);
	if (ret) return NULL;

	/* Now process the uvc event */
	while (!g_uvc_done) {
		process_uvc_gadget_device(2000000);
//		usleep(40000);
	}

//	close_uvc_gadget_device();

//	g_all_done |= 2;

	return NULL;
}  
  
int main(int argc, char *argv[])
{
	//char uvc_dev[128];
	char* uvc_dev = "/dev/video2";
	int ret = 0;
	
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);		
	dfp = stdout;	
	if(argc > 1){
		uvc_dev = argv[1];	
	}	
//	memset(uvc_dev, 0, 128);
//	strcpy(uvc_dev, "/dev/video2");	
	
	{
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
	dfp = stdout;	

	thr_data.rgb_width = 640;
	thr_data.rgb_height = 480;
	thr_data.rgb_size = thr_data.rgb_width * thr_data.rgb_height * 2;
	thr_data.rgbd_data_q = (struct fifo_t *)&thr_data.__fifo_data[0];
	INIT_FIFO(thr_data.rgbd_data_q, 4);
	
	ret = open_uvc_gadget_device(uvc_dev);
	if (ret) return ERROR_OPEN_UVC;	
	
	ret = pthread_create(&usb_device_thr, NULL, usb_device_func, (void *)&thr_data);
	if (ret != 0) return ERROR_CREATE_USB_DEVICE_THREAD;
	
	while(!g_uvc_done){
		usleep(100000);
	}
	printf("close\n");
	close_uvc_gadget_device();
	return 0;	
}