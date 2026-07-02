# Outflank Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Outflank, a 3-knob JUCE stereo crossover plugin (VST3/CLAP/Standalone) that forces bass below a crossover frequency to mono and widens highs above it by rejecting Mid content, with preset save/load.

**Architecture:** M/S-domain crossover with a single resonant lowpass SVF per bus (Mid, Side); the low band is kept from the Mid bus only (Side low band discarded — forces mono bass), the high band is derived as `input - low` and the Mid high band is attenuated by the rejection knob. JUCE `AudioProcessor`/`AudioProcessorValueTreeState` own the 3 parameters and drive a JUCE-free `CrossoverMS` DSP class. `PresetManager` follows the exact pattern already used by Catalyst/Pugilist in this workspace.

**Tech Stack:** C++20, JUCE 8.0.13 (fetched via CMake `FetchContent`), clap-juce-extensions, CMake + Ninja, no third-party test framework (plain `add_executable` + `CHECK`-macro harness, matching Pugilist).

**Reference:** `docs/superpowers/specs/2026-07-02-outflank-design.md`

## Global Constraints

- Exactly 3 automatable parameters: `frequency` (40 Hz – 2 kHz, log skew, default 250 Hz), `q` (0.3 – 4.0, default 0.707), `rejection` (0 – 100%, default 0%). No bypass, output gain, or metering.
- Stereo in / stereo out only (`isBusesLayoutSupported` rejects anything else).
- JUCE 8.0.13, C++20, CMake ≥ 3.22, targets VST3 + CLAP + Standalone (+ AU on Apple).
- `StateVariableFilter` and `CrossoverMS` (the DSP core) must be plain C++ with no JUCE dependency, so they can be unit-tested as bare executables like Pugilist's `Source/Synth/*` classes.
- Presets: factory (bundled, read-only) + user (saved to `~/.config/Outflank/presets/` on Linux), APVTS XML snapshots, following Catalyst's `PresetManager` API shape (`loadPreset`, `savePreset`, `deletePreset`, `renamePreset`, `getPresetList`, `getCurrentPresetIndex`, `refreshUserPresets`).
- Company/bundle identity: `COMPANY_NAME "Spellbound"`, `PLUGIN_MANUFACTURER_CODE Spbd`, `PLUGIN_CODE Otfk`, `BUNDLE_ID "com.spellbound.outflank"`.

---

### Task 1: Project scaffold — buildable passthrough plugin

**Files:**
- Create: `CMakeLists.txt`
- Create: `Source/PluginProcessor.h`
- Create: `Source/PluginProcessor.cpp`
- Create: `Source/PluginEditor.h`
- Create: `Source/PluginEditor.cpp`
- Create: `scripts/install.sh`
- Create: `scripts/uninstall.sh`

**Interfaces:**
- Produces: `OutflankAudioProcessor` (in `Source/PluginProcessor.h`) with public member `juce::AudioProcessorValueTreeState apvts;` (identifier `"OutflankState"`, currently zero parameters) — later tasks add parameters and a `CrossoverMS crossover_` member.
- Produces: `OutflankAudioProcessorEditor` (in `Source/PluginEditor.h`), constructed from `OutflankAudioProcessor&` — later tasks add knobs to it.

- [ ] **Step 1: Create the directory layout**

```bash
mkdir -p Source scripts Tests
```

- [ ] **Step 2: Write `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)
project(Outflank VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Must come AFTER FetchContent_MakeAvailable(JUCE)
set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(clap-juce-extensions)

set(OUTFLANK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND OUTFLANK_FORMATS AU)
endif()

juce_add_plugin(Outflank
    COMPANY_NAME                "Spellbound"
    PLUGIN_MANUFACTURER_CODE    Spbd
    PLUGIN_CODE                 Otfk
    FORMATS                     ${OUTFLANK_FORMATS}
    PRODUCT_NAME                "Outflank"
    BUNDLE_ID                   "com.spellbound.outflank"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    DESCRIPTION                 "Mono-bass / wide-highs stereo crossover"
)

target_sources(Outflank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Outflank PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(Outflank PRIVATE Source/)

target_compile_features(Outflank PUBLIC cxx_std_20)

target_link_libraries(Outflank
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_dsp
        clap_juce_extensions
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Must come AFTER target_link_libraries
clap_juce_extensions_plugin(
    TARGET      Outflank
    CLAP_ID     "com.spellbound.outflank"
    CLAP_FEATURES audio-effect utility stereo
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 64
)

# ── Install rules ─────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_ARTS "${CMAKE_BINARY_DIR}/Outflank_artefacts/${CMAKE_BUILD_TYPE}")

install(DIRECTORY  "${_ARTS}/VST3/Outflank.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

install(FILES      "${_ARTS}/CLAP/Outflank.clap"
        DESTINATION CLAP
        COMPONENT   Runtime
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

install(PROGRAMS   "${_ARTS}/Standalone/Outflank"
        DESTINATION bin
        COMPONENT   Runtime)

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

- [ ] **Step 3: Write `Source/PluginProcessor.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class OutflankAudioProcessorEditor;

class OutflankAudioProcessor final : public juce::AudioProcessor
{
public:
    OutflankAudioProcessor();
    ~OutflankAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi()  const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessor)
};
```

- [ ] **Step 4: Write `Source/PluginProcessor.cpp`**

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout OutflankAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    return { params.begin(), params.end() };
}

// ── Constructor ───────────────────────────────────────────────────────────────
OutflankAudioProcessor::OutflankAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OutflankState", createParameterLayout())
{}

OutflankAudioProcessor::~OutflankAudioProcessor() = default;

// ── Info ──────────────────────────────────────────────────────────────────────
const juce::String OutflankAudioProcessor::getName() const { return JucePlugin_Name; }
bool OutflankAudioProcessor::acceptsMidi()  const { return false; }
bool OutflankAudioProcessor::producesMidi() const { return false; }
bool OutflankAudioProcessor::isMidiEffect() const { return false; }
double OutflankAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  OutflankAudioProcessor::getNumPrograms()              { return 1; }
int  OutflankAudioProcessor::getCurrentProgram()           { return 0; }
void OutflankAudioProcessor::setCurrentProgram (int)       {}
const juce::String OutflankAudioProcessor::getProgramName (int) { return {}; }
void OutflankAudioProcessor::changeProgramName (int, const juce::String&) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void OutflankAudioProcessor::prepareToPlay (double, int) {}
void OutflankAudioProcessor::releaseResources() {}

bool OutflankAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    return true;
}

// ── processBlock ──────────────────────────────────────────────────────────────
void OutflankAudioProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Passthrough for now — DSP wired in a later task.
}

// ── State ─────────────────────────────────────────────────────────────────────
bool OutflankAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* OutflankAudioProcessor::createEditor()
{
    return new OutflankAudioProcessorEditor (*this);
}

void OutflankAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OutflankAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OutflankAudioProcessor();
}
```

