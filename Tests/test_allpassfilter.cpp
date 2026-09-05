#include "test_runner.h"
#include "../Source/DSP/AllpassFilter.h"
#include <cmath>
#include <algorithm>

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kSampleRate = 48000.0;

static float steadyStatePeak (AllpassFilter& f, double freqHz, int numSamples)
{
    float peak = 0.f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float in = static_cast<float> (std::sin (2.0 * kPi * freqHz * i / kSampleRate));
        const float out = f.process (in);
        if (i > numSamples / 2) peak = std::max (peak, std::abs (out));
    }
    return peak;
}

int main()
{
    // Magnitude stays ~1.0 at all frequencies -- the defining property of an all-pass
    // (unlike StateVariableFilter's lowpass, which rolls off above its cutoff).
    {
        const float cutoff = 1000.f;
        for (double freq : { 50.0, 1000.0, 15000.0 })
        {
            AllpassFilter f;
            f.setParameters (cutoff, kSampleRate);
            const float peak = steadyStatePeak (f, freq, 9600);
            CHECK_MSG (peak > 0.95f && peak < 1.05f,
                       "all-pass magnitude should stay near 1.0 at all frequencies");
        }
    }

    // Phase lag at the corner frequency is ~90 degrees, measured via quadrature correlation
    // against the known input sinusoid after the filter has settled into steady state.
    {
        AllpassFilter f;
        const float cutoff = 1000.f;
        f.setParameters (cutoff, kSampleRate);

        const int settleSamples  = 4800;
        const int measureSamples = 4800;
        double sumSin = 0.0, sumCos = 0.0;

        for (int i = 0; i < settleSamples + measureSamples; ++i)
        {
            const double phase = 2.0 * kPi * cutoff * i / kSampleRate;
            const float in  = static_cast<float> (std::sin (phase));
            const float out = f.process (in);
            if (i >= settleSamples)
            {
                sumSin += out * std::sin (phase);
                sumCos += out * std::cos (phase);
            }
        }

        // Once settled, output ~= cos(theta)*sin(phase) + sin(theta)*cos(phase) (unit gain,
        // confirmed by the magnitude test above), so theta = atan2(sumCos, sumSin).
        const double theta = std::atan2 (sumCos, sumSin) * 180.0 / kPi;
        CHECK_MSG (std::abs (theta - (-90.0)) < 5.0,
                   "phase lag at the corner frequency should be about -90 degrees");
    }

    // Regression test for the denormal-flush fix (see issue #2): same shape
    // as StateVariableFilter's silence-decay test -- after real excitation
    // then silence, output must reach EXACTLY 0.0f, not merely stay small.
    // Without the fix, the exact float32 recurrence shows the state still
    // sitting at -6e-45 (subnormal, not bit-exact zero) after 5000 silent
    // samples.
    {
        AllpassFilter f;
        f.setParameters (1000.f, kSampleRate);
        for (int i = 0; i < 4800; ++i)
            f.process (static_cast<float> (std::sin (2.0 * kPi * 1000.0 * i / kSampleRate)));

        float out = 1.0f;
        for (int i = 0; i < 5000; ++i)
            out = f.process (0.0f);
        CHECK_MSG (out == 0.0f, "AllpassFilter output should reach exactly 0.0f after enough silence (denormal flush engaged)");
    }

    TEST_SUMMARY();
    return 0;
}
