#include "PluginEditor.h"

static const juce::Colour kBg     { 0xff141420 };
static const juce::Colour kLabel  { 0xffaaaacc };
static const juce::Colour kAccent { 0xff44aaee };

static constexpr int kW = 320;
static constexpr int kH = 160;
static constexpr int kKnobSize = 80;
static constexpr int kLabelH = 16;
static constexpr int kGap = 24;

void OutflankAudioProcessorEditor::addKnob (juce::Slider& s, juce::Label& l, const char* name)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    s.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff333355));
    s.setColour (juce::Slider::thumbColourId, kAccent.brighter (0.3f));
    s.setColour (juce::Slider::textBoxTextColourId, kLabel);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::FontOptions (11.f));
    l.setColour (juce::Label::textColourId, kLabel);
    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

OutflankAudioProcessorEditor::OutflankAudioProcessorEditor (OutflankAudioProcessor& p)
    : AudioProcessorEditor (&p), proc_ (p)
{
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    addKnob (frequencyKnob_, frequencyLbl_, "Frequency");
    addKnob (qKnob_,         qLbl_,         "Q");
    addKnob (rejectionKnob_, rejectionLbl_, "Rejection");

    frequencyAtt_ = std::make_unique<SA> (proc_.apvts, "frequency", frequencyKnob_);
    qAtt_         = std::make_unique<SA> (proc_.apvts, "q",         qKnob_);
    rejectionAtt_ = std::make_unique<SA> (proc_.apvts, "rejection", rejectionKnob_);

    setSize (kW, kH);
}

OutflankAudioProcessorEditor::~OutflankAudioProcessorEditor() {}

void OutflankAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kLabel);
    g.setFont (juce::FontOptions (16.f).withStyle ("Bold"));
    g.drawText ("OUTFLANK", 0, 8, kW, 20, juce::Justification::centred);
}

void OutflankAudioProcessorEditor::resized()
{
    const int totalKnobsW = kKnobSize * 3 + kGap * 2;
    int x = (kW - totalKnobsW) / 2;
    const int y = 40;

    auto place = [&] (juce::Slider& s, juce::Label& l)
    {
        s.setBounds (x, y, kKnobSize, kKnobSize);
        l.setBounds (x, y + kKnobSize, kKnobSize, kLabelH);
        x += kKnobSize + kGap;
    };

    place (frequencyKnob_, frequencyLbl_);
    place (qKnob_,         qLbl_);
    place (rejectionKnob_, rejectionLbl_);
}