- [ ] **Step 5: Write `Source/PluginEditor.h`**

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class OutflankAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit OutflankAudioProcessorEditor (OutflankAudioProcessor&);
    ~OutflankAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OutflankAudioProcessor& proc_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessorEditor)
};
```

- [ ] **Step 6: Write `Source/PluginEditor.cpp`**

```cpp
#include "PluginEditor.h"

OutflankAudioProcessorEditor::OutflankAudioProcessorEditor (OutflankAudioProcessor& p)
    : AudioProcessorEditor (&p), proc_ (p)
{
    setSize (320, 160);
}

OutflankAudioProcessorEditor::~OutflankAudioProcessorEditor() {}

void OutflankAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141420));
    g.setColour (juce::Colour (0xffaaaacc));
    g.setFont (juce::FontOptions (16.f).withStyle ("Bold"));
    g.drawText ("OUTFLANK", getLocalBounds(), juce::Justification::centred);
}

void OutflankAudioProcessorEditor::resized() {}
```

- [ ] **Step 7: Write `scripts/install.sh`**

```bash
#!/usr/bin/env bash
# Install Outflank plugins and standalone app.
# Usage:
#   ./install.sh           — install to user directories (no root needed)
#   ./install.sh --system  — install system-wide to /usr/lib (requires sudo)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SYSTEM=0

for arg in "$@"; do
    case "$arg" in
        --system) SYSTEM=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

if [[ $SYSTEM -eq 1 ]]; then
    VST3_DIR="/usr/lib/vst3"
    CLAP_DIR="/usr/lib/clap"
    BIN_DIR="/usr/local/bin"
else
    VST3_DIR="${HOME}/.vst3"
    CLAP_DIR="${HOME}/.clap"
    BIN_DIR="${HOME}/.local/bin"
fi

echo "Installing Outflank..."

mkdir -p "${VST3_DIR}"
rm -rf   "${VST3_DIR}/Outflank.vst3"
cp -r    "${SCRIPT_DIR}/VST3/Outflank.vst3" "${VST3_DIR}/"
echo "  VST3  → ${VST3_DIR}/Outflank.vst3"

mkdir -p "${CLAP_DIR}"
cp       "${SCRIPT_DIR}/CLAP/Outflank.clap" "${CLAP_DIR}/"
chmod    755 "${CLAP_DIR}/Outflank.clap"
echo "  CLAP  → ${CLAP_DIR}/Outflank.clap"

mkdir -p "${BIN_DIR}"
cp       "${SCRIPT_DIR}/bin/Outflank" "${BIN_DIR}/"
chmod    755 "${BIN_DIR}/Outflank"
echo "  App   → ${BIN_DIR}/Outflank"

echo "Done."
```

- [ ] **Step 8: Write `scripts/uninstall.sh`**

```bash
#!/usr/bin/env bash
# Remove Outflank from all known install locations.
set -euo pipefail

removed=0

remove() {
    local path="$1"
    if [[ -e "$path" ]]; then
        rm -rf "$path"
        echo "  Removed: $path"
        removed=1
    fi
}

echo "Uninstalling Outflank..."

remove "${HOME}/.vst3/Outflank.vst3"
remove "${HOME}/.clap/Outflank.clap"
remove "${HOME}/.local/bin/Outflank"

if [[ $EUID -eq 0 ]]; then
    remove "/usr/lib/vst3/Outflank.vst3"
    remove "/usr/lib/clap/Outflank.clap"
    remove "/usr/local/bin/Outflank"
else
    for path in "/usr/lib/vst3/Outflank.vst3" \
                "/usr/lib/clap/Outflank.clap" \
                "/usr/local/bin/Outflank"; do
        if [[ -e "$path" ]]; then
            echo "  Skipping $path (re-run with sudo to remove)"
        fi
    done
fi

if [[ $removed -eq 0 ]]; then
    echo "  Nothing to remove."
else
    echo "Done."
fi
```

- [ ] **Step 9: Make scripts executable**

```bash
chmod +x scripts/install.sh scripts/uninstall.sh
```

- [ ] **Step 10: Configure and build**

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build --parallel`
Expected: build succeeds (first run takes ~2 min fetching JUCE); no compiler errors.

- [ ] **Step 11: Verify the Standalone artefact exists and is executable**

Run: `test -x build/Outflank_artefacts/Debug/Standalone/Outflank && echo OK`
Expected: `OK`

- [ ] **Step 12: Commit**

```bash
git add CMakeLists.txt Source/PluginProcessor.h Source/PluginProcessor.cpp \
        Source/PluginEditor.h Source/PluginEditor.cpp scripts/install.sh scripts/uninstall.sh
git commit -m "Scaffold Outflank JUCE plugin project (passthrough, no DSP yet)"
```

---

### Task 2: StateVariableFilter DSP unit (TDD)

**Files:**
- Create: `Source/DSP/StateVariableFilter.h`
- Create: `Tests/test_runner.h`
- Create: `Tests/test_statevariablefilter.cpp`
- Modify: `CMakeLists.txt` (add `enable_testing()` + test target)

**Interfaces:**
- Produces: `class StateVariableFilter` with `void reset() noexcept`, `void setParameters(float frequencyHz, float q, double sampleRate) noexcept`, `float processLowpass(float input) noexcept`. No JUCE dependency. Consumed by `CrossoverMS` in Task 3.

