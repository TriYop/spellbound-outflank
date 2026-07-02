# Outflank — Quadrature-Pair Widening (Fixes Midrange Cancellation) — Design Spec

**Date:** 2026-07-02
**Status:** Approved for planning

## Overview

The previous "wide-without-weak highs" feature (see `2026-07-02-outflank-widen-highs-design.md`)
redirected rejected Mid energy into Side by phase-rotating it through a single first-order
all-pass filter tied to the crossover frequency. In production use, this caused a real,
audible bug: with `rejection` around 30–70%, crossover frequency above ~150 Hz, and low `q`,
listeners heard significant volume loss and phasing artifacts in the midrange.

## Root cause

A single first-order all-pass only reaches its intended ~90° phase shift *at its corner
frequency*. Away from that corner, phase keeps rotating continuously toward 0° (below) or
180° (above) — unbounded, not held at 90°. Since the "kept" Mid content stayed unrotated (0°
reference) while the "moved" content was rotated by this single all-pass, the two diverged
without limit as frequency moved away from the crossover. Recombining them into `L`/`R` via
the M/S decode caused destructive interference wherever their phase difference approached
180°, which happens only a couple of octaves above the crossover — squarely in the midrange
for typical crossover settings.

Confirmed numerically (simulating the shipped algorithm, crossover=250 Hz, Q=0.5, mono input,
measuring settled peak amplitude of `L`):

| freq | rejection=0.3 | rejection=0.5 | rejection=0.7 |
|---|---|---|---|
| 200 Hz | 1.05 | 1.16 | 1.31 |
| 900 Hz | 0.45 | 0.31 | 0.57 |
| 2000 Hz | 0.41 | 0.13 | 0.43 |
| 4000 Hz | 0.40 | 0.06 | 0.40 |

At `rejection=0.5`, midrange content loses up to ~24 dB in one channel (with a mirrored boost
in the other) — matching the reported symptom exactly. A higher crossover frequency pushes
this cancellation zone further into the audible midrange (worse); a lower `q` widens how much
signal leaks into the affected band from below the crossover (also worse) — both consistent
with the reported conditions.

This is an architectural flaw in the DSP topology chosen for the previous feature, not a
coding defect: any scheme that creates an *unbounded, frequency-dependent* phase difference
between the Mid-kept and Side-moved copies of the same source will pass through the
destructive-interference zone somewhere in the band. Fixing it requires the phase difference
between the two paths to stay *pinned near 90°* across the whole band the feature operates in,
not just at one frequency.

## Non-goals

- No new parameter, knob, or UI control — same 3-knob interface as before.
- No change to the crossover split itself — `frequency`/`q` still control `CrossoverMS`'s SVF
  low/high split exactly as before; `q` plays no role in this fix.
- Not attempting sample-accurate phase-difference perfection at every possible frequency —
  targeting a documented, numerically-verified tolerance (see Approach), not an idealized
  infinite-order Hilbert transform.

## Approach

**Chosen: broadband quadrature-pair all-pass network.** Replace the single `allpassM_` with
two matched chains of cascaded first-order all-pass filters (Chain A, Chain B), with *fixed*
corner frequencies (not tied to the crossover knob), designed so the phase difference between
Chain A's and Chain B's output stays close to 90° across the full audio band the feature needs
to cover (40 Hz, the minimum crossover setting, up to ~20 kHz). Feed `mHigh` into both chains;
Chain A's output feeds what stays in Mid, Chain B's feeds what moves to Side. This is the same
class of technique real Hilbert-transform-based stereo wideners use, and it structurally
prevents the phase difference between the two paths from ever drifting into the 0°/180°
cancellation zone, at any crossover/Q/rejection combination.

Two alternatives considered and rejected:

- **Fewer all-pass stages (3–4 per branch) for a narrower guaranteed-accurate band** (e.g.
  100 Hz–10 kHz): cheaper, but the crossover parameter's own range (40–2000 Hz) already extends
  below where a narrow design would still be accurate, so worst-case settings could still show
  a smaller residual dip — not a full fix.
- **Abandon phase-based redistribution; boost pre-existing Side content instead of redirecting
  Mid content**: zero comb-filtering risk (pure real-valued gain, no phase filtering at all),
  but it only works when Side content already exists in that band to boost. It cannot make
  genuinely mono/centered content wide, which was the entire point of the original feature
  request — rejected as not solving the actual problem.

**Tradeoff accepted:** at `rejection=0`, the Mid-kept path now always passes through Chain A
(rather than staying raw), because Chain A and Chain B must both be active continuously to
avoid reintroducing an unrotated-vs-rotated seam at low rejection values. Since all-pass
filtering preserves magnitude exactly, loudness and tonal balance at `rejection=0` are
unaffected — only phase changes, which is inaudible for practical purposes. This means
`rejection=0` is no longer *bit-for-bit* identical to the plugin's original pre-widening-feature
behavior, only *audibly* identical (same magnitude response). This is a deliberate, accepted
trade — the alternative (a hard bypass that only engages at exactly `rejection=0`) would
reintroduce a seam the moment the knob starts moving, since the one-pole smoothing on
`rejection` continuously passes through small positive values.

