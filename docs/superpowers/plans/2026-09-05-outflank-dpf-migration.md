# Outflank DPF Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate Outflank ("The Spatial Tactician") off JUCE onto DPF (DISTRHO Plugin Framework, ISC-licensed), reusing its already framework-free DSP unchanged, so the plugin builds VST3/CLAP/LV2 (+AU on macOS) instead of JUCE's VST3/Standalone/CLAP-via-clap-juce-extensions.

**Architecture:** Thin `OutflankPluginAdapter : public Plugin` shim drives the existing `CrossoverMS` (which composes `StateVariableFilter`, `QuadraturePair`, `AllpassFilter`) exactly as `PluginProcessor::processBlock()` did — same 3 parameters (`frequency`, `q`, `rejection`), no DSP logic changes. `OutflankUI : public UI` replaces the JUCE editor with 3 `AudioPluginsCommon::hui::dgl::RotaryKnob`s (Theme-default palette, no bespoke colors) plus a presets bar (`PresetSelector` + `Button`s backed by `AudioPluginsCommon::presets::PresetBrowser`). Old JUCE files are preserved unchanged under `Source/_juce_reference/` as the porting reference, exactly as Hex and Pugilist did for their own DPF migrations.

**Tech Stack:** C++20, CMake + Ninja, DPF (FetchContent, pinned commit + 2 patches), `AudioPluginsCommon` v0.2.2 (FetchContent), pugixml (transitive via Common), CTest for DSP/domain unit tests, GitHub Actions CI (Linux-gating, Windows/macOS `continue-on-error`).

