#pragma once

#include "pch.h"

#include "Core/Consequence.h"
#include "Core/DefeatRegistry.h"

namespace OSFDefeat::DefeatContext
{
    // Recent attackers are the non-null actors collected by PlayerDefeat on the game thread.
    void SetPlayerContext(RE::Actor* a_aggressor, const Consequence::PlayerConsequence& a_consequence, const std::vector<RE::Actor*>& a_recentHostileAttackers = {}, DefeatCause a_cause = DefeatCause::kNone, FormID a_equippedWeapon = 0);
    void ClearPlayerContext();

    [[nodiscard]] RE::Actor* GetLastPlayerAggressor();
    [[nodiscard]] RE::TESObjectWEAP* GetLastPlayerEquippedWeapon();
    [[nodiscard]] std::string GetLastPlayerConsequenceId();
    [[nodiscard]] std::string GetLastPlayerConsequenceName();
    [[nodiscard]] std::string GetLastPlayerConsequenceQuest();
    [[nodiscard]] float GetLastPlayerAutoRecoveryDelaySeconds();
    [[nodiscard]] bool IsLastPlayerRecoveryHeld();
    [[nodiscard]] DefeatCause GetLastPlayerDefeatCause();

    [[nodiscard]] std::vector<RE::Actor*> GetLastPlayerRecentHostileAttackers();
}
