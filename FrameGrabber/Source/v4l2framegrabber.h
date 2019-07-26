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

#ifndef __V4L2FRAME_GRABBER_H__
#define __V4L2FRAME_GRABBER_H__

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <linux/videodev2.h>


struct buffer {
        void   *start;
        size_t  length;
};


class CameraFormat
{
public:
	CameraFormat();
	CameraFormat(std::string dev, __u8* crd, __u8* drv, __u32 pxlfmt, __u32 w, __u32 h, __u32 num, __u32 denom);
	~CameraFormat() {}

	void print();
	std::string to_string();

	std::string device;
	__u8 driver[16];
	__u8 card[32];
	__u32 pixelformat;
	__u32 width;
	__u32 height;
	__u32 numerator;
	__u32 denominator;
};


class Camera
{
public:
	Camera();
	Camera(int fmt_index);
	Camera(CameraFormat &fmt);
	Camera(std::string dev = std::string("/dev/video0"), __u32 w = 640, __u32 h = 480, __u32 numerator = 1, __u32 denominator = 30, __u32 pixelformat = V4L2_PIX_FMT_GREY);
	~Camera();

	int init();
	int start();
	bool is_running() { return has_started; }
	CameraFormat* get_format();

	cv::Mat read_frame();
	cv::Mat read_frame(bool timeout, int timeout_sec = 2, int timeout_usec = 0);

	static std::vector<CameraFormat> list_formats();
	static std::vector<std::string> list_formats_as_string();
	static int get_format_index(std::string s);
	static std::string get_format_string(int index);

private:
	struct buffer* buffers;
	int n_buffers;
	int fd;
	bool has_started;

	CameraFormat use_fmt;

	int set_caps();
	int init_mmap();
};

#endif

