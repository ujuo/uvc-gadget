/**
 * Copyright(c) 2020 I4VINE Inc.,
 *  @brief Support the Contrast attribute of the Processing Unit (PU) 
 *  @author JuYoung Ryu <jyryu@i4vinec.com>
 *
 *
 * Copyright(c) 2019 I4VINE Inc.,
 *
 *  @file  uvc_api.c
 *  @brief uvc gadget interface of RGBD sensor project.
 *  @author Seungwoo Kim <ksw@i4vine.com>
 *
 * uvc api 
 *
 * Based on uvc gadget test application by
 *  uvc_gadget.git with ideas and patches from Bhupesh Sharma, Robert Baldyga
 *
 * UVC gadget test application
 *
 * Copyright (C) 2010 Ideas on board SPRL <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
*/

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/usb/ch9.h>
#include <linux/usb/video.h>
#include <linux/videodev2.h>

#include "uvc.h"
#include <capis.h>

static struct uvc_device *device;

static UVC_BUFFER_FILL_FUNC fill_buffer_handler;
static UVC_BUFFER_RELEASE_FUNC buffer_release_handler;

/* Enable debug prints. */
#undef ENABLE_BUFFER_DEBUG
#undef ENABLE_USB_REQUEST_DEBUG

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define clamp(val, min, max)                                                                                           \
	({                                                                                                                 \
		typeof(val) __val = (val);                                                                                     \
		typeof(min) __min = (min);                                                                                     \
		typeof(max) __max = (max);                                                                                     \
		(void)(&__val == &__min);                                                                                      \
		(void)(&__val == &__max);                                                                                      \
		__val = __val < __min ? __min : __val;                                                                         \
		__val > __max ? __max : __val;                                                                                 \
	})

#define ARRAY_SIZE(a) ((sizeof(a) / sizeof(a[0])))
#define pixfmtstr(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

/*
 * The UVC webcam gadget kernel driver (g_webcam.ko) supports changing
 * the Brightness attribute of the Processing Unit (PU). by default. If
 * the underlying video capture device supports changing the Brightness
 * attribute of the image being acquired (like the Virtual Video, VIVI
 * driver), then we should route this UVC request to the respective
 * video capture device.
 *
 * Incase, there is no actual video capture device associated with the
 * UVC gadget and we wish to use this application as the final
 * destination of the UVC specific requests then we should return
 * pre-cooked (static) responses to GET_CUR(BRIGHTNESS) and
 * SET_CUR(BRIGHTNESS) commands to keep command verifier test tools like
 * UVC class specific test suite of USBCV, happy.
 *
 * Note that the values taken below are in sync with the VIVI driver and
 * must be changed for your specific video capture device. These values
 * also work well in case there in no actual video capture device.
 */
#define PU_BRIGHTNESS_MIN_VAL 0
#define PU_BRIGHTNESS_MAX_VAL 255
#define PU_BRIGHTNESS_STEP_SIZE 1
#define PU_BRIGHTNESS_DEFAULT_VAL 127

/* ---------------------------------------------------------------------------
 * Generic stuff
 */

/* Buffer representing one video frame */
struct buffer {
	struct v4l2_buffer buf;
	void *start;
	size_t length;
};

/* ---------------------------------------------------------------------------
 * UVC specific stuff
 */

struct uvc_frame_info {
	unsigned int width;
	unsigned int height;
	unsigned int intervals[8];
};

struct uvc_format_info {
	unsigned int fcc;
	const struct uvc_frame_info *frames;
};

static const struct uvc_frame_info uvc_frames_yuyv[] = {
	{
		640,
		550,
		{333333, 500000,666666,1000000,2000000,10000000,0},
	},
	{
		640,
		480,
		{333333, 500000,666666,1000000,2000000,10000000,0},
	},
	{
		640,
		360,
		{666666, 10000000, 50000000, 0},
	},
	{
		1280,
		720,
		{50000000, 0},
	},
	{
		0,
		0,
		{
			0,
		},
	},
};

static const struct uvc_frame_info uvc_frames_mjpeg[] = {
	{
		640,
		360,
		{666666, 10000000, 50000000, 0},
	},
	{
		1280,
		720,
		{50000000, 0},
	},
	{
		0,
		0,
		{
			0,
		},
	},
};

static const struct uvc_format_info uvc_formats[] = {
	{V4L2_PIX_FMT_YUYV, uvc_frames_yuyv},
	/*{V4L2_PIX_FMT_MJPEG, uvc_frames_mjpeg}, */
};

/* ---------------------------------------------------------------------------
 * V4L2 and UVC device instances
 */

/* Represents a V4L2 based video capture device */
struct v4l2_device {
	/* v4l2 device specific */
	int v4l2_fd;
	int is_streaming;
	char *v4l2_devname;

	/* v4l2 buffer specific */
	enum io_method io;
	struct buffer *mem;
	unsigned int nbufs;

	/* v4l2 buffer queue and dequeue counters */
	unsigned long long int qbuf_count;
	unsigned long long int dqbuf_count;

	/* uvc device hook */
	struct uvc_device *udev;
};

/* Represents a UVC based video output device */
struct uvc_device {
	/* uvc device specific */
	int uvc_fd;
	int is_streaming;
	int run_standalone;
	char *uvc_devname;

	/* uvc control request specific */

	struct uvc_streaming_control probe;
	struct uvc_streaming_control commit;
	int control;
	struct uvc_request_data request_error_code;
	unsigned int brightness_val;
	int pu_bmcontrol_val;
	uint8_t descriptor_type;

