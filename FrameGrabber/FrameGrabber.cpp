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

#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <stdio.h>
#include "v4l2framegrabber.h"
#include "FrameGrabber.h"

class FrameGrabberEditor;

/*
We have to put some class definitions that are using opencv here because
there will be a data type conflict (int64) between opencv and JUCE if these
classes are declared in the header file.
*/

class FrameWithTS
{
public:
	FrameWithTS(cv::Mat *f, juce::int64 ts, int imgQuality = 75)
	{
		frame = f;
		timestamp = ts;
		imageQuality = imgQuality;
	}

	~FrameWithTS()
	{
		if (frame != NULL)
		{
			delete frame;
		}
	}

	cv::Mat* getFrame()
	{
		return frame;
	}

	juce::int64 getTimestamp()
	{
		return timestamp;
	}

	int getImageQuality()
	{
		return imageQuality;
	}

private:
	cv::Mat* frame;
	juce::int64 timestamp;
	int imageQuality;
};


class DiskThread : public Thread
{
public:
	DiskThread(File dest) : Thread("DiskThread"), frameCounter(0)
	{
		destPath = dest;
		frameBuffer.clear();
		lastTS = -1;
	}

	~DiskThread()
	{
		frameBuffer.clear();
	}

	void setDestinationPath(File &f)
	{
		lock.enter();
		destPath = File(f);
		lock.exit();
	}

	void run()
	{
		FrameWithTS* frame_ts;
		threadRunning = true;
		int imgQuality;

		while (threadRunning)
		{
			frame_ts = NULL;

			lock.enter();
			if (frameBuffer.size() > 0)
			{
				frame_ts = frameBuffer.removeAndReturn(0);
			}
			lock.exit();

			if (frame_ts != NULL)
			{
				char filename[1024];
				sprintf(filename, "%s/frame_%.10lld_%lld.jpg", destPath.getFullPathName().toRawUTF8(), ++frameCounter, frame_ts->getTimestamp());

				std::vector<int> compression_params;
				compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
				compression_params.push_back(frame_ts->getImageQuality());
				cv::imwrite(filename, (*frame_ts->getFrame()), compression_params);
			}
		}
	}

	void stopThread()
	{
		lock.enter();
		threadRunning = false;
		lock.exit();
	}

	void addFrame(cv::Mat *frame, juce::int64 ts, int quality = 95)
	{
		lock.enter();
		frameBuffer.add(new FrameWithTS(frame, ts, quality));
		lock.exit();
	}

	juce::int64 getFrameCount()
	{
		int count;
		lock.enter();
		count = frameCounter;
		lock.exit();

		return count;
	}

private:
	OwnedArray<FrameWithTS> frameBuffer;
	juce::int64 frameCounter;
	File destPath;
	CriticalSection lock;
	bool threadRunning;

	juce::int64 lastTS;
};


FrameGrabber::FrameGrabber()
    : GenericProcessor("Frame Grabber"), camera(NULL), currentFormatIndex(-1),
	  frameCounter(0), Thread("FrameGrabberThread"), isRecording(false), framePath(""),
	  imageQuality(25), colorMode(ColorMode::GRAY), writeMode(ImageWriteMode::RECORDING)

{
	File recPath = CoreServices::RecordNode::getRecordingPath();
	framePath = String(recPath.getFullPathName() + recPath.separatorString + String("frames"));
	diskThread = new DiskThread(File(framePath));
}

FrameGrabber::~FrameGrabber()
{
	stopCamera();

	if (diskThread != NULL)
	{
		diskThread->waitForThreadToExit(10000);
		delete diskThread;
	}
}


AudioProcessorEditor* FrameGrabber::createEditor()
{
    editor = new FrameGrabberEditor(this, true);

    return editor;
}

void FrameGrabber::updateSettings()
{
	if (editor != NULL)
	{
		editor->update();
	}
}

void FrameGrabber::startRecording()
{
	File recPath = CoreServices::RecordNode::getRecordingPath();
	framePath = String(recPath.getFullPathName() + recPath.separatorString + String("frames"));

	File frameFile = File(framePath);
	if (!frameFile.isDirectory())
	{
		Result result = frameFile.createDirectory();
		if (result.failed())
		{
			std::cout << "FrameGrabber: failed to create frame path!" << "\n";
		}
	}

	lock.enter();
	diskThread->setDestinationPath(frameFile);
	diskThread->startThread();
	isRecording = true;
	lock.exit();
}


void FrameGrabber::stopRecording()
{
	lock.enter();
	isRecording = false;
	diskThread->stopThread();
	lock.exit();
}


void FrameGrabber::process(AudioSampleBuffer& buffer,
                  	       MidiBuffer& events)
{
}