**Spec:** `/home/yvan/Projects/AudioPlugins/Common/docs/superpowers/specs/2026-09-05-phase2-outflank-tank-dpf-migration-design.md` (see its "Outflank migration" section and the "Outflank" bullet under "Context" — this plan implements that section's 6 numbered steps as 6 tasks below).

**Reference template (exact pattern to copy, not the plugin-specific content):** `/home/yvan/Projects/AudioPlugins/Hex/.claude/worktrees/dpf-stage0/` — `CMakeLists.txt`, `Source/DistrhoPluginInfo.h`, `Source/HexPluginAdapter.{h,cpp}`, `Source/HexUI.{h,cpp}`, `Source/FactoryPresets.h`, `cmake/patches/*.patch`, `cmake/apply_patch.cmake`, `.github/workflows/ci.yml`, `CLAUDE.md`.

## Global Constraints

- DPF is pinned to commit SHA `4238e1c7f0351bbe488d79f0899c540543ac7583` (no tagged DPF releases exist), patched with `cmake/patches/dpf-clap-state-chunked-read.patch` and `cmake/patches/dpf-clap-activate-latency.patch` via `cmake/apply_patch.cmake` as a `PATCH_COMMAND` — copy all three files verbatim from Hex's worktree, do not hand-retype the diffs.
- `AudioPluginsCommon` is pinned to `GIT_TAG v0.2.2` via `FetchContent`, and that `FetchContent_Declare`/`FetchContent_MakeAvailable` pair must appear **after** the `dpf_add_plugin(Outflank ...)` call in `CMakeLists.txt` — DPF only creates its `dgl-opengl` target lazily inside `dpf_add_plugin()`, so pulling Common in earlier makes `AudioPluginsCommon::hui_dgl` silently not exist.
- `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` must be set before Common's `FetchContent_MakeAvailable` call (Common's static libs don't set PIC themselves; every Outflank format target is a shared object).
- No copyleft license may be introduced (workspace-wide "Common Requirements" rule in `AudioPlugins/CLAUDE.md`) — DPF is ISC, Common is permissive; nothing else gets added.
- CI matrix is `[ubuntu-latest, windows-latest, macos-latest]`; only `ubuntu-latest` gates the workflow (`continue-on-error: ${{ matrix.os != 'ubuntu-latest' }}`) — Windows/macOS fixes are explicitly out of scope (Phase 6 per the workspace roadmap).
- The isolated worktree is created at `.claude/worktrees/dpf-stage0` on branch `worktree-dpf-stage0`, matching Hex's and Pugilist's exact naming convention, via the `superpowers:using-git-worktrees` skill.
- Every validator finding gets its own GitHub issue filed against `TriYop/spellbound-outflank` (this workspace's "ticket per task" convention) before being fixed, and closed once verified fixed.
- Outflank has no bypass parameter and reports no latency: confirmed by reading `Source/PluginProcessor.cpp` directly — `getTailLengthSeconds()` returns `0.0`, there is no `AudioParameterBool` for bypass in `createParameterLayout()`, and `CrossoverMS` is a zero-latency filter chain (no delay lines). `DISTRHO_PLUGIN_WANT_LATENCY` stays `0`.
- Outflank ships no Standalone target: it's an effect, not an instrument, and this workspace only requires a Standalone build for instruments (see Hex's `CLAUDE.md`, same rule applied there) — even though the pre-migration JUCE build did produce one.
- Outflank is stereo-in/stereo-out only: no sidechain, no MIDI (`DISTRHO_PLUGIN_NUM_INPUTS 2`, `DISTRHO_PLUGIN_NUM_OUTPUTS 2`, `DISTRHO_PLUGIN_WANT_MIDI_INPUT 0`, `DISTRHO_PLUGIN_IS_SYNTH 0`).
- No bespoke UI colors: every palette value in `OutflankUI.cpp` is derived from `audioplugins::common::hui::defaultTheme()`'s 4 tokens (`windowBackground`, `widgetBackground`, `text`, `accent`), reusing/alpha-adjusting them rather than inventing new RGB values — Outflank never had a custom `LookAndFeel` to carry over.

---

## Task 1: Isolated worktree + DPF skeleton (stub plugin, build-only CI)

**Files:**
- Create (via worktree, at execution time): `.claude/worktrees/dpf-stage0/` on branch `worktree-dpf-stage0`
- Create: `cmake/apply_patch.cmake`
- Create: `cmake/patches/dpf-clap-state-chunked-read.patch`
- Create: `cmake/patches/dpf-clap-activate-latency.patch`
- Move: `Source/PluginProcessor.h` → `Source/_juce_reference/PluginProcessor.h`
- Move: `Source/PluginProcessor.cpp` → `Source/_juce_reference/PluginProcessor.cpp`
- Move: `Source/PluginEditor.h` → `Source/_juce_reference/PluginEditor.h`
- Move: `Source/PluginEditor.cpp` → `Source/_juce_reference/PluginEditor.cpp`
- Move: `Source/PresetManager.h` → `Source/_juce_reference/PresetManager.h`
- Move: `Source/PresetManager.cpp` → `Source/_juce_reference/PresetManager.cpp`
- Create: `Source/DistrhoPluginInfo.h`
- Create: `Source/OutflankPluginAdapter.h`
- Create: `Source/OutflankPluginAdapter.cpp`
- Create: `Source/OutflankUI.h`
- Create: `Source/OutflankUI.cpp`
- Modify: `CMakeLists.txt` (full rewrite: drop JUCE/clap-juce-extensions, add DPF)
- Modify: `scripts/install.sh`, `scripts/uninstall.sh` (drop the Standalone/`bin/Outflank` references)
- Modify: `.github/workflows/ci.yml` (create if absent; build-only, Linux-gating)
- Unchanged (do not touch): `Source/DSP/*.h`, `Source/DSP/CrossoverMS.cpp`, `Tests/*.cpp`, `Source/Presets/Factory/*.xml`

**Interfaces:**
- Produces: `class OutflankPluginAdapter : public Plugin` (DPF) with `getLabel()/getDescription()/getMaker()/getLicense()/getVersion()/initParameter()/getParameterValue()/setParameterValue()/activate()/deactivate()/run()` all overridden but stubbed (0 parameters, pure passthrough `run()`). `Plugin* createPlugin()`.
- Produces: `class OutflankUI : public UI` (DPF) with `onNanoDisplay()` overridden (solid background fill only). `UI* createUI()`.
- Consumes (Task 2 onward): the file/class shapes above — Task 2 edits these same files in place rather than restructuring them.

- [ ] **Step 1: Create the isolated worktree**

At execution time, in a fresh session, invoke `superpowers:using-git-worktrees` to create the worktree:

```bash
cd /home/yvan/Projects/AudioPlugins/Outflank
git fetch origin
git worktree add .claude/worktrees/dpf-stage0 -b worktree-dpf-stage0 origin/main
cd .claude/worktrees/dpf-stage0
```

All remaining steps in this plan run from inside that worktree directory.

- [ ] **Step 2: Copy the DPF patches and patch-apply helper from Hex verbatim**

```bash
mkdir -p cmake/patches
cp /home/yvan/Projects/AudioPlugins/Hex/.claude/worktrees/dpf-stage0/cmake/apply_patch.cmake cmake/apply_patch.cmake
cp /home/yvan/Projects/AudioPlugins/Hex/.claude/worktrees/dpf-stage0/cmake/patches/dpf-clap-state-chunked-read.patch cmake/patches/dpf-clap-state-chunked-read.patch
cp /home/yvan/Projects/AudioPlugins/Hex/.claude/worktrees/dpf-stage0/cmake/patches/dpf-clap-activate-latency.patch cmake/patches/dpf-clap-activate-latency.patch
```

These are generic DPF-commit-level fixes (state-load chunked-read + CLAP `activate()` latency reporting), not specific to Hex's DSP — safe to reuse unmodified for any plugin pinned to the same DPF commit.

- [ ] **Step 3: Move the JUCE-era files to `Source/_juce_reference/`**

```bash
mkdir -p Source/_juce_reference
git mv Source/PluginProcessor.h   Source/_juce_reference/PluginProcessor.h
git mv Source/PluginProcessor.cpp Source/_juce_reference/PluginProcessor.cpp
git mv Source/PluginEditor.h      Source/_juce_reference/PluginEditor.h
git mv Source/PluginEditor.cpp    Source/_juce_reference/PluginEditor.cpp
git mv Source/PresetManager.h     Source/_juce_reference/PresetManager.h
git mv Source/PresetManager.cpp   Source/_juce_reference/PresetManager.cpp
```

`Source/Presets/Factory/*.xml` stays exactly where it is (Task 4 hand-converts it into a compiled-in header; the original XMLs remain as historical reference, matching Hex's `Source/Presets/Factory/*.xml` precedent).

- [ ] **Step 4: Write `Source/DistrhoPluginInfo.h` (metadata only, no parameters yet)**

```cpp
/*
 * Spellbound Outflank — DPF plugin metadata.
 *
 * This file's macro set follows DPF's own DistrhoPluginInfo.h.template
 * and the shape of Hex's DistrhoPluginInfo.h (see
 * AudioPlugins/Hex/.claude/worktrees/dpf-stage0/Source/DistrhoPluginInfo.h).
 *
 * Outflank is a stereo effect, not a synth: it does not want MIDI input,
 * and it processes two audio inputs into two audio outputs. Unlike Hex, it
 * reports no latency (CrossoverMS is a zero-latency filter chain — no
 * delay lines) and declares no bypass parameter (the pre-migration JUCE
 * processor never had one either, confirmed by reading
 * Source/_juce_reference/PluginProcessor.cpp).
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Spellbound"
#define DISTRHO_PLUGIN_NAME    "Outflank"
#define DISTRHO_PLUGIN_URI     "https://spellbound.audio/plugins/outflank"
#define DISTRHO_PLUGIN_CLAP_ID "com.spellbound.outflank"

#define DISTRHO_PLUGIN_BRAND_ID  Spbd
#define DISTRHO_PLUGIN_UNIQUE_ID Otfk

#define DISTRHO_PLUGIN_HAS_UI      1
#define DISTRHO_PLUGIN_IS_RT_SAFE  1
#define DISTRHO_PLUGIN_IS_SYNTH    0
#define DISTRHO_PLUGIN_NUM_INPUTS  2
#define DISTRHO_PLUGIN_NUM_OUTPUTS 2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 0
#define DISTRHO_PLUGIN_WANT_LATENCY    0

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Stereo"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "audio-effect", "utility", "stereo"

#define DISTRHO_UI_USE_NANOVG     1
#define DISTRHO_UI_USER_RESIZABLE 0
#define DISTRHO_UI_DEFAULT_WIDTH  320
#define DISTRHO_UI_DEFAULT_HEIGHT 200

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
```

`DISTRHO_PLUGIN_UNIQUE_ID`/`DISTRHO_PLUGIN_BRAND_ID` reuse the exact 4-char codes from the old `CMakeLists.txt` (`PLUGIN_CODE Otfk`, `PLUGIN_MANUFACTURER_CODE Spbd`), not invented values. `DISTRHO_UI_DEFAULT_WIDTH/HEIGHT` (320x200) match the JUCE editor's `kW`/`kH` constants in `Source/_juce_reference/PluginEditor.cpp`.

- [ ] **Step 5: Write the stub `Source/OutflankPluginAdapter.h`**

```cpp
#pragma once

#include "DistrhoPlugin.hpp"

START_NAMESPACE_DISTRHO

/**
   DPF Plugin adapter for Spellbound Outflank.

   Stage 0 stub: passthrough only, zero host parameters. Task 2 wires the
   real CrossoverMS/StateVariableFilter/QuadraturePair/AllpassFilter chain
   (Source/DSP/) and the frequency/q/rejection host parameters here.
 */
class OutflankPluginAdapter : public Plugin
{
public:
    OutflankPluginAdapter();

protected:
    // -- Information -----------------------------------------------------
    const char* getLabel() const override;
    const char* getDescription() const override;
    const char* getMaker() const override;
    const char* getLicense() const override;
    uint32_t getVersion() const override;

    // -- Init -------------------------------------------------------------
    void initParameter(uint32_t index, Parameter& parameter) override;

    // -- Internal data ------------------------------------------------------
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

    // -- Process --------------------------------------------------------------
    void activate() override;
    void deactivate() override;
    void run(const float** inputs, float** outputs, uint32_t frames) override;

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutflankPluginAdapter)
};

END_NAMESPACE_DISTRHO
```

- [ ] **Step 6: Write the stub `Source/OutflankPluginAdapter.cpp`**

```cpp
#include "OutflankPluginAdapter.h"

#include <cstring>

START_NAMESPACE_DISTRHO

OutflankPluginAdapter::OutflankPluginAdapter()
    : Plugin(0, 0, 0) // Stage 0 stub: no parameters, no programs, no states yet
{
}

const char* OutflankPluginAdapter::getLabel() const { return "Outflank"; }
const char* OutflankPluginAdapter::getDescription() const { return "Mono-bass / wide-highs stereo crossover"; }
const char* OutflankPluginAdapter::getMaker() const { return "Spellbound"; }

const char* OutflankPluginAdapter::getLicense() const
{
    // Must be a URI (DPF only emits doap:license as a proper URI-typed
    // literal in the LV2 TTL when the string contains "://" -- a plain
    // word like "Proprietary" fails lv2lint's Plugin License test) -- see
    // Hex's DistrhoPluginLV2export.cpp note in HexPluginAdapter.cpp.
    return "https://spellbound.audio/plugins/outflank#license";
}

uint32_t OutflankPluginAdapter::getVersion() const
{
    return d_version(1, 0, 0);
}

void OutflankPluginAdapter::initParameter(uint32_t, Parameter&) {}
float OutflankPluginAdapter::getParameterValue(uint32_t) const { return 0.0f; }
void OutflankPluginAdapter::setParameterValue(uint32_t, float) {}

void OutflankPluginAdapter::activate() {}
void OutflankPluginAdapter::deactivate() {}

void OutflankPluginAdapter::run(const float** inputs, float** outputs, uint32_t frames)
{
    // Stage 0 stub: passthrough only. Task 2 wires the real CrossoverMS chain here.
    if (outputs[0] != inputs[0])
        std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
    if (outputs[1] != inputs[1])
        std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);
}

Plugin* createPlugin()
{
    return new OutflankPluginAdapter();
}

END_NAMESPACE_DISTRHO
```

- [ ] **Step 7: Write the stub `Source/OutflankUI.h` and `Source/OutflankUI.cpp`**

`Source/OutflankUI.h`:

```cpp
#pragma once

#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

/**
   DPF UI adapter for Spellbound Outflank.

   Stage 0 stub: solid background only. Task 3 adds the 3 RotaryKnobs
   (frequency/q/rejection) and panel/title/label theming; Task 4 adds the
   presets bar.
 */
class OutflankUI : public UI
{
public:
    OutflankUI();

protected:
    void onNanoDisplay() override;

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutflankUI)
};

END_NAMESPACE_DISTRHO
```

`Source/OutflankUI.cpp`:

```cpp
#include "OutflankUI.h"

START_NAMESPACE_DISTRHO

OutflankUI::OutflankUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
{
}

void OutflankUI::onNanoDisplay()
{
    beginPath();
    rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(DGL_NAMESPACE::Color(0x14, 0x14, 0x20));
    fill();
    closePath();
}

UI* createUI()
{
    return new OutflankUI();
}

END_NAMESPACE_DISTRHO
```

- [ ] **Step 8: Rewrite `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)
project(Outflank VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# DPF has no tagged releases -- downstream projects pin an exact commit SHA
# on `main` instead. This is the same commit Hex/Pugilist pin.
set(AUDIOPLUGINS_DPF_GIT_TAG "4238e1c7f0351bbe488d79f0899c540543ac7583" CACHE STRING "Pinned DPF commit")

# PATCH_COMMAND (two patches, copied verbatim from Hex -- see each file for
# the full explanation; neither reported upstream yet):
#
# 1. dpf-clap-state-chunked-read.patch -- PluginCLAP::stateLoad() can't tell
#    a genuine embedded NUL terminator from simply running out of bytes at a
#    chunk boundary, so a host that returns short reads from
#    clap_istream_t::read() can lose every parameter on state load.
# 2. dpf-clap-activate-latency.patch -- PluginCLAP::activate() never reports
#    the plugin's initial latency. Outflank itself reports 0 latency always
#    (DISTRHO_PLUGIN_WANT_LATENCY 0), but this patch fixes a real bug in the
#    shared DPF checkout regardless, so it's applied unconditionally like
#    every other DPF-based plugin in this workspace.
#
# CAVEAT: the patch step only runs on a *fresh* population of the source
# dir -- delete build/_deps/dpf-* when you need to be sure the patches took
# effect. cmake/apply_patch.cmake is idempotent (skips an already-applied
# patch), so a re-run is harmless but not a substitute for a clean re-fetch.
FetchContent_Declare(dpf
    GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
    GIT_TAG        ${AUDIOPLUGINS_DPF_GIT_TAG}
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-state-chunked-read.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
            COMMAND ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-activate-latency.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
)
FetchContent_MakeAvailable(dpf)

set(OUTFLANK_DPF_TARGETS vst3 clap lv2)
if(APPLE)
    list(APPEND OUTFLANK_DPF_TARGETS au)
endif()

dpf_add_plugin(Outflank
    TARGETS ${OUTFLANK_DPF_TARGETS}
    UI_TYPE opengl
    FILES_DSP
        Source/OutflankPluginAdapter.cpp
        Source/DSP/CrossoverMS.cpp
    FILES_UI
        Source/OutflankUI.cpp
)

target_include_directories(Outflank PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# DPF's dpf_add_plugin() unconditionally compiles dgl/src/pugl.cpp's inlined
# WebViewImpl.cpp (a DPF bug -- see Hex's CMakeLists.txt for the full
# explanation), which uses the GNU `typeof` extension and fails under strict
# ISO C++ (our CMAKE_CXX_EXTENSIONS OFF default). Allow GNU extensions on
# DPF's own dgl-opengl target rather than patching vendored DPF code.
if(TARGET dgl-opengl)
    set_target_properties(dgl-opengl PROPERTIES CXX_EXTENSIONS ON)
endif()

# ── Unit tests (no JUCE dependency) ──────────────────────────────────────────
# Unchanged from the pre-migration CMakeLists.txt -- CrossoverMS/AllpassFilter/
# QuadraturePair/StateVariableFilter never depended on JUCE, so these test
# executables don't need any porting.
enable_testing()

add_executable(test_statevariablefilter
    Tests/test_statevariablefilter.cpp
)
target_include_directories(test_statevariablefilter PRIVATE Source/ Tests/)
target_compile_features(test_statevariablefilter PRIVATE cxx_std_20)
add_test(NAME StateVariableFilter COMMAND test_statevariablefilter)

add_executable(test_crossoverms
    Tests/test_crossoverms.cpp
    Source/DSP/CrossoverMS.cpp
)
target_include_directories(test_crossoverms PRIVATE Source/ Tests/)
target_compile_features(test_crossoverms PRIVATE cxx_std_20)
add_test(NAME CrossoverMS COMMAND test_crossoverms)

add_executable(test_allpassfilter
    Tests/test_allpassfilter.cpp
)
target_include_directories(test_allpassfilter PRIVATE Source/ Tests/)
target_compile_features(test_allpassfilter PRIVATE cxx_std_20)
add_test(NAME AllpassFilter COMMAND test_allpassfilter)

add_executable(test_quadraturepair
    Tests/test_quadraturepair.cpp
)
target_include_directories(test_quadraturepair PRIVATE Source/ Tests/)
target_compile_features(test_quadraturepair PRIVATE cxx_std_20)
add_test(NAME QuadraturePair COMMAND test_quadraturepair)

# ── Install rules ─────────────────────────────────────────────────────────────
# DPF's dpf_add_plugin() writes all format outputs under a flat bin/ dir --
# no Standalone binary: Outflank is an effect, and this workspace only
# requires Standalone for instruments (see Hex's CLAUDE.md, same rule).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_BIN "${CMAKE_BINARY_DIR}/bin")

install(DIRECTORY  "${_BIN}/Outflank.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

# CLAP is a flat shared-library file on Linux/Windows but a bundle
# *directory* on macOS (like VST3/AU) -- see Hex's CMakeLists.txt, which hit
# this as a real CI failure.
if(APPLE)
    install(DIRECTORY  "${_BIN}/Outflank.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            USE_SOURCE_PERMISSIONS)
else()
    install(FILES      "${_BIN}/Outflank.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

install(DIRECTORY  "${_BIN}/Outflank.lv2"
        DESTINATION LV2
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

install(PROGRAMS   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install.sh"
                   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/uninstall.sh"
        DESTINATION .
        COMPONENT   Runtime)

# ── CPack (TGZ) ───────────────────────────────────────────────────────────────
set(CPACK_GENERATOR                 TGZ)
set(CPACK_PACKAGE_NAME              Outflank)
set(CPACK_PACKAGE_VENDOR            Spellbound)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Mono-bass / wide-highs stereo crossover")
set(CPACK_PACKAGE_VERSION           ${PROJECT_VERSION})
set(CPACK_SYSTEM_NAME               linux-x86_64)
set(CPACK_PACKAGE_FILE_NAME         "Outflank-${PROJECT_VERSION}-linux-x86_64")
set(CPACK_PACKAGING_INSTALL_PREFIX  "")
set(CPACK_STRIP_FILES               TRUE)
set(CPACK_PACKAGE_CHECKSUM          SHA256)
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")
include(CPack)
```

- [ ] **Step 9: Update `scripts/install.sh` and `scripts/uninstall.sh` (drop Standalone)**

Edit `scripts/install.sh`: remove the `BIN_DIR`/`bin/Outflank` block entirely (the `mkdir -p "${BIN_DIR}"` / `cp .../bin/Outflank` / `echo "  App → ..."` lines), and change `--system` BIN_DIR from `/usr/local/bin` to nothing (no longer needed) — leave only the VST3 and CLAP install blocks, then append an LV2 block matching Hex's pattern:

```bash
mkdir -p "${LV2_DIR}"
rm -rf   "${LV2_DIR}/Outflank.lv2"
cp -r    "${SCRIPT_DIR}/LV2/Outflank.lv2" "${LV2_DIR}/"
echo "  LV2   → ${LV2_DIR}/Outflank.lv2"
```

with `LV2_DIR` set alongside `VST3_DIR`/`CLAP_DIR` at the top (`/usr/lib/lv2` for `--system`, `${HOME}/.lv2` otherwise). Also update the header comment to `# Install Outflank plugins (VST3/CLAP/LV2). Outflank is an effect -- no standalone app.`

Edit `scripts/uninstall.sh` similarly: drop the `${HOME}/.local/bin/Outflank` / `/usr/local/bin/Outflank` remove/skip lines, add `${HOME}/.lv2/Outflank.lv2` and `/usr/lib/lv2/Outflank.lv2` in their place, matching Hex's `uninstall.sh` structure exactly.

- [ ] **Step 10: Write the build-only `.github/workflows/ci.yml`**

```yaml
name: CI

on:
  push:
    branches: [ "main", "master" ]
    tags: [ "v*" ]
  pull_request:
    branches: [ "main", "master" ]

jobs:
  build-and-test:
    # Windows and macOS legs are unverified -- see Hex's ci.yml for the same
    # rationale (dgl-opengl's CXX_EXTENSIONS ON workaround is a documented
    # no-op on MSVC). Gate only on the verified Linux leg.
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        build_type: [Release]

    runs-on: ${{ matrix.os }}
    continue-on-error: ${{ matrix.os != 'ubuntu-latest' }}

    steps:
      - uses: actions/checkout@v4

      - name: Install Linux build dependencies
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            libasound2-dev libjack-jackd2-dev \
            libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
            libxinerama-dev libxrandr-dev libxrender-dev \
            libfreetype-dev libfontconfig1-dev \
            libglu1-mesa-dev libwebkit2gtk-4.1-dev \
            xvfb

      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}

      - name: Build
        run: cmake --build build --config ${{ matrix.build_type }} --parallel

      - name: Test
        working-directory: build
        run: ctest --build-config ${{ matrix.build_type }} --output-on-failure
```

- [ ] **Step 11: Configure and build to verify the skeleton compiles**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```

Expected: DPF fetches into `build/_deps/dpf-src` (first configure only, ~1-2 min), both patches apply (watch for `apply_patch: applied ...` in the Configure output), and the build produces `build/bin/Outflank.vst3/`, `build/bin/Outflank.clap`, `build/bin/Outflank.lv2/`.

- [ ] **Step 12: Run the existing DSP test suite to verify nothing regressed**

```bash
ctest --test-dir build --output-on-failure
```

Expected: `StateVariableFilter`, `CrossoverMS`, `AllpassFilter`, `QuadraturePair` all pass (unchanged from before the migration — this is the "preserve existing CTest DSP tests unchanged" requirement).

- [ ] **Step 13: Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
Stage 0: DPF skeleton for Outflank (stub adapter/UI, build-only CI)

Drops JUCE + clap-juce-extensions in favor of DPF (pinned commit +
2 upstream patches, same pattern as Hex/Pugilist). PluginProcessor/
PluginEditor/PresetManager preserved unchanged under Source/_juce_reference/
as the porting reference. OutflankPluginAdapter/OutflankUI are Stage 0
stubs (passthrough audio, blank UI) -- Task 2 wires the real DSP.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Wire the real DSP chain into `OutflankPluginAdapter`

**Files:**
- Create: `Source/OutflankParams.h`
- Create: `Tests/test_outflank_parameters.cpp`
- Modify: `Source/DistrhoPluginInfo.h` (add parameter enum + range/default macros)
- Modify: `Source/OutflankPluginAdapter.h` (add parameter storage + `CrossoverMS` member)
- Modify: `Source/OutflankPluginAdapter.cpp` (real `initParameter`/`getParameterValue`/`setParameterValue`/`activate`/`deactivate`/`run`)
- Modify: `CMakeLists.txt` (add `test_outflank_parameters` target)

**Interfaces:**
- Consumes: `CrossoverMS::reset()` / `CrossoverMS::process(float* left, float* right, int numSamples, float frequencyHz, float q, float rejection01, double sampleRate)` from `Source/DSP/CrossoverMS.h` (unchanged, already exists).
- Produces: `enum OutflankParameters { kParameterFrequency = 0, kParameterQ, kParameterRejection, kParameterCount }` and macros `OUTFLANK_PARAM_{FREQUENCY,Q,REJECTION}_{MIN,MAX,DEFAULT}` in `Source/DistrhoPluginInfo.h` — consumed directly by Task 3's `OutflankUI.cpp`.
- Produces: `float outflank::rejectionPercentToUnit(float rejectionPercent) noexcept` in `Source/OutflankParams.h`.

- [ ] **Step 1: Write the failing test**

Create `Tests/test_outflank_parameters.cpp`:

```cpp
#include "test_runner.h"
#include "../Source/DistrhoPluginInfo.h"
#include "../Source/OutflankParams.h"

int main()
{
    // Ranges/defaults must match the JUCE-era
    // Source/_juce_reference/PluginProcessor.cpp createParameterLayout()
    // exactly -- these macros are the DPF-side port of that layout.
    CHECK(kParameterCount == 3);

    CHECK(OUTFLANK_PARAM_FREQUENCY_MIN == 40.0f);
    CHECK(OUTFLANK_PARAM_FREQUENCY_MAX == 2000.0f);
    CHECK(OUTFLANK_PARAM_FREQUENCY_DEFAULT == 250.0f);

    CHECK(OUTFLANK_PARAM_Q_MIN == 0.3f);
    CHECK(OUTFLANK_PARAM_Q_MAX == 4.0f);
    CHECK(OUTFLANK_PARAM_Q_DEFAULT == 0.707f);

    CHECK(OUTFLANK_PARAM_REJECTION_MIN == 0.0f);
    CHECK(OUTFLANK_PARAM_REJECTION_MAX == 100.0f);
    CHECK(OUTFLANK_PARAM_REJECTION_DEFAULT == 0.0f);

    // Boundary conversion: rejection host parameter (0-100%) -> CrossoverMS's
    // rejection01 argument (0..1) -- carried over verbatim from the
    // JUCE-era PluginProcessor::processBlock()'s
    // `apvts.getRawParameterValue("rejection")->load(...) * 0.01f`.
    CHECK(outflank::rejectionPercentToUnit(0.0f) == 0.0f);
    CHECK(outflank::rejectionPercentToUnit(100.0f) == 1.0f);
    CHECK(outflank::rejectionPercentToUnit(50.0f) == 0.5f);
    CHECK(outflank::rejectionPercentToUnit(10.0f) == 0.1f);

    TEST_SUMMARY();
    return 0;
}
```

Add the CTest target to `CMakeLists.txt` (after the `test_quadraturepair` block):

```cmake
add_executable(test_outflank_parameters
    Tests/test_outflank_parameters.cpp
)
target_include_directories(test_outflank_parameters PRIVATE Source/ Tests/)
target_compile_features(test_outflank_parameters PRIVATE cxx_std_20)
add_test(NAME OutflankParameters COMMAND test_outflank_parameters)
```

- [ ] **Step 2: Run it and verify it fails**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_outflank_parameters
```

Expected: compile failure — `OUTFLANK_PARAM_FREQUENCY_MIN` and `outflank::rejectionPercentToUnit` are not yet defined.

- [ ] **Step 3: Add the parameter enum/macros to `Source/DistrhoPluginInfo.h`**

Insert before the final `#endif`:

```cpp

/*
 * Host parameter indices, shared between the DSP adapter and the UI.
 * Declared here (rather than in OutflankPluginAdapter.h) so the UI
 * translation unit can use them without pulling in DistrhoPlugin.hpp --
 * same placement Hex's DistrhoPluginInfo.h uses.
 */
enum OutflankParameters {
    kParameterFrequency = 0,
    kParameterQ,
    kParameterRejection,
    kParameterCount   // 3 -- no bypass, no meters, on any format
};

/* Ranges/defaults carried over verbatim from the JUCE-era
   Source/_juce_reference/PluginProcessor.cpp createParameterLayout(). */
#define OUTFLANK_PARAM_FREQUENCY_MIN     40.0f
#define OUTFLANK_PARAM_FREQUENCY_MAX     2000.0f
#define OUTFLANK_PARAM_FREQUENCY_DEFAULT 250.0f

#define OUTFLANK_PARAM_Q_MIN     0.3f
#define OUTFLANK_PARAM_Q_MAX     4.0f
#define OUTFLANK_PARAM_Q_DEFAULT 0.707f

#define OUTFLANK_PARAM_REJECTION_MIN     0.0f
#define OUTFLANK_PARAM_REJECTION_MAX     100.0f
#define OUTFLANK_PARAM_REJECTION_DEFAULT 0.0f
```

- [ ] **Step 4: Create `Source/OutflankParams.h`**

```cpp
#pragma once

// Pure conversion helper for OutflankPluginAdapter's host-parameter ->
// CrossoverMS-argument boundary. Framework-free (no DPF/DGL dependency) so
// it can be exercised by a bare CTest executable, same convention as
// Source/DSP/*.h.
namespace outflank {

// CrossoverMS::process()'s `rejection01` argument is 0..1; the host
// parameter is a 0-100% control (see DistrhoPluginInfo.h's
// OUTFLANK_PARAM_REJECTION_* range) -- carried over verbatim from the
// JUCE-era PluginProcessor::processBlock()'s
// `apvts.getRawParameterValue("rejection")->load(...) * 0.01f`.
inline float rejectionPercentToUnit(float rejectionPercent) noexcept
{
    return rejectionPercent * 0.01f;
}

} // namespace outflank
```

- [ ] **Step 5: Run the test and verify it passes**

```bash
cmake --build build --target test_outflank_parameters
./build/test_outflank_parameters
```

Expected: `12 passed, 0 failed` (or similar — 3 macro checks × 3 params + 4 conversion checks + the `kParameterCount` check = 12 `CHECK`s).

- [ ] **Step 6: Wire the real adapter — `Source/OutflankPluginAdapter.h`**

```cpp
#pragma once

#include "DistrhoPlugin.hpp"
#include "DSP/CrossoverMS.h"

START_NAMESPACE_DISTRHO

/**
   DPF Plugin adapter for Spellbound Outflank.

   Thin shim over CrossoverMS (Source/DSP/CrossoverMS.h, framework-free):
   reads the frequency/q/rejection host parameters and calls
   CrossoverMS::process() once per block. All DSP logic itself lives in
   CrossoverMS (and the StateVariableFilter/QuadraturePair/AllpassFilter it
   composes) so it stays unit-tested without any DPF/host machinery, exactly
   as before this migration (see Tests/test_crossoverms.cpp etc., unchanged).
 */
class OutflankPluginAdapter : public Plugin
{
public:
    OutflankPluginAdapter();

protected:
    // -- Information -----------------------------------------------------
    const char* getLabel() const override;
    const char* getDescription() const override;
    const char* getMaker() const override;
    const char* getLicense() const override;
    uint32_t getVersion() const override;

    // -- Init -------------------------------------------------------------
    void initParameter(uint32_t index, Parameter& parameter) override;

    // -- Internal data ------------------------------------------------------
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

    // -- Process --------------------------------------------------------------
    void activate() override;
    void deactivate() override;
    void run(const float** inputs, float** outputs, uint32_t frames) override;

private:
    float frequency = OUTFLANK_PARAM_FREQUENCY_DEFAULT;
    float q = OUTFLANK_PARAM_Q_DEFAULT;
    float rejection = OUTFLANK_PARAM_REJECTION_DEFAULT;

    CrossoverMS crossover_;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutflankPluginAdapter)
};

END_NAMESPACE_DISTRHO
```

- [ ] **Step 7: Wire the real adapter — `Source/OutflankPluginAdapter.cpp`**

```cpp
#include "OutflankPluginAdapter.h"
#include "OutflankParams.h"

#include <cstring>

START_NAMESPACE_DISTRHO

OutflankPluginAdapter::OutflankPluginAdapter()
    : Plugin(kParameterCount, 0, 0) // 3 parameters, 0 programs, 0 states -- no meters, no bypass
{
}

const char* OutflankPluginAdapter::getLabel() const { return "Outflank"; }
const char* OutflankPluginAdapter::getDescription() const { return "Mono-bass / wide-highs stereo crossover"; }
const char* OutflankPluginAdapter::getMaker() const { return "Spellbound"; }

const char* OutflankPluginAdapter::getLicense() const
{
    return "https://spellbound.audio/plugins/outflank#license";
}

uint32_t OutflankPluginAdapter::getVersion() const
{
    return d_version(1, 0, 0);
}

void OutflankPluginAdapter::initParameter(const uint32_t index, Parameter& parameter)
{
    switch (index)
    {
    case kParameterFrequency:
        // kParameterIsLogarithmic asks the host to use a logarithmic
        // automation-display curve, matching the JUCE-era
        // NormalisableRange<float>(40.f, 2000.f, 1.f, 0.3f) skew's intent
        // (more resolution at low frequencies). DPF's Parameter API has no
        // direct skew-factor equivalent, and Common's RotaryKnobModel
        // (Task 3) maps drag-to-value linearly like every other knob in
        // this UI kit -- so the on-screen knob's drag feel is a documented,
        // deliberate simplification versus the JUCE original; only the
        // host automation curve hint is preserved.
        parameter.hints  = kParameterIsAutomatable | kParameterIsLogarithmic;
        parameter.name   = "Frequency";
        parameter.symbol = "frequency";
        parameter.unit   = "Hz";
        parameter.ranges.def = OUTFLANK_PARAM_FREQUENCY_DEFAULT;
        parameter.ranges.min = OUTFLANK_PARAM_FREQUENCY_MIN;
        parameter.ranges.max = OUTFLANK_PARAM_FREQUENCY_MAX;
        break;
    case kParameterQ:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Q";
        parameter.symbol = "q";
        parameter.ranges.def = OUTFLANK_PARAM_Q_DEFAULT;
        parameter.ranges.min = OUTFLANK_PARAM_Q_MIN;
        parameter.ranges.max = OUTFLANK_PARAM_Q_MAX;
        break;
    case kParameterRejection:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Rejection";
        parameter.symbol = "rejection";
        parameter.unit   = "%";
        parameter.ranges.def = OUTFLANK_PARAM_REJECTION_DEFAULT;
        parameter.ranges.min = OUTFLANK_PARAM_REJECTION_MIN;
        parameter.ranges.max = OUTFLANK_PARAM_REJECTION_MAX;
        break;
    default:
        break;
    }
}

float OutflankPluginAdapter::getParameterValue(const uint32_t index) const
{
    switch (index)
    {
    case kParameterFrequency: return frequency;
    case kParameterQ:         return q;
    case kParameterRejection: return rejection;
    default:                  return 0.0f;
    }
}

void OutflankPluginAdapter::setParameterValue(const uint32_t index, const float value)
{
    switch (index)
    {
    case kParameterFrequency: frequency = value; break;
    case kParameterQ:         q = value; break;
    case kParameterRejection: rejection = value; break;
    default: break;
    }
}

void OutflankPluginAdapter::activate()
{
    crossover_.reset();
}

void OutflankPluginAdapter::deactivate()
{
    crossover_.reset();
}

void OutflankPluginAdapter::run(const float** inputs, float** outputs, uint32_t frames)
{
    // CrossoverMS::process() is in-place; run() is called with separate
    // input/output buffers whenever the host doesn't alias them, so copy in
    // first (matches the JUCE-era processBlock(), which always operated on
    // one in-place buffer).
    if (outputs[0] != inputs[0])
        std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
    if (outputs[1] != inputs[1])
        std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);

    crossover_.process(outputs[0], outputs[1], static_cast<int>(frames),
                        frequency, q, outflank::rejectionPercentToUnit(rejection),
                        getSampleRate());
}

Plugin* createPlugin()
{
    return new OutflankPluginAdapter();
}

END_NAMESPACE_DISTRHO
```

- [ ] **Step 8: Build everything and run the full test suite**

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: all 5 tests pass — `StateVariableFilter`, `CrossoverMS`, `AllpassFilter`, `QuadraturePair` (unchanged) and the new `OutflankParameters`.

- [ ] **Step 9: Commit**

```bash
git add Source/DistrhoPluginInfo.h Source/OutflankParams.h Source/OutflankPluginAdapter.h Source/OutflankPluginAdapter.cpp Tests/test_outflank_parameters.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
Wire CrossoverMS into OutflankPluginAdapter via DPF parameters

Declares frequency/q/rejection through DPF's Parameter API and drives the
existing CrossoverMS chain unchanged in run(). Adds a small framework-free
OutflankParams.h::rejectionPercentToUnit() for the 0-100% -> 0..1 boundary
conversion CrossoverMS::process() expects, unit-tested alongside a
macro-consistency check that the ported DPF ranges/defaults match the
JUCE-era createParameterLayout() exactly.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Port the UI — 3 `RotaryKnob`s with Theme's default palette

**Files:**
- Modify: `CMakeLists.txt` (add `AudioPluginsCommon` FetchContent + link `hui_dgl` into `Outflank-ui`)
- Modify: `.github/workflows/ci.yml` (add "Configure Common repo access" step)
- Modify: `Source/OutflankUI.h`, `Source/OutflankUI.cpp` (real 3-knob UI)

**Interfaces:**
- Consumes: `audioplugins::common::hui::dgl::RotaryKnob` / `RotaryKnobPalette` (`AudioPlugins/Common/include/audioplugins/common/hui/dgl/RotaryKnob.h`) and `audioplugins::common::hui::defaultTheme()` / `Colour` (`.../hui/Theme.h`).
- Consumes: `kParameterFrequency`/`kParameterQ`/`kParameterRejection` and `OUTFLANK_PARAM_*` macros from Task 2's `Source/DistrhoPluginInfo.h`.
- Produces: `class OutflankUI` now has 3 live knobs bound to host parameters; `parameterChanged(uint32_t, float)` override — consumed unchanged by Task 4 (extended, not replaced).

- [ ] **Step 1: Add `AudioPluginsCommon` to `CMakeLists.txt`**

Insert immediately after the `if(TARGET dgl-opengl) ... endif()` block, before the `# ── Unit tests` section:

```cmake
# ── Shared HUI/DSP library ───────────────────────────────────────────────────
# github.com/TriYop/spellbound-common is private -- local dev machines clone
# it over the developer's own cached git/gh credentials, while CI
# authenticates via a short-lived `git config url.insteadOf` rewrite driven
# by the COMMON_REPO_TOKEN secret (see .github/workflows/ci.yml's "Configure
# Common repo access" step, added below, which must run before Configure).
#
# ORDERING (load-bearing): this must come *after* dpf_add_plugin(Outflank
# ...), not merely after FetchContent_MakeAvailable(dpf) -- DPF creates its
# backend-specific DGL target (dgl-opengl) lazily from inside
# dpf_add_plugin(), so at FetchContent_MakeAvailable(dpf) time no dgl-*
# target exists yet. Common guards AudioPluginsCommon::hui_dgl on one of
# those targets existing, so pulling Common in any earlier silently drops
# hui_dgl and the link below fails.
#
# Common's own CMakeLists.txt builds its STATIC libraries without setting
# POSITION_INDEPENDENT_CODE -- every one of Outflank's plugin formats
# (VST3/CLAP/LV2) is a shared object, and DPF only sets that property TRUE
# on the targets *it* creates. Setting CMAKE_POSITION_INDEPENDENT_CODE here,
# before Common's add_subdirectory runs, makes it the default for every
# target in Common (and transitively pugixml) -- without this, linking
# AudioPluginsCommon::hui_dgl into Outflank-ui/Outflank.vst3/Outflank.clap/
# Outflank.lv2 fails at link time.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(AudioPluginsCommon
    GIT_REPOSITORY https://github.com/TriYop/spellbound-common.git
    GIT_TAG        v0.2.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(AudioPluginsCommon)

# Linked into `Outflank-ui` rather than `Outflank`: dpf_add_plugin() splits
# the plugin into a common `Outflank` static lib plus `Outflank-dsp` /
# `Outflank-ui` static libs, and OutflankUI.cpp (the only consumer of the
# widget) lives in `Outflank-ui`. Linking it into `Outflank` PRIVATE would
# keep hui_dgl's include directories away from Outflank-ui's own compile,
# and linking it PUBLIC there would drag a GL/UI library into the DSP-only
# LV2 binary as well.
target_link_libraries(Outflank-ui PRIVATE AudioPluginsCommon::hui_dgl)
```

- [ ] **Step 2: Add the "Configure Common repo access" step to `.github/workflows/ci.yml`**

Insert between "Install Linux build dependencies" and "Configure":

```yaml
      - name: Configure Common repo access
        # github.com/TriYop/spellbound-common (FetchContent'd below) is
        # private -- actions/checkout's token doesn't extend to other repos,
        # so rewrite just that one clone URL to embed a read-only,
        # contents-only fine-grained PAT scoped to spellbound-common only.
        run: git config --global url."https://x-access-token:${{ secrets.COMMON_REPO_TOKEN }}@github.com/TriYop/spellbound-common".insteadOf "https://github.com/TriYop/spellbound-common"
```

**Manual follow-up (not scriptable from this plan):** the `COMMON_REPO_TOKEN` secret must be added to the `TriYop/spellbound-outflank` GitHub repo (Settings → Secrets and variables → Actions → New repository secret), using the same fine-grained PAT scoped to `spellbound-common` that Hex's repo already uses. CI will fail at Configure until this is set.

- [ ] **Step 3: Write the real `Source/OutflankUI.h`**

```cpp
#pragma once

#include "DistrhoUI.hpp"

#include "OutflankPluginAdapter.h"

#include "audioplugins/common/hui/dgl/RotaryKnob.h"

#include <memory>

START_NAMESPACE_DISTRHO

/**
   DPF UI adapter for Spellbound Outflank.

   3 RotaryKnobs (frequency/q/rejection) using
   AudioPluginsCommon::hui::Theme's default palette tokens (per design
   decision: Outflank never had a custom LookAndFeel, so no bespoke colors
   are invented here -- see
   docs/superpowers/specs/2026-09-05-phase2-outflank-tank-dpf-migration-design.md
   in the Common repo). Panel/label layout mirrors the JUCE-era editor's
   320x200 3-knob layout (see Source/_juce_reference/PluginEditor.cpp).
 */
class OutflankUI : public UI
{
public:
    OutflankUI();

protected:
    // -- DSP/Plugin Callbacks ---------------------------------------------
    void parameterChanged(uint32_t index, float value) override;

    // -- Widget Callbacks ---------------------------------------------------
    void onNanoDisplay() override;

private:
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fFrequencyKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fQKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fRejectionKnob;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutflankUI)
};

END_NAMESPACE_DISTRHO
```

- [ ] **Step 4: Write the real `Source/OutflankUI.cpp`**

```cpp
#include "OutflankUI.h"

START_NAMESPACE_DISTRHO

namespace {

namespace hui = audioplugins::common::hui;

// Plain hui::Colour (0-255 aggregate, constexpr-friendly) rather than
// DGL::Color directly -- DGL::Color's constructors aren't constexpr, so
// file-scope instances of it can't be. Converted at each draw call via
// toDglColor(), same as Hex's HexUI.cpp.
inline DGL_NAMESPACE::Color toDglColor(const hui::Colour& c, float alphaScale = 1.f) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r),
                                static_cast<int>(c.g),
                                static_cast<int>(c.b),
                                (static_cast<float>(c.a) / 255.f) * alphaScale);
}

// Palette derived directly from Theme's default tokens -- no bespoke
// Outflank colors invented, per this migration's design decision (Outflank
// never had a custom LookAndFeel to begin with).
const hui::dgl::RotaryKnobPalette kOutflankKnobPalette = [] {
    const auto& theme = hui::defaultTheme();
    hui::dgl::RotaryKnobPalette p;
    p.track = theme.widgetBackground;
    p.valueArc = theme.accent;
    p.valueArcGlow = theme.accent;
    p.knobTop = theme.windowBackground;
    p.knobBottom = theme.widgetBackground;
    p.knobRim = theme.accent;
    return p;
}();

// Layout ported from the JUCE-era resized()/paint() (see
// Source/_juce_reference/PluginEditor.cpp): kW=320, kH=200, 3 knobs at
// kKnobSize=80 with kGap=24 between them, centered horizontally. The
// JUCE-era preset bar's vertical slot (kPresetBarH=24 at the top) is
// preserved here even before Task 4 wires the real preset bar, so the knob
// row doesn't have to move again later.
constexpr float kTitleY = 46.0f;
constexpr float kTitleFontSize = 16.0f;

constexpr int kPresetBarH = 24;
constexpr int kKnobY = kPresetBarH + 48; // 72, matches JUCE original
constexpr uint kKnobSize = 80;
constexpr int kKnobGap = 24;
constexpr int kLabelY = kKnobY + static_cast<int>(kKnobSize) + 4;
constexpr int kLabelH = 16;
constexpr float kLabelFontSize = 11.0f;

constexpr int kTotalKnobsW = static_cast<int>(kKnobSize) * 3 + kKnobGap * 2;
constexpr int kRowX0 = (DISTRHO_UI_DEFAULT_WIDTH - kTotalKnobsW) / 2;
constexpr int kFrequencyX = kRowX0;
constexpr int kQX         = kFrequencyX + static_cast<int>(kKnobSize) + kKnobGap;
constexpr int kRejectionX = kQX + static_cast<int>(kKnobSize) + kKnobGap;

struct KnobSpec
{
    uint32_t parameterIndex;
    int x;
    float rangeMin, rangeMax, defaultValue;
    const char* label;
};

// Helper shared by the constructor to avoid repeating the five-line
// construct/size/position/palette/range/gesture-bracketing dance per knob.
std::unique_ptr<hui::dgl::RotaryKnob> makeKnob(OutflankUI& ui, const KnobSpec& spec)
{
    std::unique_ptr<hui::dgl::RotaryKnob> knob(new hui::dgl::RotaryKnob(&ui));
    knob->setSize(kKnobSize, kKnobSize);
    knob->setAbsolutePos(spec.x, kKnobY);
    knob->setPalette(kOutflankKnobPalette);
    knob->setRange(spec.rangeMin, spec.rangeMax);
    knob->setDefaultValue(spec.defaultValue);
    knob->setValue(spec.defaultValue);

    // Gesture bracketing follows DPF's own idiom (drag started/finished ->
    // editParameter, value changed -> setParameterValue), same as Hex.
    const uint32_t paramIndex = spec.parameterIndex;
    hui::dgl::RotaryKnob* const rawKnob = knob.get();
    rawKnob->onDragStateChanged = [&ui, paramIndex](const bool started)
    {
        ui.editParameter(paramIndex, started);
    };
    rawKnob->onValueChanged = [&ui, paramIndex](const float value)
    {
        ui.setParameterValue(paramIndex, value);
    };

    return knob;
}

constexpr KnobSpec kFrequencySpec { kParameterFrequency, kFrequencyX, OUTFLANK_PARAM_FREQUENCY_MIN, OUTFLANK_PARAM_FREQUENCY_MAX, OUTFLANK_PARAM_FREQUENCY_DEFAULT, "Frequency" };
constexpr KnobSpec kQSpec         { kParameterQ,         kQX,         OUTFLANK_PARAM_Q_MIN,         OUTFLANK_PARAM_Q_MAX,         OUTFLANK_PARAM_Q_DEFAULT,         "Q" };
constexpr KnobSpec kRejectionSpec { kParameterRejection, kRejectionX, OUTFLANK_PARAM_REJECTION_MIN, OUTFLANK_PARAM_REJECTION_MAX, OUTFLANK_PARAM_REJECTION_DEFAULT, "Rejection" };

constexpr const KnobSpec* kKnobSpecs[3] = { &kFrequencySpec, &kQSpec, &kRejectionSpec };

} // namespace

OutflankUI::OutflankUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      fFrequencyKnob(makeKnob(*this, kFrequencySpec)),
      fQKnob(makeKnob(*this, kQSpec)),
      fRejectionKnob(makeKnob(*this, kRejectionSpec))
{
    loadSharedResources();
}

void OutflankUI::parameterChanged(const uint32_t index, const float value)
{
    // Programmatic path: RotaryKnob::setValue() deliberately does not fire
    // onValueChanged, so host automation / preset recall cannot loop back
    // out to the host.
    switch (index)
    {
    case kParameterFrequency: fFrequencyKnob->setValue(value); break;
    case kParameterQ:         fQKnob->setValue(value); break;
    case kParameterRejection: fRejectionKnob->setValue(value); break;
    default: break;
    }
}

void OutflankUI::onNanoDisplay()
{
    const auto& theme = hui::defaultTheme();

    // 1. Background ---------------------------------------------------
    beginPath();
    rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(theme.windowBackground));
    fill();
    closePath();

    // 2. Title ----------------------------------------------------------
    fontFace(NANOVG_DEJAVU_SANS_TTF);
    fontSize(kTitleFontSize);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    fillColor(toDglColor(theme.text));
    text(static_cast<float>(getWidth()) * 0.5f, kTitleY, "OUTFLANK", nullptr);

    // 3. Per-knob labels --------------------------------------------------
    fontSize(kLabelFontSize);
    for (int i = 0; i < 3; ++i)
    {
        const float cx = static_cast<float>(kKnobSpecs[i]->x) + static_cast<float>(kKnobSize) * 0.5f;
        text(cx, static_cast<float>(kLabelY) + static_cast<float>(kLabelH) * 0.5f, kKnobSpecs[i]->label, nullptr);
    }
}

UI* createUI()
{
    return new OutflankUI();
}

END_NAMESPACE_DISTRHO
```

- [ ] **Step 5: Configure and build to verify Common links successfully**

```bash
rm -rf build   # force a clean re-fetch: Common wasn't in the dependency graph before this task
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```

Expected: `AudioPluginsCommon` fetches (v0.2.2) and `Outflank-ui` links against `AudioPluginsCommon::hui_dgl` without a "relocation ... can not be used when making a shared object" error (confirms the `CMAKE_POSITION_INDEPENDENT_CODE ON` placement is correct) and without an "unknown target AudioPluginsCommon::hui_dgl" error (confirms the FetchContent-after-`dpf_add_plugin` ordering is correct).

- [ ] **Step 6: Run the full test suite (unaffected by this task, should still pass)**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all 5 tests still pass — this task doesn't touch DSP or parameter logic, only the UI.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt .github/workflows/ci.yml Source/OutflankUI.h Source/OutflankUI.cpp
git commit -m "$(cat <<'EOF'
Port Outflank's UI to 3 Common RotaryKnobs, Theme's default palette

Replaces the JUCE editor's 3 juce::Slider rotary knobs with
AudioPluginsCommon::hui::dgl::RotaryKnob, using Theme's default palette
tokens directly (no bespoke Outflank colors -- it never had a custom
LookAndFeel). Pulls AudioPluginsCommon v0.2.2 into CMakeLists.txt after
dpf_add_plugin() per the load-bearing ordering requirement, and adds CI's
Common-repo-access step.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Presets — `FactoryPresets.h`, `PresetBrowser`, `PresetSelector`/`Button`

**Files:**
- Create: `Source/FactoryPresets.h`
- Create: `Tests/test_outflank_factorypresets.cpp`
- Modify: `Source/DistrhoPluginInfo.h` (add `DISTRHO_UI_FILE_BROWSER 1`)
- Modify: `CMakeLists.txt` (`USE_FILE_BROWSER TRUE`, link `AudioPluginsCommon::presets`, add test target)
- Modify: `Source/OutflankUI.h`, `Source/OutflankUI.cpp` (presets bar)

**Interfaces:**
- Consumes: `audioplugins::common::presets::Preset` / `ParameterValue` (`Preset.h`), `audioplugins::common::presets::PresetBrowser` (`PresetBrowser.h`), `audioplugins::common::hui::dgl::PresetSelector` / `Button` (`PresetSelector.h`, `Button.h`).
- Produces: `std::vector<audioplugins::common::presets::Preset> outflankFactoryPresets()` in `Source/FactoryPresets.h` — the 3 hand-converted factory presets.

The old `Source/PresetManager.{h,cpp}` was already moved to `Source/_juce_reference/` in Task 1 and excluded from the CMake build — it plays no further role from this point on. This task completes its retirement by wiring `PresetBrowser` as its full functional replacement; there's no additional file deletion needed (the `_juce_reference/` copy stays, deliberately, as historical reference, same as Hex's `_juce_reference/PresetManager.cpp`).

- [ ] **Step 1: Write the failing test**

Create `Tests/test_outflank_factorypresets.cpp`:

```cpp
#include "test_runner.h"
#include "../Source/FactoryPresets.h"

#include <set>

int main()
{
    auto presets = outflankFactoryPresets();
    CHECK(presets.size() == 3);

    const std::set<std::string> expectedNames = {
        "Tight Mono Bass", "Wide Airy", "Subtle"
    };
    std::set<std::string> actualNames;
    for (const auto& p : presets)
        actualNames.insert(p.name);
    CHECK(actualNames == expectedNames);

    const std::set<std::string> expectedIds = { "frequency", "q", "rejection" };
    for (const auto& p : presets)
    {
        CHECK_MSG(p.parameters.size() == 3, "every factory preset must have exactly 3 parameters");
        std::set<std::string> ids;
        for (const auto& param : p.parameters)
            ids.insert(param.id);
        CHECK(ids == expectedIds);
    }

    // Spot-check values against the original XML files
    // (Source/Presets/Factory/*.xml).
    for (const auto& p : presets)
    {
        if (p.name == "Tight Mono Bass")
        {
            for (const auto& param : p.parameters)
            {
                if (param.id == "frequency") CHECK(param.value == 100.0f);
                if (param.id == "q")         CHECK(param.value == 1.0f);
                if (param.id == "rejection") CHECK(param.value == 10.0f);
            }
        }
        if (p.name == "Wide Airy")
        {
            for (const auto& param : p.parameters)
                if (param.id == "rejection") CHECK(param.value == 80.0f);
        }
        if (p.name == "Subtle")
        {
            for (const auto& param : p.parameters)
                if (param.id == "frequency") CHECK(param.value == 180.0f);
        }
    }

    TEST_SUMMARY();
    return 0;
}
```

Add the CTest target to `CMakeLists.txt` (after `test_outflank_parameters`):

```cmake
add_executable(test_outflank_factorypresets
    Tests/test_outflank_factorypresets.cpp
)
target_include_directories(test_outflank_factorypresets PRIVATE Source/ Tests/)
target_link_libraries(test_outflank_factorypresets PRIVATE AudioPluginsCommon::presets)
target_compile_features(test_outflank_factorypresets PRIVATE cxx_std_20)
add_test(NAME OutflankFactoryPresets COMMAND test_outflank_factorypresets)
```

- [ ] **Step 2: Run it and verify it fails**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_outflank_factorypresets
```

Expected: compile failure — `Source/FactoryPresets.h` does not exist yet.

- [ ] **Step 3: Create `Source/FactoryPresets.h`**

```cpp
#pragma once

#include "audioplugins/common/presets/Preset.h"

#include <vector>

// Outflank's 3 factory presets, hand-converted from the JUCE-era
// Source/Presets/Factory/*.xml files (old
// <OutflankState><PARAM id="..." value=".../></OutflankState> schema) into
// Common's Preset schema. Compiled in rather than read from a bundled file
// at runtime -- same rationale as Hex's Source/FactoryPresets.h: locating a
// file relative to the plugin binary is brittle across VST3 (bundle)/CLAP
// (flat .so)/LV2 (bundle dir) layouts x 3 target OSes. The original XML
// files (Source/Presets/Factory/*.xml) are preserved unchanged as
// historical reference, not read at runtime.
inline std::vector<audioplugins::common::presets::Preset> outflankFactoryPresets()
{
    using audioplugins::common::presets::Preset;

    std::vector<Preset> presets;

    {
        Preset p;
        p.name = "Tight Mono Bass";
        p.pluginId = "com.spellbound.outflank";
        p.schemaVersion = 1;
        p.parameters = {
            {"frequency", 100.0f}, {"q", 1.0f}, {"rejection", 10.0f},
        };
        presets.push_back(std::move(p));
    }
    {
        Preset p;
        p.name = "Wide Airy";
        p.pluginId = "com.spellbound.outflank";
        p.schemaVersion = 1;
        p.parameters = {
            {"frequency", 350.0f}, {"q", 0.707f}, {"rejection", 80.0f},
        };
        presets.push_back(std::move(p));
    }
    {
        Preset p;
        p.name = "Subtle";
        p.pluginId = "com.spellbound.outflank";
        p.schemaVersion = 1;
        p.parameters = {
            {"frequency", 180.0f}, {"q", 0.707f}, {"rejection", 20.0f},
        };
        presets.push_back(std::move(p));
    }

    return presets;
}
```

- [ ] **Step 4: Run the test and verify it passes**

```bash
cmake --build build --target test_outflank_factorypresets
./build/test_outflank_factorypresets
```

Expected: all checks pass (3 presets, correct names, correct parameter ids, correct spot-checked values).

- [ ] **Step 5: Enable the file browser — `Source/DistrhoPluginInfo.h`**

Add next to the other `DISTRHO_UI_*` macros:

```cpp
#define DISTRHO_UI_FILE_BROWSER   1
```

- [ ] **Step 6: Enable the file browser and link presets — `CMakeLists.txt`**

Modify the existing `dpf_add_plugin(Outflank ...)` call to add `USE_FILE_BROWSER TRUE`:

```cmake
dpf_add_plugin(Outflank
    TARGETS ${OUTFLANK_DPF_TARGETS}
    UI_TYPE opengl
    USE_FILE_BROWSER TRUE
    FILES_DSP
        Source/OutflankPluginAdapter.cpp
        Source/DSP/CrossoverMS.cpp
    FILES_UI
        Source/OutflankUI.cpp
)
```

Extend the Task 3 link line:

```cmake
target_link_libraries(Outflank-ui PRIVATE AudioPluginsCommon::hui_dgl AudioPluginsCommon::presets)
```

- [ ] **Step 7: Wire the presets bar — `Source/OutflankUI.h`**

```cpp
#pragma once

#include "DistrhoUI.hpp"

#include "OutflankPluginAdapter.h"

#include "audioplugins/common/hui/dgl/RotaryKnob.h"
#include "audioplugins/common/hui/dgl/Button.h"
#include "audioplugins/common/hui/dgl/PresetSelector.h"
#include "audioplugins/common/presets/PresetBrowser.h"

#include <memory>
#include <vector>

START_NAMESPACE_DISTRHO

/**
   DPF UI adapter for Spellbound Outflank.

   3 RotaryKnobs (frequency/q/rejection) using
   AudioPluginsCommon::hui::Theme's default palette tokens, plus a presets
   bar (PresetSelector dropdown + SAVE/DELETE buttons) above the title --
   backed by AudioPluginsCommon::presets::PresetBrowser and Outflank's 3
   compiled-in factory presets (Source/FactoryPresets.h). Presets are a
   purely in-plugin-UI feature -- no DPF Program/State host integration --
   loading a preset calls setParameterValue() per parameter, same path
   every knob already uses. Panel/label layout mirrors the JUCE-era
   editor's 320x200 3-knob layout (see Source/_juce_reference/PluginEditor.cpp),
   which already reserved a 24px preset-bar row at the top.
 */
class OutflankUI : public UI
{
public:
    OutflankUI();

protected:
    // -- DSP/Plugin Callbacks ---------------------------------------------
    void parameterChanged(uint32_t index, float value) override;

    // -- Widget Callbacks ---------------------------------------------------
    void onNanoDisplay() override;

    // -- File Browser Callback ------------------------------------------------
    // Called by DPF after the user picks a file (or cancels) in the native
    // save dialog SAVE's onClick opens.
    void uiFileBrowserSelected(const char* filename) override;

private:
    void applyPreset(const audioplugins::common::presets::Preset& preset);
    std::vector<audioplugins::common::presets::ParameterValue> captureCurrentParameters() const;
    // Pushes fPresetBrowser's current entries/index into fPresetSelector
    // and updates fDeleteButton's enabled state. Called after construction
    // and after any load/save/delete.
    void refreshPresetControls();

    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fFrequencyKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fQKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fRejectionKnob;

    // PresetBrowser is not a DGL widget (no UI base-class dependency), so
    // it's a plain member, not heap-owned like the widgets above.
    audioplugins::common::presets::PresetBrowser fPresetBrowser;
    std::unique_ptr<audioplugins::common::hui::dgl::PresetSelector> fPresetSelector;
    std::unique_ptr<audioplugins::common::hui::dgl::Button> fSaveButton;
    std::unique_ptr<audioplugins::common::hui::dgl::Button> fDeleteButton;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutflankUI)
};