	/* uvc buffer specific */
	enum io_method io;
	struct buffer *mem;
	struct buffer *dummy_buf;
	unsigned int nbufs;
	unsigned int fcc;
	unsigned int width;
	unsigned int height;

	unsigned int bulk;
	uint8_t color;
	unsigned int imgsize;
	void *imgdata;

	/* USB speed specific */
	int mult;
	int burst;
	int maxpkt;
	enum usb_device_speed speed;

	/* uvc specific flags */
	int first_buffer_queued;
	int uvc_shutdown_requested;

	/* uvc buffer queue and dequeue counters */
	unsigned long long int qbuf_count;
	unsigned long long int dqbuf_count;

	/* v4l2 device hook */
	//struct v4l2_device *vdev;
//	int v4l2_rgbfd;
//	int v4l2_depthfd;
	void *fdata;
};

/* ---------------------------------------------------------------------------
 * UVC generic stuff
 */

static int uvc_video_set_format(struct uvc_device *dev)
{
	struct v4l2_format fmt;
	int ret;

	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width = dev->width;
	fmt.fmt.pix.height = dev->height;
	fmt.fmt.pix.pixelformat = dev->fcc;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	if (dev->fcc == V4L2_PIX_FMT_MJPEG)
		fmt.fmt.pix.sizeimage = dev->imgsize * 1.5;

	ret = ioctl(dev->uvc_fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		DBGERROR("UVC: Unable to set format %s (%d).\n", strerror(errno), errno);
		return ret;
	}

	DBGINFO("UVC: Setting format to: %c%c%c%c %ux%u\n", pixfmtstr(dev->fcc), dev->width, dev->height);

	return 0;
}


static int uvc_video_stream(struct uvc_device *dev, int enable)
{
	int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	int ret;

	if (!enable) {
		ret = ioctl(dev->uvc_fd, VIDIOC_STREAMOFF, &type);
		if (ret < 0) {
			DBGERROR("UVC: VIDIOC_STREAMOFF failed: %s (%d).\n", strerror(errno), errno);
			return ret;
		}

		DBGINFO("UVC: Stopping video stream.\n");

		return 0;
	}

	ret = ioctl(dev->uvc_fd, VIDIOC_STREAMON, &type);
	if (ret < 0) {
		DBGERROR("UVC: Unable to start streaming %s (%d).\n", strerror(errno), errno);
		return ret;
	}

	DBGINFO("UVC: Starting video stream.\n");

	dev->uvc_shutdown_requested = 0;

	return 0;
}

static int uvc_uninit_device(struct uvc_device *dev)
{
	unsigned int i;
	int ret;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		for (i = 0; i < dev->nbufs; ++i) {
			ret = munmap(dev->mem[i].start, dev->mem[i].length);
			if (ret < 0) {
				DBGERROR("UVC: munmap failed\n");
				return ret;
			}
		}

		free(dev->mem);
		break;

	case IO_METHOD_USERPTR:
	default:
		if (dev->run_standalone) {
			for (i = 0; i < dev->nbufs; ++i)
				free(dev->dummy_buf[i].start);

			free(dev->dummy_buf);
		}
		break;
	}

	return 0;
}
static int uvc_open(struct uvc_device **uvc, char *devname)
{
	struct uvc_device *dev;
	struct v4l2_capability cap;
	int fd;
	int ret = -EINVAL;

	fd = open(devname, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		DBGERROR("UVC: device open failed: %s (%d).\n", strerror(errno), errno);
		return ret;
	}

	ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		DBGERROR("UVC: unable to query uvc device: %s (%d)\n", strerror(errno), errno);
		goto err;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
		printf("UVC: %s is no video output device\n", devname);
		goto err;
	}

	dev = calloc(1, sizeof *dev);
	if (dev == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	DBGINFO("uvc device is %s on bus %s\n", cap.card, cap.bus_info);
	DBGVERBOSE("uvc open succeeded, file descriptor = %d\n", fd);

	dev->uvc_fd = fd;
	*uvc = dev;

	return 0;

err:
	close(fd);
	return ret;
}

static void uvc_close(struct uvc_device *dev)
{
	close(dev->uvc_fd);
	free(dev->imgdata);
	free(dev);
}

/* ---------------------------------------------------------------------------
 * UVC streaming related
 */

static void uvc_video_fill_buffer(struct uvc_device *dev, struct v4l2_buffer *buf)
{
	unsigned int bpl;
	unsigned int i;

	switch (dev->fcc) {
	case V4L2_PIX_FMT_YUYV:
		/* Fill the buffer with video data. */
		if (fill_buffer_handler != NULL) {
			/* data mode should be assigned correctly */
			fill_buffer_handler(dev->fdata, 0 /* DATA_MODE */, dev->mem[buf->index].start, dev->imgsize);
		}
		buf->bytesused = dev->imgsize;
		DBGVERBOSE("bytesused=%d\n", buf->bytesused);
		break;

	case V4L2_PIX_FMT_MJPEG:
		memcpy(dev->mem[buf->index].start, dev->imgdata, dev->imgsize);
		buf->bytesused = dev->imgsize;
		break;
	}
}