- [ ] **Step 1: Write `Tests/test_runner.h`** (shared harness, matches Pugilist's convention)

```cpp
#pragma once
#include <cstdio>
#include <cstdlib>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(expr) \
    do { \
        if (expr) { \
            ++g_passed; \
        } else { \
            ++g_failed; \
            std::fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr); \
        } \
    } while(0)

#define CHECK_MSG(expr, msg) \
    do { \
        if (expr) { \
            ++g_passed; \
        } else { \
            ++g_failed; \
            std::fprintf(stderr, "FAIL  %s:%d  %s  (%s)\n", __FILE__, __LINE__, #expr, msg); \
        } \
    } while(0)

#define TEST_SUMMARY() \
    do { \
        std::printf("\n%d passed, %d failed\n", g_passed, g_failed); \
        if (g_failed > 0) std::exit(1); \
    } while(0)
```

- [ ] **Step 2: Write the failing test `Tests/test_statevariablefilter.cpp`**

```cpp
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
```

- [ ] **Step 3: Add the test target to `CMakeLists.txt`** (append near the end, before the `# ── Install rules` section)

```cmake
# ── Unit tests (no JUCE dependency) ──────────────────────────────────────────
enable_testing()

add_executable(test_statevariablefilter
    Tests/test_statevariablefilter.cpp
)
target_include_directories(test_statevariablefilter PRIVATE Source/ Tests/)
target_compile_features(test_statevariablefilter PRIVATE cxx_std_20)
add_test(NAME StateVariableFilter COMMAND test_statevariablefilter)
```

- [ ] **Step 4: Configure and attempt to build the test target — confirm it fails (header missing)**

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target test_statevariablefilter`
Expected: FAIL — `Source/DSP/StateVariableFilter.h` does not exist yet.

- [ ] **Step 5: Write `Source/DSP/StateVariableFilter.h`**

```cpp
#pragma once
#include <cmath>

// Zavalishin topology-preserving 2-pole state-variable filter — resonant lowpass
// output only. The complementary high band (input - low) is derived by the
// caller (CrossoverMS), which guarantees exact reconstruction at any Q.
//
// Plain C++, no JUCE dependency, so it can be exercised by a bare unit test
// executable (see Tests/test_statevariablefilter.cpp).
class StateVariableFilter
{
public:
    void reset() noexcept
    {
        ic1eq_ = 0.0f;
        ic2eq_ = 0.0f;
    }

    void setParameters (float frequencyHz, float q, double sampleRate) noexcept
    {
        const float g = std::tan (kPi * frequencyHz / static_cast<float> (sampleRate));
        const float k = 1.0f / q;
        a1_ = 1.0f / (1.0f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    float processLowpass (float input) noexcept
    {
        const float v3 = input - ic2eq_;
        const float v1 = a1_ * ic1eq_ + a2_ * v3;
        const float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;
        ic1eq_ = 2.0f * v1 - ic1eq_;
        ic2eq_ = 2.0f * v2 - ic2eq_;
        return v2;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
};
```

- [ ] **Step 6: Build and run the test — confirm it passes**

Run: `cmake --build build --target test_statevariablefilter && ctest --test-dir build -R StateVariableFilter --output-on-failure`
Expected: `3 passed, 0 failed` and `100% tests passed`.

- [ ] **Step 7: Commit**

```bash
git add Source/DSP/StateVariableFilter.h Tests/test_runner.h Tests/test_statevariablefilter.cpp CMakeLists.txt
git commit -m "Add StateVariableFilter DSP unit with passing tests"
```

---

### Task 3: CrossoverMS DSP unit (TDD)

**Files:**
- Create: `Source/DSP/CrossoverMS.h`
- Create: `Source/DSP/CrossoverMS.cpp`
- Create: `Tests/test_crossoverms.cpp`
- Modify: `CMakeLists.txt` (add test target; add `Source/DSP/CrossoverMS.cpp` to the plugin's `target_sources`)

**Interfaces:**
- Consumes: `StateVariableFilter` (`reset()`, `setParameters(freq, q, sampleRate)`, `processLowpass(input)`) from Task 2.
- Produces: `class CrossoverMS` with `void reset() noexcept` and `void process(float* left, float* right, int numSamples, float frequencyHz, float q, float rejection01, double sampleRate) noexcept`. `rejection01` is 0..1. Consumed by `PluginProcessor` in Task 4.

- [ ] **Step 1: Write the failing test `Tests/test_crossoverms.cpp`**

```cpp
#include "test_runner.h"
#include "../Source/DSP/CrossoverMS.h"
#include <cmath>
#include <vector>
#include <algorithm>

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kSampleRate = 48000.0;

static std::vector<float> renderMono (float freqHz, float crossoverHz, float q, float rejection, int numSamples)
{
    CrossoverMS x;
    std::vector<float> left (numSamples), right (numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = static_cast<float> (std::sin (2.0 * kPi * freqHz * i / kSampleRate));
        left[i] = s;
        right[i] = s;
    }
    x.process (left.data(), right.data(), numSamples, crossoverHz, q, rejection, kSampleRate);
    return left;
}

static float peakAfter (const std::vector<float>& v, size_t from)
{
    float m = 0.f;
    for (size_t i = from; i < v.size(); ++i) m = std::max (m, std::abs (v[i]));
    return m;
}

int main()
{
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

    // Fully out-of-phase low-frequency input (pure Side, no Mid) is strongly attenuated:
    // the low band is forced mono by discarding S_low, and there's no Mid to replace it.
    // The complementary high band (S - S_low) leaks roughly linearly with frequency ratio
    // near the crossover due to phase (not just magnitude) rolloff, so the test signal must
    // sit far below the crossover for the residual to be small — 80Hz vs a 250Hz crossover
    // (only ~1.6 octaves apart) leaves a ~0.46 residual, nowhere near silent. 20Hz vs a
    // 5000Hz crossover (~8 octaves apart) settles to ~0.0055 in a double-precision
    // reference simulation of this exact difference equation; 0.02 leaves a comfortable
    // margin for float rounding.
    {
        CrossoverMS x;
        const int n = 9600;
        std::vector<float> left (n), right (n);
        for (int i = 0; i < n; ++i)
        {
            const float s = static_cast<float> (std::sin (2.0 * kPi * 20.0 * i / kSampleRate));
            left[i] = s; right[i] = -s;
        }
        x.process (left.data(), right.data(), n, 5000.f, 0.707f, 0.f, kSampleRate);
        const float peakL = peakAfter (left, 4800);
        CHECK_MSG (peakL < 0.02f, "out-of-phase low-frequency content should be strongly attenuated by forced mono bass");
    }

    // rejection = 0: mono content above the crossover passes through near unity.
    {
        auto out = renderMono (2000.f, 250.f, 0.707f, 0.f, 9600);
        CHECK_MSG (peakAfter (out, 4800) > 0.9f, "mono highs should pass through when rejection is 0");
    }

    // rejection = 1: mono content above the crossover is fully removed.
    {
        auto out = renderMono (2000.f, 250.f, 0.707f, 1.f, 9600);
        CHECK_MSG (peakAfter (out, 4800) < 0.05f, "mono highs should be fully rejected when rejection is 1");
    }

    TEST_SUMMARY();
    return 0;
}
```

- [ ] **Step 2: Write `Source/DSP/CrossoverMS.h`**

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

- [ ] **Step 3: Write `Source/DSP/CrossoverMS.cpp`**

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

- [ ] **Step 4: Add the test target to `CMakeLists.txt`** (append after the `test_statevariablefilter` block)

```cmake
add_executable(test_crossoverms
    Tests/test_crossoverms.cpp
    Source/DSP/CrossoverMS.cpp
)
target_include_directories(test_crossoverms PRIVATE Source/ Tests/)
target_compile_features(test_crossoverms PRIVATE cxx_std_20)
add_test(NAME CrossoverMS COMMAND test_crossoverms)
```

- [ ] **Step 5: Also add `Source/DSP/CrossoverMS.cpp` to the plugin's own sources**, so the DSP is linked into the plugin binary too. In `CMakeLists.txt`, change:

```cmake
target_sources(Outflank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)
```

to:

```cmake
target_sources(Outflank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/CrossoverMS.cpp
)
```

- [ ] **Step 6: Configure, build, and run both test executables**

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: both `StateVariableFilter` and `CrossoverMS` tests pass (`100% tests passed, 0 tests failed out of 2`).

- [ ] **Step 7: Commit**

```bash
git add Source/DSP/CrossoverMS.h Source/DSP/CrossoverMS.cpp Tests/test_crossoverms.cpp CMakeLists.txt
git commit -m "Add CrossoverMS DSP unit with passing tests"
```

---

### Task 4: Wire the 3 parameters and CrossoverMS into PluginProcessor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `CrossoverMS` (Task 3).
- Produces: `OutflankAudioProcessor::apvts` now exposes parameter IDs `"frequency"`, `"q"`, `"rejection"` — consumed by `PluginEditor`'s `SliderAttachment`s in Task 5, and by `PresetManager`'s XML serialization in Task 6.

- [ ] **Step 1: Modify `Source/PluginProcessor.h`** — add the `#include`, the `crossover_` member, and change `prepareToPlay`'s implementation is in the .cpp (no header change needed there since it's already declared). Replace:

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class OutflankAudioProcessorEditor;
```

with:

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/CrossoverMS.h"

class OutflankAudioProcessorEditor;
```

and replace:

```cpp
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessor)
```

with:

```cpp
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    CrossoverMS crossover_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessor)
```

- [ ] **Step 2: Modify `Source/PluginProcessor.cpp`** — replace the empty parameter layout:

```cpp
juce::AudioProcessorValueTreeState::ParameterLayout OutflankAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    return { params.begin(), params.end() };
}
```

with:

```cpp
juce::AudioProcessorValueTreeState::ParameterLayout OutflankAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> ("frequency", "Frequency",
        NormalisableRange<float> (40.f, 2000.f, 1.f, 0.3f), 250.f,
        AudioParameterFloatAttributes{}.withLabel ("Hz")));

    params.push_back (std::make_unique<AudioParameterFloat> ("q", "Q",
        NormalisableRange<float> (0.3f, 4.0f, 0.01f), 0.707f));

    params.push_back (std::make_unique<AudioParameterFloat> ("rejection", "Rejection",
        NormalisableRange<float> (0.f, 100.f, 0.1f), 0.f,
        AudioParameterFloatAttributes{}.withLabel ("%")));

    return { params.begin(), params.end() };
}
```

- [ ] **Step 3: Modify `prepareToPlay` and `processBlock`** — replace:

```cpp
// ── Lifecycle ─────────────────────────────────────────────────────────────────
void OutflankAudioProcessor::prepareToPlay (double, int) {}
void OutflankAudioProcessor::releaseResources() {}
```

with:

```cpp
// ── Lifecycle ─────────────────────────────────────────────────────────────────
void OutflankAudioProcessor::prepareToPlay (double, int)
{
    crossover_.reset();
}
void OutflankAudioProcessor::releaseResources() {}
```

and replace:

```cpp
// ── processBlock ──────────────────────────────────────────────────────────────
void OutflankAudioProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Passthrough for now — DSP wired in a later task.
}
```

with:

```cpp
// ── processBlock ──────────────────────────────────────────────────────────────
void OutflankAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float freq = apvts.getRawParameterValue ("frequency")->load (std::memory_order_relaxed);
    const float q    = apvts.getRawParameterValue ("q")->load (std::memory_order_relaxed);
    const float rej  = apvts.getRawParameterValue ("rejection")->load (std::memory_order_relaxed) * 0.01f;

    crossover_.process (buffer.getWritePointer (0), buffer.getWritePointer (1),
                         buffer.getNumSamples(), freq, q, rej, getSampleRate());
}
```

- [ ] **Step 4: Rebuild and confirm everything still compiles and tests still pass**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: build succeeds; `100% tests passed, 0 tests failed out of 2` (unchanged — this task only adds glue code, no new DSP logic to unit-test).

- [ ] **Step 5: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "Wire frequency/q/rejection parameters into CrossoverMS processing"
```

---

### Task 5: PluginEditor — 3 knobs

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

**Interfaces:**
- Consumes: `OutflankAudioProcessor::apvts` parameter IDs `"frequency"`, `"q"`, `"rejection"` (Task 4).

- [ ] **Step 1: Rewrite `Source/PluginEditor.h`**

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class OutflankAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit OutflankAudioProcessorEditor (OutflankAudioProcessor&);
    ~OutflankAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void addKnob (juce::Slider& s, juce::Label& l, const char* name);

    OutflankAudioProcessor& proc_;

    juce::Slider frequencyKnob_, qKnob_, rejectionKnob_;
    juce::Label  frequencyLbl_,  qLbl_,  rejectionLbl_;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> frequencyAtt_, qAtt_, rejectionAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessorEditor)
};
```

- [ ] **Step 2: Rewrite `Source/PluginEditor.cpp`**

```cpp
#include "PluginEditor.h"

static const juce::Colour kBg     { 0xff141420 };
static const juce::Colour kLabel  { 0xffaaaacc };
static const juce::Colour kAccent { 0xff44aaee };

static constexpr int kW = 320;
static constexpr int kH = 160;
static constexpr int kKnobSize = 80;
static constexpr int kLabelH = 16;
static constexpr int kGap = 24;

void OutflankAudioProcessorEditor::addKnob (juce::Slider& s, juce::Label& l, const char* name)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    s.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff333355));
    s.setColour (juce::Slider::thumbColourId, kAccent.brighter (0.3f));
    s.setColour (juce::Slider::textBoxTextColourId, kLabel);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::FontOptions (11.f));
    l.setColour (juce::Label::textColourId, kLabel);
    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

OutflankAudioProcessorEditor::OutflankAudioProcessorEditor (OutflankAudioProcessor& p)
    : AudioProcessorEditor (&p), proc_ (p)
{
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    addKnob (frequencyKnob_, frequencyLbl_, "Frequency");
    addKnob (qKnob_,         qLbl_,         "Q");
    addKnob (rejectionKnob_, rejectionLbl_, "Rejection");

    frequencyAtt_ = std::make_unique<SA> (proc_.apvts, "frequency", frequencyKnob_);
    qAtt_         = std::make_unique<SA> (proc_.apvts, "q",         qKnob_);
    rejectionAtt_ = std::make_unique<SA> (proc_.apvts, "rejection", rejectionKnob_);

    setSize (kW, kH);
}

OutflankAudioProcessorEditor::~OutflankAudioProcessorEditor() {}

void OutflankAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kLabel);
    g.setFont (juce::FontOptions (16.f).withStyle ("Bold"));
    g.drawText ("OUTFLANK", 0, 8, kW, 20, juce::Justification::centred);
}

void OutflankAudioProcessorEditor::resized()
{
    const int totalKnobsW = kKnobSize * 3 + kGap * 2;
    int x = (kW - totalKnobsW) / 2;
    const int y = 40;

    auto place = [&] (juce::Slider& s, juce::Label& l)
    {
        s.setBounds (x, y, kKnobSize, kKnobSize);
        l.setBounds (x, y + kKnobSize, kKnobSize, kLabelH);
        x += kKnobSize + kGap;
    };

    place (frequencyKnob_, frequencyLbl_);
    place (qKnob_,         qLbl_);
    place (rejectionKnob_, rejectionLbl_);
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build --parallel`
Expected: build succeeds.

- [ ] **Step 4: Manual verification (per spec — ear + correlation check)**

Run: `./build/Outflank_artefacts/Debug/Standalone/Outflank`
Expected: window shows "OUTFLANK" with 3 rotary knobs (Frequency, Q, Rejection). Feed stereo program material; turning Frequency/Q down low and Rejection up should visibly/audibly narrow the bass to mono (check with a correlation meter if available) while highs stay wide.

- [ ] **Step 5: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "Add 3-knob UI (Frequency, Q, Rejection) wired to APVTS"
```

---

### Task 6: PresetManager + factory presets

**Files:**
- Create: `Source/PresetManager.h`
- Create: `Source/PresetManager.cpp`
- Create: `Source/Presets/Factory/001_TightMonoBass.xml`
- Create: `Source/Presets/Factory/002_WideAiry.xml`
- Create: `Source/Presets/Factory/003_Subtle.xml`
- Modify: `CMakeLists.txt` (binary data target for factory presets, add `PresetManager.cpp` to sources, link binary data)
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `OutflankAudioProcessor::apvts` (Task 4).
- Produces: `PresetManager` with `loadPreset(int)`, `savePreset(const juce::String&)`, `deletePreset(int)`, `renamePreset(int, const juce::String&)`, `getCurrentPresetIndex()`, `getCurrentPresetName()`, `getPresetList()`, `refreshUserPresets()`. `OutflankAudioProcessor::getPresetManager()` — consumed by `PluginEditor`'s preset UI in Task 7.

- [ ] **Step 1: Write `Source/PresetManager.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

struct PresetInfo
{
    juce::String name;
    bool isFactory = false;
    int index = -1; // Position in getPresetList(); -1 if not yet assigned
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager() = default;

    bool loadPreset (int index);
    bool savePreset (const juce::String& presetName);
    bool deletePreset (int index);
    bool renamePreset (int index, const juce::String& newName);

    int getCurrentPresetIndex() const;
    juce::String getCurrentPresetName() const;
    std::vector<PresetInfo> getPresetList() const;

    void refreshUserPresets();

private:
    juce::AudioProcessorValueTreeState& apvts_;
    int currentPresetIndex_ = -1;

    std::vector<PresetInfo> factoryPresets_;
    std::vector<PresetInfo> userPresets_;

    juce::File getUserPresetsDirectory() const;
    void ensureUserPresetsDirectory() const;
    void loadFactoryPresets();
    void loadUserPresetsFromDisk();

    std::unique_ptr<juce::XmlElement> serializeAPVTS() const;
    bool deserializeAPVTS (const juce::XmlElement& xmlElement);

    bool loadPresetInternal (const PresetInfo& preset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
```

- [ ] **Step 2: Write `Source/PresetManager.cpp`**

```cpp
#include "PresetManager.h"
#include <BinaryData.h>
#include <algorithm>

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts)
    : apvts_ (apvts)
{
    ensureUserPresetsDirectory();
    loadFactoryPresets();
    loadUserPresetsFromDisk();
}

int PresetManager::getCurrentPresetIndex() const { return currentPresetIndex_; }

juce::String PresetManager::getCurrentPresetName() const
{
    auto all = getPresetList();
    if (currentPresetIndex_ >= 0 && currentPresetIndex_ < static_cast<int> (all.size()))
        return all[static_cast<size_t> (currentPresetIndex_)].name;
    return "Unknown";
}

juce::File PresetManager::getUserPresetsDirectory() const
{
#if JUCE_WINDOWS
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    return appData.getChildFile ("Outflank").getChildFile ("presets");
#elif JUCE_MAC
    auto appSupport = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                 .getChildFile ("Application Support");
    return appSupport.getChildFile ("Outflank").getChildFile ("presets");
#else // JUCE_LINUX
    auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    return home.getChildFile (".config").getChildFile ("Outflank").getChildFile ("presets");
#endif
}

void PresetManager::ensureUserPresetsDirectory() const
{
    auto dir = getUserPresetsDirectory();
    if (!dir.exists())
        dir.createDirectory();
}

std::unique_ptr<juce::XmlElement> PresetManager::serializeAPVTS() const
{
    auto state = apvts_.state;
    return state.createXml();
}

bool PresetManager::deserializeAPVTS (const juce::XmlElement& xmlElement)
{
    auto tree = juce::ValueTree::fromXml (xmlElement);
    if (tree.isValid() && tree.getType() == apvts_.state.getType())
    {
        apvts_.replaceState (tree);
        return true;
    }
    return false;
}

void PresetManager::loadFactoryPresets()
{
    const std::vector<juce::String> factoryNames = {
        "Tight Mono Bass",
        "Wide Airy",
        "Subtle",
    };

    factoryPresets_.clear();
    for (size_t i = 0; i < factoryNames.size(); ++i)
    {
        PresetInfo info;
        info.name      = factoryNames[i];
        info.isFactory = true;
        info.index     = static_cast<int> (i);
        factoryPresets_.push_back (info);
    }
}

void PresetManager::loadUserPresetsFromDisk()
{
    userPresets_.clear();
    auto dir = getUserPresetsDirectory();
    if (!dir.exists())
        return;

    for (const auto& file : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
    {
        PresetInfo info;
        info.name      = file.getFileNameWithoutExtension();
        info.isFactory = false;
        info.index     = static_cast<int> (factoryPresets_.size() + userPresets_.size());
        userPresets_.push_back (info);
    }

    std::sort (userPresets_.begin(), userPresets_.end(),
               [] (const PresetInfo& a, const PresetInfo& b) { return a.name < b.name; });

    for (size_t i = 0; i < userPresets_.size(); ++i)
        userPresets_[i].index = static_cast<int> (factoryPresets_.size() + i);
}

std::vector<PresetInfo> PresetManager::getPresetList() const
{
    std::vector<PresetInfo> result = factoryPresets_;
    result.insert (result.end(), userPresets_.begin(), userPresets_.end());
    return result;
}

void PresetManager::refreshUserPresets() { loadUserPresetsFromDisk(); }

bool PresetManager::loadPreset (int index)
{
    auto all = getPresetList();
    if (index < 0 || index >= static_cast<int> (all.size()))
        return false;
    return loadPresetInternal (all[static_cast<size_t> (index)]);
}

bool PresetManager::loadPresetInternal (const PresetInfo& preset)
{
    std::unique_ptr<juce::XmlElement> xmlElement;

    if (preset.isFactory)
    {
        // Format: _NNN_PresetNameNoSpaces_xml, e.g. index 0, "Tight Mono Bass" -> "_001_TightMonoBass_xml"
        juce::String paddedIndex  = juce::String (preset.index + 1).paddedLeft ('0', 3);
        juce::String nameNoSpaces = preset.name.removeCharacters (" ");
        juce::String resourceName = "_" + paddedIndex + "_" + nameNoSpaces + "_xml";

        int dataSize = 0;
        const char* data = Outflank_BinaryData::getNamedResource (resourceName.toRawUTF8(), dataSize);
        if (data == nullptr || dataSize <= 0)
        {
            DBG ("PresetManager: binary resource not found: " + resourceName);
            return false;
        }
        xmlElement = juce::parseXML (juce::String::fromUTF8 (data, dataSize));
        if (!xmlElement)
        {
            DBG ("PresetManager: failed to parse factory preset XML: " + resourceName);
            return false;
        }
    }
    else
    {
        auto dir        = getUserPresetsDirectory();
        auto presetFile = dir.getChildFile (preset.name + ".xml");
        if (!presetFile.exists())
            return false;
        xmlElement = juce::parseXML (presetFile);
        if (!xmlElement)
        {
            DBG ("PresetManager: failed to parse preset XML: " + presetFile.getFullPathName());
            return false;
        }
    }

    bool success = deserializeAPVTS (*xmlElement);
    if (success)
        currentPresetIndex_ = preset.index;
    return success;
}

bool PresetManager::savePreset (const juce::String& presetName)
{
    if (presetName.isEmpty())
        return false;

    if (presetName.containsAnyOf ("/\\"))
        return false;

    for (const auto& factory : factoryPresets_)
        if (factory.name == presetName)
            return false;

    ensureUserPresetsDirectory();

    auto xmlElement = serializeAPVTS();
    if (!xmlElement)
        return false;

    auto dir        = getUserPresetsDirectory();
    auto presetFile = dir.getChildFile (presetName + ".xml");
    bool success    = xmlElement->writeTo (presetFile);

    if (success)
    {
        refreshUserPresets();
        for (const auto& p : getPresetList())
        {
            if (!p.isFactory && p.name == presetName)
            {
                currentPresetIndex_ = p.index;
                break;
            }
        }
    }
    return success;
}

bool PresetManager::deletePreset (int index)
{
    auto all = getPresetList();
    if (index < 0 || index >= static_cast<int> (all.size()))
        return false;

    const auto& preset = all[static_cast<size_t> (index)];
    if (preset.isFactory)
        return false;

    auto dir        = getUserPresetsDirectory();
    auto presetFile = dir.getChildFile (preset.name + ".xml");
    if (presetFile.exists() && !presetFile.deleteFile())
        return false;

    bool wasCurrent = (currentPresetIndex_ == index);
    bool currentWasFactory = true;
    juce::String currentName;
    if (!wasCurrent && currentPresetIndex_ >= 0 && currentPresetIndex_ < static_cast<int> (all.size()))
    {
        currentWasFactory = all[static_cast<size_t> (currentPresetIndex_)].isFactory;
        currentName       = all[static_cast<size_t> (currentPresetIndex_)].name;
    }

    refreshUserPresets();

    if (wasCurrent)
    {
        currentPresetIndex_ = 0;
    }
    else if (!currentWasFactory)
    {
        bool found = false;
        for (const auto& p : getPresetList())
        {
            if (!p.isFactory && p.name == currentName)
            {
                currentPresetIndex_ = p.index;
                found = true;
                break;
            }
        }
        if (!found)
            currentPresetIndex_ = 0;
    }

    if (currentPresetIndex_ >= static_cast<int> (getPresetList().size()))
        currentPresetIndex_ = 0;
    return true;
}

bool PresetManager::renamePreset (int index, const juce::String& newName)
{
    if (newName.isEmpty())
        return false;

    if (newName.containsAnyOf ("/\\"))
        return false;

    auto all = getPresetList();
    if (index < 0 || index >= static_cast<int> (all.size()))
        return false;

    const auto& preset = all[static_cast<size_t> (index)];
    if (preset.isFactory)
        return false;

    auto dir     = getUserPresetsDirectory();
    auto oldFile = dir.getChildFile (preset.name + ".xml");
    auto newFile = dir.getChildFile (newName + ".xml");
    if (!oldFile.exists() || newFile.exists())
        return false;

    bool success = oldFile.moveFileTo (newFile);
    if (success)
    {
        bool wasCurrent = (currentPresetIndex_ == index);
        bool currentWasFactory = true;
        juce::String currentName;
        if (!wasCurrent && currentPresetIndex_ >= 0 && currentPresetIndex_ < static_cast<int> (all.size()))
        {
            currentWasFactory = all[static_cast<size_t> (currentPresetIndex_)].isFactory;
            currentName       = all[static_cast<size_t> (currentPresetIndex_)].name;
        }

        refreshUserPresets();

        if (wasCurrent)
        {
            for (const auto& p : getPresetList())
            {
                if (!p.isFactory && p.name == newName)
                {
                    currentPresetIndex_ = p.index;
                    break;
                }
            }
        }
        else if (!currentWasFactory)
        {
            bool found = false;
            for (const auto& p : getPresetList())
            {
                if (!p.isFactory && p.name == currentName)
                {
                    currentPresetIndex_ = p.index;
                    found = true;
                    break;
                }
            }
            if (!found)
                currentPresetIndex_ = 0;
        }
    }
    return success;
}
```

- [ ] **Step 3: Write the 3 factory preset XML files**

`Source/Presets/Factory/001_TightMonoBass.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<OutflankState>
  <PARAM id="frequency" value="100.0"/>
  <PARAM id="q" value="1.0"/>
  <PARAM id="rejection" value="10.0"/>
</OutflankState>
```

`Source/Presets/Factory/002_WideAiry.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<OutflankState>
  <PARAM id="frequency" value="350.0"/>
  <PARAM id="q" value="0.707"/>
  <PARAM id="rejection" value="80.0"/>
</OutflankState>
```

`Source/Presets/Factory/003_Subtle.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<OutflankState>
  <PARAM id="frequency" value="180.0"/>
  <PARAM id="q" value="0.707"/>
  <PARAM id="rejection" value="20.0"/>
</OutflankState>
```

- [ ] **Step 4: Modify `CMakeLists.txt`** — add the binary data target right after the clap-juce-extensions fetch block. Insert before `set(OUTFLANK_FORMATS VST3 Standalone)`:

```cmake
# Embed factory presets as binary data
file(GLOB FACTORY_PRESET_FILES "Source/Presets/Factory/*.xml")
juce_add_binary_data(Outflank_BinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE   Outflank_BinaryData
    SOURCES     ${FACTORY_PRESET_FILES}
)
```

Then update `target_sources` to add `PresetManager.cpp`:

```cmake
target_sources(Outflank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/CrossoverMS.cpp
    Source/PresetManager.cpp
)
```

Then update `target_link_libraries` to link the binary data (add `Outflank_BinaryData` to the `PRIVATE` list, alongside `juce::juce_audio_utils`):

```cmake
target_link_libraries(Outflank
    PRIVATE
        Outflank_BinaryData
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_dsp
        clap_juce_extensions
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
```

- [ ] **Step 5: Modify `Source/PluginProcessor.h`** — add the include, member, and accessor. Replace:

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/CrossoverMS.h"

class OutflankAudioProcessorEditor;
```

with:

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/CrossoverMS.h"
#include "PresetManager.h"

class OutflankAudioProcessorEditor;
```

Replace:

```cpp
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    CrossoverMS crossover_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessor)
```

with:

```cpp
    juce::AudioProcessorValueTreeState apvts;

    PresetManager* getPresetManager() noexcept { return presetManager_.get(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    CrossoverMS crossover_;
    std::unique_ptr<PresetManager> presetManager_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutflankAudioProcessor)
```

- [ ] **Step 6: Modify `Source/PluginProcessor.cpp`** — construct `presetManager_` and wire the program methods. Replace the constructor:

```cpp
OutflankAudioProcessor::OutflankAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OutflankState", createParameterLayout())
{}
```

with:

```cpp
OutflankAudioProcessor::OutflankAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OutflankState", createParameterLayout()),
      presetManager_ (std::make_unique<PresetManager> (apvts))
{}
```

Replace the program stubs:

```cpp
int  OutflankAudioProcessor::getNumPrograms()              { return 1; }
int  OutflankAudioProcessor::getCurrentProgram()           { return 0; }
void OutflankAudioProcessor::setCurrentProgram (int)       {}
const juce::String OutflankAudioProcessor::getProgramName (int) { return {}; }
void OutflankAudioProcessor::changeProgramName (int, const juce::String&) {}
```

with:

```cpp
int OutflankAudioProcessor::getNumPrograms()
{
    return static_cast<int> (presetManager_->getPresetList().size());
}

int OutflankAudioProcessor::getCurrentProgram()
{
    return presetManager_->getCurrentPresetIndex();
}

void OutflankAudioProcessor::setCurrentProgram (int index)
{
    presetManager_->loadPreset (index);
}

const juce::String OutflankAudioProcessor::getProgramName (int index)
{
    auto presets = presetManager_->getPresetList();
    if (index >= 0 && index < static_cast<int> (presets.size()))
        return presets[static_cast<size_t> (index)].name;
    return {};
}

void OutflankAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    presetManager_->renamePreset (index, newName);
}
```

- [ ] **Step 7: Build and run tests**

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: build succeeds (JUCE binary-data generator runs as part of the build); `100% tests passed, 0 tests failed out of 2` (unchanged).

- [ ] **Step 8: Manual verification — factory presets load correctly**

Run: `./build/Outflank_artefacts/Debug/Standalone/Outflank`
Expected: no crash on launch (the constructor now builds the preset list). (Preset selection UI comes in Task 7; for now this only confirms `PresetManager` construction and factory preset embedding don't break startup.)

- [ ] **Step 9: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp Source/Presets/Factory/ \
        CMakeLists.txt Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "Add PresetManager with 3 factory presets, wired into PluginProcessor"
```

---

### Task 7: Preset UI in PluginEditor

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

**Interfaces:**
- Consumes: `OutflankAudioProcessor::getPresetManager()`, `PresetManager::getPresetList()`, `getCurrentPresetIndex()`, `savePreset(name)`, `deletePreset(index)` (Task 6).

- [ ] **Step 1: Modify `Source/PluginEditor.h`** — add preset UI members and methods. Replace:

```cpp
private:
    void addKnob (juce::Slider& s, juce::Label& l, const char* name);

    OutflankAudioProcessor& proc_;
```

with:

```cpp
private:
    void addKnob (juce::Slider& s, juce::Label& l, const char* name);
    void updatePresetList();
    void onPresetSelected();
    void onSaveAsPressed();
    void onDeletePressed();

    OutflankAudioProcessor& proc_;

    juce::ComboBox   presetSelector_;
    juce::TextButton saveAsButton_ { "SAVE AS" };
    juce::TextButton deleteButton_ { "DELETE"  };
```

- [ ] **Step 2: Modify `Source/PluginEditor.cpp`** — grow the window and wire up the preset bar. Replace the layout constants:

```cpp
static constexpr int kW = 320;
static constexpr int kH = 160;
```

with:

```cpp
static constexpr int kW = 320;
static constexpr int kH = 200;
static constexpr int kPresetBarH = 24;
```

Replace the constructor:

```cpp
OutflankAudioProcessorEditor::OutflankAudioProcessorEditor (OutflankAudioProcessor& p)
    : AudioProcessorEditor (&p), proc_ (p)
{
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    addKnob (frequencyKnob_, frequencyLbl_, "Frequency");
    addKnob (qKnob_,         qLbl_,         "Q");
    addKnob (rejectionKnob_, rejectionLbl_, "Rejection");

    frequencyAtt_ = std::make_unique<SA> (proc_.apvts, "frequency", frequencyKnob_);
    qAtt_         = std::make_unique<SA> (proc_.apvts, "q",         qKnob_);
    rejectionAtt_ = std::make_unique<SA> (proc_.apvts, "rejection", rejectionKnob_);

    setSize (kW, kH);
}
```

with:

```cpp
OutflankAudioProcessorEditor::OutflankAudioProcessorEditor (OutflankAudioProcessor& p)
    : AudioProcessorEditor (&p), proc_ (p)
{
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    addAndMakeVisible (presetSelector_);
    addAndMakeVisible (saveAsButton_);
    addAndMakeVisible (deleteButton_);
    presetSelector_.onChange = [this] { onPresetSelected(); };
    saveAsButton_.onClick    = [this] { onSaveAsPressed();  };
    deleteButton_.onClick    = [this] { onDeletePressed();  };
    updatePresetList();

    addKnob (frequencyKnob_, frequencyLbl_, "Frequency");
    addKnob (qKnob_,         qLbl_,         "Q");
    addKnob (rejectionKnob_, rejectionLbl_, "Rejection");

    frequencyAtt_ = std::make_unique<SA> (proc_.apvts, "frequency", frequencyKnob_);
    qAtt_         = std::make_unique<SA> (proc_.apvts, "q",         qKnob_);
    rejectionAtt_ = std::make_unique<SA> (proc_.apvts, "rejection", rejectionKnob_);

    setSize (kW, kH);
}
```

Replace `resized()`:

```cpp
void OutflankAudioProcessorEditor::resized()
{
    const int totalKnobsW = kKnobSize * 3 + kGap * 2;
    int x = (kW - totalKnobsW) / 2;
    const int y = 40;

    auto place = [&] (juce::Slider& s, juce::Label& l)
    {
        s.setBounds (x, y, kKnobSize, kKnobSize);
        l.setBounds (x, y + kKnobSize, kKnobSize, kLabelH);
        x += kKnobSize + kGap;
    };

    place (frequencyKnob_, frequencyLbl_);
    place (qKnob_,         qLbl_);
    place (rejectionKnob_, rejectionLbl_);
}
```

with:

```cpp
void OutflankAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().reduced (8);

    // Preset bar: selector | SAVE AS | DELETE
    auto row = b.removeFromTop (kPresetBarH);
    presetSelector_.setBounds (row.removeFromLeft (160));
    row.removeFromLeft (8);
    saveAsButton_.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (8);
    deleteButton_.setBounds (row.removeFromLeft (70));

    const int totalKnobsW = kKnobSize * 3 + kGap * 2;
    int x = (kW - totalKnobsW) / 2;
    const int y = kPresetBarH + 48;

    auto place = [&] (juce::Slider& s, juce::Label& l)
    {
        s.setBounds (x, y, kKnobSize, kKnobSize);
        l.setBounds (x, y + kKnobSize, kKnobSize, kLabelH);
        x += kKnobSize + kGap;
    };

    place (frequencyKnob_, frequencyLbl_);
    place (qKnob_,         qLbl_);
    place (rejectionKnob_, rejectionLbl_);
}
```

Append the preset management methods at the end of the file:

```cpp
// ── Preset management ────────────────────────────────────────────────────────
void OutflankAudioProcessorEditor::updatePresetList()
{
    presetSelector_.clear (juce::dontSendNotification);
    auto* pm = proc_.getPresetManager();
    if (!pm) return;

    auto presets = pm->getPresetList();
    for (size_t i = 0; i < presets.size(); ++i)
        presetSelector_.addItem (presets[i].name, static_cast<int> (i + 1));

    int idx = pm->getCurrentPresetIndex();
    presetSelector_.setSelectedItemIndex (idx, juce::dontSendNotification);
    bool isFactory = (idx >= 0 && idx < (int) presets.size()) ? presets[(size_t) idx].isFactory : true;
    deleteButton_.setEnabled (!isFactory);
}

void OutflankAudioProcessorEditor::onPresetSelected()
{
    int idx = presetSelector_.getSelectedItemIndex();
    if (idx >= 0) proc_.setCurrentProgram (idx);
    auto* pm = proc_.getPresetManager();
    if (!pm) return;
    auto presets = pm->getPresetList();
    bool isFactory = (idx >= 0 && idx < (int) presets.size()) ? presets[(size_t) idx].isFactory : true;
    deleteButton_.setEnabled (!isFactory);
}

void OutflankAudioProcessorEditor::onSaveAsPressed()
{
    auto* w = new juce::AlertWindow ("Save Preset As", "Enter preset name:", juce::AlertWindow::NoIcon);
    w->addTextEditor ("name", "", "Preset name:");
    w->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<OutflankAudioProcessorEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result) {
        std::unique_ptr<juce::AlertWindow> owned (w);
        if (result == 1 && safe != nullptr)
        {
            auto name = w->getTextEditorContents ("name").trim();
            if (name.isNotEmpty())
            {
                auto* pm = safe->proc_.getPresetManager();
                if (pm && pm->savePreset (name)) safe->updatePresetList();
            }
        }
    }));
}

void OutflankAudioProcessorEditor::onDeletePressed()
{
    auto* pm = proc_.getPresetManager();
    if (!pm) return;
    int idx = pm->getCurrentPresetIndex();
    auto presets = pm->getPresetList();
    if (idx < 0 || idx >= (int) presets.size() || presets[(size_t) idx].isFactory) return;

    juce::Component::SafePointer<OutflankAudioProcessorEditor> safe (this);
    juce::AlertWindow::showOkCancelBox (
        juce::AlertWindow::WarningIcon, "Delete Preset",
        "Delete preset '" + presets[(size_t) idx].name + "'?", "Delete", "Cancel", nullptr,
        juce::ModalCallbackFunction::create ([safe, idx] (int result) {
            if (result == 1 && safe != nullptr)
            {
                auto* pm2 = safe->proc_.getPresetManager();
                if (pm2 && pm2->deletePreset (idx))
                {
                    // deletePreset() only updates PresetManager's bookkeeping index when the
                    // deleted preset was active — it does not reload APVTS. Without this call,
                    // the combo box would show a fallback preset selected while the knobs/audio
                    // still reflected the just-deleted preset's values.
                    safe->proc_.setCurrentProgram (pm2->getCurrentPresetIndex());
                    safe->updatePresetList();
                }
            }
        }));
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build --parallel`
Expected: build succeeds.

- [ ] **Step 4: Manual verification — full preset workflow**

Run: `./build/Outflank_artefacts/Debug/Standalone/Outflank`
Expected: preset selector shows "Tight Mono Bass", "Wide Airy", "Subtle"; selecting one updates all 3 knobs; turning a knob then clicking SAVE AS with a new name adds it to the list and selects it; DELETE is disabled for factory presets and enabled for the user preset just saved; deleting it removes it from the list.

- [ ] **Step 5: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "Add preset selector/save/delete UI to PluginEditor"
```

---

### Task 8: Full multi-format build verification

**Files:** none (verification only)

- [ ] **Step 1: Clean Debug build of all targets, including tests**

Run: `rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build --parallel`
Expected: build succeeds with no errors.

- [ ] **Step 2: Run the full test suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 2`.

- [ ] **Step 3: Confirm all plugin artefacts exist**

Run: `ls build/Outflank_artefacts/Debug/VST3/Outflank.vst3 build/Outflank_artefacts/Debug/CLAP/Outflank.clap build/Outflank_artefacts/Debug/Standalone/Outflank`
Expected: all three paths exist.

- [ ] **Step 4: Release build and packaging**

Run: `cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-release --parallel && cd build-release && cpack && cd ..`
Expected: build succeeds; `cpack` produces `Outflank-1.0.0-linux-x86_64.tar.gz` in `build-release/`.

- [ ] **Step 5: Commit** (only if any of the above steps required fixes; otherwise nothing to commit)

```bash
git status
```

If clean, no commit needed — this task is verification-only.

---

## Notes for the implementer

- Steps that say "Run: `cmake --build build ...`" assume `build/` was already configured by an earlier task's Step 1/10. Re-run `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` if `build/` doesn't exist yet.
- The first `cmake -B build` in Task 1 downloads JUCE (~2 min) and clap-juce-extensions; subsequent configures are fast.
- Manual verification steps (Tasks 5, 6, 8) require running the Standalone binary, which needs an X11/audio-capable environment. If running headless, confirm the binary builds and launches without crashing (exit code / no immediate crash log) as a fallback check, and flag that ear/visual verification still needs a human pass.
