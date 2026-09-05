#pragma once

// Pure conversion helper for OutflankPluginAdapter's host-parameter ->
// CrossoverMS-argument boundary. Framework-free (no DPF/DGL dependency) so
// it can be exercised by a bare CTest executable, same convention as
// Source/DSP/*.h.
namespace outflank {

// CrossoverMS::process()'s `rejection01` argument is 0..1; the host
// parameter is a 0-100% control (see DistrhoPluginInfo.h's
// OUTFLANK_PARAM_REJECTION_* range) -- carried over verbatim from the
// JUCE-era PluginProcessor::processBlock()'s
// `apvts.getRawParameterValue("rejection")->load(...) * 0.01f`.
inline float rejectionPercentToUnit(float rejectionPercent) noexcept
{
    return rejectionPercent / 100.0f;
}

} // namespace outflank
