/**
 * Copyright(c) 2019 I4VINE Inc.,
 * All right reserved by Seungwoo Kim <ksw@i4vine.com>
 *  @file  video_api.c
 *  @brief video interface api functions
 *  @author Seungwoo Kim <ksw@i4vine.com>
 *
 *
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <asm/types.h>          /* for videodev2.h */


#include <sys/signal.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <linux/mxc_v4l2.h>

#include <capis.h>

struct buffer {
        void   *start;
        size_t offset;
        unsigned int length;
};

static int fd_rgb;
static int fd_3d;

static int n_buffers_3d;
static int n_buffers_rgb;

static int g_capture_mode = 0;
static int g_cap_fmt_rgb;
static int g_cap_fmt_3d;
static int g_io;
static int g_camera_framerate = 30;
static int g_input = 0;

static struct buffer *buffers_3d;
static struct buffer *buffers_rgb;

#define DBG_ENTER()	DBGINFO("enter %s\n", __func__)
#define DBG_EXIT()	DBGINFO("exit %s\n", __func__)

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define errno_exit(s)		DBGERROR("%s error %d, %s\n", s, errno, strerror(errno));exit(EXIT_FAILURE)

static int xioctl(int fh, int request, void *arg)
{
	int r;
	
	do {
		r = ioctl(fh, request, arg);
	} while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));
	
	if (r == -1) {
		DBGERROR("error %d, %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return r;
}

static void print_pixelformat(char *prefix, int val)
{
	DBGINFO("%s: %c%c%c%c\n", prefix ? prefix : "pixelformat",
					val & 0xff,
					(val >> 8) & 0xff,
					(val >> 16) & 0xff,
					(val >> 24) & 0xff);
}

/**
 *  @brief "C" video device open
 *  @param[in] module  0 for RGB camera, 1 for Depth-3D
 *  @param[out] fd for RGB camera and Depth-3D.           
 *
 *  @return \b 0 for successful open
 *          \b under zero value indicated the error
            \b does not returns error code, because failure make immediately exit.
 *  @see   close_video_device
*/
int open_video_device(int module)
{
	int fd;
	char *dev_name;

	DBG_ENTER();

	if (module == MODULE_RGB)
		dev_name = "/dev/video0";
	else
	if (module == MODULE_3DDEPTH)
		dev_name = "/dev/video1";
	else
		return -1;

	fd = open(dev_name, O_RDWR , 0); ///* required */ | O_NONBLOC
	if (-1 == fd) {
		DBGERROR("Video device open failed(%s).\n", dev_name);
		exit(EXIT_FAILURE);
	}
	if (module == MODULE_RGB)
		fd_rgb = fd;
	else
	if (module == MODULE_3DDEPTH)
		fd_3d = fd;
	else
		return -1;

	/* currently only MMAP support */
	g_io = IO_METHOD_MMAP;

	return 0;
}

int get_fd(int module)
{
	if (module == MODULE_RGB)
		return fd_rgb;	
	else if(module == MODULE_3DDEPTH)
		return fd_3d;
}

/**
 *  @brief  "C" Set Set controls for video/camera
 *  @param[in] module   video module value
 *  @param[in] ctrl_id  control id
 *  @param[in] value    control value
 *  @return \b 0 for successful ioctl call
 *          \b under zero value indicated the error
 *  @see   other ioctl funcions.
*/
int vidioc_set_ctrl(int module, int ctrl_id, int value)
{
	struct v4l2_control ctrl;
	int fd;
	if (module == MODULE_RGB)
		fd = fd_rgb;
	else
	if (module == MODULE_3DDEPTH)
		fd = fd_3d;
	else
		return -1;


	bzero(&ctrl, sizeof(ctrl));

	ctrl.id	   = ctrl_id;
	ctrl.value = value;
	printf("vidioc_set_ctrl exit\n");
	return ioctl(fd, VIDIOC_S_CTRL, &ctrl);
}

static int init_mmap(int module, int num_of_driverbuf)
{
	struct v4l2_requestbuffers req;
	int  n_buffers,fd;
	char *dev_name;
	struct buffer *buffers;

	if (module == MODULE_3DDEPTH) {
		dev_name = "/dev/video1";
		fd = fd_3d;
	} else {
		dev_name = "/dev/video0";
		fd = fd_rgb;
	}

	DBG_ENTER();
	CLEAR(req);


	req.count = num_of_driverbuf;
#if defined(CAPTURE_MPLANE)
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
#else
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#endif
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		DBGERROR("VIDIOC_REQBUFS err");
		if (EINVAL == errno) {
			DBGERROR("%s does not support "
					 "memory mappingn", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}
	DBGPRINT("VIDIOC_REQBUFS done\n");
	
	if (req.count < 2) {
		DBGERROR("Insufficient buffer memory on %s\n",
					 dev_name);
		exit(EXIT_FAILURE);
	}

	if (fd == fd_3d) {
		buffers_3d = calloc(req.count, sizeof(*buffers_3d));
		if (!buffers_3d) {
			DBGERROR("Out of memory\n");
			exit(EXIT_FAILURE);
		}
		buffers = buffers_3d;
	}
	else {
		buffers_rgb = calloc(req.count, sizeof(*buffers_rgb));
		if (!buffers_rgb) {
			DBGERROR("Out of memory\n");
			exit(EXIT_FAILURE);
		}
		buffers = buffers_rgb;
	}


	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;
#if defined(CAPTURE_MPLANE)
		int i;
		struct v4l2_plane planes[FMT_NUM_PLANES];
#endif

		CLEAR(buf);

#if defined(CAPTURE_MPLANE)
		buf.type        = req.type;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;
		buf.length		= FMT_NUM_PLANES;
		buf.m.planes 	= planes;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
			errno_exit("VIDIOC_QUERYBUF");
		}

		for(i=0; i<FMT_NUM_PLANES; i++) {
			buffers[n_buffers].length[i] = planes[i].length;
			buffers[n_buffers].start[i] =
					mmap(NULL /* start anywhere */,
						 planes[i].length,
						  PROT_READ | PROT_WRITE /* required */,
						  MAP_SHARED /* recommended */,
						  fd, planes[i].m.mem_offset);

			if (MAP_FAILED == buffers[n_buffers].start[i]) {
				errno_exit("mmap");
			}
		}
#else
		buf.type        = req.type;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
			errno_exit("VIDIOC_QUERYBUF");
		}
		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].offset = (size_t) buf.m.offset;
		buffers[n_buffers].start = mmap (NULL, buffers[n_buffers].length,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			fd, buffers[n_buffers].offset);
		memset(buffers[n_buffers].start, 0xFF, buffers[n_buffers].length);
