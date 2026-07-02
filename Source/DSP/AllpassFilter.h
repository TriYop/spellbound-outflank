#pragma once
#include <cmath>

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
        const float g = std::tan (kPi * frequencyHz / static_cast<float> (sampleRate));
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
