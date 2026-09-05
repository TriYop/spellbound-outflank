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