#endif
	}
	if (fd == fd_3d) {
		n_buffers_3d = n_buffers;
	}
	else {
		n_buffers_rgb = n_buffers;
	}

	DBG_EXIT();

	return 0;
}

static unsigned int get_size(int fmt, int num, int ww, int hh)
{
    int size;

    switch (fmt) {
    case V4L2_PIX_FMT_YUYV:
        if (num > 0) return 0;
        size = (ww * hh) * 2;
        break;
    case V4L2_PIX_FMT_YUV420M:
        if (num == 0) {
            size = ww * hh;
        } else {
            size = (ww * hh) >> 2;
        }
        break;
    case V4L2_PIX_FMT_YUV422P:
        if (num == 0) {
            size = ww * hh;
        } else {
            size = (ww * hh) >> 1;
        }
        break;
    case V4L2_PIX_FMT_YUV444:
        size = ww * hh;
        break;
    default:
        size = ww * hh * 2;
        break;
    }

    return size;
}

/**
 *  @brief  "C" gain control
 *  @param[in] module   video module
 *  @param[in] val      gain value for module
 *  @return none
 *
 *  @see   exposure_ctrl, other ctrl functions
*/
void gain_ctrl(int module, int val)
{
	if (MODULE_3DDEPTH == module) {
		// Do nothing for 3d sensor
	} else 
	if (MODULE_RGB == module) {
		vidioc_set_ctrl(fd_rgb, V4L2_CID_EXPOSURE_AUTO, 0);
	}
}

