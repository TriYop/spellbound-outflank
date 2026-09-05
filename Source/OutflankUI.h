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
