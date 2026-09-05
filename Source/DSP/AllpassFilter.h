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
        // Clamp to just under Nyquist before prewarping. Without this, a
        // corner frequency at or beyond the current sample rate's Nyquist
        // wraps through tan()'s pi-periodicity and can land exactly on this
        // filter's a_ = (1-g)/(1+g) pole (g == -1, i.e. frequencyHz/sampleRate
        // landing on 0.75 mod 1) -- e.g. QuadraturePair's fixed 22kHz corner
        // at an 8kHz sample rate gives ratio 2.75, g = tan(2.75*pi) == -1.0
        // exactly, dividing by zero. Caught by clap-validator's
        // process-varying-sample-rates test; see
        // https://github.com/TriYop/spellbound-outflank/issues/1.
        const float sampleRateF = static_cast<float> (sampleRate);
        const float safeFrequencyHz = std::min (frequencyHz, 0.499f * sampleRateF);
        const float g = std::tan (kPi * safeFrequencyHz / sampleRateF);
        a_ = (1.0f - g) / (1.0f + g);
    }

    float process (float input) noexcept
    {
        const float output = -a_ * input + x1_ + a_ * y1_;
        x1_ = input;
        y1_ = output;
        return output;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    float a_  = 0.0f;
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};
