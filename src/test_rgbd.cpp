/*
 * Copyright(c) 2020 I4VINE Inc.,
 *
 *  @file  test_rgbd.cpp
 *  @brief RGBD sensor test application.
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

#include <netinet/in.h>
 #include <arpa/inet.h>
 
#include "capis.h"


#define NETWORK_CLIENT
#if defined(NETWORK_CLIENT)
#define PORT	5032
const char* netaddr = "192.168.1.46";
#endif

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
static int g_video_done = 0;
static int g_depth_done = 0;
pthread_t 		rgb_capture_thr;
int thread_ret;

void signal_handler(int sig)
{
	stop_video_capture(MODULE_RGB);
	stop_video_capture(MODULE_DEPTH);	
	g_video_done = 1;
	g_depth_done = 1;
	pthread_join(rgb_capture_thr, (void**)&thread_ret);
//close thread fd
//	close(thd.fd);
//
	uninit_video_device(MODULE_RGB);
	close_video_device(MODULE_RGB);
	
	uninit_video_device(MODULE_DEPTH);
	close_video_device(MODULE_DEPTH);	

	printf("Closed rgbd device.\n");
	exit(0);				
}

static void *rgb_capture_func(void *data)
{
	struct v4l2_buffer buf;
	struct thread_data_t *thd = (struct thread_data_t *)data;	
	int ret, idx;
#if defined(NETWORK_CLIENT)
		int sock_fd;
		int tcp_port = PORT;	
		const char* addr = netaddr;	
		struct sockaddr_in server_addr;
		
		sock_fd = socket(PF_INET, SOCK_STREAM, 0);
		if(sock_fd < 0){
			printf("==============socket failed");
			exit(-1);
		}
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr(addr);
		server_addr.sin_port = htons(tcp_port);
		
	
		if(connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) < 0){
			printf("================connect fail");
			exit(-1);
		}
#endif		
	ret = init_video_device(MODULE_RGB, thd->rgb_width, thd->rgb_height, 4);
	if (ret) return NULL;
	start_video_capture(MODULE_RGB);
	thd->rgb_index = 0;
	
	while (!g_video_done) {
		idx = thd->rgb_index;
		dequeue_and_capture(MODULE_RGB, &buf, &thd->rgb[idx][0]);
#if defined(NETWORK_CLIENT)
		send(sock_fd, &thd->rgb[idx][0], thd->rgb_size, 0);		
#endif			
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
#if defined(NETWORK_CLIENT)	
	close(sock_fd);	
#endif
//	uninit_video_device(MODULE_RGB);
//	close_video_device(MODULE_RGB);
//	g_all_done |= 1;

	return NULL;
}

int main(int argc, char *argv[])
{
	int ret;
	struct v4l2_buffer buf;	
//	pthread_t 		rgb_capture_thr;
//	pthread_t		depth_capture_thr;	
//	int rgb_fd=0, depth_fd=0;
	
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);	
	dfp = stdout;
	if(argc > 1){
		netaddr = argv[1];	
	}
	thr_data.rgb_width = 640;
	thr_data.rgb_height = 480;
	thr_data.rgb_size = thr_data.rgb_width * thr_data.rgb_height * 2;
	thr_data.rgbd_data_q = (struct fifo_t *)&thr_data.__fifo_data[0];
	INIT_FIFO(thr_data.rgbd_data_q, 4);
	
	ret = open_video_device(MODULE_RGB);
	if (ret) return ERROR_OPEN_RGB;
	ret = open_video_device(MODULE_DEPTH);
	if (ret) return ERROR_OPEN_DEPTH;	
	
//	thr_data.rgb_fd = rgb_fd;
//	thr_data.depth_fd = depth_fd;
	
	ret = pthread_create(&rgb_capture_thr, NULL, rgb_capture_func, (void *)&thr_data);
	if (ret != 0) return ERROR_CREATE_RGB_THREAD;

	ret = init_video_device(MODULE_DEPTH, 224, 173 * 9, 4);
	if (ret) return ERROR_INIT_DEPTH;
	start_video_capture(MODULE_DEPTH);
	
	while(!g_depth_done){
		int idx;
		
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
	}

	
	
	return 0;
}
		