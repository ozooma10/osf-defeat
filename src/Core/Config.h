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
        bool npcAutoDefeat{ false };             // auto-down hostile NPCs on a would-be-lethal hit
        bool npcAutoDefeatIncludeRobots{ false };  // include robots/turrets in NPC auto-defeat
        std::set<FormID> excludedLocations;
        std::set<FormID> excludedQuests;
    };

    void Load();
    void Reload();

    [[nodiscard]] RuntimeConfig Get();
    [[nodiscard]] bool IsProcessingEnabled();
    [[nodiscard]] bool IsLocationExcluded(RE::BGSLocation* a_location);
    [[nodiscard]] bool HasExcludedLocations();
    [[nodiscard]] bool ShouldDisableInScenes();
    [[nodiscard]] bool IsNpcAutoDefeatEnabled();
    [[nodiscard]] bool NpcAutoDefeatIncludesRobots();
    [[nodiscard]] std::filesystem::path RootPath();
}