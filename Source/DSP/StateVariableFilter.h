#pragma once
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
        const float g = std::tan (kPi * frequencyHz / static_cast<float> (sampleRate));
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
        ic1eq_ = 2.0f * v1 - ic1eq_;
        ic2eq_ = 2.0f * v2 - ic2eq_;
        return v2;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
};