END_NAMESPACE_DISTRHO
```

- [ ] **Step 8: Wire the presets bar — `Source/OutflankUI.cpp`**

```cpp
#include "OutflankUI.h"
#include "FactoryPresets.h"

#include <cstdlib>
#include <string>

START_NAMESPACE_DISTRHO

namespace {

namespace hui = audioplugins::common::hui;

hui::Colour withAlpha(hui::Colour c, uint8_t a) noexcept
{
    c.a = a;
    return c;
}

inline DGL_NAMESPACE::Color toDglColor(const hui::Colour& c, float alphaScale = 1.f) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r),
                                static_cast<int>(c.g),
                                static_cast<int>(c.b),
                                (static_cast<float>(c.a) / 255.f) * alphaScale);
}

// All palettes derived directly from Theme's default tokens -- no bespoke
// Outflank colors invented, per this migration's design decision.
const hui::dgl::RotaryKnobPalette kOutflankKnobPalette = [] {
    const auto& theme = hui::defaultTheme();
    hui::dgl::RotaryKnobPalette p;
    p.track = theme.widgetBackground;
    p.valueArc = theme.accent;
    p.valueArcGlow = theme.accent;
    p.knobTop = theme.windowBackground;
    p.knobBottom = theme.widgetBackground;
    p.knobRim = theme.accent;
    return p;
}();

