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
        // Same Nyquist clamp as AllpassFilter::setParameters -- and, for
        // this filter, a REAL, REACHABLE bug given the plugin's own shipped
        // parameter range, not merely a defensive/theoretical guard. This
        // filter's a1_ = 1/(1 + g*(g+k)) has real roots (1 + g*(g+k) == 0)
        // whenever k = 1/Q >= 2, i.e. Q <= 0.5 -- and OUTFLANK_PARAM_Q_MIN
        // is 0.3. Once an unclamped-past-Nyquist frequency/sampleRate ratio
        // pushes g negative, that root becomes reachable: at the shipped
        // Q_MIN (0.3) and FREQUENCY_MAX (2000Hz), measured via the exact
        // float32 recurrence, a1_ blows up to ~1302 at sampleRate=2228Hz and
        // ~1776 at sampleRate=3320Hz. Not observed to fail with this
        // plugin's DEFAULT Q of 0.707 (no real root at that Q, since
        // k=1/0.707 < 2) -- which is why clap-validator's default-parameter
        // sweep caught AllpassFilter's identical-pattern bug (see
        // https://github.com/TriYop/spellbound-outflank/issues/1) but not
        // this one -- but the fix applies equally once a low-Q/extreme-
        // sample-rate combination is reachable, which it is here.
        // Regression-tested in Tests/test_statevariablefilter.cpp.
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
    // https://github.com/TriYop/spellbound-outflank/issues/2). A CPU-level
    // flush-to-zero/denormals-are-zero mode (_MM_SET_FLUSH_ZERO_MODE /
    // _MM_SET_DENORMALS_ZERO_MODE on x86, FPCR bits on aarch64) IS available
    // and would be another way to solve this, independent of DPF -- but
    // this codebase deliberately uses the portable, per-state explicit-flush
    // pattern below instead, to avoid adding platform-specific
    // (x86-vs-aarch64) intrinsics across this plugin's 3-OS (Linux/Windows/
    // macOS) CI matrix for what a fixed-threshold flush already solves.
    // Values below this threshold are ~194dB below full scale, far beneath
    // anything audible or numerically meaningful for this filter.
    static constexpr float kDenormalThreshold = 1.0e-10f;
    static float flushDenormal (float x) noexcept
    {
        return (x > -kDenormalThreshold && x < kDenormalThreshold) ? 0.0f : x;
    }

    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
};
