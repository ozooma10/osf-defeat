#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Config
{
    struct RuntimeConfig
    {
        bool processingEnabled{ true };
        bool disableInScenes{ false };
        float defaultRecoverySeconds{ 8.0f };
        std::set<FormID> excludedLocations;
        std::set<FormID> excludedQuests;
    };

    void Load();
    void Reload();

    [[nodiscard]] RuntimeConfig Get();
    [[nodiscard]] bool IsProcessingEnabled();
    [[nodiscard]] bool IsLocationExcluded(RE::BGSLocation* a_location);
    [[nodiscard]] bool ShouldDisableInScenes();
    [[nodiscard]] std::filesystem::path RootPath();
}