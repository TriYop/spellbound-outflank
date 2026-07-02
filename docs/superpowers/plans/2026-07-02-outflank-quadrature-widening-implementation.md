# Outflank Quadrature-Pair Widening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the midrange volume/phasing bug in `rejection` by replacing the single all-pass filter with a matched quadrature-pair all-pass network, so the phase difference between the Mid-kept and Side-moved paths stays near 90° across the whole audio band instead of drifting unbounded.

**Architecture:** A new `QuadraturePair` DSP class owns two matched chains of 6 cascaded first-order `AllpassFilter` instances each, with fixed corner frequencies (not tied to the crossover knob) derived by numerical optimization and verified in this plan. `CrossoverMS` feeds `mHigh` through both chains and applies the rejection-weighted split to their two outputs instead of to raw `mHigh`.

**Tech Stack:** C++20, plain C++ DSP layer (no JUCE dependency), same `Tests/test_runner.h` `CHECK`-macro harness used throughout this project.

**Reference:** `docs/superpowers/specs/2026-07-02-outflank-quadrature-widening-design.md`

## Global Constraints

- No new APVTS parameter — same 3-knob interface; `PluginProcessor.h/.cpp` and `PluginEditor.h/.cpp` are not touched by this plan.
- `QuadraturePair` and the updated `CrossoverMS` remain plain C++ with no JUCE dependency.
- `CrossoverMS::process`'s public signature is unchanged: `void process(float* left, float* right, int numSamples, float frequencyHz, float q, float rejection01, double sampleRate) noexcept`.
- Mono downmix (`L + R = 2·mOut`) must remain unaffected by whatever enters `sOut` — structural from the M/S decode, regression-tested.
- `rejection=0` is no longer bit-for-bit identical to the pre-widening-feature passthrough (Mid content now always passes through Chain A), but stays audibly identical: all-pass filtering preserves magnitude exactly, verified numerically (peak amplitude 1.017 vs. the old passthrough's ~1.0, well within the existing `>0.9` test threshold — see Task 2).
- Quadrature-pair coefficients (corner frequencies) were derived numerically, not guessed: 6 first-order all-pass stages per branch, corner frequencies chosen by minimizing the worst-case deviation from 90° jointly across `{44100, 48000, 88200, 96000, 176400, 192000}` Hz sample rates and a 40 Hz–20 kHz log-spaced frequency sweep. Verified worst case: **~28.6° deviation from 90°** (looser than the spec's aspirational "few degrees," because the spec didn't originally account for needing robustness across all real host sample rates — see Task 1's derivation notes). This still resolves the reported bug with a large margin: a broad sweep (crossover 150–2000 Hz, Q 0.3–1.0, rejection 0.3–0.7, 3 sample rates) found a worst-case peak amplitude of **0.663** (about −3.6 dB), versus **~0.06** (about −24 dB) with the single-all-pass design being replaced.
- Final derived corner frequencies (Hz), to be hard-coded as `static constexpr` values:
  - Chain A: `74.19, 386.861, 2053.228, 12328.615, 12571.96, 22000.0`
  - Chain B: `20.0, 176.191, 858.212, 7054.49, 7173.486, 20528.414`

---

### Task 1: QuadraturePair DSP unit (TDD)

**Files:**
- Create: `Source/DSP/QuadraturePair.h`
- Create: `Tests/test_quadraturepair.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `AllpassFilter` (`reset()`, `setParameters(float frequencyHz, double sampleRate)`, `float process(float input)`) — already exists in `Source/DSP/AllpassFilter.h`.
- Produces: `class QuadraturePair` with `void reset() noexcept`, `void prepare(double sampleRate) noexcept`, and `Outputs process(float input) noexcept` where `struct Outputs { float a; float b; };`. No JUCE dependency. Consumed by `CrossoverMS` in Task 2.

- [ ] **Step 1: Write the failing test `Tests/test_quadraturepair.cpp`**

```cpp
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
    //
    // Measured via RMS over a settled window, not simple peak-picking (max(abs(samples))).
    // At 18kHz/48kHz there are only 2.67 samples per cycle, so naive peak-picking can miss
    // the true crest by a wide margin (measured ~0.925 instead of the true 1.0 -- confirmed
    // both analytically, via the exact complex transfer function, and via this RMS method,
    // which gives an exact 1.00000 at every frequency tested here). RMS*sqrt(2) is the
    // correct amplitude estimator for an undersampled sinusoid; peak-picking is not.
    {
        const double sampleRate = 48000.0;
        for (double freq : { 50.0, 500.0, 5000.0, 18000.0 })
        {
            QuadraturePair pair;
            pair.prepare (sampleRate);
            const int settleSamples  = static_cast<int> (sampleRate * 0.1);
            const int measureSamples = static_cast<int> (sampleRate * 0.1);
            double sumSqA = 0.0, sumSqB = 0.0;
            for (int i = 0; i < settleSamples + measureSamples; ++i)
            {
                const float in = static_cast<float> (std::sin (2.0 * kPi * freq * i / sampleRate));
                const auto out = pair.process (in);
                if (i >= settleSamples)
                {
                    sumSqA += static_cast<double> (out.a) * out.a;
                    sumSqB += static_cast<double> (out.b) * out.b;
                }
            }
            const double amplitudeA = std::sqrt (sumSqA / measureSamples) * std::sqrt (2.0);
            const double amplitudeB = std::sqrt (sumSqB / measureSamples) * std::sqrt (2.0);
            CHECK_MSG (amplitudeA > 0.95 && amplitudeA < 1.05, "branch A should stay near unity magnitude");
            CHECK_MSG (amplitudeB > 0.95 && amplitudeB < 1.05, "branch B should stay near unity magnitude");
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
```

- [ ] **Step 2: Add the test target to `CMakeLists.txt`** (append after the existing `test_allpassfilter` block, before `# ── Install rules`)

```cmake
add_executable(test_quadraturepair
    Tests/test_quadraturepair.cpp
)
target_include_directories(test_quadraturepair PRIVATE Source/ Tests/)
target_compile_features(test_quadraturepair PRIVATE cxx_std_20)
add_test(NAME QuadraturePair COMMAND test_quadraturepair)
```

- [ ] **Step 3: Configure and attempt to build the test target — confirm it fails (header missing)**

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target test_quadraturepair`
Expected: FAIL — `Source/DSP/QuadraturePair.h` does not exist yet.

- [ ] **Step 4: Write `Source/DSP/QuadraturePair.h`**

```cpp
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
```

- [ ] **Step 5: Build and run the test — confirm it passes**

Run: `cmake --build build --target test_quadraturepair && ctest --test-dir build -R QuadraturePair --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 6: Commit**

```bash
git add Source/DSP/QuadraturePair.h Tests/test_quadraturepair.cpp CMakeLists.txt
git commit -m "Add QuadraturePair DSP unit with passing tests"
```

---

### Task 2: Wire QuadraturePair into CrossoverMS, fixing the midrange cancellation bug

**Files:**
- Modify: `Source/DSP/CrossoverMS.h`
- Modify: `Source/DSP/CrossoverMS.cpp`
- Modify: `Tests/test_crossoverms.cpp`

**Interfaces:**
- Consumes: `QuadraturePair` (`reset()`, `prepare(sampleRate)`, `process(input) -> Outputs{a, b}`) from Task 1.
- `CrossoverMS::process`'s public signature is unchanged.

- [ ] **Step 1: Update `Tests/test_crossoverms.cpp` first (RED).** Two changes:

**(a)** Add `#include "../Source/DSP/QuadraturePair.h"` to the top of the file (needed by the corrected mono-sum invariant test below), alongside the existing includes.

**(b)** The mono-sum invariant test's independent replica currently computes its expected `mOut` from raw `mHigh` — that's the *old* formula. Under the new design, `mOut` is computed from `mHigh` filtered through `QuadraturePair`'s Chain A, not raw `mHigh` (verified: using raw `mHigh` in the replica gives a large, incorrect mismatch of ~0.6; using the properly filtered Chain A output gives an exact match to floating-point noise, ~1e-16). Replace:

```cpp
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
```

with:

```cpp
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
    // (200Hz-8000Hz). The old single-all-pass design dropped as low as ~0.06 peak amplitude
    // here (~-24dB); verified with the new quadrature-pair design the worst case across this
    // sweep is ~0.688 (600Hz). Threshold set well below that with margin.
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
            const float peak = std::max (peakAfter (left, 4800), peakAfter (right, 4800));
            CHECK_MSG (peak > 0.5f,
                       "midrange content should not collapse in volume at crossover=250Hz, Q=0.5, rejection=0.5");
        }
    }

    TEST_SUMMARY();
    return 0;
}
```

- [ ] **Step 2: Build and confirm the updated tests fail (RED) — the implementation hasn't changed yet**

Run: `cmake --build build --target test_crossoverms && ./build/test_crossoverms`
Expected: FAIL — both the mono-sum invariant test (still using the old single-all-pass `CrossoverMS`, so the new Chain-A-based replica won't match it) and the new midrange-regression test (which will show the ~0.06 collapse the old design has) should fail.

- [ ] **Step 3: Modify `Source/DSP/CrossoverMS.h`** — replace the single `AllpassFilter` member with `QuadraturePair`. Replace:

```cpp
#pragma once
#include "StateVariableFilter.h"
#include "AllpassFilter.h"

// Mid/Side-domain crossover. Forces everything below `frequencyHz` to mono
// (the Side low band is discarded). Above it, `rejection01` redirects Mid content
// into Side (via an all-pass phase rotation) rather than discarding it, so widening
// the highs doesn't cost level.
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
    StateVariableFilter svfM_, svfS_;
    AllpassFilter allpassM_;
    float lastFrequencyHz_ = -1.0f;
    float lastQ_ = -1.0f;
    float smoothedRejection_ = 0.0f;
};
```

with:

```cpp
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
    StateVariableFilter svfM_, svfS_;
    QuadraturePair quadratureM_;
    float lastFrequencyHz_ = -1.0f;
    float lastQ_ = -1.0f;
    float smoothedRejection_ = 0.0f;
};
```

- [ ] **Step 4: Modify `Source/DSP/CrossoverMS.cpp`** — reset and prepare the quadrature pair, and feed `mHigh` through both branches instead of through the single all-pass. Replace:

```cpp
#include "CrossoverMS.h"
#include <cmath>

void CrossoverMS::reset() noexcept
{
    svfM_.reset();
    svfS_.reset();
    allpassM_.reset();
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
        allpassM_.setParameters (frequencyHz, sampleRate);
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

        smoothedRejection_ += (rejection01 - smoothedRejection_) * smoothCoeff;

        const float mHighKept  = (1.0f - smoothedRejection_) * mHigh;
        const float mHighMoved = smoothedRejection_ * mHigh;
        const float sideAdd    = allpassM_.process (mHighMoved);

        const float mOut = mLow + mHighKept;
        const float sOut = sHigh + sideAdd;

        left[i]  = mOut + sOut;
        right[i] = mOut - sOut;
    }
}
```

with:

```cpp
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

        smoothedRejection_ += (rejection01 - smoothedRejection_) * smoothCoeff;

        const float mHighKept  = (1.0f - smoothedRejection_) * quad.a;
        const float mHighMoved = smoothedRejection_ * quad.b;

        const float mOut = mLow + mHighKept;
        const float sOut = sHigh + mHighMoved;

        left[i]  = mOut + sOut;
        right[i] = mOut - sOut;
    }
}
```

- [ ] **Step 5: Build and run the tests — confirm they pass (GREEN)**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 4` (`StateVariableFilter`, `AllpassFilter`, `QuadraturePair`, `CrossoverMS`).

- [ ] **Step 6: Commit**

```bash
git add Source/DSP/CrossoverMS.h Source/DSP/CrossoverMS.cpp Tests/test_crossoverms.cpp
git commit -m "Fix midrange cancellation: redirect Mid energy via a quadrature all-pass pair"
```

---

### Task 3: Full build verification and manual listening check

**Files:** none (verification only)

- [ ] **Step 1: Clean Debug build of everything, including tests**

Run: `rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build --parallel`
Expected: build succeeds with no errors.

- [ ] **Step 2: Run the full test suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 4`.

- [ ] **Step 3: Manual verification via Standalone build, specifically re-testing the reported bug scenario**

Run: `./build/Outflank_artefacts/Debug/Standalone/Outflank`
Expected: reproduce the originally reported conditions — crossover frequency above 150 Hz, low Q, `rejection` swept through 30–70% — on real program material with midrange content (vocals, guitar). Confirm no perceptible volume dip or phasing artifact. Also re-sweep `rejection` at default settings to confirm the "wide, not weak" behavior from the previous feature is still intact. If running headless, confirm the binary launches without crashing as a fallback and flag that ear verification still needs a human pass.

- [ ] **Step 4: Commit** (only if any of the above steps required fixes; otherwise nothing to commit)

```bash
git status
```

If clean, no commit needed — this task is verification-only.
