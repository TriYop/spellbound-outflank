#include "test_runner.h"
#include "../Source/DSP/QuadraturePair.h"
#include <cmath>
#include <algorithm>

static constexpr double kPi = 3.14159265358979323846;

// Runs `freqHz` through a fresh QuadraturePair for 0.2s at `sampleRate`, measuring the
// settled-state phase (in degrees, via quadrature correlation against the known input
// sinusoid) of both branch outputs in a single pass.
struct PhasePair { double a; double b; };

static PhasePair measurePhasesDeg (double freqHz, double sampleRate)
{
    QuadraturePair pair;
    pair.prepare (sampleRate);

    const int settleSamples  = static_cast<int> (sampleRate * 0.1);
    const int measureSamples = static_cast<int> (sampleRate * 0.1);
    double sumSinA = 0.0, sumCosA = 0.0;
    double sumSinB = 0.0, sumCosB = 0.0;

    for (int i = 0; i < settleSamples + measureSamples; ++i)
    {
        const double phase = 2.0 * kPi * freqHz * i / sampleRate;
        const float in = static_cast<float> (std::sin (phase));
        const auto out = pair.process (in);
        if (i >= settleSamples)
        {
            sumSinA += out.a * std::sin (phase);
            sumCosA += out.a * std::cos (phase);
            sumSinB += out.b * std::sin (phase);
            sumCosB += out.b * std::cos (phase);
        }
    }

    PhasePair result;
    result.a = std::atan2 (sumCosA, sumSinA) * 180.0 / kPi;
    result.b = std::atan2 (sumCosB, sumSinB) * 180.0 / kPi;
    return result;
}

int main()
{
    // Each branch stays close to unity magnitude at all frequencies -- all-pass cascades
    // preserve magnitude exactly; this guards against a transcription error turning a
    // stage into something other than a pure all-pass.
    // Note: RMS-based measurement is used instead of peak-picking, because at high
    // frequencies relative to sample rate (e.g., 18kHz at 48kHz = 2.67 samples/cycle),
    // peak-picking systematically misses the true crest due to sampling, giving false
    // negatives. The analytic transfer function confirms |H| = 1.0 exactly for all-pass
    // cascades; RMS-based measurement correctly reflects this.
    {
        const double sampleRate = 48000.0;
        for (double freq : { 50.0, 500.0, 5000.0, 18000.0 })
        {
            QuadraturePair pair;
            pair.prepare (sampleRate);

            const int settleSamples  = static_cast<int> (sampleRate * 0.1);
            const int measureSamples = static_cast<int> (sampleRate * 0.1);
            double sumSqA = 0.0, sumSqB = 0.0;

            const int n = settleSamples + measureSamples;
            for (int i = 0; i < n; ++i)
            {
                const float in = static_cast<float> (std::sin (2.0 * kPi * freq * i / sampleRate));
                const auto out = pair.process (in);
                if (i >= settleSamples)
                {
                    sumSqA += static_cast<double> (out.a) * static_cast<double> (out.a);
                    sumSqB += static_cast<double> (out.b) * static_cast<double> (out.b);
                }
            }

            const double amplitudeA = std::sqrt (sumSqA / measureSamples) * std::sqrt (2.0);
            const double amplitudeB = std::sqrt (sumSqB / measureSamples) * std::sqrt (2.0);

            CHECK_MSG (amplitudeA > 0.99 && amplitudeA < 1.01, "branch A should stay near unity magnitude (RMS-based)");
            CHECK_MSG (amplitudeB > 0.99 && amplitudeB < 1.01, "branch B should stay near unity magnitude (RMS-based)");
        }
    }

    // Phase difference between the two branches stays within the verified tolerance of 90
    // degrees across the design band (40Hz-20kHz) and across sample rates real hosts commonly
    // use. Tolerance (35 degrees) matches the worst case found during coefficient derivation
    // (~28.6 degrees, see docs/superpowers/plans/2026-07-02-outflank-quadrature-widening-
    // implementation.md Task 1 derivation notes), with margin.
    {
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
        const double freqs[]       = { 40.0, 100.0, 500.0, 2000.0, 8000.0, 18000.0 };
        for (double sampleRate : sampleRates)
        {
            for (double freq : freqs)
            {
                if (freq >= sampleRate * 0.45) continue;
                const auto phases = measurePhasesDeg (freq, sampleRate);
                double diff = phases.a - phases.b;
                while (diff > 180.0) diff -= 360.0;
                while (diff < -180.0) diff += 360.0;
                CHECK_MSG (std::abs (diff - 90.0) < 35.0,
                           "phase difference between branch A and B should stay near 90 degrees");
            }
        }
    }

    TEST_SUMMARY();
    return 0;
}