static int uvc_video_process(struct uvc_device *dev)
{
	struct v4l2_buffer ubuf;
	struct v4l2_buffer vbuf;
	unsigned int i;
	int ret;

	/*
	 * Return immediately if UVC video output device has not started
	 * streaming yet.
	 */
	if (!dev->is_streaming)
		return 0;
	/* Prepare a v4l2 buffer to be dequeued from UVC domain. */
	CLEAR(ubuf);

	ubuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	switch (dev->io) {
	case IO_METHOD_MMAP:
		ubuf.memory = V4L2_MEMORY_MMAP;
		break;

	case IO_METHOD_USERPTR:
	default:
		ubuf.memory = V4L2_MEMORY_USERPTR;
		break;
	}
	if (dev->run_standalone) {
		/* UVC stanalone setup. */
		ret = ioctl(dev->uvc_fd, VIDIOC_DQBUF, &ubuf);
		if (ret < 0){
			printf("======================VIDIOC_DQBUF error\n");
			return ret;
		}

		dev->dqbuf_count++;
#ifdef ENABLE_BUFFER_DEBUG
		DBGINFO("DeQueued buffer at UVC side = %d\n", ubuf.index);
#endif
		uvc_video_fill_buffer(dev, &ubuf);

		ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &ubuf);
		if (ret < 0){
			printf("=======================VIDIOC_QBUF error\n");
			return ret;
		}
		dev->qbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
		DBGINFO("ReQueueing buffer at UVC side = %d\n", ubuf.index);
#endif
	}

	return 0;
}

static int uvc_video_qbuf_mmap(struct uvc_device *dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < dev->nbufs; ++i) {
		memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

		dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
		dev->mem[i].buf.index = i;

		/* UVC standalone setup. */
		if (dev->run_standalone)
			uvc_video_fill_buffer(dev, &(dev->mem[i].buf));

		ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &(dev->mem[i].buf));
		if (ret < 0) {
			DBGERROR("UVC: VIDIOC_QBUF failed : %s (%d).\n", strerror(errno), errno);
			return ret;
		}

		dev->qbuf_count++;
	}

	return 0;
}

static int uvc_video_qbuf_userptr(struct uvc_device *dev)
{
	unsigned int i;
	int ret;

	/* UVC standalone setup. */
	if (dev->run_standalone) {
		for (i = 0; i < dev->nbufs; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.m.userptr = (unsigned long)dev->dummy_buf[i].start;
			buf.length = dev->dummy_buf[i].length;
			buf.index = i;
			buf.bytesused = dev->dummy_buf[i].length;

			ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &buf);
			if (ret < 0) {
				DBGERROR("UVC: VIDIOC_QBUF failed : %s (%d).\n", strerror(errno), errno);
				return ret;
			}

			dev->qbuf_count++;
		}
	}

	return 0;
}

