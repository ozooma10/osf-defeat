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
        float participantScanRadius{ 4096.0f };  // radius filter for the defeat-time participant snapshot (recent attackers); <=0 = no radius filter
        bool explainDecisions{ false };          // verbose decision logging: NPC passthrough reasons + per-outcome selection filter/roll detail
        float watchdogSeconds{ 600.0f };         // max held-down time before the watchdog force-rescues (player held entries always; NPC entries only when leased); <=0 disables
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
    [[nodiscard]] float ParticipantScanRadius();
    [[nodiscard]] bool IsExplainEnabled();
    [[nodiscard]] float WatchdogSeconds();
    [[nodiscard]] std::filesystem::path RootPath();
}