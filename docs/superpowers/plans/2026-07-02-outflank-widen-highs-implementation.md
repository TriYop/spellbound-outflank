# Outflank Wide-Without-Weak Highs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redirect the Mid energy that `rejection` currently discards into the Side channel via an all-pass phase rotation, so widening the highs no longer costs level.

**Architecture:** A new plain-C++ `AllpassFilter` (1st-order, Direct Form I) mirrors the existing `StateVariableFilter`'s style. `CrossoverMS` gains one `AllpassFilter` instance on the Mid bus; instead of discarding `rejection * mHigh`, it phase-rotates that portion and adds it to `sOut`. No new parameter, no UI change, no `PluginProcessor`/`PluginEditor`/`PresetManager` changes.

**Tech Stack:** C++20, plain C++ DSP layer (no JUCE dependency), same `Tests/test_runner.h` `CHECK`-macro harness used throughout this project.

**Reference:** `docs/superpowers/specs/2026-07-02-outflank-widen-highs-design.md`

## Global Constraints

- No new APVTS parameter — `rejection`'s id, range (0–100%), and default (0%) are unchanged; `PluginProcessor.h/.cpp` and `PluginEditor.h/.cpp` are not touched by this plan.
- `AllpassFilter` and the updated `CrossoverMS` remain plain C++ with no JUCE dependency, so they stay testable as bare executables via `Tests/test_runner.h`'s `CHECK`/`CHECK_MSG` macros.
- Mono downmix (`L + R`) must remain unaffected by whatever the all-pass redirects into `sOut` — structural from the M/S decode (`L = mOut + sOut`, `R = mOut - sOut`), not something to be separately "guarded" in code, but explicitly regression-tested.
- Factory preset XML files (`Source/Presets/Factory/*.xml`) are not modified — their numeric values stay as-is per design decision; only their resulting sound changes.
- At `rejection = 0`, output must remain bit-for-bit identical to today's passthrough behavior (verified by the existing, unchanged `rejection = 0` test).

---

### Task 1: AllpassFilter DSP unit (TDD)

**Files:**
- Create: `Source/DSP/AllpassFilter.h`
- Create: `Tests/test_allpassfilter.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `class AllpassFilter` with `void reset() noexcept`, `void setParameters(float frequencyHz, double sampleRate) noexcept`, `float process(float input) noexcept`. No JUCE dependency. Consumed by `CrossoverMS` in Task 2.

- [ ] **Step 1: Write the failing test `Tests/test_allpassfilter.cpp`**

```cpp
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

    TEST_SUMMARY();
    return 0;
}
```

- [ ] **Step 2: Add the test target to `CMakeLists.txt`** (append after the existing `test_crossoverms` block, before `# ── Install rules`)

```cmake
add_executable(test_allpassfilter
    Tests/test_allpassfilter.cpp
)
target_include_directories(test_allpassfilter PRIVATE Source/ Tests/)
target_compile_features(test_allpassfilter PRIVATE cxx_std_20)
add_test(NAME AllpassFilter COMMAND test_allpassfilter)
```

- [ ] **Step 3: Configure and attempt to build the test target — confirm it fails (header missing)**

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target test_allpassfilter`
Expected: FAIL — `Source/DSP/AllpassFilter.h` does not exist yet.

- [ ] **Step 4: Write `Source/DSP/AllpassFilter.h`**

```cpp
#pragma once
#include <cmath>

// First-order digital all-pass filter (Direct Form I). Flat magnitude response at every
// frequency; phase shifts continuously from 0 degrees at DC to -180 degrees at Nyquist,
// crossing exactly -90 degrees at the corner frequency. CrossoverMS uses this to phase-rotate
// Mid energy being redirected into Side, so it decorrelates from what remains in Mid without
// changing level.
//
// Plain C++, no JUCE dependency, matching StateVariableFilter.h's convention.
class AllpassFilter
{
public:
    void reset() noexcept
    {
        x1_ = 0.0f;
        y1_ = 0.0f;
    }

    void setParameters (float frequencyHz, double sampleRate) noexcept
    {
        const float g = std::tan (kPi * frequencyHz / static_cast<float> (sampleRate));
        a_ = (1.0f - g) / (1.0f + g);
    }

    float process (float input) noexcept
    {
        const float output = -a_ * input + x1_ + a_ * y1_;
        x1_ = input;
        y1_ = output;
        return output;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    float a_  = 0.0f;
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};
```

- [ ] **Step 5: Build and run the test — confirm it passes**

Run: `cmake --build build --target test_allpassfilter && ctest --test-dir build -R AllpassFilter --output-on-failure`
Expected: `2 passed, 0 failed` and `100% tests passed`.

- [ ] **Step 6: Commit**

```bash
git add Source/DSP/AllpassFilter.h Tests/test_allpassfilter.cpp CMakeLists.txt
git commit -m "Add AllpassFilter DSP unit with passing tests"
```

---

### Task 2: Redirect rejected Mid energy through the all-pass in CrossoverMS

**Files:**
- Modify: `Source/DSP/CrossoverMS.h`
- Modify: `Source/DSP/CrossoverMS.cpp`
- Modify: `Tests/test_crossoverms.cpp`

**Interfaces:**
- Consumes: `AllpassFilter` (`reset()`, `setParameters(frequencyHz, sampleRate)`, `process(input)`) from Task 1.
- `CrossoverMS::process`'s public signature is unchanged: `void process(float* left, float* right, int numSamples, float frequencyHz, float q, float rejection01, double sampleRate) noexcept`. `PluginProcessor` (Task 4 of the original plan) already calls this signature and needs no changes.

- [ ] **Step 1: Update the existing `rejection = 1` test in `Tests/test_crossoverms.cpp` first (RED), since its old assertion is no longer correct behavior by design.** Replace:

```cpp
    // rejection = 1: mono content above the crossover is fully removed.
    {
        auto out = renderMono (2000.f, 250.f, 0.707f, 1.f, 9600);
        CHECK_MSG (peakAfter (out, 4800) < 0.05f, "mono highs should be fully rejected when rejection is 1");
    }

