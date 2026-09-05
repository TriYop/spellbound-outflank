#pragma once
#include "StateVariableFilter.h"
#include "QuadraturePair.h"

// Mid/Side-domain crossover. Forces everything below `frequencyHz` to mono
// (the Side low band is discarded). Above it, `rejection01` redirects Mid content
// into Side via a matched quadrature all-pass pair (not a single all-pass -- a single
// all-pass caused destructive interference in the midrange, see QuadraturePair.h)
// rather than discarding it, so widening the highs doesn't cost level.
//
// Plain C++, no JUCE dependency (see Tests/test_crossoverms.cpp). PluginProcessor
// adapts this to juce::AudioBuffer<float> via raw channel pointers.
class CrossoverMS
{
public:
    void reset() noexcept;

    // `left`/`right` are processed in place, `numSamples` samples each.
    // `rejection01` is 0..1 (0 = no effect, 1 = Mid fully redirected to Side above the crossover).
    void process (float* left, float* right, int numSamples,
                  float frequencyHz, float q, float rejection01,
                  double sampleRate) noexcept;

private:
    // Same denormal-decay issue as StateVariableFilter::processLowpass and
    // AllpassFilter::process: this one-pole smoother's state
    // (smoothedRejection_) asymptotically approaches its target and, when
    // that target is 0 (Rejection's default), decays through the subnormal
    // float range on the way rather than snapping to it -- clap-validator's
    // process-audio-denormals check flags the resulting per-sample
    // subnormal-operand slowdown. Same fixed threshold as the filters
    // (~194dB below full scale, i.e. far below any meaningful Rejection
    // value) so a converged-to-zero smoother reads back as exact 0.0f.
    static constexpr float kDenormalThreshold = 1.0e-10f;
    static float flushDenormal (float x) noexcept
    {
        return (x > -kDenormalThreshold && x < kDenormalThreshold) ? 0.0f : x;
    }

    StateVariableFilter svfM_, svfS_;
    QuadraturePair quadratureM_;
    float lastFrequencyHz_ = -1.0f;
    float lastQ_ = -1.0f;
    float smoothedRejection_ = 0.0f;
};
