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

#ifndef FRAMEGRABBER_H_INCLUDED
#define FRAMEGRABBER_H_INCLUDED

#ifdef _WIN32
#include <Windows.h>
#endif

#include <ProcessorHeaders.h>
#include "FrameGrabberEditor.h"

class Camera;
class CameraFormat;
class WriteThread;

enum ImageWriteMode {NEVER = 0, RECORDING = 1, ACQUISITION = 2};
enum ColorMode {GRAY = 0, RGB = 1};


class FrameGrabber : public GenericProcessor,  public Thread
{
public:

    FrameGrabber();
    ~FrameGrabber();

    void process(AudioSampleBuffer& buffer);

	void startRecording();
	void stopRecording();

	void run();

	AudioProcessorEditor* createEditor();
	void updateSettings();

	int startCamera(int fmt_index);
	int stopCamera();
	bool isCameraRunning();

	std::vector<std::string> getFormats();
	int getCurrentFormatIndex();

	void setImageQuality(int q);
	int getImageQuality();

	void setColorMode(int value);
	int getColorMode();

	void setWriteMode(int value);
	int getWriteMode();

	void setResetFrameCounter(bool enable);
	bool getResetFrameCounter();

	void setDirectoryName(String name);
	String getDirectoryName();

	juce::int64 getFrameCount();
	juce::int64 getWrittenFrameCount();

	void saveCustomParametersToXml(XmlElement* parentElement);
	void loadCustomParametersFromXml();

private:

	Camera* camera;
	juce::int64 frameCounter;
	bool threadRunning;
	bool isRecording;
	File framePath;
	int imageQuality;
	int colorMode;
	int writeMode;
	bool resetFrameCounter;
	String dirName;
	int currentFormatIndex;
	WriteThread* writeThread;

	CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrameGrabber);
};


#endif  // FrameGrabber_H_INCLUDED

