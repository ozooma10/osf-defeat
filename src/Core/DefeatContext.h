#pragma once

#include "pch.h"

#include "Core/Consequence.h"
#include "Core/DefeatRegistry.h"

namespace OSFDefeat::DefeatContext
{
    struct PlayerContext
    {
        FormID player{ 0x14 };
        FormID aggressor{ 0 };
        std::string consequenceId;
        std::string consequenceDisplayName;
        std::string consequenceQuest;
        float autoRecoveryDelaySeconds{ 0.0f };
        bool recoveryHeld{ false };
        DefeatCause cause{ DefeatCause::kNone };  // how the player went down (Health vs scripted/capture)
        std::vector<FormID> recentHostileAttackerIds;  // aggressor plus recent live hostile attackers
    };

    void SetPlayerContext(RE::Actor* a_player, RE::Actor* a_aggressor, const Consequence::PlayerConsequence& a_consequence, const std::vector<RE::Actor*>& a_recentHostileAttackers = {}, DefeatCause a_cause = DefeatCause::kNone);
    void ClearPlayerContext();

    [[nodiscard]] PlayerContext GetPlayerContext();
    [[nodiscard]] RE::Actor* GetLastPlayerAggressor();
    [[nodiscard]] std::string GetLastPlayerConsequenceId();
    [[nodiscard]] std::string GetLastPlayerConsequenceName();
    [[nodiscard]] std::string GetLastPlayerConsequenceQuest();
    [[nodiscard]] float GetLastPlayerRecoverySeconds();
    [[nodiscard]] float GetLastPlayerAutoRecoveryDelaySeconds();
    [[nodiscard]] bool IsLastPlayerRecoveryHeld();
    [[nodiscard]] DefeatCause GetLastPlayerDefeatCause();

    [[nodiscard]] std::vector<RE::Actor*> GetLastPlayerRecentHostileAttackers();
    // Compatibility alias for the original alpha API terminology.
    [[nodiscard]] std::vector<RE::Actor*> GetLastPlayerParticipants();
}
