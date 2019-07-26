/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2014 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "v4l2framegrabber.h"


#define CLEAR(x) memset(&(x), 0, sizeof(x))


/*
	Helper functions
*/

static int xioctl(int fd, int request, void *arg)
{
    int r;

    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);

    return r;
}


static std::string fourcc_to_string(__u32 pixelformat)
{
	std::string s;

	s += pixelformat & 0x7f;
	s += (pixelformat >> 8) & 0x7f;
	s += (pixelformat >> 16) & 0x7f;
	s += (pixelformat >> 24) & 0x7f;
	if (pixelformat & (1 << 31))
		s += "-BE";

	return s;
}


/*
	Camera format class
*/

CameraFormat::CameraFormat()
{
	device = std::string("");
	pixelformat = -1;
	width = -1;
	height = -1;
	numerator = -1;
	denominator = -1;
}


CameraFormat::CameraFormat(std::string dev, __u8* crd, __u8* drv, __u32 pxlfmt, __u32 w, __u32 h, __u32 num, __u32 denom)
{
	device = dev;
	for (int i=0; i< sizeof(driver); i++)
		driver[i] = drv[i];
	for (int i=0; i< sizeof(card); i++)
		card[i] = crd[i];

	pixelformat = pxlfmt;
	width = w;
	height = h;
	numerator = num;
	denominator = denom;
}


void CameraFormat::print()
{
	std::cout << "Device: " << device << "\n";
	std::cout << "Card: " << card << "\n";
	std::cout << "Driver: " << driver << "\n";
	std::cout << "Pixelformat: " << fourcc_to_string(pixelformat) << "\n";
	std::cout << "Width: " << width << "\n";
	std::cout << "Height: " << height << "\n";
	std::cout << "Framerate: " << numerator << "/" << denominator << "\n";
}


std::string CameraFormat::to_string()
{
	std::ostringstream oss;

	oss << device;
	oss << " " << card;
	oss << " (" << fourcc_to_string(pixelformat) << ")";
	oss << " " << width << "x" << height;
	oss << " " << numerator << "/" << denominator;

	return oss.str();
}


/*
	Camera class
*/

Camera::Camera()
{
	fd = -1;
	n_buffers = 0;
	has_started = false;
}


Camera::Camera(int fmt_index)
{
	std::vector<CameraFormat> formats = Camera::list_formats();
	use_fmt = CameraFormat(formats.at(fmt_index));
}


Camera::Camera(CameraFormat &fmt)
{
	use_fmt.device = fmt.device;
	use_fmt.width = fmt.width;
	use_fmt.height = fmt.height;
	use_fmt.numerator = fmt.numerator;
	use_fmt.denominator = fmt.denominator;
	use_fmt.pixelformat = fmt.pixelformat;
}


Camera::Camera(std::string dev, __u32 w, __u32 h, __u32 numerator, __u32 denominator, __u32 pixelfmt)
{
	use_fmt.device = dev;
	use_fmt.width = w;
	use_fmt.height = h;
	use_fmt.numerator = numerator;
	use_fmt.denominator = denominator;
	use_fmt.pixelformat = pixelfmt;
}


Camera::~Camera()
{
	if (n_buffers > 0)
	{
        for (int i=0; i<n_buffers; i++)
        	if (-1 == munmap(buffers[i].start, buffers[i].length))
			{
                std::cerr << "ERROR: munmap\n";
			}
	}
	if (fd > -1)
	{
		close(fd);
	}
}


int Camera::init()
{
    fd = open(use_fmt.device.c_str(), O_RDWR);
    if (fd == -1)
    {
		std::cerr << "ERROR: Could not open camera: " << use_fmt.device.c_str() << "\n";
        return 1;
    }

	if (set_caps())
	{
		std::cerr << "ERROR: Could not set camera capabilities" << "\n";
		return 1;
	}

    if(init_mmap())
	{
		std::cerr << "ERROR: Could not initialize mmap" << "\n";
		return 1;
	}

	return 0;
}


int Camera::start()
{
    enum v4l2_buf_type type;

    for (int i=0; i<n_buffers; i++) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		{
            std::cerr << "ERROR: VIDIOC_QBUF\n";
			return 1;
		}
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
	{
        std::cerr << "ERROR: VIDIOC_STREAMON\n";
	}

	has_started = true;

	return 0;
}


