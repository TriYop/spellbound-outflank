#include "OutflankPluginAdapter.h"
#include "OutflankParams.h"

#include <cstring>

START_NAMESPACE_DISTRHO

OutflankPluginAdapter::OutflankPluginAdapter()
    : Plugin(kParameterCount, 0, 0) // 3 parameters, 0 programs, 0 states -- no meters, no bypass
{
}

const char* OutflankPluginAdapter::getLabel() const { return "Outflank"; }
const char* OutflankPluginAdapter::getDescription() const { return "Mono-bass / wide-highs stereo crossover"; }
const char* OutflankPluginAdapter::getMaker() const { return "Spellbound"; }

const char* OutflankPluginAdapter::getLicense() const
{
    return "https://spellbound.audio/plugins/outflank#license";
}

const char* OutflankPluginAdapter::getHomePage() const
{
    // DPF's LV2 TTL export (DistrhoPluginLV2export.cpp) only emits
    // foaf:homepage when this is non-empty; left unset, lv2lint flags a
    // "Plugin Author Homepage" WARN. See
    // https://github.com/TriYop/spellbound-outflank/issues/3.
    return "https://spellbound.audio/plugins/outflank";
}

uint32_t OutflankPluginAdapter::getVersion() const
{
    return d_version(1, 0, 0);
}

void OutflankPluginAdapter::initParameter(const uint32_t index, Parameter& parameter)
{
    switch (index)
    {
    case kParameterFrequency:
        // kParameterIsLogarithmic asks the host to use a logarithmic
        // automation-display curve, matching the JUCE-era
        // NormalisableRange<float>(40.f, 2000.f, 1.f, 0.3f) skew's intent
        // (more resolution at low frequencies). DPF's Parameter API has no
        // direct skew-factor equivalent, and Common's RotaryKnobModel
        // (Task 3) maps drag-to-value linearly like every other knob in
        // this UI kit -- so the on-screen knob's drag feel is a documented,
        // deliberate simplification versus the JUCE original; only the
        // host automation curve hint is preserved.
        parameter.hints  = kParameterIsAutomatable | kParameterIsLogarithmic;
        parameter.name   = "Frequency";
        parameter.symbol = "frequency";
        parameter.unit   = "Hz";
        parameter.ranges.def = OUTFLANK_PARAM_FREQUENCY_DEFAULT;
        parameter.ranges.min = OUTFLANK_PARAM_FREQUENCY_MIN;
        parameter.ranges.max = OUTFLANK_PARAM_FREQUENCY_MAX;
        break;
    case kParameterQ:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Q";
        parameter.symbol = "q";
        parameter.ranges.def = OUTFLANK_PARAM_Q_DEFAULT;
        parameter.ranges.min = OUTFLANK_PARAM_Q_MIN;
        parameter.ranges.max = OUTFLANK_PARAM_Q_MAX;
        break;
    case kParameterRejection:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Rejection";
        parameter.symbol = "rejection";
        parameter.unit   = "%";
        parameter.ranges.def = OUTFLANK_PARAM_REJECTION_DEFAULT;
        parameter.ranges.min = OUTFLANK_PARAM_REJECTION_MIN;
        parameter.ranges.max = OUTFLANK_PARAM_REJECTION_MAX;
        break;
    default:
        break;
    }
}

float OutflankPluginAdapter::getParameterValue(const uint32_t index) const
{
    switch (index)
    {
    case kParameterFrequency: return frequency;
    case kParameterQ:         return q;
    case kParameterRejection: return rejection;
    default:                  return 0.0f;
    }
}

void OutflankPluginAdapter::setParameterValue(const uint32_t index, const float value)
{
    switch (index)
    {
    case kParameterFrequency: frequency = value; break;
    case kParameterQ:         q = value; break;
    case kParameterRejection: rejection = value; break;
    default: break;
    }
}

void OutflankPluginAdapter::activate()
{
    crossover_.reset();
}

void OutflankPluginAdapter::deactivate()
{
    crossover_.reset();
}

void OutflankPluginAdapter::run(const float** inputs, float** outputs, uint32_t frames)
{
    // CrossoverMS::process() is in-place; run() is called with separate
    // input/output buffers whenever the host doesn't alias them, so copy in
    // first (matches the JUCE-era processBlock(), which always operated on
    // one in-place buffer).
    if (outputs[0] != inputs[0])
        std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
    if (outputs[1] != inputs[1])
        std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);

    crossover_.process(outputs[0], outputs[1], static_cast<int>(frames),
                        frequency, q, outflank::rejectionPercentToUnit(rejection),
                        getSampleRate());
}

Plugin* createPlugin()
{
    return new OutflankPluginAdapter();
}

END_NAMESPACE_DISTRHO
