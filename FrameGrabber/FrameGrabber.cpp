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
	FrameWithTS(cv::Mat *f, juce::int64 src_ts, juce::int64 sw_ts, int imgQuality = 75)
	{
		frame = f;
		sourceTimestamp = src_ts;
		softwareTimestamp = sw_ts;
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

	juce::int64 getSourceTimestamp()
	{
		return sourceTimestamp;
	}

	juce::int64 getSoftwareTimestamp()
	{
		return softwareTimestamp;
	}

	int getImageQuality()
	{
		return imageQuality;
	}

private:
	cv::Mat* frame;
	juce::int64 sourceTimestamp;
	juce::int64 softwareTimestamp;
	int imageQuality;
};


class WriteThread : public Thread
{
public:
    WriteThread()
        : Thread ("WriteThread"), framePath(), timestampFile(), frameCounter(0), experimentNumber(1), recordingNumber(0), isRecording(false)
    {
		frameBuffer.clear();

        startThread();
    }

    ~WriteThread()
    {
        stopThread(1000);
		clearBuffer();
    }

	void setFramePath(File &f)
	{
		lock.enter();
		framePath = File(f);
		lock.exit();
	}

	void createTimestampFile(String name = "frame_timestamps")
	{

		if (!framePath.exists() || !framePath.isDirectory())
		{
			 framePath.createDirectory();
		}

	    String filePath(framePath.getFullPathName() + framePath.separatorString + name + ".csv");

		timestampFile = File(filePath);
		if (!timestampFile.exists())
		{
			timestampFile.create();
			timestampFile.appendText("# Frame index, Recording number, Experiment number, Source timestamp, Software timestamp\n");
		}
	}

	juce::int64 getFrameCount()
	{
		int count;
		lock.enter();
		count = frameCounter;
		lock.exit();

		return count;
	}

	void resetFrameCounter()
	{
		lock.enter();
		frameCounter = 0;
		lock.exit();
	}

	void setExperimentNumber(int n)
	{
		lock.enter();
		experimentNumber = n;
		lock.exit();
	}

	void setRecordingNumber(int n)
	{
		lock.enter();
		recordingNumber = n;
		lock.exit();
	}

	bool addFrame(cv::Mat *frame, juce::int64 srcTs, juce::int64 swTs, int quality = 95)
	{
		bool status;

		if (isThreadRunning())
		{
			lock.enter();
			frameBuffer.add(new FrameWithTS(frame, srcTs, swTs, quality));
			lock.exit();
			status = true;
		}
		else
		{
			status = false;
		}

		return status;
	}

	void clearBuffer()
	{
		lock.enter();
		/* for some reason frameBuffer.clear() didn't delete the content ... */
		while (frameBuffer.size() > 0)
		{
			frameBuffer.removeAndReturn(0);
		}
		lock.exit();
	}

	bool hasValidPath()
	{
		bool status;

		lock.enter();
		status = (framePath.exists() && timestampFile.exists());
		lock.exit();

		return status;
	}

	void setRecording(bool status)
	{
		lock.enter();
		isRecording = status;
		lock.exit();
	}

    void run() override
    {
		FrameWithTS* frame_ts;
		int imgQuality;
		String fileName;
		String filePath;
		String line;

        while (!threadShouldExit())
        {

			if (isRecording && hasValidPath())
			{
				if (frameBuffer.size() > 0)
				{
					frame_ts = frameBuffer.removeAndReturn(0);
				}
				else
				{
					frame_ts = NULL;
				}

				if (frame_ts != NULL)
				{
					lock.enter();

					++frameCounter;

					fileName = String::formatted("frame_%.10lld_%d_%d.jpg", frameCounter, experimentNumber, recordingNumber);
		            filePath = String(framePath.getFullPathName() + framePath.separatorString + fileName);

					lock.exit();

					std::vector<int> compression_params;
					compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
					compression_params.push_back(frame_ts->getImageQuality());
					cv::imwrite(filePath.toRawUTF8(), (*frame_ts->getFrame()), compression_params);

					lock.enter();
					line = String::formatted("%lld,%d,%d,%lld,%lld\n", frameCounter, experimentNumber, recordingNumber, frame_ts->getSourceTimestamp(), frame_ts->getSoftwareTimestamp());
					lock.exit();
					timestampFile.appendText(line);

				}

            }
			else
			{
				sleep(50);
			}
        }
    }

private:
	OwnedArray<FrameWithTS> frameBuffer;
	juce::int64 frameCounter;
	int experimentNumber;
	int recordingNumber;
	File framePath;
	File timestampFile;
	bool isRecording;
	CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WriteThread)
};


FrameGrabber::FrameGrabber()
    : GenericProcessor("Frame Grabber"), camera(NULL), currentFormatIndex(-1),
	  frameCounter(0), Thread("FrameGrabberThread"), isRecording(false), framePath(""),
	  imageQuality(25), colorMode(ColorMode::GRAY), writeMode(ImageWriteMode::RECORDING),
	  resetFrameCounter(false), dirName("frames")

{
    setProcessorType(PROCESSOR_TYPE_SOURCE);

	File recPath = CoreServices::RecordNode::getRecordingPath();
	framePath = File(recPath.getFullPathName() + recPath.separatorString + dirName);

	writeThread = new WriteThread();
}


