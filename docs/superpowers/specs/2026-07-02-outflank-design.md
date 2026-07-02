# Outflank — Design Spec

**Date:** 2026-07-02
**Status:** Approved for planning

## Overview

Outflank ("The Spatial Tactician") is a minimal stereo crossover utility plugin. It splits the
signal at a crossover frequency, forces everything below that frequency to mono (anchoring the
low end dead-center), and widens everything above it by attenuating the mono/center ("Mid")
content in the high band — outflanking the listener with stereo width without smearing the bass.

The entire feature set is 3 knobs:

1. **Frequency** — crossover center frequency
2. **Q** — resonance/steepness of the crossover filter at the seam
3. **Rejection** — how much Mid content is removed from the high band

Plus preset save/load/selection, matching the pattern used by the other plugins in this
workspace.

## Non-goals

- No bypass switch, output gain, or metering — the DAW's own bypass covers on/off, and this is
  deliberately as simple as possible.
- No automated phase-flatness guarantee at neutral settings (Q at 0.707 with a resonant SVF is
  not perfectly flat by design — the resonance is a wanted creative effect, not a bug to correct).
- No DAW-integration/host testing — out of proportion for a 3-knob utility; none of the sibling
  plugins in this workspace do this either.

## Component layout

Follows the JUCE + CMake/Ninja pattern shared by the other `AudioPlugins/` projects in this
workspace (VST3/CLAP/Standalone targets via CMake, JUCE fetched into `build/_deps/`).

```
CMakeLists.txt              — JUCE fetch, VST3/CLAP/Standalone targets
Source/
  PluginProcessor.h/.cpp    — AudioProcessor; owns APVTS (3 params) + PresetManager;
                              processBlock() drives CrossoverMS
  PluginEditor.h/.cpp       — AudioProcessorEditor; 3 rotary knobs + preset selector/save/delete
  PresetManager.h/.cpp      — factory + user preset load/save/delete/rename, APVTS XML
                              serialization (same shape as Catalyst/Pugilist/Arcanist/TrueSight)
  DSP/
    StateVariableFilter.h   — reusable 2-pole resonant SVF (simultaneous LP/HP outputs)
    CrossoverMS.h/.cpp      — owns 2 SVF instances (M-bus, S-bus); M/S encode/decode;
                              applies the rejection gain
Tests/                      — doctest target: StateVariableFilter and CrossoverMS math
```

`CrossoverMS` is the one meaningful DSP unit: input is a stereo block, output is a stereo block,
parameters are `freq`, `q`, `rejection`. Everything else (M/S math, filtering) is private to it —
the processor just feeds it audio and three floats.

## Parameters (APVTS)

| ID | Range | Default | Notes |
|----|-------|---------|-------|
| `frequency` | 40 Hz – 2 kHz, log skew | 250 Hz | Crossover center frequency |
| `q` | 0.3 – 4.0 | 0.707 | SVF resonance at the crossover seam; 0.707 ≈ neutral/no resonant hump |
| `rejection` | 0 – 100 % | 0 % | Attenuation applied to `M_high` (0% = high band untouched, 100% = `M_high` fully removed — high band becomes pure Side) |

At `rejection = 100%` the high band loses its Mid entirely, which can sound extreme/phasey in
mono playback. This is an intentional creative extreme, not a bug.

State save/load via APVTS XML `getStateInformation()`/`setStateInformation()`, same pattern as
sibling plugins.

## Presets

`PresetManager` (`Source/PresetManager.h/.cpp`) takes a reference to the APVTS and
serializes/deserializes it to/from XML, following the exact pattern already used by
Catalyst/Pugilist/Arcanist/TrueSight in this workspace:

- **Factory presets**: bundled, read-only. Given Outflank has only 3 parameters, these are just a
  handful of named `(frequency, q, rejection)` triples (e.g. "Tight Mono Bass", "Wide Airy",
  "Subtle") — no separate preset file format needed, just XML snapshots of the APVTS.
- **User presets**: saved to disk in a plugin-specific presets directory.
- API: `loadPreset(index)`, `savePreset(name)`, `deletePreset(index)`, `renamePreset(index, newName)`,
  `getCurrentPresetIndex()`, `getCurrentPresetName()`, `getPresetList()`, `refreshUserPresets()`.
- Editor UI: a `juce::ComboBox presetSelector_` populated from `getPresetList()`, plus save/delete
  controls — same minimal UI pattern as the sibling plugins, in addition to the 3 knobs (not
  instead of them).

## DSP data flow

Signal flow is M/S-domain with a single crossover per bus (2 filter instances total — one for
the Mid bus, one for the Side bus). This was chosen over splitting L/R independently first (which
would need 4 filter instances for an identical result) and over an allpass/complementary crossover
with a guaranteed-flat neutral response (which would fight the intentional Q-driven resonant
coloration at the seam).

```
processBlock(buffer):
  for each sample:
    L, R = buffer[ch0][i], buffer[ch1][i]
    M = 0.5*(L+R);  S = 0.5*(L-R)          // M/S encode

    M_low, M_high = svfM.process(M)        // 2-pole resonant SVF, simultaneous LP/HP
    S_low, S_high = svfS.process(S)        // identical coefficients (shared freq/q)

    M_out = M_low + (1 - rejection) * M_high
    S_out = S_high                          // S_low discarded — forces bass to mono

    L_out = M_out + S_out;  R_out = M_out - S_out   // M/S decode
```

- `StateVariableFilter`: standard Chamberlin/Zavalishin topology-preserving 2-pole SVF producing
  LP and HP outputs from one pass; coefficients (`g`, damping from `q`) recomputed via a
  `setParameters(freq, q, sampleRate)` call whenever a parameter changes, not per-sample — same
  "compute coefficients on change" convention used by the sibling DSP classes.
- `rejection` and the filter coefficients are smoothed with `juce::SmoothedValue<float>` to avoid
  zipper noise/clicks when knobs move — same click-prevention pattern as Bastos.
- Mono compatibility is a first-class concern (it's the whole point of the plugin), so no
  additional phase-correction stage is needed: discarding `S_low` and using a single shared SVF
  per bus keeps the L+R mono sum well-behaved.

## Testing

- **`doctest` target** (`Tests/`, following Codex's use of the same framework): pure-function
  tests on `StateVariableFilter` coefficient math (e.g., at `rejection=0` and `q=0.707`, verify
  `M_low + M_high ≈ M` — the filter reconstructs its input) and on `CrossoverMS` (e.g., feed a
  pure mono signal and verify `S_out ≈ 0` at all settings since `S` was 0 to begin with; feed a
  fully out-of-phase signal and verify the low band still collapses correctly).
- **Manual verification via Standalone build**: sweep `frequency`/`q`/`rejection` by ear, confirm
  the low end collapses to mono (check with a mono-sum/correlation meter) and the highs widen as
  rejection increases.
- No DAW-host integration testing (see Non-goals).