const hui::dgl::ButtonPalette kOutflankButtonPalette = [] {
    const auto& theme = hui::defaultTheme();
    hui::dgl::ButtonPalette p;
    p.background = theme.widgetBackground;
    p.backgroundDisabled = theme.windowBackground;
    p.border = theme.accent;
    p.text = theme.text;
    p.textDisabled = withAlpha(theme.text, 0x80);
    return p;
}();

const hui::dgl::PresetSelectorPalette kOutflankPresetSelectorPalette = [] {
    const auto& theme = hui::defaultTheme();
    hui::dgl::PresetSelectorPalette p;
    p.closedBackground = theme.widgetBackground;
    p.listBackground = theme.windowBackground;
    p.border = theme.accent;
    p.text = theme.text;
    p.textFactory = withAlpha(theme.text, 0x80);
    p.rowHighlight = withAlpha(theme.accent, 0x40);
    return p;
}();

// Layout ported from the JUCE-era resized()/paint() (see
// Source/_juce_reference/PluginEditor.cpp): kW=320, kH=200. Preset bar
// (selector | SAVE | DELETE) occupies an 8px-margin top row, matching the
// JUCE original's `getLocalBounds().reduced(8)` + `removeFromTop(24)`
// exactly (148 + 8 + 70 + 8 + 70 = 304 = 320 - 2*8).
constexpr float kMarginX = 8.0f;
constexpr float kPresetBarY = 8.0f;
constexpr uint kPresetBarRowH = 24;
constexpr uint kPresetSelectorW = 148;
constexpr uint kPresetButtonW = 70;
constexpr float kPresetButtonGap = 8.0f;
constexpr float kSaveButtonX = kMarginX + static_cast<float>(kPresetSelectorW) + kPresetButtonGap;
constexpr float kDeleteButtonX = kSaveButtonX + static_cast<float>(kPresetButtonW) + kPresetButtonGap;