FrameGrabber::~FrameGrabber()
{
	stopCamera();
	delete writeThread;
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
	if (writeMode == RECORDING)
	{
		File recPath = CoreServices::RecordNode::getRecordingPath();
		framePath = File(recPath.getFullPathName() + recPath.separatorString + dirName);

		if (!framePath.exists() && !framePath.isDirectory())
		{
			Result result = framePath.createDirectory();
			if (result.failed())
			{
				std::cout << "FrameGrabber: failed to create frame path " << framePath.getFullPathName().toRawUTF8() << "\n";
				framePath = File();
			}
		}

		if (framePath.exists())
		{
			writeThread->setRecording(false);
			writeThread->setFramePath(framePath);
			writeThread->setExperimentNumber(CoreServices::RecordNode::getExperimentNumber());
			writeThread->setRecordingNumber(CoreServices::RecordNode::getRecordingNumber());
			writeThread->createTimestampFile();
			if (resetFrameCounter)
			{
				writeThread->resetFrameCounter();
			}
			writeThread->setRecording(true);

			FrameGrabberEditor* e = (FrameGrabberEditor*) editor.get();
			e->disableControls();
		}
	}

	isRecording = true;
}


void FrameGrabber::stopRecording()
{
	isRecording = false;
	if (writeMode == RECORDING)
	{
		writeThread->setRecording(false);
		FrameGrabberEditor* e = (FrameGrabberEditor*) editor.get();
		e->enableControls();
	}
}


void FrameGrabber::process(AudioSampleBuffer& buffer)
{
}


void FrameGrabber::run()
{
	juce::int64 srcTS;
	juce::int64 swTS;
	bool recStatus;
	int imgQuality;
	bool winState;
	bool cMode;
	int wMode;

    while (!threadShouldExit())
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

				writeImage = (wMode == ImageWriteMode::ACQUISITION) || (wMode == ImageWriteMode::RECORDING && recStatus);
				lock.exit();

				if (writeImage)
				{
					srcTS = CoreServices::getGlobalTimestamp();
					swTS = CoreServices::getSoftwareTimestamp();
					writeThread->addFrame(&frame, srcTS, swTS, imgQuality);
				}

				cv::imshow("FrameGrabber", frame);
				cv::waitKey(1);

				frameCounter++;
			}
		}
    }

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
		std::cout << "FrameGrabber: opened camera " << camera->get_format()->to_string() << "\n";
		currentFormatIndex = fmt_index;

		try
		{
			cv::namedWindow("FrameGrabber", cv::WINDOW_OPENGL & cv::WND_PROP_ASPECT_RATIO);
			std::cout << "FrameGrabber using opengl window\n";
		}
		catch (cv::Exception& ex)
		{
			cv::namedWindow("FrameGrabber", cv::WINDOW_NORMAL & cv::WND_PROP_ASPECT_RATIO);
			std::cout << "FrameGrabber using normal window (opencv not compiled with opengl support)\n";
		}

		startThread();
	}

	return 0;
}

int FrameGrabber::stopCamera()
{
	if (isThreadRunning())
		stopThread(1000);

    if (isRecording)
        stopRecording();

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

void FrameGrabber::setResetFrameCounter(bool enable)
{
	resetFrameCounter = enable;
}

bool FrameGrabber::getResetFrameCounter()
{
	return resetFrameCounter;
}

void FrameGrabber::setDirectoryName(String name)
{
	if (name != getDirectoryName())
	{
		if (File::createLegalFileName(name) == name)
		{
			dirName = name;
		}
		else
		{
			std::cout << "FrameGrabber invalid directory name: " << name.toStdString() << "\n";
		}
	}
}

String FrameGrabber::getDirectoryName()
{
	return dirName;
}

juce::int64 FrameGrabber::getWrittenFrameCount()
{
	return writeThread->getFrameCount();
}

void FrameGrabber::saveCustomParametersToXml(XmlElement* xml)
{
    xml->setAttribute("Type", "FrameGrabber");

    XmlElement* paramXml = xml->createNewChildElement("PARAMETERS");
    paramXml->setAttribute("ImageQuality", getImageQuality());
	paramXml->setAttribute("ColorMode", getColorMode());
	paramXml->setAttribute("WriteMode", getWriteMode());
	paramXml->setAttribute("ResetFrameCounter", getResetFrameCounter());
	paramXml->setAttribute("DirectoryName", getDirectoryName());

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
		if (paramXml->hasAttribute("ImageQuality"))
			setImageQuality(paramXml->getIntAttribute("ImageQuality"));

		if (paramXml->hasAttribute("ColorMode"))
			setColorMode(paramXml->getIntAttribute("ColorMode"));

    	if (paramXml->hasAttribute("WriteMode"))
			setWriteMode(paramXml->getIntAttribute("WriteMode"));

		if (paramXml->hasAttribute("ResetFrameCounter"))
			setResetFrameCounter(paramXml->getIntAttribute("ResetFrameCounter"));

		if (paramXml->hasAttribute("DirectoryName"))
			setDirectoryName(paramXml->getStringAttribute("DirectoryName"));
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