/**
 *  @brief  "C" exposure control
 *  @param[in] module   video module
 *  @param[in] val      exposure value for module
 *  @return none
 *
 *  @see   gain_ctrl, other ctrl functions
*/
void exposure_ctrl(int module, int val)
{
	if (MODULE_3DDEPTH == module) {
	} else
	if (MODULE_RGB == module) {
		vidioc_set_ctrl(fd_rgb, V4L2_CID_EXPOSURE_AUTO, 0);
	}	
}

/**
 *  @brief  "C" init video device
 *  @param[in] module   video module
 *  @param[in] width    width of camera
 *  @param[in] height   height of camera
 *  @param[in] num_of_driverbuf    number of buffers 
 *  @return \b zero for success
 *			\b none zero for fail, error value
 *
 *  @see   uninit_video_device
*/
int init_video_device(int module, int width, int height, int num_of_driverbuf)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_frmsizeenum fsize;
	struct v4l2_streamparm parm;
	struct v4l2_fmtdesc ffmt;
	char  *dev_name;
	int fd,ret,cap_fmt;
	int top = 0, left = 0;

	DBG_ENTER();
	if (module == MODULE_RGB) {
		fd = fd_rgb;
		dev_name = "/dev/video0";
		g_cap_fmt_rgb = V4L2_PIX_FMT_YUYV;
		cap_fmt = g_cap_fmt_rgb;
	}
	else
	if (module == MODULE_3DDEPTH) {
		fd = fd_3d;
		dev_name = "/dev/video1";
		g_cap_fmt_3d = V4L2_PIX_FMT_SBGGR12P;
		cap_fmt = g_cap_fmt_3d;
	} else
		return -1;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		 DBGERROR("VIDIOC_QUERYCAP err");
		if (EINVAL == errno) {
			DBGERROR("%s is no V4L2 device",
					 dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
		  !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
		DBGERROR("V4L2_CAP_VIDEO_CAPTURE err");
		DBGERROR("%s is no video capture device\n",
					 dev_name);
		exit(EXIT_FAILURE);
	}
	DBGINFO("card:%s driver:%s bus:%s version : %X\n", cap.card, cap.driver, cap.bus_info, cap.version);
	switch (g_io) {
	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
	case IO_METHOD_DMABUF:
		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			DBGERROR("V4L2_CAP_STREAMING err\n");
			DBGERROR("%s does not support streaming i/o\n",
						 dev_name);
			exit(EXIT_FAILURE);
		}
		break;
	}
	
	/* Select video input, video standard and tune here. */
	CLEAR(cropcap);

#if defined(CAPTURE_MPLANE)	
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
#else
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#endif
	cropcap.defrect.width = width;
	cropcap.defrect.height = height;
	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		DBGPRINT("VIDIOC_CROPCAP enter\n");
#if defined(CAPTURE_MPLANE)
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;//V4L2_BUF_TYPE_VIDEO_CAPTURE;
#else
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#endif
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
				DBGPRINT("VIDIOC_S_CROP err\n");
				switch (errno) {
				case EINVAL:
						/* Cropping not supported. */
						break;
				default:
						/* Errors ignored. */
						break;
				}
		}
		DBGPRINT("VIDIOC_S_CROP done\n");
	} else {
			/* Errors ignored. */
		DBGPRINT("VIDIOC_CROPCAP err\n");
	}

	DBGINFO("Enum sensor supported frame size:\n");
	fsize.index = 0;
	fsize.pixel_format = cap_fmt;
	DBGVERBOSE("fsize.pixel_format %d\n", fsize.pixel_format);
	while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize) >= 0) {
		DBGVERBOSE(" %dx%d\n", fsize.discrete.width,
					       fsize.discrete.height);
		fsize.index++;
	}

	ffmt.index = 0;
	while (ioctl(fd, VIDIOC_ENUM_FMT, &ffmt) >= 0) {
		print_pixelformat("sensor frame format", ffmt.pixelformat);
		ffmt.index++;
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = g_camera_framerate;
	parm.parm.capture.capturemode = g_capture_mode;
	DBGINFO("V4L2_BUF_TYPE_VIDEO_CAPTURE %d \n", V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (ioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
		DBGERROR("VIDIOC_S_PARM failed\n");
		return -1;
	}

	if (ioctl(fd, VIDIOC_S_INPUT, &g_input) < 0) {
		DBGERROR("VIDIOC_S_INPUT failed\n");
		return -1;
	}

	/* UVC driver does not implement CROP */
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_CROP, &crop) < 0) {
		DBGERROR("VIDIOC_G_CROP failed\n");
		return -1;
	}

	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c.width = width;
	crop.c.height = height;
	crop.c.top = top;
	crop.c.left = left;
	if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0) {
		DBGERROR("VIDIOC_S_CROP failed\n");
		return -1;
	}

