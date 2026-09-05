#include "OutflankPluginAdapter.h"

#include <cstring>

START_NAMESPACE_DISTRHO

OutflankPluginAdapter::OutflankPluginAdapter()
    : Plugin(0, 0, 0) // Stage 0 stub: no parameters, no programs, no states yet
{
}

const char* OutflankPluginAdapter::getLabel() const { return "Outflank"; }
const char* OutflankPluginAdapter::getDescription() const { return "Mono-bass / wide-highs stereo crossover"; }
const char* OutflankPluginAdapter::getMaker() const { return "Spellbound"; }

const char* OutflankPluginAdapter::getLicense() const
{
    // Must be a URI (DPF only emits doap:license as a proper URI-typed
    // literal in the LV2 TTL when the string contains "://" -- a plain
    // word like "Proprietary" fails lv2lint's Plugin License test) -- see
    // Hex's DistrhoPluginLV2export.cpp note in HexPluginAdapter.cpp.
    return "https://spellbound.audio/plugins/outflank#license";
}

uint32_t OutflankPluginAdapter::getVersion() const
{
    return d_version(1, 0, 0);
}

void OutflankPluginAdapter::initParameter(uint32_t, Parameter&) {}
float OutflankPluginAdapter::getParameterValue(uint32_t) const { return 0.0f; }
void OutflankPluginAdapter::setParameterValue(uint32_t, float) {}

void OutflankPluginAdapter::activate() {}
void OutflankPluginAdapter::deactivate() {}

void OutflankPluginAdapter::run(const float** inputs, float** outputs, uint32_t frames)
{
    // Stage 0 stub: passthrough only. Task 2 wires the real CrossoverMS chain here.
    if (outputs[0] != inputs[0])
        std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
    if (outputs[1] != inputs[1])
        std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);
}

Plugin* createPlugin()
{
    return new OutflankPluginAdapter();
}

END_NAMESPACE_DISTRHO
