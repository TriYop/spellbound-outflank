#pragma once
#include "StateVariableFilter.h"

// Mid/Side-domain crossover. Forces everything below `frequencyHz` to mono
// (the Side low band is discarded) and attenuates Mid content above it by
// `rejection01` to widen the highs.
//
// Plain C++, no JUCE dependency (see Tests/test_crossoverms.cpp). PluginProcessor
// adapts this to juce::AudioBuffer<float> via raw channel pointers.
class CrossoverMS
{
public:
    void reset() noexcept;

    // `left`/`right` are processed in place, `numSamples` samples each.
    // `rejection01` is 0..1 (0 = no effect, 1 = Mid fully removed above the crossover).
    void process (float* left, float* right, int numSamples,
                  float frequencyHz, float q, float rejection01,
                  double sampleRate) noexcept;

private:
    StateVariableFilter svfM_, svfS_;
    float lastFrequencyHz_ = -1.0f;
    float lastQ_ = -1.0f;
    float smoothedRejection_ = 0.0f;
};
