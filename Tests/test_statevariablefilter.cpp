#include "test_runner.h"
#include "../Source/DSP/StateVariableFilter.h"
#include <cmath>
#include <algorithm>
#include <random>

static constexpr double kSampleRate = 48000.0;

int main()
{
    // A lowpass filter settles to unity gain on a DC input.
    {
        StateVariableFilter f;
        f.setParameters (250.f, 0.707f, kSampleRate);
        float out = 0.f;
        for (int i = 0; i < 20000; ++i)
            out = f.processLowpass (1.0f);
        CHECK_MSG (std::abs (out - 1.0f) < 0.01f, "DC should settle near the input value");
    }

    // A sine well above the cutoff is strongly attenuated once settled.
    {
        StateVariableFilter f;
        f.setParameters (250.f, 0.707f, kSampleRate);
        float maxOut = 0.f;
        for (int i = 0; i < 48000; ++i)
        {
            const float in = std::sin (2.0 * 3.14159265358979323846 * 8000.0 * i / kSampleRate);
            const float out = f.processLowpass (in);
            if (i > 24000) maxOut = std::max (maxOut, std::abs (out));
        }
        CHECK_MSG (maxOut < 0.05f, "8kHz sine through a 250Hz lowpass should be strongly attenuated");
    }

    // Higher Q resonates more strongly right at the cutoff frequency.
    {
        auto peakAtCutoff = [] (float q)
        {
            StateVariableFilter f;
            f.setParameters (250.f, q, kSampleRate);
            float maxOut = 0.f;
            for (int i = 0; i < 48000; ++i)
            {
                const float in = std::sin (2.0 * 3.14159265358979323846 * 250.0 * i / kSampleRate);
                const float out = f.processLowpass (in);
                if (i > 24000) maxOut = std::max (maxOut, std::abs (out));
            }
            return maxOut;
        };
        const float lowQPeak  = peakAtCutoff (0.5f);
        const float highQPeak = peakAtCutoff (3.0f);
        CHECK_MSG (highQPeak > lowQPeak, "higher Q should resonate more strongly at the cutoff frequency");
    }

    // Regression test for the Nyquist-clamp fix (see issue #1): with the
    // plugin's shipped Q_MIN (0.3, so k=1/Q≈3.33 >= 2) and FREQUENCY_MAX
    // (2000Hz), 1 + g*(g+k) has real roots once an unclamped-past-Nyquist
    // frequency ratio pushes g negative -- e.g. without the clamp, a1 blows
    // up to ~1302 at fs=2228Hz and ~1776 at fs=3320Hz (both measured via the
    // exact float32 recurrence). Asserts finite output at those exact rates
    // when fed noise.
    {
        std::minstd_rand rng (54321u);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

        for (double sampleRate : { 2228.0, 3320.0 })
        {
            StateVariableFilter f;
            f.setParameters (2000.0f, 0.3f, sampleRate);

            bool allFinite = true;
            for (int i = 0; i < 500; ++i)
            {
                const float in = dist (rng);
                const float out = f.processLowpass (in);
                if (! std::isfinite (out))
                {
                    allFinite = false;
                    break;
                }
            }
            CHECK_MSG (allFinite, "StateVariableFilter output should stay finite at Q_MIN/FREQUENCY_MAX combinations that push g negative without the Nyquist clamp");
        }
    }

    // Regression test for the denormal-flush fix (see issue #2): after
    // feeding a real signal (so the feedback state is non-zero) and then
    // silence for long enough to decay past the flush threshold, the
    // filter's output must reach EXACTLY 0.0f -- not merely "doesn't
    // crash" or "is very small" -- proving flushDenormal actually engages.
    // Without the fix, the state instead decays gradually through the
    // subnormal range and does not reach bit-exact zero within any
    // reasonable sample budget (confirmed via the exact float32
    // recurrence: still -6e-45, not 0.0, after 5000 silent samples).
    {
        StateVariableFilter f;
        f.setParameters (250.f, 0.707f, kSampleRate);
        for (int i = 0; i < 4800; ++i)
            f.processLowpass (static_cast<float> (std::sin (2.0 * 3.14159265358979323846 * 250.0 * i / kSampleRate)));

        float out = 1.0f;
        for (int i = 0; i < 5000; ++i)
            out = f.processLowpass (0.0f);
        CHECK_MSG (out == 0.0f, "StateVariableFilter output should reach exactly 0.0f after enough silence (denormal flush engaged)");
    }

    TEST_SUMMARY();
    return 0;
}