/* only MXC support this */
	{
		struct v4l2_mxc_dest_crop of;

		of.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		of.offset.u_offset = 0;
		of.offset.v_offset = 0;

		DBGINFO("VIDIOC_S_DEST_CROP 0x%X\n", (unsigned int)VIDIOC_S_DEST_CROP);
		//if (ioctl(fd, VIDIOC_S_DEST_CROP, &of) < 0)	{
		//	DBGERROR("set dest crop failed\n");
		//	return -1;
		//}
	}
	

	
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = cap_fmt;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.bytesperline = width;
	fmt.fmt.pix.sizeimage = 0;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		DBGERROR("set format failed\n");
		return -1;
	}
	DBGINFO("imgsize=%d\n", fmt.fmt.pix.sizeimage);

	/*
 	* Set rotation
 	* It's mxc-specific definition for rotation.
 	*/
	/*	ctrl.id = V4L2_CID_PRIVATE_BASE + 0;
	ctrl.value = g_rotate;
	if (ioctl(fd_v4l, VIDIOC_S_CTRL, &ctrl) < 0)
	{
		printf("set ctrl failed\n");
		return 0;
	}*/


	/* Buggy driver paranoia. */
	/*min = fmt.fmt.pix_mp.width * 2;
	if (fmt.fmt.pix_mp.bytesperline < min)
			fmt.fmt.pix_mp.bytesperline = min;
	min = fmt.fmt.pix_mp.bytesperline * fmt.fmt.pix_mp.height;
	if (fmt.fmt.pix_mp.sizeimage < min)
			fmt.fmt.pix_mp.sizeimage = min;
	*/
	switch (g_io) {
	/*case IO_METHOD_READ:
		DEBUG_PRINT("IO_METHOD_READ\n");
		//init_read(fmt.fmt.pix_mp.sizeimage);
		break;
	*/
	case IO_METHOD_MMAP:
		DBGPRINT("IO_METHOD_MMAP\n");
		ret = init_mmap(module, num_of_driverbuf);
		break;
	
	case IO_METHOD_USERPTR:
		DBGPRINT("IO_METHOD_USERPTR\n");
		//	init_userp(fmt.fmt.pix_mp.sizeimage);
		break;
	case IO_METHOD_DMABUF:
		// Not support
		break;
	}
	/* Now I try to allocate memory for frames that slow file operation
	   slow down the performance. */
	/*{
		int i;
		for (i=0; i< num_of_framebuf; i++) {
			frame_buf[i] = (char *)malloc(width * height * 2);
		}
	}*/
	
	DBG_EXIT();

	return 0;
}

/**
 *  @brief  "C" start capture
 *  @param[in] module   video module
 *  @return none
 *
 *  @see   stop_capture
*/
void start_video_capture(int module)
{
	unsigned int i, n_buffers;
	int fd;
	enum v4l2_buf_type type;

	if (module == MODULE_RGB) {
		fd = fd_rgb;
		n_buffers = n_buffers_rgb;
	}
	else
	if (module == MODULE_3DDEPTH) {
		fd = fd_3d;
		n_buffers = n_buffers_3d;
	} else {
		DBGERROR("module is not correct value\n");
		return;
	}

	DBG_ENTER();
	switch (g_io) {
	case IO_METHOD_MMAP:
#if defined(CAPTURE_MPLANE)	
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
				errno_exit("VIDIOC_QBUF");
			}
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
			errno_exit("VIDIOC_STREAMON");
		}
