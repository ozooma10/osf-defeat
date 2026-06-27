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
    };

    void SetPlayerContext(RE::Actor* a_player, RE::Actor* a_aggressor, const Consequence::PlayerOutcome& a_outcome);
    void ClearPlayerContext();

    [[nodiscard]] PlayerContext GetPlayerContext();
    [[nodiscard]] RE::Actor* GetLastPlayerAggressor();
    [[nodiscard]] std::string GetLastPlayerConsequenceName();
    [[nodiscard]] std::string GetLastPlayerConsequenceQuest();
    [[nodiscard]] float GetLastPlayerRecoverySeconds();
}