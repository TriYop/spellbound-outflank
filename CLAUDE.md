# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Outflank ("The Spatial Tactician") is a stereo crossover / mono-bass-anchor
utility effect plugin (VST3 / CLAP / LV2, +AU on macOS). From `README.md`:
splits the signal into low vs. mid/high bands via a Mid/Side crossover,
collapses everything below the crossover frequency to mono (discarding the
Side low band — "anchors" the bass dead-center), and widens everything
above it by redirecting Mid energy into Side through a matched quadrature
all-pass pair rather than a naive single all-pass (which caused audible
midrange cancellation — see `Source/DSP/QuadraturePair.h`'s header comment
for the root-cause analysis).

**Current state: implemented and migrated off JUCE onto
[DPF](https://github.com/DISTRHO/DPF)** (DISTRHO Plugin Framework,
ISC-licensed — see `AudioPlugins/CLAUDE.md`'s workspace-wide migration
note). The DSP (`Source/DSP/CrossoverMS.{h,cpp}`, `StateVariableFilter.h`,
`QuadraturePair.h`, `AllpassFilter.h`) is unchanged in shape by the
migration — it was already framework-free before DPF arrived, with its own
JUCE-free CTest suite (`Tests/test_crossoverms.cpp`,
`test_statevariablefilter.cpp`, `test_quadraturepair.cpp`,
`test_allpassfilter.cpp`, plus `test_outflank_parameters.cpp` and
`test_outflank_factorypresets.cpp` added during the migration). Two real
DSP bugs were found and fixed during the migration's validator pass — see
"CI / validator status" below; the DSP is not simply a straight port.
`Source/OutflankPluginAdapter.{h,cpp}` declares the 3 host parameters via
DPF's `Parameter` API and drives that chain unchanged; `Source/OutflankUI.{h,cpp}`
has 3 `AudioPluginsCommon::hui::dgl::RotaryKnob`s (frequency/q/rejection)
using `Theme`'s default palette (Outflank never had a custom look, so none
was invented for this migration) plus a presets bar (`PresetSelector`
dropdown + SAVE/DELETE `Button`s, backed by
`AudioPluginsCommon::presets::PresetBrowser` and the 3 factory presets
hand-converted into `Source/FactoryPresets.h`). The JUCE-era
`PluginProcessor`/`PluginEditor`/`PresetManager` are preserved unchanged at
`Source/_juce_reference/` as the porting reference this was ported from.

## Build Commands

### Linux prerequisites (one-time)

```bash
sudo apt install cmake ninja-build build-essential git \
    libasound2-dev libjack-jackd2-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libfreetype-dev libfontconfig1-dev \
    libglu1-mesa-dev libwebkit2gtk-4.1-dev
```

(freetype/fontconfig/webkit2gtk are required by DPF's DGL/NanoVG UI backend
and its WebView-leak workaround, not JUCE leftovers.)

### Configure / build / run

```bash
# Configure (first run fetches DPF + AudioPluginsCommon into build/_deps/)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build --parallel

# Build outputs (DPF layout, under build/bin/ -- no Standalone target:
# Outflank is an effect, and this workspace only requires Standalone for
# instruments)
#   build/bin/Outflank.vst3/
#   build/bin/Outflank.clap
#   build/bin/Outflank.lv2/

# Run the unit tests (DSP + factory-presets + parameter-metadata checks)
ctest --test-dir build --output-on-failure

# Install/uninstall (Linux, user directories by default; --system for /usr/lib)
./scripts/install.sh
./scripts/uninstall.sh
```

## Architecture

### Signal flow

```
Input (L/R) → M/S encode → per-band StateVariableFilter lowpass (M and S)
            → discard S low band (forces mono bass)
            → QuadraturePair phase-rotates M's high band, proportional to `rejection`
            → recombine M/S → L/R decode → Output
```

`CrossoverMS::process()` (`Source/DSP/CrossoverMS.cpp`) is the single entry
point `OutflankPluginAdapter::run()` calls once per block; all state
(filter coefficients, smoothed rejection amount) lives inside it, matching
the pre-migration `PluginProcessor::processBlock()` call exactly.

### Parameters

| Parameter | Symbol | Range | Default | Unit | DPF hints |
|---|---|---|---|---|---|
| Frequency | `frequency` | 40 – 2000 | 250 | Hz | `kParameterIsAutomatable \| kParameterIsLogarithmic` |
| Q | `q` | 0.3 – 4.0 | 0.707 | — | `kParameterIsAutomatable` |
| Rejection | `rejection` | 0 – 100 | 0 | % | `kParameterIsAutomatable` |

(Ranges/defaults verified against `Source/DistrhoPluginInfo.h`'s
`OUTFLANK_PARAM_*` macros — carried over verbatim from the JUCE-era
`Source/_juce_reference/PluginProcessor.cpp`'s `createParameterLayout()`.)

`rejection` is converted from its 0-100% host range to `CrossoverMS::process()`'s
0..1 `rejection01` argument via `Source/OutflankParams.h::rejectionPercentToUnit()`
(unit-tested in `Tests/test_outflank_parameters.cpp`).

`frequency`'s `kParameterIsLogarithmic` hint approximates the JUCE-era
`NormalisableRange<float>(40.f, 2000.f, 1.f, 0.3f)` skew's intent (more
automation resolution at low frequencies) for hosts that honor it. This is
a documented simplification, not a full port: `Common`'s `RotaryKnobModel`
maps on-screen knob drag-to-value linearly (like every other knob in this
UI kit), so the DPF host automation curve is logarithmic-hinted but the
knob's own drag feel is linear — different from the JUCE original's skewed
slider drag, by deliberate design-consistency choice with the rest of the
`Common`-based UI kit.

