#include "test_runner.h"
#include "../Source/DSP/StateVariableFilter.h"
#include <cmath>
#include <algorithm>

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

    TEST_SUMMARY();
    return 0;
}
