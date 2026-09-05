#include "OutflankUI.h"
#include "FactoryPresets.h"

#include <cstdlib>
#include <string>

START_NAMESPACE_DISTRHO

namespace {

namespace hui = audioplugins::common::hui;

hui::Colour withAlpha(hui::Colour c, uint8_t a) noexcept
{
    c.a = a;
    return c;
}

inline DGL_NAMESPACE::Color toDglColor(const hui::Colour& c, float alphaScale = 1.f) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r),
                                static_cast<int>(c.g),
                                static_cast<int>(c.b),
                                (static_cast<float>(c.a) / 255.f) * alphaScale);
}

// All palettes derived directly from Theme's default tokens -- no bespoke
// Outflank colors invented, per this migration's design decision.
const hui::dgl::RotaryKnobPalette kOutflankKnobPalette = [] {
    const auto& theme = hui::defaultTheme();
    hui::dgl::RotaryKnobPalette p;
    p.track = theme.widgetBackground;
    p.valueArc = theme.accent;
    p.valueArcGlow = theme.accent;
    p.knobTop = theme.windowBackground;
    p.knobBottom = theme.widgetBackground;
    p.knobRim = theme.accent;
    return p;
}();

const hui::dgl::ButtonPalette kOutflankButtonPalette = [] {
    const auto& theme = hui::defaultTheme();
    hui::dgl::ButtonPalette p;
    p.background = theme.widgetBackground;
    p.backgroundDisabled = theme.windowBackground;
    p.border = theme.accent;
    p.text = theme.text;
    p.textDisabled = withAlpha(theme.text, 0x80);
    return p;
}();

const hui::dgl::PresetSelectorPalette kOutflankPresetSelectorPalette = [] {
    const auto& theme = hui::defaultTheme();
    hui::dgl::PresetSelectorPalette p;
    p.closedBackground = theme.widgetBackground;
    p.listBackground = theme.windowBackground;
    p.border = theme.accent;
    p.text = theme.text;
    p.textFactory = withAlpha(theme.text, 0x80);
    p.rowHighlight = withAlpha(theme.accent, 0x40);
    return p;
}();

// Layout ported from the JUCE-era resized()/paint() (see
// Source/_juce_reference/PluginEditor.cpp): kW=320, kH=200. Preset bar
// (selector | SAVE | DELETE) occupies an 8px-margin top row, matching the
// JUCE original's `getLocalBounds().reduced(8)` + `removeFromTop(24)`
// exactly (148 + 8 + 70 + 8 + 70 = 304 = 320 - 2*8).
constexpr float kMarginX = 8.0f;
constexpr float kPresetBarY = 8.0f;
constexpr uint kPresetBarRowH = 24;
constexpr uint kPresetSelectorW = 148;
constexpr uint kPresetButtonW = 70;
constexpr float kPresetButtonGap = 8.0f;
constexpr float kSaveButtonX = kMarginX + static_cast<float>(kPresetSelectorW) + kPresetButtonGap;
constexpr float kDeleteButtonX = kSaveButtonX + static_cast<float>(kPresetButtonW) + kPresetButtonGap;

constexpr float kTitleY = 46.0f;
constexpr float kTitleFontSize = 16.0f;

constexpr int kPresetBarH = 24;
constexpr int kKnobY = kPresetBarH + 48; // 72, matches JUCE original
constexpr uint kKnobSize = 80;
constexpr int kKnobGap = 24;
constexpr int kLabelY = kKnobY + static_cast<int>(kKnobSize) + 4;
constexpr int kLabelH = 16;
constexpr float kLabelFontSize = 11.0f;

constexpr int kTotalKnobsW = static_cast<int>(kKnobSize) * 3 + kKnobGap * 2;
constexpr int kRowX0 = (DISTRHO_UI_DEFAULT_WIDTH - kTotalKnobsW) / 2;
constexpr int kFrequencyX = kRowX0;
constexpr int kQX         = kFrequencyX + static_cast<int>(kKnobSize) + kKnobGap;
constexpr int kRejectionX = kQX + static_cast<int>(kKnobSize) + kKnobGap;