constexpr float kTitleY = 46.0f;
constexpr float kTitleFontSize = 16.0f;

constexpr int kPresetBarH = 24;
constexpr int kKnobY = kPresetBarH + 48; // 72, matches JUCE original
constexpr uint kKnobSize = 80;
constexpr int kKnobGap = 24;
constexpr int kLabelY = kKnobY + static_cast<int>(kKnobSize) + 4;
constexpr int kLabelH = 16;
constexpr float kLabelFontSize = 11.0f;

constexpr int kTotalKnobsW = static_cast<int>(kKnobSize) * 3 + kKnobGap * 2;
constexpr int kRowX0 = (DISTRHO_UI_DEFAULT_WIDTH - kTotalKnobsW) / 2;
constexpr int kFrequencyX = kRowX0;
constexpr int kQX         = kFrequencyX + static_cast<int>(kKnobSize) + kKnobGap;
constexpr int kRejectionX = kQX + static_cast<int>(kKnobSize) + kKnobGap;

struct KnobSpec
{
    uint32_t parameterIndex;
    int x;
    float rangeMin, rangeMax, defaultValue;
    const char* label;
};

constexpr KnobSpec kFrequencySpec { kParameterFrequency, kFrequencyX, OUTFLANK_PARAM_FREQUENCY_MIN, OUTFLANK_PARAM_FREQUENCY_MAX, OUTFLANK_PARAM_FREQUENCY_DEFAULT, "Frequency" };
constexpr KnobSpec kQSpec         { kParameterQ,         kQX,         OUTFLANK_PARAM_Q_MIN,         OUTFLANK_PARAM_Q_MAX,         OUTFLANK_PARAM_Q_DEFAULT,         "Q" };
constexpr KnobSpec kRejectionSpec { kParameterRejection, kRejectionX, OUTFLANK_PARAM_REJECTION_MIN, OUTFLANK_PARAM_REJECTION_MAX, OUTFLANK_PARAM_REJECTION_DEFAULT, "Rejection" };

