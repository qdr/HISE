/*
  ==============================================================================

  This is an automatically generated GUI class created by the Introjucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Introjucer version: 4.1.0

  ------------------------------------------------------------------------------

  The Introjucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright (c) 2015 - ROLI Ltd.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
//[/Headers]

#include "AudioLooperEditor.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
AudioLooperEditor::AudioLooperEditor (ProcessorEditor *p)
    : ProcessorEditorBody(p)
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    addAndMakeVisible (sampleBufferContent = new AudioSampleBufferComponent (dynamic_cast<AudioSampleProcessor*>(getProcessor())->getCache()));
    sampleBufferContent->setName ("new component");

    addAndMakeVisible (label = new Label ("new label",
                                          TRANS("LOOPER")));
    label->setFont (Font ("Arial", 24.00f, Font::bold));
    label->setJustificationType (Justification::centredRight);
    label->setEditable (false, false, false);
    label->setColour (Label::textColourId, Colour (0x52ffffff));
    label->setColour (TextEditor::textColourId, Colours::black);
    label->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (syncToHost = new HiComboBox ("Mode Selection"));
    syncToHost->setTooltip (TRANS("Sync the loop to the host tempo"));
    syncToHost->setEditableText (false);
    syncToHost->setJustificationType (Justification::centredLeft);
    syncToHost->setTextWhenNothingSelected (TRANS("Sync to Tempo"));
    syncToHost->setTextWhenNoChoicesAvailable (TRANS("(no choices)"));
    syncToHost->addItem (TRANS("Free running"), 1);
    syncToHost->addItem (TRANS("1 Beat"), 2);
    syncToHost->addItem (TRANS("2 Beats"), 3);
    syncToHost->addItem (TRANS("1 Bar"), 4);
    syncToHost->addItem (TRANS("2 Bars"), 5);
    syncToHost->addItem (TRANS("4 Bars"), 6);
    syncToHost->addSeparator();
    syncToHost->addListener (this);

    addAndMakeVisible (pitchButton = new HiToggleButton ("FM Synthesiser"));
    pitchButton->setTooltip (TRANS("Enables FM Modulation\n"));
    pitchButton->setButtonText (TRANS("Pitch Tracking"));
    pitchButton->addListener (this);
    pitchButton->setColour (ToggleButton::textColourId, Colours::white);

    addAndMakeVisible (loopButton = new HiToggleButton ("FM Synthesiser"));
    loopButton->setTooltip (TRANS("Enables FM Modulation\n"));
    loopButton->setButtonText (TRANS("Loop"));
    loopButton->addListener (this);
    loopButton->setColour (ToggleButton::textColourId, Colours::white);

    addAndMakeVisible (rootNote = new HiSlider ("Root Note"));
    rootNote->setRange (0, 127, 1);
    rootNote->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    rootNote->setTextBoxStyle (Slider::TextBoxRight, false, 40, 20);
    rootNote->addListener (this);


    //[UserPreSize]

	AudioSampleProcessor *asp = dynamic_cast<AudioSampleProcessor*>(getProcessor());

	sampleBufferContent->setAudioSampleBuffer(asp->getBuffer(), asp->getFileName());

	sampleBufferContent->addChangeListener(asp);

	getProcessor()->addChangeListener(sampleBufferContent);



	sampleBufferContent->addAreaListener(this);


	syncToHost->setup(getProcessor(), AudioLooper::SyncMode, "Sync to host");
	loopButton->setup(getProcessor(), AudioLooper::LoopEnabled, "Loop Enabled");
	pitchButton->setup(getProcessor(), AudioLooper::PitchTracking, "Pitch Tracking");
	rootNote->setup(getProcessor(), AudioLooper::RootNote, "Root Note");


#if JUCE_DEBUG
	startTimer(150);
#else
	startTimer(30);
#endif

    //[/UserPreSize]

    setSize (830, 250);


    //[Constructor] You can add your own custom stuff here..

	h = getHeight();
    //[/Constructor]
}

AudioLooperEditor::~AudioLooperEditor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    sampleBufferContent = nullptr;
    label = nullptr;
    syncToHost = nullptr;
    pitchButton = nullptr;
    loopButton = nullptr;
    rootNote = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void AudioLooperEditor::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

	ProcessorEditorLookAndFeel::fillEditorBackgroundRect(g, this);

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void AudioLooperEditor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    sampleBufferContent->setBounds ((getWidth() / 2) - ((getWidth() - 112) / 2), 77, getWidth() - 112, 144);
    label->setBounds (getWidth() - 52 - 264, 12, 264, 40);
    syncToHost->setBounds (56, 45, 134, 28);
    pitchButton->setBounds (354 - 128, 26, 128, 32);
    loopButton->setBounds (184 - 128, 11, 128, 32);
    rootNote->setBounds (358, 20, 128, 48);
    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void AudioLooperEditor::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{
    //[UsercomboBoxChanged_Pre]
    //[/UsercomboBoxChanged_Pre]

    if (comboBoxThatHasChanged == syncToHost)
    {
        //[UserComboBoxCode_syncToHost] -- add your combo box handling code here..
        //[/UserComboBoxCode_syncToHost]
    }

    //[UsercomboBoxChanged_Post]
    //[/UsercomboBoxChanged_Post]
}

void AudioLooperEditor::buttonClicked (Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == pitchButton)
    {
        //[UserButtonCode_pitchButton] -- add your button handler code here..
		rootNote->setEnabled(buttonThatWasClicked->getToggleState());
        //[/UserButtonCode_pitchButton]
    }
    else if (buttonThatWasClicked == loopButton)
    {
        //[UserButtonCode_loopButton] -- add your button handler code here..
        //[/UserButtonCode_loopButton]
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}

void AudioLooperEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    //[UsersliderValueChanged_Pre]
    //[/UsersliderValueChanged_Pre]

    if (sliderThatWasMoved == rootNote)
    {
        //[UserSliderCode_rootNote] -- add your slider handling code here..
        //[/UserSliderCode_rootNote]
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Introjucer information section --

    This is where the Introjucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="AudioLooperEditor" componentName=""
                 parentClasses="public ProcessorEditorBody, public Timer, public AudioDisplayComponent::Listener"
                 constructorParams="ProcessorEditor *p" variableInitialisers="ProcessorEditorBody(p)"
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330"
                 fixedSize="1" initialWidth="830" initialHeight="250">
  <BACKGROUND backgroundColour="ffffff">
    <ROUNDRECT pos="0Cc 6 84M 16M" cornerSize="6" fill="solid: 30000000" hasStroke="1"
               stroke="2, mitered, butt" strokeColour="solid: 25ffffff"/>
  </BACKGROUND>
  <GENERICCOMPONENT name="new component" id="e2252e55bedecdc5" memberName="sampleBufferContent"
                    virtualName="" explicitFocusOrder="0" pos="0Cc 77 112M 144" class="AudioSampleBufferComponent"
                    params="dynamic_cast&lt;AudioSampleProcessor*&gt;(getProcessor())-&gt;getCache()"/>
  <LABEL name="new label" id="bd1d8d6ad6d04bdc" memberName="label" virtualName=""
         explicitFocusOrder="0" pos="52Rr 12 264 40" textCol="52ffffff"
         edTextCol="ff000000" edBkgCol="0" labelText="LOOPER" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Arial"
         fontsize="24" bold="1" italic="0" justification="34"/>
  <COMBOBOX name="Mode Selection" id="63f5b1527f75c45b" memberName="syncToHost"
            virtualName="HiComboBox" explicitFocusOrder="0" pos="56 45 134 28"
            tooltip="Sync the loop to the host tempo" editable="0" layout="33"
            items="Free running&#10;1 Beat&#10;2 Beats&#10;1 Bar&#10;2 Bars&#10;4 Bars&#10;"
            textWhenNonSelected="Sync to Tempo" textWhenNoItems="(no choices)"/>
  <TOGGLEBUTTON name="FM Synthesiser" id="e77edc03c117de85" memberName="pitchButton"
                virtualName="HiToggleButton" explicitFocusOrder="0" pos="354r 26 128 32"
                tooltip="Enables FM Modulation&#10;" txtcol="ffffffff" buttonText="Pitch Tracking"
                connectedEdges="0" needsCallback="1" radioGroupId="0" state="0"/>
  <TOGGLEBUTTON name="FM Synthesiser" id="3ef6a10c1e23368a" memberName="loopButton"
                virtualName="HiToggleButton" explicitFocusOrder="0" pos="184r 11 128 32"
                tooltip="Enables FM Modulation&#10;" txtcol="ffffffff" buttonText="Loop"
                connectedEdges="0" needsCallback="1" radioGroupId="0" state="0"/>
  <SLIDER name="Root Note" id="baa9524f7348f05" memberName="rootNote" virtualName="HiSlider"
          explicitFocusOrder="0" pos="358 20 128 48" min="0" max="127"
          int="1" style="RotaryHorizontalVerticalDrag" textBoxPos="TextBoxRight"
          textBoxEditable="1" textBoxWidth="40" textBoxHeight="20" skewFactor="1"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]