#else
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
				DBGERROR("What error?\n")
				errno_exit("VIDIOC_QBUF");
			}
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		DBGINFO("stream on\n");
		if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
			errno_exit("VIDIOC_STREAMON");	
		}
#endif
		break;
	
	case IO_METHOD_USERPTR:
		break;
	case IO_METHOD_DMABUF:
		break;
	}
	DBG_EXIT();
}

/**
 *  @brief  "C" stop capture
 *  @param[in] module   video module
 *  @return none
 *
 *  @see   start_capture
*/
void stop_video_capture(int module)
{
	int fd;
	enum v4l2_buf_type type;

	if (module == MODULE_RGB) {
		fd = fd_rgb;
	}
	else
	if (module == MODULE_3DDEPTH) {
		fd = fd_3d;
	} else {
		DBGERROR("invalid module value\n");
		return;
	}

	DBG_ENTER();
#if defined(CAPTURE_MPLANE)	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
#else
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#endif
	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");
	DBG_EXIT();
}

/**
 *  @brief  "C" uninit device
 *  @param[in] module   video module
 *  @return \b zero for success
 *
 *  @see   init_video_device
*/
int uninit_video_device(int module)
{
	unsigned int i;
	int n_buffers;
	struct buffer *buffers;

	if (module == MODULE_RGB) {
		buffers = buffers_rgb;
		n_buffers = n_buffers_rgb;
	}
	else
	if (module == MODULE_3DDEPTH) {
		buffers = buffers_3d;
		n_buffers = n_buffers_3d;
	} else
		return -1;

	DBG_ENTER();
	switch (g_io) {
	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i) {
#if defined(CAPTURE_MPLANE)
			int j;

			for (j=0; j< FMT_NUM_PLANES;j++) {
				if (-1 == munmap(buffers[i].start[j], buffers[i].length[j]))
					errno_exit("munmap");
			}
#else
			if (-1 == munmap(buffers->start, buffers->length)) {
				errno_exit("munmap");	
			}
#endif
		}
		free(buffers);
		break;
	case IO_METHOD_DMABUF:
		break;
	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i)
			free(buffers[i].start);
		free(buffers);
		break;
	}
	DBG_EXIT();
	/*{
		int i;
		for (i=0; i< frame_count; i++) {
			free(frame_buf[i]);
		}
	}*/
	return 0;
}

int dequeue_and_capture(int module, struct v4l2_buffer *buf, void *data)
{
	int              r;
	int              fd;
	fd_set           fds;
	struct timeval   tv;
	struct buffer *buffers;

	if (module == MODULE_RGB) {
		fd = fd_rgb;
		buffers = buffers_rgb;
	}
	else
	if (module == MODULE_3DDEPTH) {
		fd = fd_3d;
		buffers = buffers_3d;
	} else {
		DBGERROR("invalid module value\n");
		return;
	}

	//DBG_ENTER();
	do {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);
	} while ( (r == 0) || ((r == -1) && (errno = EINTR)));

	if (r == -1) {
		perror("select");
		return errno;
	}

	CLEAR(*buf);
	buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf->memory = V4L2_MEMORY_MMAP;
	
	if (-1 == xioctl(fd, VIDIOC_DQBUF, buf)) {
		DBGERROR("What error?\n")
		errno_exit("VIDIOC_DQBUF");
	}
	//xioctl(fd, VIDIOC_DQBUF, buf);


	/* Copy memory to data */
	if (data != NULL)
		memcpy(data, buffers[buf->index].start, buf->bytesused);

	return r;
}

int queue_capture(int module, struct v4l2_buffer *buf)
{
	int fd;

	if (module == MODULE_RGB) {
		fd = fd_rgb;
	}
	else
	if (module == MODULE_3DDEPTH) {
		fd = fd_3d;
	} else {
		DBGERROR("invalid module value\n");
		return;
	}
	if (-1 == xioctl(fd, VIDIOC_QBUF, buf)) {
		DBGERROR("What error?\n")
		errno_exit("VIDIOC_QBUF");
	}
//	xioctl(fd, VIDIOC_QBUF, buf);
}

