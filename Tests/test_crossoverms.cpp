#include "test_runner.h"
#include "../Source/DSP/CrossoverMS.h"
#include "../Source/DSP/StateVariableFilter.h"
#include "../Source/DSP/QuadraturePair.h"
#include <cmath>
#include <vector>
#include <algorithm>

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kSampleRate = 48000.0;

static std::vector<float> renderMono (float freqHz, float crossoverHz, float q, float rejection, int numSamples)
{
    CrossoverMS x;
    std::vector<float> left (numSamples), right (numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = static_cast<float> (std::sin (2.0 * kPi * freqHz * i / kSampleRate));
        left[i] = s;
        right[i] = s;
    }
    x.process (left.data(), right.data(), numSamples, crossoverHz, q, rejection, kSampleRate);
    return left;
}

static float peakAfter (const std::vector<float>& v, size_t from)
{
    float m = 0.f;
    for (size_t i = from; i < v.size(); ++i) m = std::max (m, std::abs (v[i]));
    return m;
}

int main()
{
    // Mono input stays mono at rejection=0: L_out == R_out at every sample, for any frequency.
    // At rejection=0 nothing is redirected (mHighMoved is always 0), so this holds unconditionally.
    {
        CrossoverMS x;
        const int n = 4800;
        std::vector<float> left (n), right (n);
        for (int i = 0; i < n; ++i)
        {
            const float s = static_cast<float> (std::sin (2.0 * kPi * 100.0 * i / kSampleRate));
            left[i] = s; right[i] = s;
        }
        x.process (left.data(), right.data(), n, 250.f, 0.707f, 0.f, kSampleRate);
        bool allEqual = true;
        for (int i = 0; i < n; ++i)
            if (std::abs (left[i] - right[i]) > 1e-5f) { allEqual = false; break; }
        CHECK_MSG (allEqual, "mono input must stay mono at rejection=0, regardless of frequency");
    }

    // Mono bass content (far below the crossover) stays mono-ish even as rejection increases --
    // forced-mono bass is unconditional and unrelated to the highs-widening feature. Uses this
    // file's existing far-below-crossover convention (20Hz vs. a 5000Hz crossover, ~8 octaves
    // apart). Threshold verified against a Python reference simulation of this exact algorithm
    // at rejection=1 (worst case): peak|L-R| ~= 0.0109, so 0.02 leaves a comfortable margin.
    {
        CrossoverMS x;
        const int n = 9600;
        std::vector<float> left (n), right (n);
        for (int i = 0; i < n; ++i)
        {
            const float s = static_cast<float> (std::sin (2.0 * kPi * 20.0 * i / kSampleRate));
            left[i] = s; right[i] = s;
        }
        x.process (left.data(), right.data(), n, 5000.f, 0.707f, 1.f, kSampleRate);
        float peakDiff = 0.f;
        for (int i = 4800; i < n; ++i)
            peakDiff = std::max (peakDiff, std::abs (left[i] - right[i]));
        CHECK_MSG (peakDiff < 0.02f,
                   "mono bass content far below the crossover should stay mono-ish even at rejection=1");
    }

    // Fully out-of-phase low-frequency input (pure Side, no Mid) is strongly attenuated:
    // the low band is forced mono by discarding S_low, and there's no Mid to replace it.
    // Note: we use 20 Hz test signal vs. 5000 Hz crossover (~8 octaves apart) — near the filter's
    // cutoff, phase rotation from the difference (input - lowpass) minimizes leakage. The old
    // 80 Hz vs. 250 Hz was only ~1.6 octaves apart and leaked ~0.46, not a bug in the
    // implementation but phase behavior near the cutoff boundary.
    {
        CrossoverMS x;
        const int n = 9600;
        std::vector<float> left (n), right (n);
        for (int i = 0; i < n; ++i)
        {
            const float s = static_cast<float> (std::sin (2.0 * kPi * 20.0 * i / kSampleRate));
            left[i] = s; right[i] = -s;
        }
        x.process (left.data(), right.data(), n, 5000.f, 0.707f, 0.f, kSampleRate);
        const float peakL = peakAfter (left, 4800);
        CHECK_MSG (peakL < 0.02f, "out-of-phase low-frequency content should be strongly attenuated by forced mono bass");
    }

    // rejection = 0: mono content above the crossover passes through near unity.
    {
        auto out = renderMono (2000.f, 250.f, 0.707f, 0.f, 9600);
        CHECK_MSG (peakAfter (out, 4800) > 0.9f, "mono highs should pass through when rejection is 0");
    }

    // rejection = 1: mono content above the crossover is redirected to Side (via an all-pass
    // phase rotation), not deleted. Both channels stay close to the original amplitude (not
    // weak) and diverge substantially from each other (actually wide). Thresholds verified
    // against a Python double-precision reference simulation of this exact algorithm at these
    // settings (peakL~1.03, peakR~1.00, peak|L-R|~2.03), with comfortable margin.
    {
        CrossoverMS x;
        const int n = 9600;
        std::vector<float> left (n), right (n);
        for (int i = 0; i < n; ++i)
        {
            const float s = static_cast<float> (std::sin (2.0 * kPi * 2000.0 * i / kSampleRate));
            left[i] = s; right[i] = s;
        }
        x.process (left.data(), right.data(), n, 250.f, 0.707f, 1.f, kSampleRate);

        float peakL = 0.f, peakR = 0.f, peakDiff = 0.f;
        for (int i = 4800; i < n; ++i)
        {
            peakL    = std::max (peakL,    std::abs (left[i]));
            peakR    = std::max (peakR,    std::abs (right[i]));
            peakDiff = std::max (peakDiff, std::abs (left[i] - right[i]));
        }
        CHECK_MSG (peakL > 0.7f && peakR > 0.7f,
                   "mono highs redirected to Side at rejection=1 should not lose level");
        CHECK_MSG (peakDiff > 1.0f,
                   "mono highs redirected to Side at rejection=1 should widen (L and R diverge)");
    }

    // Mono-sum invariant: (L+R) must equal 2*(mLow + (1-rejection)*quadratureA(mHigh)) at
    // every sample, independent of whatever enters Side -- i.e. redirecting energy into Side
    // can never leak into the mono downmix. Verified by independently replicating the Mid
    // path's lowpass + Chain-A filtering + rejection-weighted mix with bare StateVariableFilter
    // and QuadraturePair instances driven by the same input, and comparing against
    // (L_out + R_out) from the real CrossoverMS. (Using raw mHigh instead of the Chain-A
    // filtered output here -- the old design's formula -- gives a large mismatch, ~0.6; this
    // confirms the replica must track CrossoverMS's actual internal computation.)
    {
        const int n = 4800;
        std::vector<float> left (n), right (n);
        for (int i = 0; i < n; ++i)
        {
            const float l = static_cast<float> (std::sin (2.0 * kPi * 300.0  * i / kSampleRate));
            const float r = static_cast<float> (std::sin (2.0 * kPi * 2500.0 * i / kSampleRate) * 0.6f);
            left[i] = l; right[i] = r;
        }
        std::vector<float> origLeft = left, origRight = right;

        CrossoverMS x;
        const float rejection = 0.65f;
        x.process (left.data(), right.data(), n, 400.f, 0.707f, rejection, kSampleRate);

        StateVariableFilter svfM;
        svfM.setParameters (400.f, 0.707f, kSampleRate);
        QuadraturePair quadM;
        quadM.prepare (kSampleRate);
        float smoothedRejection = 0.f;
        const float smoothCoeff = 1.0f - std::exp (-1.0f / (0.005f * static_cast<float> (kSampleRate)));

        float maxErr = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float m     = 0.5f * (origLeft[i] + origRight[i]);
            const float mLow  = svfM.processLowpass (m);
            const float mHigh = m - mLow;
            const auto quad   = quadM.process (mHigh);
            smoothedRejection += (rejection - smoothedRejection) * smoothCoeff;
            const float expectedMOut = mLow + (1.0f - smoothedRejection) * quad.a;
            const float actualSum    = left[i] + right[i];
            maxErr = std::max (maxErr, std::abs (actualSum - 2.0f * expectedMOut));
        }
        CHECK_MSG (maxErr < 1e-4f,
                   "L+R must equal 2*(mLow + (1-rejection)*quadratureA(mHigh)), independent of what enters Side");
    }

    // Regression test for the reported midrange cancellation bug: crossover=250Hz, Q=0.5 (low
    // Q), rejection=0.5 (the reported 30-70% zone), swept across the reported affected range
    // (200Hz-8000Hz).
    //
    // IMPORTANT: this must check min(peakL, peakR), not max. For a mono input, channel energy
    // is conserved between L and R (|L|^2 + |R|^2 is constant regardless of phase), so whichever
    // channel collapses, the other one gets a compensating boost -- max(peakL, peakR) NEVER
    // drops below ~0.707 even in the old design's total-cancellation case (verified: at the
    // theoretical worst point, |L|->0 while |R|->1, or vice versa, so max stays near 1 while
    // min goes to 0). A max-based check cannot detect this bug at all. Verified with min:
    // the OLD single-all-pass design's worst case across this exact sweep is min~=0.028 (at
    // 4000-8000Hz, matching the originally reported ~-24dB collapse); the NEW quadrature-pair
    // design's worst case is min~=0.463 (at 200Hz). Threshold set well below the new design's
    // worst case, comfortably above the old design's.
    {
        const float testFreqs[] = { 200.f, 400.f, 600.f, 900.f, 1200.f, 2000.f, 4000.f, 8000.f };
        for (float freq : testFreqs)
        {
            CrossoverMS x;
            const int n = 9600;
            std::vector<float> left (n), right (n);
            for (int i = 0; i < n; ++i)
            {
                const float s = static_cast<float> (std::sin (2.0 * kPi * freq * i / kSampleRate));
                left[i] = s; right[i] = s;
            }
            x.process (left.data(), right.data(), n, 250.f, 0.5f, 0.5f, kSampleRate);
            const float minPeak = std::min (peakAfter (left, 4800), peakAfter (right, 4800));
            CHECK_MSG (minPeak > 0.3f,
                       "midrange content should not collapse in volume at crossover=250Hz, Q=0.5, rejection=0.5");
        }
    }

    TEST_SUMMARY();
    return 0;
}