constexpr const KnobSpec* kKnobSpecs[3] = { &kFrequencySpec, &kQSpec, &kRejectionSpec };

std::unique_ptr<hui::dgl::RotaryKnob> makeKnob(OutflankUI& ui, const KnobSpec& spec)
{
    std::unique_ptr<hui::dgl::RotaryKnob> knob(new hui::dgl::RotaryKnob(&ui));
    knob->setSize(kKnobSize, kKnobSize);
    knob->setAbsolutePos(spec.x, kKnobY);
    knob->setPalette(kOutflankKnobPalette);
    knob->setRange(spec.rangeMin, spec.rangeMax);
    knob->setDefaultValue(spec.defaultValue);
    knob->setValue(spec.defaultValue);

    const uint32_t paramIndex = spec.parameterIndex;
    hui::dgl::RotaryKnob* const rawKnob = knob.get();
    rawKnob->onDragStateChanged = [&ui, paramIndex](const bool started)
    {
        ui.editParameter(paramIndex, started);
    };
    rawKnob->onValueChanged = [&ui, paramIndex](const float value)
    {
        ui.setParameterValue(paramIndex, value);
    };

    return knob;
}

std::unique_ptr<hui::dgl::PresetSelector> makePresetSelector(OutflankUI& ui)
{
    std::unique_ptr<hui::dgl::PresetSelector> selector(new hui::dgl::PresetSelector(&ui));
    selector->setPalette(kOutflankPresetSelectorPalette);
    selector->setClosedSize(kPresetSelectorW, kPresetBarRowH);
    selector->setAbsolutePos(static_cast<int>(kMarginX), static_cast<int>(kPresetBarY));
    return selector;
}

