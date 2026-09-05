#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/CrossoverMS.h"
#include "PresetManager.h"

class OutflankAudioProcessorEditor;

class OutflankAudioProcessor final : public juce::AudioProcessor
{
public:
    OutflankAudioProcessor();
    ~OutflankAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi()  const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    PresetManager* getPresetManager() noexcept { return presetManager_.get(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    CrossoverMS crossover_;
    std::unique_ptr<PresetManager> presetManager_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessor)
};
