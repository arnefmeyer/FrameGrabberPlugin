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

#include "FrameGrabberEditor.h"
#include <stdio.h>


FrameGrabberEditor::FrameGrabberEditor(GenericProcessor* parentNode, bool useDefaultParameterEditors=true)
    : GenericEditor(parentNode, useDefaultParameterEditors), lastFrameCount(0)

{
    desiredWidth = 300;

	FrameGrabber* proc = (FrameGrabber*) parentNode;

	sourceLabel = new Label("video source", "Source");
	sourceLabel->setBounds(10,25,90,20);
    sourceLabel->setFont(Font("Small Text", 12, Font::plain));
    sourceLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(sourceLabel);

    sourceCombo = new ComboBox();
    sourceCombo->setBounds(110,25,175,20);
    sourceCombo->addListener(this);

	std::vector<std::string> formats = proc->getFormats();
    for (int i=0; i<formats.size(); i++)
    {
        sourceCombo->addItem(formats.at(i), i+1);
    }
	addAndMakeVisible(sourceCombo);

	qualityLabel = new Label("image quality label", "Image quality");
	qualityLabel->setBounds(10,50,90,20);
    qualityLabel->setFont(Font("Small Text", 12, Font::plain));
    qualityLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(qualityLabel);

    qualityCombo = new ComboBox();
    qualityCombo->setBounds(110,50,75,20);
    qualityCombo->addListener(this);

    for (int i=0; i<100; i++)
    {
        qualityCombo->addItem(String(i+1), i+1);
    }
	qualityCombo->setSelectedItemIndex(proc->getImageQuality() - 1, dontSendNotification);
	addAndMakeVisible(qualityCombo);

	colorLabel = new Label("color mode label", "Color");
	colorLabel->setBounds(10,75,50,20);
    colorLabel->setFont(Font("Small Text", 12, Font::plain));
    colorLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(colorLabel);

    colorCombo = new ComboBox();
    colorCombo->setBounds(110,75,75,20);
    colorCombo->addListener(this);
    colorCombo->addItem("Gray", 1);
	colorCombo->addItem("RGB", 2);
	colorCombo->setSelectedItemIndex(proc->getColorMode(), dontSendNotification);
	addAndMakeVisible(colorCombo);

	writeModeLabel = new Label("write mode label", "Save frames");
	writeModeLabel->setBounds(10,100,150,20);
    writeModeLabel->setFont(Font("Small Text", 12, Font::plain));
    writeModeLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(writeModeLabel);

    writeModeCombo = new ComboBox();
    writeModeCombo->setBounds(110,100,75,20);
    writeModeCombo->addListener(this);
	writeModeCombo->addItem("Never", ImageWriteMode::NEVER+1);
    writeModeCombo->addItem("Recording", ImageWriteMode::RECORDING+1);
//	writeModeCombo->addItem("Acquisition", ImageWriteMode::ACQUISITION+1);
	writeModeCombo->setSelectedItemIndex(proc->getWriteMode(), dontSendNotification);
	addAndMakeVisible(writeModeCombo);

	fpsLabel = new Label("fps label", "FPS:");
	fpsLabel->setBounds(200,100,50,20);
    fpsLabel->setFont(Font("Small Text", 12, Font::plain));
    fpsLabel->setColour(Label::textColourId, Colours::darkgrey);
	addAndMakeVisible(fpsLabel);

    refreshButton = new UtilityButton("Refresh", Font ("Small Text", 12, Font::plain));
    refreshButton->addListener(this);
    refreshButton->setBounds(200, 50, 75, 20);
    addAndMakeVisible(refreshButton);

	startTimer(1000);
}


FrameGrabberEditor::~FrameGrabberEditor()
{
}

void FrameGrabberEditor::updateSettings()
{
	FrameGrabber* proc = (FrameGrabber*) getProcessor();

	qualityCombo->setSelectedItemIndex(proc->getImageQuality()-1, dontSendNotification);
	colorCombo->setSelectedItemIndex(proc->getColorMode(), dontSendNotification);
	writeModeCombo->setSelectedItemIndex(proc->getWriteMode(), dontSendNotification);

	updateDevices();
	int deviceIndex = proc->getCurrentFormatIndex();
	if (deviceIndex >= 0)
	{
		sourceCombo->setSelectedItemIndex(deviceIndex, sendNotificationAsync);
	}
}

void FrameGrabberEditor::comboBoxChanged(ComboBox* cb)
{
    if (cb == qualityCombo)
    {
		int index = cb->getSelectedItemIndex();
		FrameGrabber* proc = (FrameGrabber*) getProcessor();
		proc->setImageQuality(index + 1);
    }
    else if (cb == colorCombo)
    {
		int index = cb->getSelectedItemIndex();
		FrameGrabber* proc = (FrameGrabber*) getProcessor();
		proc->setColorMode(index);
    }
	else if (cb == sourceCombo)
	{
		int index = cb->getSelectedItemIndex();
		FrameGrabber* proc = (FrameGrabber*) getProcessor();
		if (proc->isCameraRunning())
		{
			proc->stopCamera();
		}
		proc->startCamera(index);
	}
    else if (cb == writeModeCombo)
    {
		int index = cb->getSelectedItemIndex();
		FrameGrabber* proc = (FrameGrabber*) getProcessor();
		proc->setWriteMode(index);
    }
}


void FrameGrabberEditor::buttonEvent(Button* button)
{
	if (button == refreshButton)
	{
		updateDevices();
	}
}


void FrameGrabberEditor::updateDevices()
{
	sourceCombo->clear(dontSendNotification);
	FrameGrabber* proc = (FrameGrabber*) getProcessor();
	std::vector<std::string> formats = proc->getFormats();
	for (int i=0; i<formats.size(); i++)
	{
	    sourceCombo->addItem(formats.at(i), i+1);
	}
}

void FrameGrabberEditor::timerCallback()
{
	FrameGrabber* proc = (FrameGrabber*) getProcessor();
	juce::int64 frameCount = proc->getFrameCount();
	juce::int64 fps = frameCount - lastFrameCount;
	lastFrameCount = frameCount;

	fpsLabel->setText(String("FPS: ") + String(fps), dontSendNotification);
}

