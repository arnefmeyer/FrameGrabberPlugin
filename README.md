# FrameGrabber

A simple video4linux2-based video frame grabber plugin for the open-ephys 
[plugin-GUI](https://github.com/open-ephys/plugin-GUI/). It retrieves frames 
from any device supported by [v4l2](http://linuxtv.org/downloads/v4l-dvb-apis/) 
and displays them using [opencv](http://opencv.org/). Moreover, 
frames can be saved to disk (in jpg format) together with open-ephys hardware 
timestamps making it easy to synchronize frames and recorded data later on. 
Depending on your camera this approach should be useful for most 
(behavioral) tracking experiments. In case the experiment requires very precise 
time stamps, e.g., high-speed whisker tracking, it might be better to use a 
camera that can send TTL pulses for each frame.

The plugin currently runs only under Linux. However, contributions to make it 
also run under Windows (e.g., using directshow) are highly welcome.

## Dependencies

This plugin requires the following libraries

- video4linux2 (i.e. libv4l-0 and libv4l-dev under Ubuntu and Linux Mint)
- opencv (version 2.4.x; core, dev, and highgui packages)

# Installation

Copy the FrameGrabber folder to the plugin folder of your GUI. Then build 
the all plugins as described in the [wiki](https://open-ephys.atlassian.net/wiki/display/OEW/Linux).

**Important** 
It seems that there is a data type clash between JUCE and opencv. In my case, I 
had to replace all "int64" by "juce::int64" in 
[CoreServices.h](https://github.com/open-ephys/plugin-GUI/blob/master/Source/CoreServices.h) 
and [CoreServices.cpp](https://github.com/open-ephys/plugin-GUI/blob/master/Source/CoreServices.cpp). 
Note that this does not affect the rest of the GUI as it is using the juce 
namespace anyway.