int Camera::set_caps()
{
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = use_fmt.width;
    fmt.fmt.pix.height = use_fmt.height;
    fmt.fmt.pix.pixelformat = use_fmt.pixelformat;  //V4L2_PIX_FMT_GREY;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
    {
        std::cerr << "ERROR: Could not set pixel format" << "\n";
        return 1;
    }

	struct v4l2_streamparm streamparm;
	memset(&streamparm, 0, sizeof(streamparm));
	streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_G_PARM, &streamparm) != 0)
	{
		std::cerr << "ERROR: Could not get streaming parameters" << "\n";
        return 1;
	}

	if (streamparm.parm.capture.capability == V4L2_CAP_TIMEPERFRAME)
	{
		streamparm.parm.capture.timeperframe.numerator = use_fmt.numerator;
		streamparm.parm.capture.timeperframe.denominator = use_fmt.denominator;
		if(xioctl(fd, VIDIOC_S_PARM, &streamparm) !=0)
		{
			std::cerr << "ERROR: Failed to set frame rate. Check valid frame rates using v4l2-ctl --list-formats-ext" << "\n";
			return 1;
		}
	}
	else
	{
		std::cerr << "ERROR: device does not support setting of frame rate" << "\n";
		return 1;
	}
}


int Camera::init_mmap()
{
    struct v4l2_requestbuffers req = {0};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
        std::cerr << "ERROR: Requesting Buffer\n";
        return 1;
    }

	n_buffers = req.count;
	buffers = (struct buffer*) calloc(n_buffers, sizeof(*buffers));

    if (!buffers) {
        std::cerr << "ERROR: Out of memory\n";
        return 1;
    }

	for (int i=0; i<n_buffers; i++) {

	    struct v4l2_buffer buf;

	    CLEAR(buf);

	    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	    buf.memory      = V4L2_MEMORY_MMAP;
	    buf.index       = i;

	    if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
		{
	    	std::cerr << "ERROR: VIDIOC_QUERYBUF\n";
			return 1;
		}

	    buffers[i].length = buf.length;
	    buffers[i].start =
	            mmap(NULL /* start anywhere */,
	                  buf.length,
	                  PROT_READ | PROT_WRITE /* required */,
	                  MAP_SHARED /* recommended */,
	                  fd, buf.m.offset);

	    if (MAP_FAILED == buffers[i].start)
		{
        	std::cerr << "ERROR: MAP_FAILED\n";
			return 1;
		}
	}

    return 0;
}


CameraFormat* Camera::get_format()
{
	return &use_fmt;
}


cv::Mat Camera::read_frame()
{
	return read_frame(false, 0, 0);
}

cv::Mat Camera::read_frame(bool timeout, int timeout_sec, int timeout_usec)
{
	cv::Mat mat;

	fd_set fds;
	int r;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (timeout)
	{
		/* use timeout */
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);
		if (0 == r) {
			std::cerr << "ERROR: select timeout\n";
			return mat;
		}
	}
	else
	{
		r = select(fd + 1, &fds, NULL, NULL, NULL);
	}

	if (-1 == r) {
		if (EINTR == errno)
    		std::cerr <<  "ERROR: select\n";
	}

	struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
            case EAGAIN:
            	return mat;

            case EIO:
                    /* Can ignore EIO, see specs */
				break;

            default:
                std::cerr << "ERROR: VIDIOC_DQBUF\n";
				return mat;
            }
    }

    assert(buf.index < n_buffers);
	assert(buffers[buf.index].start != NULL);

	if (use_fmt.pixelformat == V4L2_PIX_FMT_GREY)
	{
		mat = cv::Mat(use_fmt.height, use_fmt.width, CV_8UC1, buffers[buf.index].start);
	}
	else if (use_fmt.pixelformat == V4L2_PIX_FMT_MJPEG)
	{
		mat = cv::Mat(use_fmt.height, use_fmt.width, CV_8U);
    	imdecode(cv::Mat(1, use_fmt.width*use_fmt.height, CV_8U, (unsigned char*)buffers[buf.index].start),  cv::IMREAD_COLOR, &mat);

	}
	else if (use_fmt.pixelformat == V4L2_PIX_FMT_YUYV)
	{
		mat = cv::Mat(use_fmt.height, use_fmt.width, CV_8UC2, buffers[buf.index].start);
		cvtColor(mat, mat, cv::COLOR_YUV2BGR_YUY2);
	}
	else
	{
		std::cerr << "ERROR: unknown pixelformat (" << fourcc_to_string(use_fmt.pixelformat) << ") => add it to Camera::read_frame()!\n";
	}

	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
	{
    	std::cerr << "ERROR: camera read frame VIDIOC_QBUF\n";
		mat = cv::Mat();
		return mat;
	}

	return mat;
}


