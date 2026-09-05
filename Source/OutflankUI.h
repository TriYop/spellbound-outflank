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
