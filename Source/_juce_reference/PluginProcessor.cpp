#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout OutflankAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> ("frequency", "Frequency",
        NormalisableRange<float> (40.f, 2000.f, 1.f, 0.3f), 250.f,
        AudioParameterFloatAttributes{}.withLabel ("Hz")));

    params.push_back (std::make_unique<AudioParameterFloat> ("q", "Q",
        NormalisableRange<float> (0.3f, 4.0f, 0.01f), 0.707f));

    params.push_back (std::make_unique<AudioParameterFloat> ("rejection", "Rejection",
        NormalisableRange<float> (0.f, 100.f, 0.1f), 0.f,
        AudioParameterFloatAttributes{}.withLabel ("%")));

    return { params.begin(), params.end() };
}

// ── Constructor ───────────────────────────────────────────────────────────────
OutflankAudioProcessor::OutflankAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OutflankState", createParameterLayout()),
      presetManager_ (std::make_unique<PresetManager> (apvts))
{}

OutflankAudioProcessor::~OutflankAudioProcessor() = default;

// ── Info ──────────────────────────────────────────────────────────────────────
const juce::String OutflankAudioProcessor::getName() const { return JucePlugin_Name; }
bool OutflankAudioProcessor::acceptsMidi()  const { return false; }
bool OutflankAudioProcessor::producesMidi() const { return false; }
bool OutflankAudioProcessor::isMidiEffect() const { return false; }
double OutflankAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int OutflankAudioProcessor::getNumPrograms()
{
    return static_cast<int> (presetManager_->getPresetList().size());
}

int OutflankAudioProcessor::getCurrentProgram()
{
    return presetManager_->getCurrentPresetIndex();
}

void OutflankAudioProcessor::setCurrentProgram (int index)
{
    presetManager_->loadPreset (index);
}

const juce::String OutflankAudioProcessor::getProgramName (int index)
{
    auto presets = presetManager_->getPresetList();
    if (index >= 0 && index < static_cast<int> (presets.size()))
        return presets[static_cast<size_t> (index)].name;
    return {};
}

void OutflankAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    presetManager_->renamePreset (index, newName);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void OutflankAudioProcessor::prepareToPlay (double, int)
{
    crossover_.reset();
}
void OutflankAudioProcessor::releaseResources() {}

bool OutflankAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    return true;
}

// ── processBlock ──────────────────────────────────────────────────────────────
void OutflankAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float freq = apvts.getRawParameterValue ("frequency")->load (std::memory_order_relaxed);
    const float q    = apvts.getRawParameterValue ("q")->load (std::memory_order_relaxed);
    const float rej  = apvts.getRawParameterValue ("rejection")->load (std::memory_order_relaxed) * 0.01f;

    crossover_.process (buffer.getWritePointer (0), buffer.getWritePointer (1),
                         buffer.getNumSamples(), freq, q, rej, getSampleRate());
}

// ── State ─────────────────────────────────────────────────────────────────────
bool OutflankAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* OutflankAudioProcessor::createEditor()
{
    return new OutflankAudioProcessorEditor (*this);
}

void OutflankAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OutflankAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OutflankAudioProcessor();
}