struct KnobSpec
{
    uint32_t parameterIndex;
    int x;
    float rangeMin, rangeMax, defaultValue;
    const char* label;
};

constexpr KnobSpec kFrequencySpec { kParameterFrequency, kFrequencyX, OUTFLANK_PARAM_FREQUENCY_MIN, OUTFLANK_PARAM_FREQUENCY_MAX, OUTFLANK_PARAM_FREQUENCY_DEFAULT, "Frequency" };
constexpr KnobSpec kQSpec         { kParameterQ,         kQX,         OUTFLANK_PARAM_Q_MIN,         OUTFLANK_PARAM_Q_MAX,         OUTFLANK_PARAM_Q_DEFAULT,         "Q" };
constexpr KnobSpec kRejectionSpec { kParameterRejection, kRejectionX, OUTFLANK_PARAM_REJECTION_MIN, OUTFLANK_PARAM_REJECTION_MAX, OUTFLANK_PARAM_REJECTION_DEFAULT, "Rejection" };

constexpr const KnobSpec* kKnobSpecs[3] = { &kFrequencySpec, &kQSpec, &kRejectionSpec };

std::unique_ptr<hui::dgl::RotaryKnob> makeKnob(OutflankUI& ui, const KnobSpec& spec)
{
    std::unique_ptr<hui::dgl::RotaryKnob> knob(new hui::dgl::RotaryKnob(&ui));
    knob->setSize(kKnobSize, kKnobSize);
    knob->setAbsolutePos(spec.x, kKnobY);
    knob->setPalette(kOutflankKnobPalette);
    knob->setRange(spec.rangeMin, spec.rangeMax);
    knob->setDefaultValue(spec.defaultValue);
    knob->setValue(spec.defaultValue);

    const uint32_t paramIndex = spec.parameterIndex;
    hui::dgl::RotaryKnob* const rawKnob = knob.get();
    rawKnob->onDragStateChanged = [&ui, paramIndex](const bool started)
    {
        ui.editParameter(paramIndex, started);
    };
    rawKnob->onValueChanged = [&ui, paramIndex](const float value)
    {
        ui.setParameterValue(paramIndex, value);
    };

    return knob;
}

std::unique_ptr<hui::dgl::PresetSelector> makePresetSelector(OutflankUI& ui)
{
    std::unique_ptr<hui::dgl::PresetSelector> selector(new hui::dgl::PresetSelector(&ui));
    selector->setPalette(kOutflankPresetSelectorPalette);
    selector->setClosedSize(kPresetSelectorW, kPresetBarRowH);
    selector->setAbsolutePos(static_cast<int>(kMarginX), static_cast<int>(kPresetBarY));
    return selector;
}

std::unique_ptr<hui::dgl::Button> makeButton(OutflankUI& ui, const char* label, float x)
{
    std::unique_ptr<hui::dgl::Button> button(new hui::dgl::Button(&ui));
    button->setPalette(kOutflankButtonPalette);
    button->setLabel(label);
    button->setSize(kPresetButtonW, kPresetBarRowH);
    button->setAbsolutePos(static_cast<int>(x), static_cast<int>(kPresetBarY));
    return button;
}

// Linux-only for now, matching Hex's hexUserPresetsDirectory() and the
// JUCE-era PresetManager::getUserPresetsDirectory()'s Linux branch
// (~/.config/<Name>/presets).
std::string outflankUserPresetsDirectory()
{
    const char* home = std::getenv("HOME");
    if (home == nullptr)
        return "/tmp/Outflank/presets"; // extremely unlikely fallback, still functional
    return std::string(home) + "/.config/Outflank/presets";
}

} // namespace

