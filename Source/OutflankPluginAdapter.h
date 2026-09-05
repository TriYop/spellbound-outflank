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
    const char* getHomePage() const override;
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
