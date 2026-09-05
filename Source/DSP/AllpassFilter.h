#pragma once
#include <cmath>
#include <algorithm>

// First-order digital all-pass filter (Direct Form I). Flat magnitude response at every
// frequency; phase shifts continuously from 0 degrees at DC to -180 degrees at Nyquist,
// crossing exactly -90 degrees at the corner frequency. CrossoverMS uses this to phase-rotate
// Mid energy being redirected into Side, so it decorrelates from what remains in Mid without
// changing level.
//
// Plain C++, no JUCE dependency, matching StateVariableFilter.h's convention.
class AllpassFilter
{
public:
    void reset() noexcept
    {
        x1_ = 0.0f;
        y1_ = 0.0f;
    }

    void setParameters (float frequencyHz, double sampleRate) noexcept
    {
        // Clamp to just under Nyquist before prewarping. This filter's pole
        // is at z = a_ = (1-g)/(1+g); stability requires |a_| < 1, which
        // holds iff g > 0, iff frequencyHz < Nyquist. The prewarp is only
        // valid below Nyquist in the first place -- ANY corner frequency at
        // or beyond the current sample rate's Nyquist wraps through tan()'s
        // pi-periodicity into g < 0, putting the pole at |a_| > 1 and making
        // the filter diverge (not just a single exact-boundary case: at an
        // 8kHz sample rate, 3 of QuadraturePair's 6 corners per chain are
        // unstable this way, not only the one that happens to be the
        // clearest example below). In float32 this isn't a literal division
        // by zero either -- e.g. at frequencyHz=22000/sampleRate=8000,
        // measured via the exact recurrence, g = -1.0000006 and
        // 1+g = -5.96e-07 (not exactly 0), giving a_ = -3.36e6 and the
        // filter reaching +-inf within ~6 samples through the IIR feedback,
        // not on the very first sample. Caught by clap-validator's
        // process-varying-sample-rates test; see
        // https://github.com/TriYop/spellbound-outflank/issues/1.
        // Regression-tested in Tests/test_quadraturepair.cpp.
        const float sampleRateF = static_cast<float> (sampleRate);
        const float safeFrequencyHz = std::min (frequencyHz, 0.499f * sampleRateF);
        const float g = std::tan (kPi * safeFrequencyHz / sampleRateF);
        a_ = (1.0f - g) / (1.0f + g);
    }

    float process (float input) noexcept
    {
        const float output = -a_ * input + x1_ + a_ * y1_;
        x1_ = flushDenormal (input);
        y1_ = flushDenormal (output);
        return output;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    // Same denormal-decay issue as StateVariableFilter::processLowpass: on
    // silence, y1_'s feedback settles toward zero through the subnormal
    // float range rather than snapping to it -- clap-validator's
    // process-sleep-constant-mask test flags that as invalid output (see
    // https://github.com/TriYop/spellbound-outflank/issues/2). Threshold
    // matches StateVariableFilter's (~194dB below full scale).
    static constexpr float kDenormalThreshold = 1.0e-10f;
    static float flushDenormal (float x) noexcept
    {
        return (x > -kDenormalThreshold && x < kDenormalThreshold) ? 0.0f : x;
    }

    float a_  = 0.0f;
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};
