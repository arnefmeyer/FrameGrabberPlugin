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

#ifndef __FRAMEGRABBEREDITOR_H__
#define __FRAMEGRABBEREDITOR_H__


#include <EditorHeaders.h>
#include "FrameGrabber.h"

/**

  User interface for the FrameGrabber plugin

  @see FrameGrabber

*/

class FrameGrabberEditor : public GenericEditor, public ComboBox::Listener,
public Label::Listener

{
public:
    FrameGrabberEditor(GenericProcessor* parentNode, bool useDefaultParameterEditors);
    virtual ~FrameGrabberEditor();

	void updateSettings();
	void updateDevices();

    void comboBoxChanged(ComboBox* comboBoxThatHasChanged);
	void buttonEvent(Button* button);
	void labelTextChanged(juce::Label *);
	void timerCallback();

    void disableControls();
	void enableControls();

private:

	ScopedPointer<ComboBox> sourceCombo;
	ScopedPointer<Label> sourceLabel;
    ScopedPointer<ComboBox> qualityCombo;
    ScopedPointer<Label> qualityLabel;
    ScopedPointer<ComboBox> colorCombo;
    ScopedPointer<Label> colorLabel;
    ScopedPointer<ComboBox> writeModeCombo;
    ScopedPointer<Label> writeModeLabel;
	ScopedPointer<Label> fpsLabel;
	ScopedPointer<UtilityButton> refreshButton;
	ScopedPointer<UtilityButton> resetCounterButton;
	ScopedPointer<Label> dirNameEdit;

	juce::int64 lastFrameCount;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrameGrabberEditor);

};


#endif  // __FRAMEGRABBEREDITOR_H__