OutflankUI::OutflankUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      fFrequencyKnob(makeKnob(*this, kFrequencySpec)),
      fQKnob(makeKnob(*this, kQSpec)),
      fRejectionKnob(makeKnob(*this, kRejectionSpec)),
      fPresetBrowser(outflankFactoryPresets(), outflankUserPresetsDirectory(), "com.spellbound.outflank"),
      fPresetSelector(makePresetSelector(*this)),
      fSaveButton(makeButton(*this, "SAVE", kSaveButtonX)),
      fDeleteButton(makeButton(*this, "DELETE", kDeleteButtonX))
{
    loadSharedResources();

    fPresetSelector->onIndexSelected = [this](const int index)
    {
        if (const auto* preset = fPresetBrowser.selectIndex(index))
        {
            applyPreset(*preset);
            refreshPresetControls();
        }
    };

    fDeleteButton->onClick = [this]()
    {
        if (fPresetBrowser.deleteCurrent())
            refreshPresetControls();
    };

    fSaveButton->onClick = [this]()
    {
        const std::string startDir = outflankUserPresetsDirectory();
        FileBrowserOptions options;
        options.saving = true;
        options.defaultName = "New Preset.xml";
        options.title = "Save Outflank Preset";
        options.startDir = startDir.c_str();
        openFileBrowser(options);
    };

    refreshPresetControls();
}

void OutflankUI::uiFileBrowserSelected(const char* filename)
{
    if (filename == nullptr)
        return; // user cancelled the dialog

    std::string path(filename);
    const size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos)
        base = base.substr(0, dot);

    if (fPresetBrowser.saveAs(base, captureCurrentParameters()))
        refreshPresetControls();
}

void OutflankUI::parameterChanged(const uint32_t index, const float value)
{
    switch (index)
    {
    case kParameterFrequency: fFrequencyKnob->setValue(value); break;
    case kParameterQ:         fQKnob->setValue(value); break;
    case kParameterRejection: fRejectionKnob->setValue(value); break;
    default: break;
    }
}

void OutflankUI::applyPreset(const audioplugins::common::presets::Preset& preset)
{
    for (const auto& pv : preset.parameters)
    {
        uint32_t paramIndex;

        if (pv.id == "frequency")      { fFrequencyKnob->setValue(pv.value); paramIndex = kParameterFrequency; }
        else if (pv.id == "q")         { fQKnob->setValue(pv.value);         paramIndex = kParameterQ; }
        else if (pv.id == "rejection") { fRejectionKnob->setValue(pv.value); paramIndex = kParameterRejection; }
        else continue; // unknown id (forward-compatible with a future schema addition) -- ignore

        editParameter(paramIndex, true);
        setParameterValue(paramIndex, pv.value);
        editParameter(paramIndex, false);
    }
}

std::vector<audioplugins::common::presets::ParameterValue> OutflankUI::captureCurrentParameters() const
{
    return {
        {"frequency", fFrequencyKnob->getValue()},
        {"q", fQKnob->getValue()},
        {"rejection", fRejectionKnob->getValue()},
    };
}

void OutflankUI::refreshPresetControls()
{
    fPresetSelector->setEntries(fPresetBrowser.getEntries());
    fPresetSelector->setCurrentIndex(fPresetBrowser.getCurrentIndex());

    const auto entries = fPresetBrowser.getEntries();
    const int idx = fPresetBrowser.getCurrentIndex();
    const bool isFactory = (idx >= 0 && static_cast<size_t>(idx) < entries.size()) ? entries[static_cast<size_t>(idx)].isFactory : true;
    fDeleteButton->setEnabled(!isFactory);
}

void OutflankUI::onNanoDisplay()
{
    const auto& theme = hui::defaultTheme();

    beginPath();
    rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(theme.windowBackground));
    fill();
    closePath();

    fontFace(NANOVG_DEJAVU_SANS_TTF);
    fontSize(kTitleFontSize);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    fillColor(toDglColor(theme.text));
    text(static_cast<float>(getWidth()) * 0.5f, kTitleY, "OUTFLANK", nullptr);

    fontSize(kLabelFontSize);
    for (int i = 0; i < 3; ++i)
    {
        const float cx = static_cast<float>(kKnobSpecs[i]->x) + static_cast<float>(kKnobSize) * 0.5f;
        text(cx, static_cast<float>(kLabelY) + static_cast<float>(kLabelH) * 0.5f, kKnobSpecs[i]->label, nullptr);
    }
}

UI* createUI()
{
    return new OutflankUI();
}

END_NAMESPACE_DISTRHO
