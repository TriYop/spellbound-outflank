#pragma once
#include "AllpassFilter.h"
#include <array>

// Two matched chains of 6 cascaded first-order all-pass filters each, with fixed
// (sample-rate-independent target) corner frequencies chosen so the phase difference
// between the two chains' outputs stays close to 90 degrees across the full audio band
// (40Hz-20kHz) and across the sample rates real DAW hosts commonly use (44.1kHz-192kHz).
//
// This replaces a single all-pass filter, whose phase only reached 90 degrees at one
// frequency and drifted unbounded elsewhere -- CrossoverMS redirecting content through
// that single filter caused destructive interference in the midrange (up to -24dB) when
// recombined with the un-rotated Mid content. See docs/superpowers/specs/2026-07-02-
// outflank-quadrature-widening-design.md for the root-cause analysis.
//
// Coefficients were derived numerically (not hand-derived/guessed) by minimizing the
// worst-case deviation from 90 degrees jointly across {44100, 48000, 88200, 96000, 176400,
// 192000} Hz sample rates. Verified worst case ~28.6 degrees, which keeps a CrossoverMS-
// level regression sweep (crossover 150-2000Hz, Q 0.3-1.0, rejection 0.3-0.7) above ~0.66
// peak amplitude (vs. ~0.06 with the single-all-pass design it replaces).
//
// Plain C++, no JUCE dependency, matching AllpassFilter.h's convention.
class QuadraturePair
{
public:
    struct Outputs { float a; float b; };

    void reset() noexcept
    {
        for (auto& f : chainA_) f.reset();
        for (auto& f : chainB_) f.reset();
    }

    void prepare (double sampleRate) noexcept
    {
        for (size_t i = 0; i < kCornersA.size(); ++i)
            chainA_[i].setParameters (kCornersA[i], sampleRate);
        for (size_t i = 0; i < kCornersB.size(); ++i)
            chainB_[i].setParameters (kCornersB[i], sampleRate);
    }

    // Returns {outA, outB}: outA is the reference branch, outB is ~90 degrees from outA
    // at every frequency in the design band.
    Outputs process (float input) noexcept
    {
        float a = input;
        for (auto& f : chainA_) a = f.process (a);
        float b = input;
        for (auto& f : chainB_) b = f.process (b);
        return { a, b };
    }

private:
    static constexpr std::array<float, 6> kCornersA {
        74.19f, 386.861f, 2053.228f, 12328.615f, 12571.96f, 22000.0f
    };
    static constexpr std::array<float, 6> kCornersB {
        20.0f, 176.191f, 858.212f, 7054.49f, 7173.486f, 20528.414f
    };

    std::array<AllpassFilter, 6> chainA_;
    std::array<AllpassFilter, 6> chainB_;
};