No bypass parameter and no reported latency: the pre-migration JUCE
processor never declared bypass, and `CrossoverMS` is a zero-latency filter
chain (`DISTRHO_PLUGIN_WANT_LATENCY 0`).

### Presets

3 factory presets, compiled into `Source/FactoryPresets.h` (hand-converted
from the original `Source/Presets/Factory/*.xml`, preserved unchanged as
historical reference):

| Name | Frequency | Q | Rejection |
|---|---|---|---|
| Tight Mono Bass | 100 Hz | 1.0 | 10% |
| Wide Airy | 350 Hz | 0.707 | 80% |
| Subtle | 180 Hz | 0.707 | 20% |

User presets save to `~/.config/Outflank/presets/*.xml` (Linux only for
now) via a native OS save dialog (DPF's `openFileBrowser`); DELETE is
disabled on factory presets. Presets are a purely in-plugin-UI feature — no
DPF Program/State host integration, same as Hex.

## Key design constraints

- **Effect, not synth** — `DISTRHO_PLUGIN_IS_SYNTH 0`; simple stereo
  in/stereo out, no MIDI, no sidechain (unlike its sibling Tank).
- **No custom look-and-feel** — `OutflankUI.cpp`'s knob/button/preset-selector
  palettes are all derived directly from `AudioPluginsCommon::hui::Theme::defaultTheme()`'s
  4 tokens (`windowBackground`, `widgetBackground`, `text`, `accent`); no
  bespoke Outflank colors were invented, since it never had a custom
  `LookAndFeel` to carry over.
- **DSP structurally untouched by the migration, but not bug-for-bug
  identical** — `Source/DSP/*.h` and `CrossoverMS.cpp` have zero DPF/JUCE
  dependency either before or after this migration, and only the adapter
  shim around them changed shape. However, the validator pass (Task 5)
  found and fixed 2 real DSP bugs pre-existing in the ported code (see "CI
  / validator status" below) — the DSP is more correct post-migration than
  the `Source/_juce_reference/` original, not merely relocated.
- **DPF pinned to a commit SHA** (`4238e1c7f0351bbe488d79f0899c540543ac7583`,
  no tagged DPF releases exist) with 2 upstream patches
  (`cmake/patches/dpf-clap-state-chunked-read.patch`,
  `dpf-clap-activate-latency.patch`) applied via `cmake/apply_patch.cmake` —
  same pin and patches as Hex/Pugilist, for consistency across the workspace's
  DPF-migrated plugins.

## CI / validator status

Three validators run in Linux CI (`.github/workflows/ci.yml`): pluginval
(VST3), clap-validator (CLAP), and lv2lint (LV2). The validator pass (Task
5 of `docs/superpowers/plans/2026-09-05-outflank-dpf-migration.md`) found
and fixed real issues — it was **not** a clean first pass. Filed/closed
issues, per `gh issue list --repo TriYop/spellbound-outflank --state closed`:

- **#1 — `process-varying-sample-rates`** (clap-validator): `AllpassFilter`
  had a divide-by-zero-adjacent pole instability whenever a `QuadraturePair`
  corner frequency landed at or above the current sample rate's Nyquist
  frequency at low sample rates. Fixed by clamping the crossover frequency
  below Nyquist before prewarping (commit `a1a1705`); a regression test was
  added later in commit `af1695f`.
- **#2 — `process-sleep-constant-mask`** (clap-validator): `StateVariableFilter`
  and `AllpassFilter` IIR state decayed through the subnormal (denormal)
  float range on silence, tripping clap-validator's CPU/behavior warning.
  Fixed via an explicit per-state denormal flush (`flushDenormal()`)
  applied to `StateVariableFilter`, `AllpassFilter`, and — in a follow-up
  fix after the first pass missed one call site — `CrossoverMS`'s
  `smoothedRejection_` rejection smoother (commits `7fe17dd` and `ccf9886`).
  Regression tests for the first two sites were added in `af1695f`, which
  precedes `ccf9886` and so does not cover it; `CrossoverMS`'s own
  regression test was added later, in commit `ef3eec8`.
- **#3 — lv2lint "Plugin Author Homepage" WARN**: `OutflankPluginAdapter`
  never overrode `getHomePage()`. Fixed by overriding it (commit `7f0c14e`).
- **#5 — lv2lint "Plugin Author Email" WARN**: filed and closed immediately
  as won't-fix, needs upstream DPF — DPF has no email/`foaf:mbox` API
  surface at all, so there is nothing in this plugin's code to change.
  Parity with the pre-accepted "Plugin Class" finding below. (There is no
  issue #4 in this repo's tracker — #4 is the pull request, not an issue.)

**Known, accepted, non-blocking findings** (intentionally left as-is, not
bugs to fix):

- **lv2lint "Plugin Class" FAIL** (`rdf:type` includes `doap:Project`
  alongside `lv2:Plugin`) is baked unconditionally into DPF's own LV2 TTL
  generator (`distrho/src/DistrhoPluginLV2export.cpp`) — every DPF-based
  LV2 plugin in this workspace emits this identically (matches Hex's
  identical accepted finding); fixing it means patching DPF, not Outflank.
  This is why the `lv2lint` CI step runs with `continue-on-error: true`.
- **lv2lint "Plugin Author Email" WARN** (issue #5 above) — same
  upstream-DPF-limitation class as "Plugin Class".
- **clap-validator's `process-audio-denormals` warning is reduced but not
  fully eliminated.** The per-state flush (issue #2) removes denormals that
  originate *inside* the DSP's own feedback state. The remaining trigger is
  externally-injected denormal *input* samples in clap-validator's own
  test signal, which sit upstream of anything a per-state flush can reach.
  This was assessed and accepted as out of scope for this migration, not a
  remaining TODO.
