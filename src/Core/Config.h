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
        bool factionPacify{ false };             // OPT-IN full "surrender" calm: add downed player to captiveFaction so ALL hostiles disengage (default off; base behavior just drops the player as a target)
        FormID captiveFaction{ 0x0003E0C8 };     // vanilla Starfield.esm captive faction (SAF Surrender parity)
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
    [[nodiscard]] bool IsFactionPacifyEnabled();
    [[nodiscard]] FormID CaptiveFactionID();
    [[nodiscard]] std::filesystem::path RootPath();
}