#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

struct PresetInfo
{
    juce::String name;
    bool isFactory = false;
    int index = -1; // Position in getPresetList(); -1 if not yet assigned
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager() = default;

    bool loadPreset (int index);
    bool savePreset (const juce::String& presetName);
    bool deletePreset (int index);
    bool renamePreset (int index, const juce::String& newName);

    int getCurrentPresetIndex() const;
    juce::String getCurrentPresetName() const;
    std::vector<PresetInfo> getPresetList() const;

    void refreshUserPresets();

private:
    juce::AudioProcessorValueTreeState& apvts_;
    int currentPresetIndex_ = -1;

    std::vector<PresetInfo> factoryPresets_;
    std::vector<PresetInfo> userPresets_;

    juce::File getUserPresetsDirectory() const;
    void ensureUserPresetsDirectory() const;
    void loadFactoryPresets();
    void loadUserPresetsFromDisk();

    std::unique_ptr<juce::XmlElement> serializeAPVTS() const;
    bool deserializeAPVTS (const juce::XmlElement& xmlElement);

    bool loadPresetInternal (const PresetInfo& preset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