std::vector<CameraFormat> Camera::list_formats()
{
	int max_devices = 64;
	std::vector<CameraFormat> formats;

	for (int i=0; i<max_devices; i++)
	{
		char dev[50];
		sprintf(dev, "/dev/video%d", i);
		int fd = open(dev, O_RDWR);
		if (fd != -1)
		{

			struct v4l2_capability cap;
			CLEAR(cap);
			if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
			{
				std::cerr << "ERROR: VIDIOC_QUERYCAP\n";
				close(fd);
				break;
			}

			struct v4l2_fmtdesc fmt;
			CLEAR(fmt);
			fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			fmt.index = 0;
			while (1)
			{
				if (xioctl(fd, VIDIOC_ENUM_FMT, &fmt))
				{
					break;
				}

				struct v4l2_frmsizeenum frmsize;
				CLEAR(frmsize);
				frmsize.pixel_format= fmt.pixelformat;
				frmsize.index = 0;

				while (1)
				{
					if (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == -1)
					{
						break;
					}

					int width = 0;
					int height = 0;

					if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
					{
						width = frmsize.discrete.width;
						height = frmsize.discrete.height;
					}
					else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
					{
						std::cout << "V4L2_FRMSIZE_TYPE_STEPWISE not supported at the moment\n";
	//					std::cout << "      index: " << frmsize.index << "\n";
	//					std::cout << "      type: stepwise\n";
	//					std::cout << "      width: " << frmsize.stepwise.min_width << ":" << frmsize.stepwise.max_width << ":" << frmsize.stepwise.step_width << "\n";
	//					std::cout << "      height: " << frmsize.stepwise.min_height << ":" << frmsize.stepwise.max_height << ":" << frmsize.stepwise.step_height << "\n";
					}
					else
					{
						std::cout << "Unsupported type for VIDIOC_ENUM_FRAMESIZES\n";
	//					std::cout << "      index: " << frmsize.index << "\n";
	//					std::cout << "      type: continuous\n";
					}

					if (width > 0 && height > 0)
					{
						struct v4l2_frmivalenum frmival;
						CLEAR(frmival);
						frmival.index = 0;
						frmival.pixel_format = frmsize.pixel_format;
						frmival.width = width;
						frmival.height = height;

						while (1)
						{
							if (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == -1)
							{
								break;
							}

							int num = 0;
							int denom = 0;
							if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
							{
								num = frmival.discrete.numerator;
								denom = frmival.discrete.denominator;

								CameraFormat cfmt(std::string(dev), &cap.card[0], &cap.driver[0], frmsize.pixel_format, width, height, num, denom);
								formats.push_back(cfmt);
							}

							frmival.index++;
						}
					}

					frmsize.index++;
				}

				fmt.index++;
			}

			close(fd);
		}
	}

	return formats;
}


std::vector<std::string> Camera::list_formats_as_string()
{
	std::vector<std::string> strings;

	std::vector<CameraFormat> formats = Camera::list_formats();
	for (int i=0; i<formats.size(); i++)
	{
		strings.push_back(formats.at(i).to_string());
	}

	return strings;
}


int Camera::get_format_index(std::string fmt)
{
	std::vector<std::string> formats = list_formats_as_string();
	auto it = std::find(formats.begin(), formats.end(), fmt);
	if (it != formats.end())
	{
		return std::distance(formats.begin(), it);
	}
	else
	{
		return -1;
	}
}


std::string Camera::get_format_string(int index)
{
	std::vector<std::string> formats = list_formats_as_string();
	if (index < formats.size())
	{
		return formats.at(index);
	}
	else
	{
		std::string("");
	}
}