#if defined(CAPTURE_MPLANE)
static void process_image(char *ptr, const void *p1, int size1, const void *p2, int size2,const void *p3, int size3)
{	
	memcpy(ptr, p1, size1); ptr += size1;
	memcpy(ptr, p2, size2); ptr += size2;
	memcpy(ptr, p3, size3);
}
#else
static void process_image(char *ptr,  const void *p1, int size1)
{	
	memcpy(ptr, p1, size1);
}
#endif
int read_frame(int module, void *ptr)
{
	struct v4l2_buffer buf;
	struct buffer *buffer;
	unsigned int i, n_buffers;
	int fd;

	if (module == MODULE_RGB) {
		n_buffers = n_buffers_rgb;
		fd = fd_rgb;
	} else
	if (module == MODULE_3DDEPTH) {
		n_buffers = n_buffers_3d;
		fd = fd_3d;
	} else
		return -1;

	DBG_ENTER();
	switch (g_io) {
		//process_image(buffers[0].start[0], buffers[0].length[0]);
		break;
	
	case IO_METHOD_MMAP:
	case IO_METHOD_DMABUF:
		{
			struct v4l2_buffer buf;
#if defined(CAPTURE_MPLANE)			
			struct v4l2_plane planes[3];
#endif
			CLEAR(buf);
#if defined(CAPTURE_MPLANE)
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			buf.length		= FMT_NUM_PLANES;
			//
			memset(&planes[0], 0, sizeof(struct v4l2_plane) * 3);
			buf.m.planes = &planes[0];
#else
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;		
#endif
			
			if (g_io == IO_METHOD_MMAP) {
				buf.memory = V4L2_MEMORY_MMAP;
				//DBG_PRINT("IO_METHOD_MMAP\n");
			} else {
				buf.memory = V4L2_MEMORY_DMABUF;
				//DBG_PRINT("IO_METHOD_DMABUF\n");
			}
		
			if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
				//DBG_PRINT("DQ error\n");
				switch (errno) {
				case EAGAIN:
						return 0;

				case EIO:
						/* Could ignore EIO, see spec. */

						/* fall through */

				default:
						errno_exit("VIDIOC_DQBUF");
				}
			}
			//DBG_PRINT("index=%d\n", buf.index);
			assert(buf.index < n_buffers);
			//DBG_PRINT("IO_METHOD_MMAP/DMABUF VIDIOC_DQBUF done\n");
			if (module == MODULE_RGB)
				buffer = &buffers_rgb[buf.index];
			else {
				buffer = &buffers_3d[buf.index];
			}

#if defined(CAPTURE_MPLANE)
			/* we need number of planes buffer */
#else
			process_image((char *)ptr, buffer->start, buffer->length);
#endif

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
			//DBG_PRINT("O_METHOD_MMAP/DMABUF VIDIOC_QBUF done\n");
		}
		break;
	
	case IO_METHOD_USERPTR:
		CLEAR(buf);
#if defined(CAPTURE_MPLANE)
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
					return 0;

			case EIO:
					/* Could ignore EIO, see spec. */

					/* fall through */

			default:
					errno_exit("VIDIOC_DQBUF");
			}
		}

		for (i = 0; i < n_buffers; ++i)
			if (buf.m.userptr == (unsigned long)buffers[i].start[0]
				&& buf.length == buffers[i].length[0])
				break;
#else
		/* Not supported YET */
#endif
		assert(i < n_buffers);

		//process_image((void *)buf.m.userptr, buf.bytesused);

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");
		break;
	}
	DBG_EXIT();
	return 1;
}

int close_video_device(int module)
{
	int fd;

	if (module == MODULE_RGB)
		fd = fd_rgb;
	else if (module == MODULE_3DDEPTH)
		fd = fd_3d;
	else
		return -1;

	close(fd);

	return 0;
}

double elasped_time(void)
{
	struct timeval tv;
	double t;
	gettimeofday(&tv,NULL);
	t = (tv.tv_sec*1000 + tv.tv_usec/1000);
	return t;
}

