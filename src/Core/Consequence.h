#pragma once

#include "pch.h"

#include "Core/Conditions.h"
#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Consequence
{
    // Defeat domains. "onfoot" is the only live domain; "ship" is schema-reserved for the ship
    // defeat milestone — authoring it today parses cleanly but the outcome never competes, so v2
    // content packs stay forward-compatible when the ship domain lands.
    enum class Domain
    {
        kOnFoot,
        kShip
    };

    struct PlayerOutcome
    {
        std::string name{ "Built-in down then recover" };
        int priority{ 0 };
        int weight{ 1 };
        float recoverySeconds{ 8.0f };
        std::optional<std::string> quest;
        bool startQuest{ true };       // dispatch Quest.Start on the resolved `quest` token
        bool holdRecovery{ false };    // no auto-recovery timer; content owns RescueActor
        float maxHoldSeconds{ 0.0f };  // watchdog deadline for holdRecovery; <= 0 = config watchdogSeconds
        bool enabled{ true };          // authoring kill-switch; also user-overridable via Overrides.json
        float cooldownSeconds{ 0.0f };  // real seconds between firings (session-scoped); <= 0 = none
        bool oncePerLocation{ false };  // at most once per cell per session (parentCell proxy; GetCurrentLocation is RE-blocked)
        Domain domain{ Domain::kOnFoot };
        Conditions::OutcomeConditions conditions;
    };

    void Load();
    void Reload();

    // Selection: outcomes that are disabled, off-domain, cooling down, spent for this
    // cell, or whose conditions fail for this victim/aggressor pair are filtered out
    // first (each filtered outcome logs its reason); among the survivors the highest
    // priority tier wins and weights break ties randomly. Falls back to the built-in
    // down-then-recover outcome when nothing is eligible. Game thread (condition
    // evaluation and the cell read touch engine state).
    [[nodiscard]] PlayerOutcome SelectPlayerOutcome(RE::Actor* a_victim, RE::Actor* a_aggressor);

    // Arbitration bookkeeping: call once when the selected outcome actually fires.
    // Stamps the cooldown clock and the oncePerLocation cell. Game thread.
    void MarkFired(const PlayerOutcome& a_outcome, RE::Actor* a_victim);

    [[nodiscard]] float DefaultRecoverySeconds();
}
