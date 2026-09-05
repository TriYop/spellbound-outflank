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
