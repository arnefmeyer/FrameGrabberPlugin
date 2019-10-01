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

- capturing frames from any v4l2 supported camera (using mmap; pixel formats: YUYV, MJPG)
- saving frames in jpeg format; the file name format is "frame\_{frame\_index}\_{experiment\_number}\_{recording\_number}.jpg"
- saving frame index, experiment number, recording number, and hardware/software time stamps to a separate csv file to make post-processing easier
- basic controls via ui (image quality, color, recording mode, frame counter resetting, frame directory name)
- saving/loading parameters

To-do:

- add further video APIs, e.g., directshow
- python/matlab functions to read frames and timestamps (including optional interpolation)

## Dependencies

This plugin requires the following libraries

- video4linux2 (i.e. libv4l-0 and libv4l-dev under Ubuntu and Linux Mint)
- opencv (version 2.4.x; core, dev, and highgui packages)

## Installation

_Note: when using the old build system (e.g., the current master branch of the open-ephys plugin-GUI) use the ["old_makefile" branch](https://github.com/arnefmeyer/FrameGrabberPlugin/tree/old_makefile) of the FrameGrabber plugin._

1. Build [plugin-gui](https://github.com/open-ephys/plugin-gui) (development branch with cmake) following
[Building plugin-GUI](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/1301643269/Creating+Build+files)
2. Install plugin-GUI
[Installing plugin-GUI](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/491544/Installation)
3. Follow instructions for building third-party Plugins [Building Plugins](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/1259110401/Plugin+CMake+Builds)  
In the ```FrameGrabberPlugin\FrameGrabber\Build``` folder run  
```
export GUI_BASE_DIR=path/to/GUI
```
```path/to/GUI``` should be changed to the absolute path where the plugin-gui folder is located. Finally, run
```
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
make
make install
```
to build and install the plugin.  
(Thanks to Ariel Burman for updating the plugin to the new cmake build system.)

## Changing camera parameters

The v4l2 library comes with some tools that can be used to control camera 
parameters. The easiest way to see all available v4l2 controls is to use the 
v4l2-ctl tool from cmdline:

*v4l2-ctl --all*

An alternative is to use [guvcview](http://guvcview.sourceforge.net).


