#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Config
{
    struct FrameworkSettings
    {
        bool processingEnabled{ true };
        bool disableInScenes{ false };
        float defaultAutoRecoveryDelaySeconds{ 8.0f };
        bool npcAutoDefeat{ false };             // auto-down hostile NPCs on a would-be-lethal hit
        bool npcAutoDefeatIncludeRobots{ false };  // include robots/turrets in NPC auto-defeat
        bool applyCaptiveFactionOnPlayerDefeat{ false };  // opt-in captive-faction neutrality for the player
        FormID captiveFaction{ 0x0003E0C8 };     // vanilla Starfield.esm captive faction (SAF Surrender parity)
        float recentHostileAttackerRadius{ 4096.0f };  // radius filter for the defeat-time attacker snapshot; <=0 = unlimited
        bool explainDecisions{ false };          // verbose decision logging: NPC passthrough reasons + per-consequence selection filter/roll detail
        float watchdogSeconds{ 600.0f };         // max held-down time before the watchdog force-recovers (player held entries always; NPC entries only when leased); <=0 disables
        float imminentHealthFraction{ 0.25f };   // player-defeat-imminent event fires when a Health hit lands below this fraction of max; <=0 disables
        std::set<FormID> excludedLocations;
        std::set<FormID> excludedQuests;
    };

    void Load();
    void Reload();

    [[nodiscard]] FrameworkSettings Get();
    [[nodiscard]] bool IsProcessingEnabled();
    [[nodiscard]] bool IsLocationExcluded(RE::BGSLocation* a_location);
    [[nodiscard]] bool HasExcludedLocations();
    [[nodiscard]] bool ShouldDisableInScenes();
    [[nodiscard]] bool IsNpcAutoDefeatEnabled();
    [[nodiscard]] bool NpcAutoDefeatIncludesRobots();
    [[nodiscard]] bool ShouldApplyCaptiveFactionOnPlayerDefeat();
    [[nodiscard]] FormID CaptiveFactionID();
    [[nodiscard]] float RecentHostileAttackerRadius();
    [[nodiscard]] bool IsExplainEnabled();
    [[nodiscard]] float WatchdogSeconds();
    [[nodiscard]] float ImminentHealthFraction();
    [[nodiscard]] std::filesystem::path RootPath();
}