std::unique_ptr<hui::dgl::Button> makeButton(OutflankUI& ui, const char* label, float x)
{
    std::unique_ptr<hui::dgl::Button> button(new hui::dgl::Button(&ui));
    button->setPalette(kOutflankButtonPalette);
    button->setLabel(label);
    button->setSize(kPresetButtonW, kPresetBarRowH);
    button->setAbsolutePos(static_cast<int>(x), static_cast<int>(kPresetBarY));
    return button;
}

// Linux-only for now, matching Hex's hexUserPresetsDirectory() and the
// JUCE-era PresetManager::getUserPresetsDirectory()'s Linux branch
// (~/.config/<Name>/presets).
std::string outflankUserPresetsDirectory()
{
    const char* home = std::getenv("HOME");
    if (home == nullptr)
        return "/tmp/Outflank/presets"; // extremely unlikely fallback, still functional
    return std::string(home) + "/.config/Outflank/presets";
}

} // namespace

OutflankUI::OutflankUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      fFrequencyKnob(makeKnob(*this, kFrequencySpec)),
      fQKnob(makeKnob(*this, kQSpec)),
      fRejectionKnob(makeKnob(*this, kRejectionSpec)),
      fPresetBrowser(outflankFactoryPresets(), outflankUserPresetsDirectory(), "com.spellbound.outflank"),
      fPresetSelector(makePresetSelector(*this)),
      fSaveButton(makeButton(*this, "SAVE", kSaveButtonX)),
      fDeleteButton(makeButton(*this, "DELETE", kDeleteButtonX))
{
    loadSharedResources();

    fPresetSelector->onIndexSelected = [this](const int index)
    {
        if (const auto* preset = fPresetBrowser.selectIndex(index))
        {
            applyPreset(*preset);
            refreshPresetControls();
        }
    };

    fDeleteButton->onClick = [this]()
    {
        if (fPresetBrowser.deleteCurrent())
            refreshPresetControls();
    };

    fSaveButton->onClick = [this]()
    {
        const std::string startDir = outflankUserPresetsDirectory();
        FileBrowserOptions options;
        options.saving = true;
        options.defaultName = "New Preset.xml";
        options.title = "Save Outflank Preset";
        options.startDir = startDir.c_str();
        openFileBrowser(options);
    };

    refreshPresetControls();
}

void OutflankUI::uiFileBrowserSelected(const char* filename)
{
    if (filename == nullptr)
        return; // user cancelled the dialog

    std::string path(filename);
    const size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos)
        base = base.substr(0, dot);

    if (fPresetBrowser.saveAs(base, captureCurrentParameters()))
        refreshPresetControls();
}

void OutflankUI::parameterChanged(const uint32_t index, const float value)
{
    switch (index)
    {
    case kParameterFrequency: fFrequencyKnob->setValue(value); break;
    case kParameterQ:         fQKnob->setValue(value); break;
    case kParameterRejection: fRejectionKnob->setValue(value); break;
    default: break;
    }
}

void OutflankUI::applyPreset(const audioplugins::common::presets::Preset& preset)
{
    for (const auto& pv : preset.parameters)
    {
        uint32_t paramIndex;

        if (pv.id == "frequency")      { fFrequencyKnob->setValue(pv.value); paramIndex = kParameterFrequency; }
        else if (pv.id == "q")         { fQKnob->setValue(pv.value);         paramIndex = kParameterQ; }
        else if (pv.id == "rejection") { fRejectionKnob->setValue(pv.value); paramIndex = kParameterRejection; }
        else continue; // unknown id (forward-compatible with a future schema addition) -- ignore

        editParameter(paramIndex, true);
        setParameterValue(paramIndex, pv.value);
        editParameter(paramIndex, false);
    }
}

std::vector<audioplugins::common::presets::ParameterValue> OutflankUI::captureCurrentParameters() const
{
    return {
        {"frequency", fFrequencyKnob->getValue()},
        {"q", fQKnob->getValue()},
        {"rejection", fRejectionKnob->getValue()},
    };
}

void OutflankUI::refreshPresetControls()
{
    fPresetSelector->setEntries(fPresetBrowser.getEntries());
    fPresetSelector->setCurrentIndex(fPresetBrowser.getCurrentIndex());

    const auto entries = fPresetBrowser.getEntries();
    const int idx = fPresetBrowser.getCurrentIndex();
    const bool isFactory = (idx >= 0 && static_cast<size_t>(idx) < entries.size()) ? entries[static_cast<size_t>(idx)].isFactory : true;
    fDeleteButton->setEnabled(!isFactory);
}

void OutflankUI::onNanoDisplay()
{
    const auto& theme = hui::defaultTheme();

    beginPath();
    rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(theme.windowBackground));
    fill();
    closePath();

    fontFace(NANOVG_DEJAVU_SANS_TTF);
    fontSize(kTitleFontSize);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    fillColor(toDglColor(theme.text));
    text(static_cast<float>(getWidth()) * 0.5f, kTitleY, "OUTFLANK", nullptr);

    fontSize(kLabelFontSize);
    for (int i = 0; i < 3; ++i)
    {
        const float cx = static_cast<float>(kKnobSpecs[i]->x) + static_cast<float>(kKnobSize) * 0.5f;
        text(cx, static_cast<float>(kLabelY) + static_cast<float>(kLabelH) * 0.5f, kKnobSpecs[i]->label, nullptr);
    }
}

UI* createUI()
{
    return new OutflankUI();
}

END_NAMESPACE_DISTRHO
```

- [ ] **Step 9: Build everything and run the full test suite**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: all 6 tests pass (`StateVariableFilter`, `CrossoverMS`, `AllpassFilter`, `QuadraturePair`, `OutflankParameters`, `OutflankFactoryPresets`).

- [ ] **Step 10: Commit**

```bash
git add Source/DistrhoPluginInfo.h Source/FactoryPresets.h Source/OutflankUI.h Source/OutflankUI.cpp Tests/test_outflank_factorypresets.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
Add Outflank's presets bar (PresetBrowser + PresetSelector/Button)

Hand-converts the 3 existing factory XMLs (Tight Mono Bass, Wide Airy,
Subtle) into a compiled-in Source/FactoryPresets.h using Common's Preset
schema, same approach as Hex. Wires AudioPluginsCommon::presets::PresetBrowser
+ the PresetSelector/Button DGL widgets; SAVE opens a native file-browser
dialog, DELETE is disabled on factory presets. The old JUCE-era
PresetManager (already relocated to Source/_juce_reference/ in Task 1) is
now fully superseded.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Validator CI legs (`pluginval` / `clap-validator` / `lv2lint`) + fix findings

**Files:**
- Modify: `.github/workflows/ci.yml` (add Linux-only validator steps)
- Possibly modify: any `Source/DSP/*.h`, `Source/DistrhoPluginInfo.h`, `Source/OutflankPluginAdapter.{h,cpp}` file, depending on what findings turn up (not predictable ahead of time — see Step 3)

**Interfaces:**
- No new interfaces produced — this task verifies and, if needed, repairs the adapter/DSP boundary built in Tasks 1-4 against real validator tools.

- [ ] **Step 1: Add the validator steps to `.github/workflows/ci.yml`**

Append after the existing "Test" step (all Linux-only, matching Hex's `ci.yml` exactly, adapted to Outflank's paths):