    TEST_SUMMARY();
    return 0;
}
```

with:

```cpp
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
```

Also add `#include "../Source/DSP/StateVariableFilter.h"` to the top of `Tests/test_crossoverms.cpp` (alongside the existing `#include "../Source/DSP/CrossoverMS.h"`), needed by the new mono-sum invariant test.

**Also fix the pre-existing "mono input stays mono" test.** Its title claims mono input stays
mono "for any settings," which was true under the old subtractive design (Side only ever held
`sHigh`, independent of `rejection`/`mHigh`) but is no longer universally true: the whole point
of this feature is that a mono input's *highs* now widen when `rejection > 0`, by design. The
test's actual signal (100Hz vs. a 250Hz crossover, only ~1.3 octaves apart) sees substantial
`mHigh` leakage at `rejection=0.5`, which now measurably redirects into Side (verified with a
Python reference simulation of this exact algorithm: `peak|L-R| ≈ 0.58` at those settings — not
noise, the widening working as intended). Split it into two tests that each state a true
invariant: passthrough stays mono at `rejection=0` regardless of frequency, and deep bass
(many octaves below the crossover, using this file's existing far-below-crossover convention)
stays mono-ish regardless of `rejection`, since forced-mono bass is unconditional and unrelated
to the highs-widening feature. Replace:

```cpp
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
```

with:

```cpp
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
```

- [ ] **Step 2: Build and confirm the updated tests fail (RED) — the implementation hasn't changed yet**

Run: `cmake --build build --target test_crossoverms && ./build/test_crossoverms`
Expected: FAIL — the "not weak"/"actually wide" assertions fail because `CrossoverMS` still discards rejected energy (old behavior gives near-silent output, not `peakL > 0.7f`).

- [ ] **Step 3: Modify `Source/DSP/CrossoverMS.h`** — add the `AllpassFilter` member. Replace:

```cpp
#pragma once
#include "StateVariableFilter.h"

// Mid/Side-domain crossover. Forces everything below `frequencyHz` to mono
// (the Side low band is discarded) and attenuates Mid content above it by
// `rejection01` to widen the highs.
//
// Plain C++, no JUCE dependency (see Tests/test_crossoverms.cpp). PluginProcessor
// adapts this to juce::AudioBuffer<float> via raw channel pointers.
class CrossoverMS
{
public:
    void reset() noexcept;

    // `left`/`right` are processed in place, `numSamples` samples each.
    // `rejection01` is 0..1 (0 = no effect, 1 = Mid fully removed above the crossover).
    void process (float* left, float* right, int numSamples,
                  float frequencyHz, float q, float rejection01,
                  double sampleRate) noexcept;

private:
    StateVariableFilter svfM_, svfS_;
    float lastFrequencyHz_ = -1.0f;
    float lastQ_ = -1.0f;
    float smoothedRejection_ = 0.0f;
};
```

with:

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

- [ ] **Step 4: Modify `Source/DSP/CrossoverMS.cpp`** — reset the all-pass, recompute its coefficient alongside the SVFs, and redirect the rejected energy instead of discarding it. Replace:

```cpp
#include "CrossoverMS.h"
#include <cmath>

void CrossoverMS::reset() noexcept
{
    svfM_.reset();
    svfS_.reset();
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

        const float mOut = mLow + (1.0f - smoothedRejection_) * mHigh;
        const float sOut = sHigh;

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

- [ ] **Step 5: Build and run the tests — confirm they pass (GREEN)**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 3` (`StateVariableFilter`, `AllpassFilter`, `CrossoverMS`).

- [ ] **Step 6: Commit**

```bash
git add Source/DSP/CrossoverMS.h Source/DSP/CrossoverMS.cpp Tests/test_crossoverms.cpp
git commit -m "Redirect rejected Mid energy into Side via all-pass instead of discarding it"
```

---

### Task 3: Full build verification and manual listening check

**Files:** none (verification only)

- [ ] **Step 1: Clean Debug build of everything, including tests**

Run: `rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build --parallel`
Expected: build succeeds with no errors.

- [ ] **Step 2: Run the full test suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 3`.

- [ ] **Step 3: Manual verification via Standalone build**

Run: `./build/Outflank_artefacts/Debug/Standalone/Outflank`
Expected: sweep `rejection` from 0% to 100% by ear on real stereo program material with the `frequency`/`q` knobs at their defaults. Confirm the top end widens progressively without perceptibly losing level or turning hollow, and that summing to mono (via the DAW's own mono button, or a correlation meter if available) leaves the low end and overall balance unaffected regardless of `rejection`. If running headless, confirm the binary launches without crashing as a fallback and flag that ear verification still needs a human pass.

- [ ] **Step 4: Commit** (only if any of the above steps required fixes; otherwise nothing to commit)

```bash
git status
```

If clean, no commit needed — this task is verification-only.
