#pragma once
#include <algorithm>
#include <cmath>

// Zavalishin topology-preserving 2-pole state-variable filter — resonant lowpass
// output only. The complementary high band (input - low) is derived by the
// caller (CrossoverMS), which guarantees exact reconstruction at any Q.
//
// Plain C++, no JUCE dependency, so it can be exercised by a bare unit test
// executable (see Tests/test_statevariablefilter.cpp).
class StateVariableFilter
{
public:
    void reset() noexcept
    {
        ic1eq_ = 0.0f;
        ic2eq_ = 0.0f;
    }

    void setParameters (float frequencyHz, float q, double sampleRate) noexcept
    {
        // Same Nyquist clamp as AllpassFilter::setParameters, applied here
        // defensively: this filter's a1_ = 1/(1 + g*(g+k)) has its own real
        // pole (1 + g*(g+k) == 0) for low-Q values once g is pushed past
        // Nyquist and wraps through tan()'s periodicity, the same
        // structural issue clap-validator caught in AllpassFilter (see
        // https://github.com/TriYop/spellbound-outflank/issues/1). Not
        // observed to fail with this plugin's default Q of 0.707 (no real
        // solution to that pole at that Q), but the fix is applied
        // symmetrically since both filters share the identical unguarded
        // tan() prewarp pattern.
        const float sampleRateF = static_cast<float> (sampleRate);
        const float safeFrequencyHz = std::min (frequencyHz, 0.499f * sampleRateF);
        const float g = std::tan (kPi * safeFrequencyHz / sampleRateF);
        const float k = 1.0f / q;
        a1_ = 1.0f / (1.0f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    float processLowpass (float input) noexcept
    {
        const float v3 = input - ic2eq_;
        const float v1 = a1_ * ic1eq_ + a2_ * v3;
        const float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;
        ic1eq_ = flushDenormal (2.0f * v1 - ic1eq_);
        ic2eq_ = flushDenormal (2.0f * v2 - ic2eq_);
        return v2;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    // On silence (or any decaying signal), this filter's feedback state
    // (ic1eq_/ic2eq_) asymptotically approaches zero and passes through the
    // subnormal float range on the way -- clap-validator's
    // process-sleep-constant-mask test flags that as invalid output (see
    // https://github.com/TriYop/spellbound-outflank/issues/2). There is no
    // per-plugin-instance way to force the CPU's flush-to-zero mode in a
    // DPF plugin, so flush explicitly here instead: values below this
    // threshold are ~194dB below full scale, far beneath anything audible
    // or numerically meaningful for this filter.
    static constexpr float kDenormalThreshold = 1.0e-10f;
    static float flushDenormal (float x) noexcept
    {
        return (x > -kDenormalThreshold && x < kDenormalThreshold) ? 0.0f : x;
    }

    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
};
