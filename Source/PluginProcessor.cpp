#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout OutflankAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    return { params.begin(), params.end() };
}

// ── Constructor ───────────────────────────────────────────────────────────────
OutflankAudioProcessor::OutflankAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OutflankState", createParameterLayout())
{}

OutflankAudioProcessor::~OutflankAudioProcessor() = default;

// ── Info ──────────────────────────────────────────────────────────────────────
const juce::String OutflankAudioProcessor::getName() const { return JucePlugin_Name; }
bool OutflankAudioProcessor::acceptsMidi()  const { return false; }
bool OutflankAudioProcessor::producesMidi() const { return false; }
bool OutflankAudioProcessor::isMidiEffect() const { return false; }
double OutflankAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  OutflankAudioProcessor::getNumPrograms()              { return 1; }
int  OutflankAudioProcessor::getCurrentProgram()           { return 0; }
void OutflankAudioProcessor::setCurrentProgram (int)       {}
const juce::String OutflankAudioProcessor::getProgramName (int) { return {}; }
void OutflankAudioProcessor::changeProgramName (int, const juce::String&) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void OutflankAudioProcessor::prepareToPlay (double, int) {}
void OutflankAudioProcessor::releaseResources() {}

bool OutflankAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    return true;
}

// ── processBlock ──────────────────────────────────────────────────────────────
void OutflankAudioProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Passthrough for now — DSP wired in a later task.
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
