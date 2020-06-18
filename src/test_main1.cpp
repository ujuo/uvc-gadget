
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/signal.h>
#include <stdlib.h>

#include <object.h>
#include <apis.h>

/*
int main(int argc, char *argv[])
{
	RGBDSensor sensor;

	sensor = new TRGBDSensor(4, 4);

	sensor->Run();
}
*/
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
//	int rgb_fd;
//	int depth_fd;

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
		if (g_all_done == 7)
			break;
		usleep(10000);
	}
	close_uvc_gadget_device();
	uninit_video_device(MODULE_DEPTH);
	close_video_device(MODULE_DEPTH);

	printf("Closed all device.\n");
	exit(0);
}

static void register_signal( void )
{
	signal( SIGINT,  signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGABRT, signal_handler );
}

static void *rgb_capture_func(void *data)
{
	struct v4l2_buffer buf;
	struct thread_data_t *thd = (struct thread_data_t *)data;	
	int ret, idx;

	ret = init_video_device(MODULE_RGB, thd->rgb_width, thd->rgb_height, 4);
	if (ret) return NULL;
	start_video_capture(MODULE_RGB);
	thd->rgb_index = 0;
	while (!g_video_done) {
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
	uninit_video_device(MODULE_RGB);
	close_video_device(MODULE_RGB);
	g_all_done |= 1;

	return NULL;
}

static void *depth_capture_func(void *data)
{
	struct v4l2_buffer buf;
	struct thread_data_t *thd = (struct thread_data_t *)data;
	struct fifo_mem_t *fmem;
	int ret, idx;

	thd->rgbd_data_q = (struct fifo_t *)&thd->__fifo_data[0];
	INIT_FIFO(thd->rgbd_data_q, 4);

	ret = init_video_device(MODULE_DEPTH, 224, 173 * 9, 4);
	if (ret) return NULL;
	start_video_capture(MODULE_DEPTH);

	while (!g_video_done) {
		dequeue_and_capture(MODULE_DEPTH, &buf, &thd->depth[0]);
		/* Copy data into some where */
		idx = (thd->rgb_index - 1) & 31;
		if (!FIFO_FULL(thd->rgbd_data_q)) {
			fmem = DQUE_FIFO_HEAD(thd->rgbd_data_q);
			fmem->rgb_stamp = thd->rgb_stamps[idx];
			fmem->depth_stamp = buf.timestamp;
			memcpy(&fmem->rgb[0], &thd->rgb[idx][0], thd->rgb_size);
			memcpy(&fmem->depth[0], &thd->depth[0], buf.bytesused);
			QUE_FIFO_HEAD(thd->rgbd_data_q);
			//DBGINFO("fifo que head=%d tail=%d\n", thd->rgbd_data_q->head, thd->rgbd_data_q->tail);
		}else{
			printf("FIFO_FULL\n");
		}
		/* call calback User calc functions */
		/* thd->callback((void *)thd); */
		/* We need synchronize with RGB */
		queue_capture(MODULE_DEPTH, &buf);
#if 0		
		if (thd->depth_index == 15) {
			FILE *fp;
			int j;
			char str[128];

			for (j=0; j<14; j++) {
				sprintf(str, "testdepth_%02d.bin", j);
				fp = fopen(str, "wb");
				if (fp) {
					fwrite(&thd->depth[j][0], 1, 224 * 173 * 2 * 9, fp);
					fclose(fp);
				}
			}
			break;
		}
#endif
	}
	uninit_video_device(MODULE_DEPTH);
	close_video_device(MODULE_DEPTH);

	g_all_done |= 2;

	return NULL;
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
	while (!g_video_done) {
		process_uvc_gadget_device(2000000);
	}

	close_uvc_gadget_device();

	g_all_done |= 2;

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_attr_t attr;
	int ret;
	struct v4l2_buffer buf;
	pthread_t 		rgb_capture_thr;
	pthread_t		depth_capture_thr;
	pthread_t		usb_device_thr;
	char uvc_dev[128];
//	int rgb_fd=0, depth_fd=0;


	memset(uvc_dev, 0, 128);
	strcpy(uvc_dev, "/dev/video2");

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

	timer_init();

	thr_data.rgb_width = 640;
	thr_data.rgb_height = 480;
	thr_data.rgb_size = thr_data.rgb_width * thr_data.rgb_height * 2;
	thr_data.rgbd_data_q = (struct fifo_t *)&thr_data.__fifo_data[0];
	INIT_FIFO(thr_data.rgbd_data_q, 4);
#if 1
	/* 1. open video devices. if error, return ERROR CODE */
	ret = open_video_device(MODULE_RGB);
	if (ret) return ERROR_OPEN_RGB;

	ret = open_video_device(MODULE_DEPTH);
	if (ret) return ERROR_OPEN_DEPTH;
	
//	thr_data.rgb_fd = rgb_fd;
//	thr_data.depth_fd = depth_fd;
#endif
	ret = open_uvc_gadget_device(uvc_dev);
	if (ret) return ERROR_OPEN_UVC;

	/* 2. make a thread for get RGB camera */
	pthread_attr_init(&attr);
	/* stack size of thread to 512K, enough for our usage. */
	ret = pthread_attr_setstacksize(&attr, 0x800000); 
	if (ret != 0) {
		DBGINFO("pthread attr setstacksize fail = %d\n", ret);
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
		//DBGERROR("========== detach fail\n");
		return ERROR_SETTING_THREAD_ATTR_DETACH;
	}
#if 1	
	ret = pthread_create(&rgb_capture_thr, &attr, rgb_capture_func, (void *)&thr_data);
	if (ret != 0) return ERROR_CREATE_RGB_THREAD;
#endif
	/* 3. make a thread for usb device */
	//ret = pthread_create(&depth_capture_thr, &attr, depth_capture_func, (void *)&thr_data);
	//if (ret != 0) return ERROR_CREATE_DEPTH_THREAD;
	ret = pthread_create(&usb_device_thr, &attr, usb_device_func, (void *)&thr_data);
	if (ret != 0) return ERROR_CREATE_USB_DEVICE_THREAD;

	/* 4. destroy thread attribute */
	if( pthread_attr_destroy(&attr) != 0 ) {
		return ERROR_DESTROY_THREAD_ATTR;
	}

	register_signal();

#if 1
	ret = init_video_device(MODULE_DEPTH, 224, 173 * 9, 4);
	if (ret) return ERROR_INIT_DEPTH;
	start_video_capture(MODULE_DEPTH);
#endif
	while (!g_video_done) {
		int idx;

#if 1
		dequeue_and_capture(MODULE_DEPTH, &buf, &thr_data.depth[0]);
		/* Copy data into some where */
		idx = (thr_data.rgb_index - 1) & 31;
		if (!FIFO_FULL(thr_data.rgbd_data_q)) {
			struct fifo_mem_t *fmem;

			fmem = DQUE_FIFO_HEAD(thr_data.rgbd_data_q);
			fmem->rgb_stamp = thr_data.rgb_stamps[idx];
			fmem->depth_stamp = buf.timestamp;
			memcpy(&fmem->rgb[0], &thr_data.rgb[idx][0], thr_data.rgb_size);
			memcpy(&fmem->depth[0], &thr_data.depth[0], buf.bytesused);
			/* The we need to call the callback func */
			QUE_FIFO_HEAD(thr_data.rgbd_data_q);
			//DBGINFO("fifo que head=%d tail=%d\n", thr_data.rgbd_data_q->head, thr_data.rgbd_data_q->tail);
		}
		/* call calback User calc functions */
		/* thd->callback((void *)thd); */
		/* We need synchronize with RGB */
		queue_capture(MODULE_DEPTH, &buf);
#else
		usleep(100000);
#endif

	}
#if 1
	uninit_video_device(MODULE_DEPTH);
	close_video_device(MODULE_DEPTH);
#endif
	g_all_done |= 4;

	return ret;
}
