#include "PluginEditor.h"

static const juce::Colour kBg     { 0xff141420 };
static const juce::Colour kLabel  { 0xffaaaacc };
static const juce::Colour kAccent { 0xff44aaee };

static constexpr int kW = 320;
static constexpr int kH = 200;
static constexpr int kPresetBarH = 24;
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

    addAndMakeVisible (presetSelector_);
    addAndMakeVisible (saveAsButton_);
    addAndMakeVisible (deleteButton_);
    presetSelector_.onChange = [this] { onPresetSelected(); };
    saveAsButton_.onClick    = [this] { onSaveAsPressed();  };
    deleteButton_.onClick    = [this] { onDeletePressed();  };
    updatePresetList();

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
    g.drawText ("OUTFLANK", 0, 36, kW, 20, juce::Justification::centred);
}

void OutflankAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().reduced (8);

    // Preset bar: selector | SAVE AS | DELETE
    auto row = b.removeFromTop (kPresetBarH);
    presetSelector_.setBounds (row.removeFromLeft (148));
    row.removeFromLeft (8);
    saveAsButton_.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (8);
    deleteButton_.setBounds (row.removeFromLeft (70));

    const int totalKnobsW = kKnobSize * 3 + kGap * 2;
    int x = (kW - totalKnobsW) / 2;
    const int y = kPresetBarH + 48;

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

// ── Preset management ────────────────────────────────────────────────────────
void OutflankAudioProcessorEditor::updatePresetList()
{
    presetSelector_.clear (juce::dontSendNotification);
    auto* pm = proc_.getPresetManager();
    if (!pm) return;

    auto presets = pm->getPresetList();
    for (size_t i = 0; i < presets.size(); ++i)
        presetSelector_.addItem (presets[i].name, static_cast<int> (i + 1));

    int idx = pm->getCurrentPresetIndex();
    presetSelector_.setSelectedItemIndex (idx, juce::dontSendNotification);
    bool isFactory = (idx >= 0 && idx < (int) presets.size()) ? presets[(size_t) idx].isFactory : true;
    deleteButton_.setEnabled (!isFactory);
}

void OutflankAudioProcessorEditor::onPresetSelected()
{
    int idx = presetSelector_.getSelectedItemIndex();
    if (idx >= 0) proc_.setCurrentProgram (idx);
    auto* pm = proc_.getPresetManager();
    if (!pm) return;
    auto presets = pm->getPresetList();
    bool isFactory = (idx >= 0 && idx < (int) presets.size()) ? presets[(size_t) idx].isFactory : true;
    deleteButton_.setEnabled (!isFactory);
}

void OutflankAudioProcessorEditor::onSaveAsPressed()
{
    auto* w = new juce::AlertWindow ("Save Preset As", "Enter preset name:", juce::AlertWindow::NoIcon);
    w->addTextEditor ("name", "", "Preset name:");
    w->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<OutflankAudioProcessorEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result) {
        std::unique_ptr<juce::AlertWindow> owned (w);
        if (result == 1 && safe != nullptr)
        {
            auto name = w->getTextEditorContents ("name").trim();
            if (name.isNotEmpty())
            {
                auto* pm = safe->proc_.getPresetManager();
                if (pm && pm->savePreset (name)) safe->updatePresetList();
            }
        }
    }));
}

void OutflankAudioProcessorEditor::onDeletePressed()
{
    auto* pm = proc_.getPresetManager();
    if (!pm) return;
    int idx = pm->getCurrentPresetIndex();
    auto presets = pm->getPresetList();
    if (idx < 0 || idx >= (int) presets.size() || presets[(size_t) idx].isFactory) return;

    juce::Component::SafePointer<OutflankAudioProcessorEditor> safe (this);
    juce::AlertWindow::showOkCancelBox (
        juce::AlertWindow::WarningIcon, "Delete Preset",
        "Delete preset '" + presets[(size_t) idx].name + "'?", "Delete", "Cancel", nullptr,
        juce::ModalCallbackFunction::create ([safe, idx] (int result) {
            if (result == 1 && safe != nullptr)
            {
                auto* pm2 = safe->proc_.getPresetManager();
                if (pm2 && pm2->deletePreset (idx))
                {
                    // deletePreset() only updates PresetManager's bookkeeping index when the
                    // deleted preset was active — it does not reload APVTS. Without this call,
                    // the combo box would show a fallback preset selected while the knobs/audio
                    // still reflected the just-deleted preset's values.
                    safe->proc_.setCurrentProgram (pm2->getCurrentPresetIndex());
                    safe->updatePresetList();
                }
            }
        }));
}
