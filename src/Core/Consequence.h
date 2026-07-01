#pragma once

#include "pch.h"

#include "Core/Conditions.h"

namespace OSFDefeat::Consequence
{
    struct PlayerOutcome
    {
        std::string name{ "Built-in down then recover" };
        int priority{ 0 };
        int weight{ 1 };
        float recoverySeconds{ 8.0f };
        std::optional<std::string> quest;
        bool startQuest{ true };     // dispatch Quest.Start on the resolved `quest` token
        bool holdRecovery{ false };  // no auto-recovery timer; content owns RescueActor
        Conditions::OutcomeConditions conditions;
    };

    void Load();
    void Reload();

    // Selection: outcomes whose conditions fail for this victim/aggressor pair are
    // filtered out first; among the survivors the highest priority tier wins and
    // weights break ties randomly. Falls back to the built-in down-then-recover
    // outcome when nothing is eligible. Game thread (condition evaluation reads
    // engine state).
    [[nodiscard]] PlayerOutcome SelectPlayerOutcome(RE::Actor* a_victim, RE::Actor* a_aggressor);
    [[nodiscard]] float DefaultRecoverySeconds();
}