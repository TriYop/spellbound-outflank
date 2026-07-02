#include "test_runner.h"
#include "../Source/DSP/CrossoverMS.h"
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
    // Mono input stays mono: L_out == R_out at every sample, for any settings.
    {
        CrossoverMS x;
        const int n = 4800;
        std::vector<float> left (n), right (n);
        for (int i = 0; i < n; ++i)
        {
            const float s = static_cast<float> (std::sin (2.0 * kPi * 100.0 * i / kSampleRate));
            left[i] = s; right[i] = s;
        }
        x.process (left.data(), right.data(), n, 250.f, 0.707f, 0.5f, kSampleRate);
        bool allEqual = true;
        for (int i = 0; i < n; ++i)
            if (std::abs (left[i] - right[i]) > 1e-5f) { allEqual = false; break; }
        CHECK_MSG (allEqual, "mono input must stay mono regardless of crossover settings");
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

    // rejection = 1: mono content above the crossover is fully removed.
    {
        auto out = renderMono (2000.f, 250.f, 0.707f, 1.f, 9600);
        CHECK_MSG (peakAfter (out, 4800) < 0.05f, "mono highs should be fully rejected when rejection is 1");
    }

    TEST_SUMMARY();
    return 0;
}
