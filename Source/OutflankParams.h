#pragma once

// Pure conversion helper for OutflankPluginAdapter's host-parameter ->
// CrossoverMS-argument boundary. Framework-free (no DPF/DGL dependency) so
// it can be exercised by a bare CTest executable, same convention as
// Source/DSP/*.h.
namespace outflank {

// CrossoverMS::process()'s `rejection01` argument is 0..1; the host
// parameter is a 0-100% control (see DistrhoPluginInfo.h's
// OUTFLANK_PARAM_REJECTION_* range). Mathematically equivalent to the
// JUCE-era `* 0.01f`, but implemented as `/ 100.0f` to get bit-exact
// float32 results at round percentages (avoiding `0.01f`'s float32 error),
// as verified by test_outflank_parameters.cpp's exact-equality checks.
inline float rejectionPercentToUnit(float rejectionPercent) noexcept
{
    return rejectionPercent / 100.0f;
}

} // namespace outflank
