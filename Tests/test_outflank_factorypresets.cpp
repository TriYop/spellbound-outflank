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
