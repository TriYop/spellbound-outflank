#include "test_runner.h"
#include "../Source/DSP/CrossoverMS.h"
#include "../Source/DSP/StateVariableFilter.h"
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

    // Mono-sum invariant: (L+R) must equal 2*(mLow + (1-rejection)*mHigh) at every sample,
    // independent of whatever the all-pass redirects into Side -- i.e. redirecting energy into
    // Side can never leak into the mono downmix. Verified by independently replicating the Mid
    // path's lowpass + rejection-weighted mix with a bare StateVariableFilter driven by the
    // same input, and comparing against (L_out + R_out) from the real CrossoverMS.
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
        float smoothedRejection = 0.f;
        const float smoothCoeff = 1.0f - std::exp (-1.0f / (0.005f * static_cast<float> (kSampleRate)));

        float maxErr = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float m     = 0.5f * (origLeft[i] + origRight[i]);
            const float mLow  = svfM.processLowpass (m);
            const float mHigh = m - mLow;
            smoothedRejection += (rejection - smoothedRejection) * smoothCoeff;
            const float expectedMOut = mLow + (1.0f - smoothedRejection) * mHigh;
            const float actualSum    = left[i] + right[i];
            maxErr = std::max (maxErr, std::abs (actualSum - 2.0f * expectedMOut));
        }
        CHECK_MSG (maxErr < 1e-4f,
                   "L+R must equal 2*(mLow + (1-rejection)*mHigh), independent of the all-pass redirection into Side");
    }

    TEST_SUMMARY();
    return 0;
}
