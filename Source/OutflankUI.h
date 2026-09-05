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