static int uvc_video_qbuf(struct uvc_device *dev)
{
	int ret = 0;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		ret = uvc_video_qbuf_mmap(dev);
		break;

	case IO_METHOD_USERPTR:
		ret = uvc_video_qbuf_userptr(dev);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int uvc_video_reqbufs_mmap(struct uvc_device *dev, int nbufs)
{
	struct v4l2_requestbuffers rb;
	unsigned int i;
	int ret;

	CLEAR(rb);

	rb.count = nbufs;
	rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	rb.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(dev->uvc_fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0) {
		if (ret == -EINVAL) {
			DBGERROR("UVC: does not support memory mapping\n");
		} else {
			DBGERROR("UVC: Unable to allocate buffers: %s (%d).\n", strerror(errno), errno);
		}
		
		goto err;
	}

	if (!rb.count)
		return 0;

	if (rb.count < 2) {
		DBGINFO("UVC: Insufficient buffer memory.\n");
		ret = -EINVAL;
		goto err;
	}

	/* Map the buffers. */
	dev->mem = calloc(rb.count, sizeof dev->mem[0]);
	if (!dev->mem) {
		DBGERROR("UVC: Out of memory\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < rb.count; ++i) {
		memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

		dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
		dev->mem[i].buf.index = i;

		ret = ioctl(dev->uvc_fd, VIDIOC_QUERYBUF, &(dev->mem[i].buf));
		if (ret < 0) {
			DBGERROR(
				"UVC: VIDIOC_QUERYBUF failed for buf %d: "
				"%s (%d).\n",
				i, strerror(errno), errno);
			ret = -EINVAL;
			goto err_free;
		}
		dev->mem[i].start =
			mmap(NULL /* start anywhere */, dev->mem[i].buf.length, PROT_READ | PROT_WRITE /* required */,
				 MAP_SHARED /* recommended */, dev->uvc_fd, dev->mem[i].buf.m.offset);

		if (MAP_FAILED == dev->mem[i].start) {
			DBGERROR("UVC: Unable to map buffer %u: %s (%d).\n", i, strerror(errno), errno);
			dev->mem[i].length = 0;
			ret = -EINVAL;
			goto err_free;
		}

		dev->mem[i].length = dev->mem[i].buf.length;
		DBGVERBOSE("UVC: Buffer %u mapped at address %p.\n", i, dev->mem[i].start);
	}

	dev->nbufs = rb.count;
	DBGVERBOSE("UVC: %u buffers allocated.\n", rb.count);

	return 0;

err_free:
	free(dev->mem);
err:
	return ret;
}

static int uvc_video_reqbufs_userptr(struct uvc_device *dev, int nbufs)
{
	struct v4l2_requestbuffers rb;
	unsigned int i, j, bpl, payload_size;
	int ret;

	CLEAR(rb);

	rb.count = nbufs;
	rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	rb.memory = V4L2_MEMORY_USERPTR;

	ret = ioctl(dev->uvc_fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0) {
		if (ret == -EINVAL) {
			DBGERROR("UVC: does not support user pointer i/o\n");
		} else {
			DBGERROR("UVC: VIDIOC_REQBUFS error %s (%d).\n", strerror(errno), errno);
		}
		goto err;
	}

	if (!rb.count)
		return 0;

	dev->nbufs = rb.count;
	DBGVERBOSE("UVC: %u buffers allocated.\n", rb.count);

	if (dev->run_standalone) {
		/* Allocate buffers to hold dummy data pattern. */
		dev->dummy_buf = calloc(rb.count, sizeof dev->dummy_buf[0]);
		if (!dev->dummy_buf) {
			DBGERROR("UVC: Out of memory\n");
			ret = -ENOMEM;
			goto err;
		}

		switch (dev->fcc) {
		case V4L2_PIX_FMT_YUYV:
			bpl = dev->width * 2;
			payload_size = dev->width * dev->height * 2;
			break;
		case V4L2_PIX_FMT_MJPEG:
			payload_size = dev->imgsize;
			break;
		}

		for (i = 0; i < rb.count; ++i) {
			dev->dummy_buf[i].length = payload_size;
			dev->dummy_buf[i].start = malloc(payload_size);
			if (!dev->dummy_buf[i].start) {
				DBGERROR("UVC: Out of memory\n");
				ret = -ENOMEM;
				goto err;
			}

			if (V4L2_PIX_FMT_YUYV == dev->fcc)
				for (j = 0; j < dev->height; ++j)
					memset(dev->dummy_buf[i].start + j * bpl, dev->color++, bpl);

			if (V4L2_PIX_FMT_MJPEG == dev->fcc)
				memcpy(dev->dummy_buf[i].start, dev->imgdata, dev->imgsize);
		}

		dev->mem = dev->dummy_buf;
	}

	return 0;

err:
	return ret;
}

static int uvc_video_reqbufs(struct uvc_device *dev, int nbufs)
{
	int ret = 0;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		printf("IO_METHOD_MMAP nbufs %d\n", nbufs);
		ret = uvc_video_reqbufs_mmap(dev, nbufs);
		break;

	case IO_METHOD_USERPTR:
		printf("IO_METHOD_USERPTR nbufs %d\n", nbufs);
		ret = uvc_video_reqbufs_userptr(dev, nbufs);
		break;

	default:
		printf("IO_METHOD_DEFAULT\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * This function is called in response to either:
 * 	- A SET_ALT(interface 1, alt setting 1) command from USB host,
 * 	  if the UVC gadget supports an ISOCHRONOUS video streaming endpoint
 * 	  or,
 *
 *	- A UVC_VS_COMMIT_CONTROL command from USB host, if the UVC gadget
 *	  supports a BULK type video streaming endpoint.
 */
static int uvc_handle_streamon_event(struct uvc_device *dev)
{
	int ret;
	printf("%s enter\n" , __func__);
	printf("============================dev->is_streaming %d\n", dev->is_streaming);

	ret = uvc_video_reqbufs(dev, dev->nbufs);
	if (ret < 0)
		goto err;

	/* Common setup. */

	/* Queue buffers to UVC domain and start streaming. */
	ret = uvc_video_qbuf(dev);
	if (ret < 0)
		goto err;

	if (dev->run_standalone) {
		uvc_video_stream(dev, 1);
		dev->first_buffer_queued = 1;
		dev->is_streaming = 1;
	}

	return 0;

err:
	return ret;
}

/* ---------------------------------------------------------------------------
 * UVC Request processing
 */

static void
uvc_fill_streaming_control(struct uvc_device *dev, struct uvc_streaming_control *ctrl, int iframe, int iformat)
{
	const struct uvc_format_info *format;
	const struct uvc_frame_info *frame;
	unsigned int nframes;

	if (iformat < 0)
		iformat = ARRAY_SIZE(uvc_formats) + iformat;
	if (iformat < 0 || iformat >= (int)ARRAY_SIZE(uvc_formats))
		return;
	format = &uvc_formats[iformat];

	nframes = 0;
	while (format->frames[nframes].width != 0)
		++nframes;

	if (iframe < 0)
		iframe = nframes + iframe;
	if (iframe < 0 || iframe >= (int)nframes)
		return;
	frame = &format->frames[iframe];

	memset(ctrl, 0, sizeof *ctrl);

	ctrl->bmHint = 1;
	ctrl->bFormatIndex = iformat + 1;
	ctrl->bFrameIndex = iframe + 1;
	ctrl->dwFrameInterval = frame->intervals[0];
	switch (format->fcc) {
	case V4L2_PIX_FMT_YUYV:
		ctrl->dwMaxVideoFrameSize = frame->width * frame->height * 2;
		break;
	case V4L2_PIX_FMT_MJPEG:
		ctrl->dwMaxVideoFrameSize = dev->imgsize;
		break;
	}

	/* TODO: the UVC maxpayload transfer size should be filled
	 * by the driver.
	 */
	if (!dev->bulk)
		ctrl->dwMaxPayloadTransferSize = (dev->maxpkt) * (dev->mult + 1) * (dev->burst + 1);
	else
		ctrl->dwMaxPayloadTransferSize = ctrl->dwMaxVideoFrameSize;

	ctrl->bmFramingInfo = 3;
	ctrl->bPreferedVersion = 1;
	ctrl->bMaxVersion = 1;
}

static void
uvc_events_process_standard(struct uvc_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
	printf("standard request\n");
	(void)dev;
	(void)ctrl;
	(void)resp;
}

static void uvc_events_process_control(
	struct uvc_device *dev, uint8_t req, uint8_t cs, uint8_t entity_id, uint8_t len, struct uvc_request_data *resp)
{
	dev->pu_bmcontrol_val = cs;
	dev->descriptor_type = entity_id;
	switch (entity_id) {
	case 0:
		switch (cs) {
		case UVC_VC_REQUEST_ERROR_CODE_CONTROL:
			/* Send the request error code last prepared. */
			resp->data[0] = dev->request_error_code.data[0];
			resp->length = dev->request_error_code.length;
			break;

		default:
			/*
			 * If we were not supposed to handle this
			 * 'cs', prepare an error code response.
			 */
			dev->request_error_code.data[0] = 0x06;
			dev->request_error_code.length = 1;
			break;
		}
		break;

	/* Camera terminal unit 'UVC_VC_INPUT_TERMINAL'. */
	case 1:
		switch (cs) {
		/*
		 * We support only 'UVC_CT_AE_MODE_CONTROL' for CAMERA
		 * terminal, as our bmControls[0] = 2 for CT. Also we
		 * support only auto exposure.
		 */
		case UVC_CT_AE_MODE_CONTROL:
			switch (req) {
			case UVC_SET_CUR:
				/* Incase of auto exposure, attempts to
				 * programmatically set the auto-adjusted
				 * controls are ignored.
				 */
				resp->data[0] = 0x01;
				resp->length = 1;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;

			case UVC_GET_INFO:
				/*
				 * TODO: We support Set and Get requests, but
				 * don't support async updates on an video
				 * status (interrupt) endpoint as of
				 * now.
				 */
				resp->data[0] = 0x03;
				resp->length = 1;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;

			case UVC_GET_CUR:
			case UVC_GET_DEF:
			case UVC_GET_RES:
				// Auto Mode auto Exposure Time, auto Iris. 
				resp->data[0] = 0x02;
				resp->length = 1;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			default:
				/*
				 * We don't support this control, so STALL the
				 * control ep.
				 */
				resp->length = -EL2HLT;
				/*
				 * For every unsupported control request
				 * set the request error code to appropriate
				 * value.
				 */
				dev->request_error_code.data[0] = 0x07;
				dev->request_error_code.length = 1;
				break;
			}
			break;

		default:
			/*
			 * We don't support this control, so STALL the control
			 * ep.
			 */
			resp->length = -EL2HLT;
			/*
			 * If we were not supposed to handle this
			 * 'cs', prepare a Request Error Code response.
			 */
			dev->request_error_code.data[0] = 0x06;
			dev->request_error_code.length = 1;
			break;
		}
		break;

	/* processing unit 'UVC_VC_PROCESSING_UNIT' */
	case 2:
		switch (cs) {
		/*
		 * We support only 'UVC_PU_BRIGHTNESS_CONTROL' for Processing
		 * Unit, as our bmControls[0] = 1 for PU.
		 */
		case UVC_PU_BRIGHTNESS_CONTROL:
			switch (req) {
			case UVC_SET_CUR:
				resp->data[0] = 0x0;
				resp->length = len;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_MIN:
				resp->data[0] = PU_BRIGHTNESS_MIN_VAL;
				resp->length = 2;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_MAX:
				resp->data[0] = PU_BRIGHTNESS_MAX_VAL;
				resp->length = 2;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_CUR:
				resp->length = 2;
				memcpy(&resp->data[0], &dev->brightness_val, resp->length);
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_INFO:
				/*
				 * We support Set and Get requests and don't
				 * support async updates on an interrupt endpt
				 */
				resp->data[0] = 0x03;
				resp->length = 1;
				/*
				 * For every successfully handled control
				 * request, set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_DEF:
				resp->data[0] = PU_BRIGHTNESS_DEFAULT_VAL;
				resp->length = 2;
				/*
				 * For every successfully handled control
				 * request, set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_RES:
				resp->data[0] = PU_BRIGHTNESS_STEP_SIZE;
				resp->length = 2;
				/*
				 * For every successfully handled control
				 * request, set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			default:
				/*
				 * We don't support this control, so STALL the
				 * default control ep.
				 */
				resp->length = -EL2HLT;
				/*
				 * For every unsupported control request
				 * set the request error code to appropriate
				 * code.
				 */
				dev->request_error_code.data[0] = 0x07;
				dev->request_error_code.length = 1;
				break;
			}
			break;
		case UVC_PU_CONTRAST_CONTROL:
			switch (req) {
			case UVC_SET_CUR:
				resp->data[0] = 0x0;
				resp->length = len;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_MIN:
				resp->data[0] = PU_BRIGHTNESS_MIN_VAL;
				resp->length = 2;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_MAX:
				resp->data[0] = PU_BRIGHTNESS_MAX_VAL;
				resp->length = 2;
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_CUR:
				resp->length = 2;
				memcpy(&resp->data[0], &dev->brightness_val, resp->length);
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_INFO:
				/*
				 * We support Set and Get requests and don't
				 * support async updates on an interrupt endpt
				 */
				resp->data[0] = 0x03;
				resp->length = 1;
				/*
				 * For every successfully handled control
				 * request, set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_DEF:
				resp->data[0] = PU_BRIGHTNESS_DEFAULT_VAL;
				resp->length = 2;
				/*
				 * For every successfully handled control
				 * request, set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			case UVC_GET_RES:
				resp->data[0] = PU_BRIGHTNESS_STEP_SIZE;
				resp->length = 2;
				/*
				 * For every successfully handled control
				 * request, set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
				break;
			default:
				/*
				 * We don't support this control, so STALL the
				 * default control ep.
				 */
				resp->length = -EL2HLT;
				/*
				 * For every unsupported control request
				 * set the request error code to appropriate
				 * code.
				 */
				dev->request_error_code.data[0] = 0x07;
				dev->request_error_code.length = 1;
				break;
			}
			break;
				
		default:
			/*
			 * We don't support this control, so STALL the control
			 * ep.
			 */
			resp->length = -EL2HLT;
			/*
			 * If we were not supposed to handle this
			 * 'cs', prepare a Request Error Code response.
			 */
			dev->request_error_code.data[0] = 0x06;
			dev->request_error_code.length = 1;
			break;
		}

		break;

	default:
		/*
		 * If we were not supposed to handle this
		 * 'cs', prepare a Request Error Code response.
		 */
		dev->request_error_code.data[0] = 0x06;
		dev->request_error_code.length = 1;
		break;
	}

	DBGVERBOSE("control request (req %02x cs %02x)\n", req, cs);
}

static void uvc_events_process_streaming(struct uvc_device *dev, uint8_t req, uint8_t cs, struct uvc_request_data *resp)
{
	struct uvc_streaming_control *ctrl;

	DBGVERBOSE("streaming request (req %02x cs %02x)\n", req, cs);

	if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
		return;

	ctrl = (struct uvc_streaming_control *)&resp->data;
	resp->length = sizeof *ctrl;

	switch (req) {
	case UVC_SET_CUR:
		dev->control = cs;
		resp->length = 34;
		break;

	case UVC_GET_CUR:
		if (cs == UVC_VS_PROBE_CONTROL)
			memcpy(ctrl, &dev->probe, sizeof *ctrl);
		else
			memcpy(ctrl, &dev->commit, sizeof *ctrl);
		break;

	case UVC_GET_MIN:
	case UVC_GET_MAX:
	case UVC_GET_DEF:
		uvc_fill_streaming_control(dev, ctrl, req == UVC_GET_MAX ? -1 : 0, req == UVC_GET_MAX ? -1 : 0);
		break;

	case UVC_GET_RES:
		CLEAR(ctrl);
		break;

	case UVC_GET_LEN:
		resp->data[0] = 0x00;
		resp->data[1] = 0x22;
		resp->length = 2;
		break;

	case UVC_GET_INFO:
		resp->data[0] = 0x03;
		resp->length = 1;
		break;
	}
}

static void
uvc_events_process_class(struct uvc_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
	if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
		return;

	switch (ctrl->wIndex & 0xff) {
	case UVC_INTF_CONTROL:
//		printf("UVC_INTF_CONTROL\n");
		uvc_events_process_control(dev, ctrl->bRequest, ctrl->wValue >> 8, ctrl->wIndex >> 8, ctrl->wLength, resp);
		break;

	case UVC_INTF_STREAMING:
//		printf("UVC_INTF_STREAMING\n");
		uvc_events_process_streaming(dev, ctrl->bRequest, ctrl->wValue >> 8, resp);
		break;

	default:
		break;
	}
}
static void
uvc_events_process_setup(struct uvc_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
	dev->control = 0;

#ifdef ENABLE_USB_REQUEST_DEBUG
	DBGINFO(
		"\nbRequestType %02x bRequest %02x wValue %04x wIndex %04x "
		"wLength %04x\n",
		ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wIndex, ctrl->wLength);
#endif
	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
//		printf("USB_TYPE_STANDARD\n");
		uvc_events_process_standard(dev, ctrl, resp);
		break;

	case USB_TYPE_CLASS:
//		printf("USB_TYPE_CLASS\n");
		uvc_events_process_class(dev, ctrl, resp);
		break;

	default:
		break;
	}
}

static int
uvc_events_process_control_data(struct uvc_device *dev, int cs, uint8_t entity_id, struct uvc_request_data *data)
{
	int fd = get_fd(0);
	unsigned int val;
	memcpy(&val, data->data, data->length);
	switch (entity_id) {
	/* Processing unit 'UVC_VC_PROCESSING_UNIT'. */
	case 2:
		switch (cs) {
		/*
		 * We support only 'UVC_PU_BRIGHTNESS_CONTROL' for Processing
		 * Unit, as our bmControls[0] = 1 for PU.
		 */
		case UVC_PU_BRIGHTNESS_CONTROL:
			memcpy(&dev->brightness_val, data->data, data->length);		
			printf("UVC_PU_BRIGHTNESS_CONTROL fd %d\n", fd);
			if(fd >= 0)
				//v4l2_set_ctrl(dev->v4l2_rgbfd, dev->brightness_val, V4L2_CID_BRIGHTNESS);
				vidioc_set_ctrl(0, V4L2_CID_BRIGHTNESS, dev->brightness_val);
			break;
		case UVC_PU_CONTRAST_CONTROL:
			printf("UVC_PU_CONTRAST_CONTROL fd %d\n", fd);
			if(fd >=0)
				vidioc_set_ctrl(0, V4L2_CID_CONTRAST, val);
			break;
		case UVC_PU_GAIN_CONTROL:
			
			break;
		case UVC_PU_HUE_CONTROL	:
			
			break;
		case UVC_PU_SATURATION_CONTROL:
			
			break;
		case UVC_PU_SHARPNESS_CONTROL:
			
			break;
		case UVC_PU_GAMMA_CONTROL:
			
			break;
		case UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL:
			
			break;	
			
		default:
			break;
		}

		break;

	default:
		break;
	}

	DBGVERBOSE("Control Request data phase (cs %02x entity %02x)\n", cs, entity_id);

	return 0;
}

static int uvc_events_process_data(struct uvc_device *dev, struct uvc_request_data *data)
{
	struct uvc_streaming_control *target;
	struct uvc_streaming_control *ctrl;
	struct v4l2_format fmt;
	const struct uvc_format_info *format;
	const struct uvc_frame_info *frame;
	const unsigned int *interval;
	unsigned int iformat, iframe;
	unsigned int nframes;
	unsigned int *val = (unsigned int *)data->data;
	int ret;

	switch (dev->control) {
	case UVC_VS_PROBE_CONTROL:
		DBGVERBOSE("setting probe control, length = %d\n", data->length);
		target = &dev->probe;
		break;

	case UVC_VS_COMMIT_CONTROL:
		DBGVERBOSE("setting commit control, length = %d\n", data->length);
		target = &dev->commit;
		break;

	default:
		DBGVERBOSE("setting unknown control, length = %d\n", data->length);

		/*
		 * As we support only BRIGHTNESS control, this request is
		 * for setting BRIGHTNESS control.
		 * Check for any invalid SET_CUR(BRIGHTNESS) requests
		 * from Host. Note that we support Brightness levels
		 * from 0x0 to 0x10 in a step of 0x1. So, any request
		 * with value greater than 0x10 is invalid.
		 */
		if (*val > PU_BRIGHTNESS_MAX_VAL) {
			return -EINVAL;
		} else {
			printf("uvc_events_process_data UVC_PU_BRIGHTNESS_CONTROL\n");
			ret = uvc_events_process_control_data(dev, dev->pu_bmcontrol_val, 2, data);
		
			//ret = uvc_events_process_control_data(dev, UVC_PU_CONTRAST_CONTROL, 2, data);

			if (ret < 0)
				goto err;

			return 0;
		}
	}

	ctrl = (struct uvc_streaming_control *)&data->data;
	iformat = clamp((unsigned int)ctrl->bFormatIndex, 1U, (unsigned int)ARRAY_SIZE(uvc_formats));
	format = &uvc_formats[iformat - 1];

	nframes = 0;
	while (format->frames[nframes].width != 0)
		++nframes;

	iframe = clamp((unsigned int)ctrl->bFrameIndex, 1U, nframes);
	frame = &format->frames[iframe - 1];
	interval = frame->intervals;

	while (interval[0] < ctrl->dwFrameInterval && interval[1])
		++interval;

	target->bFormatIndex = iformat;
	target->bFrameIndex = iframe;
	switch (format->fcc) {
	case V4L2_PIX_FMT_YUYV:
		target->dwMaxVideoFrameSize = frame->width * frame->height * 2;
		break;
	case V4L2_PIX_FMT_MJPEG:
		if (dev->imgsize == 0)
			DBGPRINT("WARNING: MJPEG requested and no image loaded.\n");
		target->dwMaxVideoFrameSize = dev->imgsize;
		break;
	}
	target->dwFrameInterval = *interval;

	if (dev->control == UVC_VS_COMMIT_CONTROL) {
		dev->fcc = format->fcc;
		dev->width = frame->width;
		dev->height = frame->height;
		dev->width = 640;
		dev->height = 480;//550;//480;
		dev->imgsize = dev->width * dev->height * 2;
		DBGINFO("usb req W=%d H=%d F=%x\n", dev->width, dev->height, dev->fcc);
		uvc_video_set_format(dev);

		if (dev->bulk){
			printf("dev->is_streaming %d\n",dev->is_streaming);
		//	if(dev->is_streaming)
				uvc_video_stream(dev,0); // streamon after streamoff 
			printf("dev->nbufs %d\n", dev->nbufs);
			uvc_handle_streamon_event(dev);

		}
	}

	return 0;

err:
	return ret;
}

static void uvc_events_process(struct uvc_device *dev)
{
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
	struct uvc_request_data resp;
	int ret;
//	printf("%s enter\n", __func__);
	ret = ioctl(dev->uvc_fd, VIDIOC_DQEVENT, &v4l2_event);
	if (ret < 0) {
		DBGERROR("VIDIOC_DQEVENT failed: %s (%d)\n", strerror(errno), errno);
		return;
	}

	memset(&resp, 0, sizeof resp);
	resp.length = -EL2HLT;

	switch (v4l2_event.type) {
	case UVC_EVENT_CONNECT:
//		printf("UVC_EVENT_CONNECT\n");
		return;

	case UVC_EVENT_DISCONNECT:
//		printf("UVC_EVENT_DISCONNECT\n");		
		dev->uvc_shutdown_requested = 1;
		DBGINFO(
			"UVC: Possible USB shutdown requested from "
			"Host, seen via UVC_EVENT_DISCONNECT\n");
		return;

	case UVC_EVENT_SETUP:
//		printf("UVC_EVENT_SETUP\n");			
		uvc_events_process_setup(dev, &uvc_event->req, &resp);
		break;

	case UVC_EVENT_DATA:
//		printf("UVC_EVENT_DATA\n");
		ret = uvc_events_process_data(dev, &uvc_event->data);
		if (ret < 0)
			break;
		return;

	case UVC_EVENT_STREAMON:
//		printf("UVC_EVENT_STREAMON\n");
		if (!dev->bulk){
			printf("######UVC_EVENT_STREAMON !dev->bulk\n");
			uvc_handle_streamon_event(dev);
		}
		return;

	case UVC_EVENT_STREAMOFF:
//		printf("UVC_EVENT_STREAMOFF\n");
		/* Stop V4L2 streaming... */
		/* ... and now UVC streaming.. */
		if (dev->is_streaming) {
			uvc_video_stream(dev, 0);
			uvc_uninit_device(dev);
			uvc_video_reqbufs(dev, 0);
			dev->is_streaming = 0;
			dev->first_buffer_queued = 0;
		}

		return;
	}

	ret = ioctl(dev->uvc_fd, UVCIOC_SEND_RESPONSE, &resp);
	if (ret < 0) {
		printf("UVCIOC_S_EVENT failed: %s (%d)\n", strerror(errno), errno);
		return;
	}
}

static void uvc_events_init(struct uvc_device *dev)
{
	struct v4l2_event_subscription sub;
	unsigned int payload_size;

	switch (dev->fcc) {
	case V4L2_PIX_FMT_YUYV:
		payload_size = dev->width * dev->height * 2;
		break;
	case V4L2_PIX_FMT_MJPEG:
		payload_size = dev->imgsize;
		break;
	}

	uvc_fill_streaming_control(dev, &dev->probe, 0, 0);
	uvc_fill_streaming_control(dev, &dev->commit, 0, 0);

	if (dev->bulk) {
		/* FIXME Crude hack, must be negotiated with the driver. */
		dev->probe.dwMaxPayloadTransferSize = dev->commit.dwMaxPayloadTransferSize = payload_size;
	}

	memset(&sub, 0, sizeof sub);
	sub.type = UVC_EVENT_SETUP;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_DATA;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_STREAMON;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_STREAMOFF;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
}

/* PUBLIC APIS for capis */

/**
 *  @brief  uvc gadget device open, initialize and querries.
 *  @param[in] name  device name for uvc gadget.
 *  @return zero for success, none zero for error.
 *  @see    init_uvc_gadget, close_uvc_gadget
*/

int open_uvc_gadget_device(char *name)
{
	return uvc_open(&device, name);
}

/**
 *  @brief  uvc gadget device open, initialize and querries.
 *  @param[in]  fill_buf_func     Callback function for Fill buffer.
 *  @param[in]  release_buf_func  Callback function for release buffer.
 *  @return none zero for error
 *  @see    open_uvc_gadget_device, close_uvc_gadget
*/
int init_uvc_gadget_device(void *fdata, UVC_BUFFER_FILL_FUNC fill_buf_func, UVC_BUFFER_RELEASE_FUNC release_buf_func)
{
	device->width = 640;
	device->height = 550;
	device->fcc = V4L2_PIX_FMT_YUYV;
	device->io = IO_METHOD_USERPTR;
	device->bulk = 1; /* currently not supported. */
	device->nbufs = 2; /* currently only two buffers would be emough */
	device->mult = 0;
	device->burst = 0;
	device->speed = USB_SPEED_SUPER;
	device->is_streaming = 0;
	device->run_standalone = 1;
	device->imgsize = device->width * device->height * 2;
	device->fdata = fdata;
//	device->v4l2_rgbfd = fdata->rgb_fd;
//	device->v4l2_depthfd = fdata->depth_fd;
	
	switch (device->speed) {
	case USB_SPEED_FULL:
		/* Full Speed. */
		if (device->bulk)
			device->maxpkt = 64;
		else
			device->maxpkt = 1023;
	break;

	case USB_SPEED_HIGH:
		/* High Speed. */
		if (device->bulk)
			device->maxpkt = 512;
		else
			device->maxpkt = 1024;
	break;

	case USB_SPEED_SUPER:
	default:
	/* Super Speed. */
		if (device->bulk)
			device->maxpkt = 1024;
		else
			device->maxpkt = 1024;
		break;
	}



	fill_buffer_handler = fill_buf_func;
	buffer_release_handler = release_buf_func;

	/* We are using USERPTR */
	device->io = IO_METHOD_USERPTR;

	uvc_events_init(device);
	/* v4l2 process */
	uvc_video_set_format(device);
	uvc_video_reqbufs(device, 2); /* two buffers are enough? */
	/* Now event process should handle qbuf */

	return 0;
}

/**
 *  @brief  uvc gadget internal process function to service usb events.
 *  @param[in]  useconds  micro seconds to waiting selection
 *  @return none zero for error
 *  @see    open_uvc_gadget_device, close_uvc_gadget
 *          user should call this function as regular basis to not missing usb ecent.
*/
int process_uvc_gadget_device(int useconds)
{
	struct timeval tv;
	fd_set fds_snd, fds_rcv, fds_ext;
	int nfds, ret;

	FD_ZERO(&fds_rcv);

	FD_SET(device->uvc_fd, &fds_rcv);
	fds_snd = fds_rcv;
	fds_ext = fds_rcv;

	/* Timeout. */
	if (useconds != 0) {
		if (useconds > 1000000) {
			tv.tv_sec = useconds / 1000000;
			tv.tv_usec = useconds - tv.tv_sec * 1000000;
		} else {
			tv.tv_sec = 0;
			tv.tv_usec = useconds;
		}
	} else {
		tv.tv_sec = 2;
		tv.tv_usec = 0;
	}
	
	nfds = device->uvc_fd + 1;

	ret = select(nfds, &fds_rcv, &fds_snd, &fds_ext, &tv);
	if (FD_ISSET(device->uvc_fd, &fds_ext))
		uvc_events_process(device);
	if (FD_ISSET(device->uvc_fd, &fds_snd))
		uvc_video_process(device);

	/* we don't have rcv message in this context */
	if(ret == 0){
		printf("select timeout\n");
	}
	return ret;
}

/**
 *  @brief  uvc gadget device close.
 *  @return none
 *  @see    open_uvc_gadget_device, init_uvc_gadget
*/
void close_uvc_gadget_device()
{
//	printf("close_uvc_gadget_device\n");
	if (device->is_streaming) {
//		printf("close\n");
        /* ... and now UVC streaming.. */
        uvc_video_stream(device, 0);
        uvc_uninit_device(device);
        uvc_video_reqbufs(device, 0);
        device->is_streaming = 0;
    }
	uvc_close(device);
}