## Architecture

```
Source/DSP/
  QuadraturePair.h     — new: owns two AllpassChain instances (Chain A, Chain B), each a
                          cascade of N first-order all-pass stages with fixed corner
                          frequencies (compile-time constants, derived via numerical
                          optimization -- see Implementation Notes). process(input) -> two
                          outputs (outA, outB). Plain C++, no JUCE dependency.
  CrossoverMS.h/.cpp    — modified: replaces the single AllpassFilter allpassM_ with one
                          QuadraturePair. process() feeds mHigh through both chains, then
                          applies the rejection-weighted split to the two filtered outputs
                          (not to the raw signal).
```

`AllpassFilter.h` (the single-stage filter added for the previous, now-superseded design)
remains as a building block — `QuadraturePair` is built from N instances of it, cascaded per
branch.

## DSP algorithm

```
mHighA = chainA.process (mHigh)   // reference branch
mHighB = chainB.process (mHigh)   // ~90 degrees from chainA at every frequency in-band

smoothedRejection += (rejection01 - smoothedRejection) * smoothCoeff   // unchanged, ~5ms one-pole

mHighKept  = (1 - smoothedRejection) * mHighA
mHighMoved = smoothedRejection * mHighB

mOut = mLow + mHighKept
sOut = sHigh + mHighMoved

L = mOut + sOut
R = mOut - sOut
```

Gain (the `(1 - rejection)` / `rejection` split) is applied *after* filtering, not before —
filtering a fixed signal then scaling is well-defined per-sample, whereas scaling by a
continuously-smoothed gain before feeding a filter with memory would let the filter respond to
a modulated signal, an unnecessary complication avoided by keeping the existing
filter-then-gain ordering (matching the previous design's structure).

The Mid/Side encode/decode and the low-band handling (`mLow`, `sLow` discarded to force mono
bass) are entirely unchanged from the existing design — this fix is scoped to the high-band
redistribution mechanism only.

## Implementation notes: deriving the quadrature-pair coefficients

Rather than quoting all-pass corner frequencies from memory (risk of a subtly wrong "textbook"
table baked into shipped code), the implementation plan derives them with a small,
reproducible numerical procedure:

1. Write a Python script that models N cascaded first-order all-pass stages per branch (same
   difference equation as `AllpassFilter.h`), parameterized by N corner frequencies per branch.
2. Use a least-squares/optimization routine to choose both branches' corner frequencies such
   that the phase difference between Chain A and Chain B stays as close to 90° as possible
   across a log-spaced frequency sweep from 40 Hz to 20 kHz.
3. Verify the resulting design's worst-case phase error across that sweep. Target: within a
   few degrees of 90° everywhere in-band, starting from N=6 stages per branch; increase N if
   the tolerance isn't met.
4. Re-run the exact bug-reproduction numbers from this spec's Root Cause section (crossover=250
   Hz, Q=0.5, rejection=0.5, sweep 200 Hz–8000 Hz) against the new design and confirm the dip is
   gone (e.g. settled peak amplitude stays within a reasonable bound of unity, not the ~0.06–0.13
   measured today).
5. Hard-code the verified corner frequencies as `static constexpr` values in `QuadraturePair.h`
   (or a table it includes) — fixed at compile time, not derived from the `frequency`/`q`
   parameters at runtime. This also simplifies the runtime code: unlike the SVF (whose
   coefficients must be recomputed whenever `frequency`/`q` change), the quadrature pair's
   coefficients never change, so there's no per-block coefficient-recompute logic needed for
   this stage.

## Testing

- **`QuadraturePair` unit tests**: each chain stays unity-magnitude across a frequency sweep
  (all-pass property preserved through the cascade); the phase *difference* between Chain A's
  and Chain B's output stays within the verified tolerance of 90° across the same sweep — the
  property this entire fix depends on.
- **`CrossoverMS` regression test reproducing the reported bug**: crossover=250 Hz, Q=0.5,
  rejection=0.5, mono input swept across 200 Hz–8000 Hz — assert settled peak amplitude stays
  within a safe bound of the input's amplitude (no significant dip), directly guarding against
  regressing to the old cancellation behavior.
- **Existing invariants re-verified, not re-designed**: mono-sum invariant (`L+R = 2·mOut`,
  structurally unaffected by this change), mono bass stays mono-ish regardless of rejection,
  "not weak"/"actually wide" behavior at `rejection=1`. These tests' *assertions* carry over
  unchanged; only the internal computation of `mOut`/`sOut` they exercise changes.
- **Manual verification**: repeat the reported scenario by ear (crossover>150 Hz, low Q,
  rejection swept through 30–70%) on real program material with midrange content (vocals,
  guitar) and confirm no perceptible volume dip or phasing artifact.
