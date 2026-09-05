#include "OutflankUI.h"

START_NAMESPACE_DISTRHO

namespace {

namespace hui = audioplugins::common::hui;

// Plain hui::Colour (0-255 aggregate, constexpr-friendly) rather than
// DGL::Color directly -- DGL::Color's constructors aren't constexpr, so
// file-scope instances of it can't be. Converted at each draw call via
// toDglColor(), same as Hex's HexUI.cpp.
inline DGL_NAMESPACE::Color toDglColor(const hui::Colour& c, float alphaScale = 1.f) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r),
                                static_cast<int>(c.g),
                                static_cast<int>(c.b),
                                (static_cast<float>(c.a) / 255.f) * alphaScale);
}

// Palette derived directly from Theme's default tokens -- no bespoke
// Outflank colors invented, per this migration's design decision (Outflank
// never had a custom LookAndFeel to begin with).
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

// Layout ported from the JUCE-era resized()/paint() (see
// Source/_juce_reference/PluginEditor.cpp): kW=320, kH=200, 3 knobs at
// kKnobSize=80 with kGap=24 between them, centered horizontally. The
// JUCE-era preset bar's vertical slot (kPresetBarH=24 at the top) is
// preserved here even before Task 4 wires the real preset bar, so the knob
// row doesn't have to move again later.
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

// Helper shared by the constructor to avoid repeating the five-line
// construct/size/position/palette/range/gesture-bracketing dance per knob.
std::unique_ptr<hui::dgl::RotaryKnob> makeKnob(OutflankUI& ui, const KnobSpec& spec)
{
    std::unique_ptr<hui::dgl::RotaryKnob> knob(new hui::dgl::RotaryKnob(&ui));
    knob->setSize(kKnobSize, kKnobSize);
    knob->setAbsolutePos(spec.x, kKnobY);
    knob->setPalette(kOutflankKnobPalette);
    knob->setRange(spec.rangeMin, spec.rangeMax);
    knob->setDefaultValue(spec.defaultValue);
    knob->setValue(spec.defaultValue);

    // Gesture bracketing follows DPF's own idiom (drag started/finished ->
    // editParameter, value changed -> setParameterValue), same as Hex.
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

constexpr KnobSpec kFrequencySpec { kParameterFrequency, kFrequencyX, OUTFLANK_PARAM_FREQUENCY_MIN, OUTFLANK_PARAM_FREQUENCY_MAX, OUTFLANK_PARAM_FREQUENCY_DEFAULT, "Frequency" };
constexpr KnobSpec kQSpec         { kParameterQ,         kQX,         OUTFLANK_PARAM_Q_MIN,         OUTFLANK_PARAM_Q_MAX,         OUTFLANK_PARAM_Q_DEFAULT,         "Q" };
constexpr KnobSpec kRejectionSpec { kParameterRejection, kRejectionX, OUTFLANK_PARAM_REJECTION_MIN, OUTFLANK_PARAM_REJECTION_MAX, OUTFLANK_PARAM_REJECTION_DEFAULT, "Rejection" };

constexpr const KnobSpec* kKnobSpecs[3] = { &kFrequencySpec, &kQSpec, &kRejectionSpec };

} // namespace

OutflankUI::OutflankUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      fFrequencyKnob(makeKnob(*this, kFrequencySpec)),
      fQKnob(makeKnob(*this, kQSpec)),
      fRejectionKnob(makeKnob(*this, kRejectionSpec))
{
    loadSharedResources();
}

void OutflankUI::parameterChanged(const uint32_t index, const float value)
{
    // Programmatic path: RotaryKnob::setValue() deliberately does not fire
    // onValueChanged, so host automation / preset recall cannot loop back
    // out to the host.
    switch (index)
    {
    case kParameterFrequency: fFrequencyKnob->setValue(value); break;
    case kParameterQ:         fQKnob->setValue(value); break;
    case kParameterRejection: fRejectionKnob->setValue(value); break;
    default: break;
    }
}

void OutflankUI::onNanoDisplay()
{
    const auto& theme = hui::defaultTheme();

    // 1. Background ---------------------------------------------------
    beginPath();
    rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(theme.windowBackground));
    fill();
    closePath();

    // 2. Title ----------------------------------------------------------
    fontFace(NANOVG_DEJAVU_SANS_TTF);
    fontSize(kTitleFontSize);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    fillColor(toDglColor(theme.text));
    text(static_cast<float>(getWidth()) * 0.5f, kTitleY, "OUTFLANK", nullptr);

    // 3. Per-knob labels --------------------------------------------------
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