int FrameGrabber::startCamera(int fmt_index)
{
	if (isCameraRunning())
	{
		if (stopCamera())
		{
			return 1;
		}
	}

	camera = new Camera(fmt_index);

	if (camera->init())
	{
		std::cout <<  "FrameGrabber: could not open camera\n";
		return 1;
	}

	if (camera->start())
	{
		std::cout <<  "FrameGrabber: could not open camera\n";
		return 1;
	}

	if (camera->is_running())
	{
		std::cout << "FrameGrabber: opened camera" << "\n";
		currentFormatIndex = fmt_index;

		File recPath = CoreServices::RecordNode::getRecordingPath();
		framePath = String(recPath.getFullPathName() + recPath.separatorString + String("frames"));
		diskThread = new DiskThread(File(framePath));

		cv::namedWindow("FrameGrabber", cv::WINDOW_NORMAL | cv::WND_PROP_ASPECT_RATIO);
		threadRunning = true;
		startThread();
	}

	return 0;
}

int FrameGrabber::stopCamera()
{
	threadRunning = false;
	waitForThreadToExit(1000);
	currentFormatIndex = -1;

	if (camera != NULL)
	{
		delete camera;
		camera = NULL;
	}
}

bool FrameGrabber::isCameraRunning()
{
	return (camera != NULL && camera->is_running());
}

std::vector<std::string> FrameGrabber::getFormats()
{
	std::vector<std::string> formats = Camera::list_formats_as_string();
	return formats;
}

int FrameGrabber::getCurrentFormatIndex()
{
	return currentFormatIndex;
}

void FrameGrabber::setImageQuality(int q)
{
	lock.enter();
	imageQuality = q;
	if (imageQuality <= 0)
	{
		imageQuality = 1;
	}
	else if (imageQuality > 100)
	{
		imageQuality = 100;
	}
	lock.exit();
}


int FrameGrabber::getImageQuality()
{
	int q;
	lock.enter();
	q = imageQuality;
	lock.exit();

	return q;
}


void FrameGrabber::setColorMode(int value)
{
	lock.enter();
	colorMode = value;
	lock.exit();
}


int FrameGrabber::getColorMode()
{
	int mode;
	lock.enter();
	mode = colorMode;
	lock.exit();

	return mode;
}


void FrameGrabber::setWriteMode(int mode)
{
	writeMode = mode;
}


int FrameGrabber::getWriteMode()
{
	return writeMode;
}


juce::int64 FrameGrabber::getFrameCount()
{
	int count;
	lock.enter();
	count = frameCounter;
	lock.exit();

	return count;
}


juce::int64 FrameGrabber::getWrittenFrameCount()
{
	int count;
	lock.enter();
	count = diskThread->getFrameCount();
	lock.exit();

	return count;
}

void FrameGrabber::run()
{
	juce::int64 lastTS;
	bool recStatus;
	int imgQuality;
	bool winState;
	bool cMode;
	int wMode;

    while (threadRunning)
    {

		if (camera != NULL && camera->is_running())
		{

			cv::Mat frame = camera->read_frame();
			if (!frame.empty())
			{
				bool writeImage = false;

				lock.enter();
				imgQuality = imageQuality;
				recStatus = isRecording;
				wMode = writeMode;
				cMode = colorMode;
				lock.exit();

				writeImage = (wMode == ImageWriteMode::ACQUISITION) || (wMode == ImageWriteMode::RECORDING && recStatus);
				if (writeImage)
				{
					lastTS = CoreServices::getGlobalTimestamp();
					diskThread->addFrame(&frame, lastTS, imgQuality);
				}

				cv::imshow("FrameGrabber", frame);
				cv::waitKey(1);

				frameCounter++;
			}
		}
    }

    return;
}

void FrameGrabber::saveCustomParametersToXml(XmlElement* xml)
{
    xml->setAttribute("Type", "FrameGrabber");

    XmlElement* paramXml = xml->createNewChildElement("PARAMETERS");
    paramXml->setAttribute("ImageQuality", getImageQuality());
	paramXml->setAttribute("ColorMode", getColorMode());
	paramXml->setAttribute("WriteMode", getWriteMode());

	XmlElement* deviceXml = xml->createNewChildElement("DEVICE");
	deviceXml->setAttribute("API", "V4L2");
	if (currentFormatIndex >= 0)
	{
		deviceXml->setAttribute("Format", Camera::get_format_string(currentFormatIndex));
	}
	else
	{
		deviceXml->setAttribute("Format", "");
	}
}

void FrameGrabber::loadCustomParametersFromXml()
{
	forEachXmlChildElementWithTagName(*parametersAsXml,	paramXml, "PARAMETERS")
	{
    	int value;
		value = paramXml->getIntAttribute("ImageQuality");
		setImageQuality(value);
    	value = paramXml->getIntAttribute("ColorMode");
		setColorMode(value);
    	value = paramXml->getIntAttribute("WriteMode");
		setWriteMode(value);
	}

	forEachXmlChildElementWithTagName(*parametersAsXml,	deviceXml, "DEVICE")
	{
		String api = deviceXml->getStringAttribute("API");
		if (api.compareIgnoreCase("V4L2") == 0)
		{
			String format = deviceXml->getStringAttribute("Format");
			int index = Camera::get_format_index(format.toStdString());
			if (index >= 0)
			{
				if (isCameraRunning())
				{
					stopCamera();
				}
				startCamera(index);
			}
		}
		else
		{
			std::cout << "FrameGrabber API " << api << " not supported\n";
		}
	}

	updateSettings();
}

