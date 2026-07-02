#include "PluginEditor.h"

OutflankAudioProcessorEditor::OutflankAudioProcessorEditor (OutflankAudioProcessor& p)
    : AudioProcessorEditor (&p), proc_ (p)
{
    setSize (320, 160);
}

OutflankAudioProcessorEditor::~OutflankAudioProcessorEditor() {}

void OutflankAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141420));
    g.setColour (juce::Colour (0xffaaaacc));
    g.setFont (juce::FontOptions (16.f).withStyle ("Bold"));
    g.drawText ("OUTFLANK", getLocalBounds(), juce::Justification::centred);
}

void OutflankAudioProcessorEditor::resized() {}
