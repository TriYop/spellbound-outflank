# Outflank — Wide-Without-Weak Highs — Design Spec

**Date:** 2026-07-02
**Status:** Approved for planning

## Overview

Today, `rejection` widens the highs by subtracting Mid energy from the high band
(`mOut = mLow + (1 - rejection) * mHigh`). At high `rejection` values this makes centered
high-frequency content sound thin/hollow, because the energy is simply deleted rather than
moved anywhere.

This change redirects the removed Mid energy into the Side channel instead of discarding it,
using a phase-rotating all-pass filter, so highs widen by redistributing energy rather than
subtracting it. The `rejection` knob keeps its existing meaning, range, and default — this is
purely an internal DSP change behind the same 3-knob interface.

## Non-goals

- No new parameter, knob, or UI control — `rejection`'s id, range (0–100%), and default (0%)
  are unchanged.
- No change to the crossover split itself (`frequency`/`q` still control `CrossoverMS`'s
  low/high split exactly as before) — only what happens to the rejected portion of the high band.
- No re-tuning of the 3 factory presets — their saved `(frequency, q, rejection)` values stay as
  they are; they'll simply sound fuller/wider at those same values.
- No Haas-delay-based widening — ruled out because it introduces frequency-dependent comb
  filtering in the stereo image itself (see Approach discussion below).

## Approach

Considered 3 approaches (all-pass phase rotation, micro-delay/Haas, dual complementary all-pass
network); chose **all-pass phase rotation**:

- **All-pass phase rotation (chosen)**: route the rejected Mid energy through an all-pass filter
  before adding it to Side. Mono-safe by construction — Side never contributes to the `L + R`
  mono sum, by M/S math, so no matter what the all-pass does, mono downmix is unaffected. Cheap
  (one extra 1st-order filter), fits the existing plain-C++ DSP style.
- **Micro-delay (Haas effect)**: simpler to implement, but a fixed delay applied to only part of
  the signal produces frequency-dependent comb filtering when the two channels are compared in
  the stereo image — an audible coloration this feature is specifically trying to avoid.
- **Dual complementary all-pass network**: a richer Schroeder/Freeverb-style decorrelator across
  the whole high band. More diffuse, but more CPU, more internal tuning parameters, and risks
  sounding like subtle reverb/smear rather than a clean crossover effect. Rejected as
  disproportionate to a 3-knob utility.

## DSP algorithm

Replace the subtractive line in `CrossoverMS::process` with an energy-redistributing one.

Today:
```
mOut = mLow + (1 - rejection) * mHigh
sOut = sHigh
```

New:
```
mHighKept  = (1 - rejection) * mHigh
mHighMoved = rejection * mHigh              // previously just discarded
sideAdd    = allpassM_.process (mHighMoved) // phase-rotate so it decorrelates from what's left in M

mOut = mLow + mHighKept
sOut = sHigh + sideAdd
```

- **All-pass filter**: a single 1st-order all-pass. Corner frequency tied to the existing
  `frequency` parameter (no new parameter) — phase rotation is centered right where the energy
  is being pulled from, which is where it's most audible. Coefficients recomputed on change,
  same convention as the SVF (`setParameters` called only when the cached frequency changes).
- **Mono-safety is structural, not tuned**: `S` never appears in `L + R = 2·M` by M/S
  construction, so no matter what enters `sOut`, mono downmix stays bit-identical to today's
  behavior. This is the reason the all-pass option is safe where Haas delay would not be.
- **At `rejection = 0`**: `mHighMoved = 0`, `sideAdd = 0` → identical to today's passthrough.
  Only `rejection > 0` behavior changes.

## Component changes

```
Source/DSP/
  AllpassFilter.h     — new: 1st-order all-pass, plain C++ (no JUCE dependency, matching
                        StateVariableFilter.h's convention). setParameters(freqHz, sampleRate),
                        process(input).
  CrossoverMS.h/.cpp   — modified: owns one AllpassFilter allpassM_ instance (only needed on the
                        Mid bus, since we're rotating the phase of energy moving from Mid into
                        Side). process() gains the redistribution logic above.
```

No changes to `PluginProcessor`, `PluginEditor`, `PresetManager`, or the APVTS parameter layout.

## Testing

**New test** `Tests/test_allpassfilter.cpp` — the defining property of an all-pass is flat
magnitude with frequency-dependent phase shift:
- Magnitude stays ≈1.0 across a frequency sweep (well below, at, and well above the corner
  frequency) — unlike the lowpass's rolloff, this should *not* attenuate.
- Phase lag at the corner frequency is ≈90°, measured via zero-crossing timing after settling
  (same time-domain-simulation style already used for `StateVariableFilter`'s tests — no FFT
  needed).

**Updated tests** in `Tests/test_crossoverms.cpp` — the existing `rejection=1` test asserts
near-silence, which is no longer correct behavior by design. It's replaced with 3 assertions
that directly encode "wide but not weak":

1. **Mono-sum invariant** (new, most important): for any `rejection` value, `(L + R)` equals
   `2·(mLow + mHighKept)` — i.e., whatever the all-pass redirects into `S` provably cannot leak
   into the mono downmix. This is what makes the all-pass choice safe, so it gets its own
   regression test rather than just a design claim.
2. **Not weak**: at `rejection=1` with a mono sine above the crossover, `peak(|L|)` and
   `peak(|R|)` stay close to the original input amplitude (e.g. `> 0.5`) — replaces the old
   test's `peak < 0.05` assertion.
3. **Actually wide**: at `rejection=1`, `L` and `R` are no longer near-identical (e.g.
   `peak(|L - R|)` is significant) — proving the energy moved to Side rather than just
   disappearing.

The `rejection=0` passthrough test is unchanged (still identical behavior, since `mHighMoved`
is always 0 there).

**Manual verification**: sweep `rejection` by ear on real program material; confirm the top end
widens progressively without perceptibly losing level or turning hollow, and that mono downmix
(sum to mono in the DAW / correlation meter) is unaffected regardless of `rejection`.
