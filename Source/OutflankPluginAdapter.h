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
