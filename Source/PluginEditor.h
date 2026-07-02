#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class OutflankAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit OutflankAudioProcessorEditor (OutflankAudioProcessor&);
    ~OutflankAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OutflankAudioProcessor& proc_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessorEditor)
};