```yaml
      # --- Plugin validators (Linux-only for now) ---
      #
      # Windows/macOS validator support is explicitly deferred: those legs
      # don't yet have a verified build, so there's nothing meaningful to
      # validate there yet.
      #
      # Both validators need a real GL context to load-and-close Outflank's
      # UI (NanoVG/DGL), even headlessly, hence xvfb-run below.

      - name: Download pluginval
        if: runner.os == 'Linux'
        run: |
          # v1.0.4 was the latest pluginval release as of this task, per
          # https://github.com/Tracktion/pluginval/releases. Its Linux asset
          # (pluginval_Linux.zip) is a zip containing one bare executable
          # named `pluginval` (no wrapper directory, not an AppImage).
          curl -sL -o pluginval_Linux.zip \
            https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Linux.zip
          unzip -o pluginval_Linux.zip
          chmod +x pluginval

      - name: Validate VST3 with pluginval
        if: runner.os == 'Linux'
        run: |
          # Strictness 5 is pluginval's own documented default and
          # recommended minimum for host-compatibility testing.
          xvfb-run -a ./pluginval --strictness-level 5 --validate build/bin/Outflank.vst3

      - name: Download clap-validator
        if: runner.os == 'Linux'
        run: |
          # 0.4.1 was the latest clap-validator release as of this task, per
          # https://github.com/free-audio/clap-validator/releases. It ships
          # a prebuilt Linux binary (built on ubuntu-22.04, forward-compatible
          # with newer glibc on ubuntu-latest) -- no Rust/cargo toolchain
          # needed in CI. Note: the release's zip asset is named for tag
          # 0.4.1, but the tar.gz inside it is misnamed with an internal dev
          # version (0.3.2-127-g152b982) -- `clap-validator --version` itself
          # still reports 0.4.1.
          curl -sL -o clap-validator.zip \
            https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-ubuntu-22.04.zip
          unzip -o clap-validator.zip
          tar -xzf clap-validator-*-ubuntu-22.04.tar.gz
          chmod +x clap-validator

      - name: Validate CLAP with clap-validator
        if: runner.os == 'Linux'
        run: |
          xvfb-run -a ./clap-validator validate build/bin/Outflank.clap

      - name: Install lv2lint build dependencies
        if: runner.os == 'Linux'
        run: |
          # libelf-dev is required by -Delf-tests=enabled below (the ELF
          # symbol-visibility test the -s lv2_generate_ttl whitelist
          # targets) -- a real CI failure Hex hit and fixed; included here
          # from the start rather than rediscovering it.
          sudo apt-get install -y liblilv-dev lv2-dev libelf-dev meson ninja-build

      - name: Build lv2lint
        if: runner.os == 'Linux'
        run: |
          # lv2lint has no apt package on Ubuntu -- built from source per its
          # own README (mirrored at github.com/sfztools/lv2lint). x11-tests
          # are disabled (src/lv2lint_x11.c fails to compile on gcc 14/glibc,
          # an upstream bug). online-tests are disabled to avoid depending on
          # outbound network access in CI.
          git clone --depth 1 https://github.com/sfztools/lv2lint /tmp/lv2lint
          meson setup -Donline-tests=disabled -Delf-tests=enabled -Dx11-tests=disabled /tmp/lv2lint/build /tmp/lv2lint
          ninja -C /tmp/lv2lint/build

      - name: Validate LV2 with lv2lint
        if: runner.os == 'Linux'
        # lv2lint's Plugin Class FAIL (rdf:type includes doap:Project
        # alongside lv2:Plugin) is baked unconditionally into DPF's own LV2
        # TTL generator (distrho/src/DistrhoPluginLV2export.cpp) -- every
        # DPF-based LV2 plugin emits this; fixing it means patching DPF, not
        # Outflank. Non-blocking until/unless that's done upstream.
        continue-on-error: true
        run: |
          export LV2_PATH="/usr/lib/lv2:${GITHUB_WORKSPACE}/build/bin"
          export LD_PRELOAD="/tmp/lv2lint/build/lv2lint.so"
          /tmp/lv2lint/build/lv2lint.bin -s lv2_generate_ttl "https://spellbound.audio/plugins/outflank"
```

- [ ] **Step 2: Commit the CI changes and push**

```bash
git add .github/workflows/ci.yml
git commit -m "$(cat <<'EOF'
Add pluginval/clap-validator/lv2lint Linux CI legs

Same three validators Hex/Pugilist run, including the libelf-dev lv2lint
dependency fix from the start (a real CI failure Hex hit and fixed
separately -- copied in here rather than rediscovered).

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
git push -u origin worktree-dpf-stage0
```

- [ ] **Step 3: Run clap-validator locally against the real build and read its output**

```bash
curl -sL -o clap-validator.zip \
  https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-ubuntu-22.04.zip
unzip -o clap-validator.zip
tar -xzf clap-validator-*-ubuntu-22.04.tar.gz
chmod +x clap-validator
xvfb-run -a ./clap-validator validate build/bin/Outflank.clap
```

Read the full output. For each `FAIL` (if any):

1. Root-cause it by reading the relevant `Source/DSP/*.h`, `Source/OutflankPluginAdapter.cpp`, or `Source/DistrhoPluginInfo.h` code the failing test path exercises — do not guess; trace the actual failing assertion/sample index the validator reports.
2. File a GitHub issue for it before fixing (this workspace's "ticket per task" convention):
   ```bash
   gh issue create --repo TriYop/spellbound-outflank \
     --title "<clap-validator test name>: <one-line summary of the root cause>" \
     --body "Found via: xvfb-run -a ./clap-validator validate build/bin/Outflank.clap

   <paste the validator's failure output>

   Root cause: <what you found by reading the code>"
   ```
3. Fix the root cause, rebuild, re-run `ctest` (all existing + new tests must still pass), and re-run `xvfb-run -a ./clap-validator validate build/bin/Outflank.clap` to confirm the specific test now passes.
4. Commit the fix referencing the issue number, e.g. `git commit -m "Fix <test-name> (see #<N>)\n\n<root cause + fix summary>\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"`.
5. Close the issue: `gh issue close <N> --repo TriYop/spellbound-outflank --comment "Fixed in <commit SHA>."`.

Repeat for every distinct failing test clap-validator reports. Do not pre-guess what (if anything) will fail — Outflank's DSP is a straightforward SVF + all-pass chain with no known edge cases going in, unlike Hex's Nyquist-adjacent chemistry filter; it may pass cleanly, or it may not. Follow the process above either way.

- [ ] **Step 4: Run pluginval locally and repeat the same process for any findings**

```bash
curl -sL -o pluginval_Linux.zip \
  https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Linux.zip
unzip -o pluginval_Linux.zip
chmod +x pluginval
xvfb-run -a ./pluginval --strictness-level 5 --validate build/bin/Outflank.vst3
```

Apply the same root-cause → issue → fix → verify → close cycle from Step 3 to any `pluginval` failures.

- [ ] **Step 5: Run lv2lint locally and repeat the same process for any findings (excluding the known DPF-wide Plugin Class finding)**

```bash
git clone --depth 1 https://github.com/sfztools/lv2lint /tmp/lv2lint
meson setup -Donline-tests=disabled -Delf-tests=enabled -Dx11-tests=disabled /tmp/lv2lint/build /tmp/lv2lint
ninja -C /tmp/lv2lint/build
export LV2_PATH="/usr/lib/lv2:$(pwd)/build/bin"
export LD_PRELOAD="/tmp/lv2lint/build/lv2lint.so"
/tmp/lv2lint/build/lv2lint.bin -s lv2_generate_ttl "https://spellbound.audio/plugins/outflank"
```

The one finding known ahead of time not to file a ticket for: a Plugin Class FAIL for `rdf:type` including `doap:Project` alongside `lv2:Plugin` — this is baked into DPF's own LV2 TTL generator (`distrho/src/DistrhoPluginLV2export.cpp`), affects every DPF-based LV2 plugin in this workspace identically, and fixing it means patching DPF, not Outflank (matches Hex's identical, already-accepted finding). File and fix tickets for any *other* finding lv2lint reports.

- [ ] **Step 6: Push all fixes and verify CI is actually green**

```bash
git push
gh run list --repo TriYop/spellbound-outflank --branch worktree-dpf-stage0 --limit 5
```

Do not report this task done from a "should pass now" assumption — confirm the actual run status from `gh run list` (or `gh run view <run-id> --log-failed` if it's still red) before moving to Task 6.

---

## Task 6: Rewrite `CLAUDE.md` for the real, DPF-based, implemented state

**Files:**
- Modify: `CLAUDE.md` (full rewrite — the current version claims "no source code... has been added yet", which is false even before this migration)

**Interfaces:** None — documentation only.

- [ ] **Step 1: Find Task 5's actual validator findings before writing the CI/validator section**

```bash
git log --oneline main..worktree-dpf-stage0
gh issue list --repo TriYop/spellbound-outflank --state closed
```

Use the real output to write the "CI / validator status" section below: for
each closed issue from Task 5, name the specific validator test, its root
cause, and the commit SHA that fixed it. If Task 5's log shows no fix
commits beyond CI-config changes (i.e. nothing failed beyond the known
DPF-wide lv2lint finding), write that plainly instead — the template below
shows the "nothing else failed" case; replace it with real findings if
`gh issue list` shows any.

- [ ] **Step 2: Rewrite `CLAUDE.md`**

```markdown
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
`QuadraturePair.h`, `AllpassFilter.h`) is unchanged by the migration — it
was already framework-free before DPF arrived, with its own JUCE-free CTest
suite (`Tests/test_crossoverms.cpp`, `test_statevariablefilter.cpp`,
`test_quadraturepair.cpp`, `test_allpassfilter.cpp`). `Source/OutflankPluginAdapter.{h,cpp}`
declares the 3 host parameters via DPF's `Parameter` API and drives that
chain unchanged; `Source/OutflankUI.{h,cpp}` has 3
`AudioPluginsCommon::hui::dgl::RotaryKnob`s (frequency/q/rejection) using
`Theme`'s default palette (Outflank never had a custom look, so none was
invented for this migration) plus a presets bar (`PresetSelector` dropdown +
SAVE/DELETE `Button`s, backed by `AudioPluginsCommon::presets::PresetBrowser`
and the 3 factory presets hand-converted into `Source/FactoryPresets.h`).
The JUCE-era `PluginProcessor`/`PluginEditor`/`PresetManager` are preserved
unchanged at `Source/_juce_reference/` as the porting reference this was
ported from.

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
- **DSP untouched by the migration** — `Source/DSP/*.h` and `CrossoverMS.cpp`
  have zero DPF/JUCE dependency either before or after this migration; only
  the adapter shim around them changed.
- **DPF pinned to a commit SHA** (`4238e1c7f0351bbe488d79f0899c540543ac7583`,
  no tagged DPF releases exist) with 2 upstream patches
  (`cmake/patches/dpf-clap-state-chunked-read.patch`,
  `dpf-clap-activate-latency.patch`) applied via `cmake/apply_patch.cmake` —
  same pin and patches as Hex/Pugilist, for consistency across the workspace's
  DPF-migrated plugins.

## CI / validator status

clap-validator, pluginval, and lv2lint (aside from the one known DPF-wide
finding below) all pass against this build — no fixes beyond CI
configuration were needed during the migration's validator pass (Task 5 of
`docs/superpowers/plans/2026-09-05-outflank-dpf-migration.md`; confirmed via
`gh issue list --repo TriYop/spellbound-outflank --state closed` showing no
DSP/adapter-fix issues from that task, only the CI-leg-addition commits
themselves).

The known, accepted, non-blocking finding: lv2lint reports a Plugin Class
FAIL (`rdf:type` includes `doap:Project` alongside `lv2:Plugin`) baked
unconditionally into DPF's own LV2 TTL generator
(`distrho/src/DistrhoPluginLV2export.cpp`) — every DPF-based LV2 plugin in
this workspace emits this identically; fixing it means patching DPF, not
Outflank. The `lv2lint` CI step runs with `continue-on-error: true` for
this reason.
```

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "$(cat <<'EOF'
Rewrite CLAUDE.md for the real, DPF-based, implemented state

Replaces the stale "no source code has been added yet" placeholder with
the actual architecture (CrossoverMS chain unchanged, DPF adapter/UI,
Theme-default palette, presets bar), build commands, parameter table, and
CI/validator status.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 4: Open a pull request from `worktree-dpf-stage0` into `main`**

```bash
git push -u origin worktree-dpf-stage0
gh pr create --repo TriYop/spellbound-outflank \
  --title "Migrate Outflank off JUCE onto DPF" \
  --body "$(cat <<'EOF'
## Summary
- Drops JUCE + clap-juce-extensions in favor of DPF (ISC-licensed), same
  pinned commit + patches Hex/Pugilist use.
- CrossoverMS DSP chain unchanged; OutflankPluginAdapter is a thin DPF
  Parameter-API shim over it.
- UI ported to 3 Common RotaryKnobs using Theme's default palette, plus a
  presets bar (PresetBrowser + PresetSelector/Button).
- Linux CI gates on build/test + pluginval/clap-validator/lv2lint; Windows/
  macOS legs are continue-on-error per the workspace's Phase 6 scope.

## Test plan
- [ ] `ctest --test-dir build --output-on-failure` passes all 6 tests
- [ ] CI is green on the Linux leg (`gh run list`)
- [ ] Manual pass in a real host (Carla) confirms the presets panel works
      end-to-end (save/load/delete)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Then follow this workspace's normal `superpowers:finishing-a-development-branch` flow to merge once reviewed.
