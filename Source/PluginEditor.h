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
    void addKnob (juce::Slider& s, juce::Label& l, const char* name);
    void updatePresetList();
    void onPresetSelected();
    void onSaveAsPressed();
    void onDeletePressed();

    OutflankAudioProcessor& proc_;

    juce::ComboBox   presetSelector_;
    juce::TextButton saveAsButton_ { "SAVE AS" };
    juce::TextButton deleteButton_ { "DELETE"  };

    juce::Slider frequencyKnob_, qKnob_, rejectionKnob_;
    juce::Label  frequencyLbl_,  qLbl_,  rejectionLbl_;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> frequencyAtt_, qAtt_, rejectionAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessorEditor)
};
