/**
 * Copyright(c) 2020 I4VINE Inc.,
 * All right reserved by Juyoung Ryu <jyryu@i4vine.com>
 *
 *  @file  RGBDClass.hpp
 *  @brief RGBD sensor c++ header
 *  @author Juyoung Ryu <jyryu@i4vine.com>
 *
 *
*/ 

 
#ifndef __RGBD_CLASS_HPP__
#define __RGBD_CLASS_HPP__



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
	int num_of_buffer;	
	int g_video_done;
	int g_depth_done;
	int g_uvc_done;		

	struct timeval rgb_stamps[32];
	char rgb[32][RGB_DATA_SIZE];

	char depth[DEPTH9_DATA_SIZE];

	struct fifo_t *rgbd_data_q;
	char __fifo_data[sizeof(struct fifo_mem_t) * 4 + sizeof(struct fifo_t)];
};

typedef void (*cb_func_type)(void *);

class TRGBDClass {
private:	
	cb_func_type CB_Func;	
protected:
	virtual void preRun(void *data);
/*	virtual bool waitFor(void **data);


	virtual void postRun(void *data);
	virtual void do_delay();
	*/
public:	
	struct thread_data_t thr_data;

	int exit_requested;
	pthread_t rgb_capture_thr;
	pthread_t usb_device_thr;
	int thread_ret;
	int thread_ret1;
	
	TRGBDClass(int num_buf = 4);
	virtual ~TRGBDClass();

	virtual void runner(void *data);		
	virtual int  Init();	
	virtual void Uninit();	
	virtual int  RegisterCallback(void *func);	
};






























#endif