#include "CrossoverMS.h"
#include <cmath>

void CrossoverMS::reset() noexcept
{
    svfM_.reset();
    svfS_.reset();
    quadratureM_.reset();
    smoothedRejection_ = 0.0f;
    lastFrequencyHz_ = -1.0f;
    lastQ_ = -1.0f;
}

void CrossoverMS::process (float* left, float* right, int numSamples,
                            float frequencyHz, float q, float rejection01,
                            double sampleRate) noexcept
{
    if (frequencyHz != lastFrequencyHz_ || q != lastQ_)
    {
        svfM_.setParameters (frequencyHz, q, sampleRate);
        svfS_.setParameters (frequencyHz, q, sampleRate);
        quadratureM_.prepare (sampleRate);
        lastFrequencyHz_ = frequencyHz;
        lastQ_ = q;
    }

    // One-pole smoothing over ~5 ms so moving the rejection knob doesn't click.
    const float smoothCoeff = 1.0f - std::exp (-1.0f / (0.005f * static_cast<float> (sampleRate)));

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = left[i];
        const float r = right[i];
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r);

        const float mLow  = svfM_.processLowpass (m);
        const float mHigh = m - mLow;
        const float sLow  = svfS_.processLowpass (s);
        const float sHigh = s - sLow;   // sLow is intentionally discarded (forces mono bass)

        const auto quad = quadratureM_.process (mHigh);

        smoothedRejection_ = flushDenormal (smoothedRejection_ + (rejection01 - smoothedRejection_) * smoothCoeff);

        const float mHighKept  = (1.0f - smoothedRejection_) * quad.a;
        const float mHighMoved = smoothedRejection_ * quad.b;

        const float mOut = mLow + mHighKept;
        const float sOut = sHigh + mHighMoved;

        left[i]  = mOut + sOut;
        right[i] = mOut - sOut;
    }
}
