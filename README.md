# FrameGrabber

A simple video4linux2-based video frame grabber plugin for the open-ephys 
[plugin-GUI](https://github.com/open-ephys/plugin-GUI/). It retrieves frames 
from any device supported by [v4l2](http://linuxtv.org/downloads/v4l-dvb-apis/) 
and displays them using [opencv](http://opencv.org/). Moreover, 
frames can be saved to disk (in jpg format) together with open-ephys hardware 
time stamps making it easy to synchronize frames and recorded data later on. 
Depending on your camera this approach should be useful for most 
(behavioral) tracking experiments. In case the experiment requires very precise 
time stamps, e.g., high-speed whisker tracking, it might be a good idea to use a 
camera that can send TTL pulses for each frame.

Currently, the plugin only runs under Linux. However, contributions to make it 
also run under Windows (e.g., using directshow) are highly welcome.

What should work:

- capturing frames from any v4l2 supported camera (using mmap; pixel format: YUYV)
- saving frames in jpeg format; the file name format is "frame\_{frame\_index}\_{experiment\_number}\_{recording\_number}.jpg"
- saving frame index, experiment number, recording number, and hardware/software time stamps to a separate csv file to make post-processing easier
- basic controls via ui (image quality, color, recording mode, frame counter resetting)
- saving/loading parameters

To-do:

- add support for other pixel formats, e.g., MJPG
- add further video APIs, e.g., directshow
- python/matlab functions to read frames and timestamps (including optional interpolation)

## Dependencies

This plugin requires the following libraries

- video4linux2 (i.e. libv4l-0 and libv4l-dev under Ubuntu and Linux Mint)
- opencv (version 2.4.x; core, dev, and highgui packages)

## Installation

Copy the FrameGrabber folder to the plugin folder of your GUI. Then build 
the all plugins as described in the [wiki](https://open-ephys.atlassian.net/wiki/display/OEW/Linux).

**Important** 
It seems that there is a data type clash between JUCE and opencv. In my case, I 
had to replace all "int64" by "juce::int64" in 
[CoreServices.h](https://github.com/open-ephys/plugin-GUI/blob/master/Source/CoreServices.h), 
[CoreServices.cpp](https://github.com/open-ephys/plugin-GUI/blob/master/Source/CoreServices.cpp), 
[GenericProcessor.h](https://github.com/open-ephys/plugin-GUI/blob/master/Source/Processors/GenericProcessor/GenericProcessor.h),
and [GenericProcessor.cpp](https://github.com/open-ephys/plugin-GUI/blob/master/Source/Processors/GenericProcessor/GenericProcessor.cpp).
Note that this does not affect the rest of the GUI as it is using the juce 
namespace anyway.

## Changing camera parameters

The v4l2 library comes with some tools that can be used to control camera 
parameters. The easiest way to see all available v4l2 controls is to use the 
v4l2-ctl tool from cmdline:

*v4l2-ctl --all*

An alternative is to use [guvcview](http://guvcview.sourceforge.net).


