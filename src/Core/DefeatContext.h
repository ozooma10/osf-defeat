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
        std::string consequenceName;
        std::string consequenceQuest;
        float recoverySeconds{ 0.0f };
        std::vector<FormID> participants;  // hostiles near the player at defeat time (incl. aggressor if hostile)
    };

    void SetPlayerContext(RE::Actor* a_player, RE::Actor* a_aggressor, const Consequence::PlayerOutcome& a_outcome, const std::vector<RE::Actor*>& a_participants = {});
    void ClearPlayerContext();

    [[nodiscard]] PlayerContext GetPlayerContext();
    [[nodiscard]] RE::Actor* GetLastPlayerAggressor();
    [[nodiscard]] std::string GetLastPlayerConsequenceName();
    [[nodiscard]] std::string GetLastPlayerConsequenceQuest();
    [[nodiscard]] float GetLastPlayerRecoverySeconds();

    // Defeat-time participant snapshot resolved back to live actors (unloaded/dead-since entries drop out). Scene casting for content mods; "who else was in the fight".
    [[nodiscard]] std::vector<RE::Actor*> GetLastPlayerParticipants();
}