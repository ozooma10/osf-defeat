#pragma once

#include "pch.h"

#include "Core/Conditions.h"
#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Consequence
{
    // Defeat domains. "onfoot" is the only live domain; "ship" is schema-reserved for the ship
    // defeat milestone — authoring it today parses cleanly but the consequence never competes, so
    // legacy and v3 content packs stay forward-compatible when the ship domain lands.
    enum class Domain
    {
        kOnFoot,
        kShip
    };

    enum class RecoveryMode
    {
        kAutomatic,
        kHeld
    };

    struct PlayerConsequence
    {
        std::string id{ "osfdefeat.builtin.down_then_recover" };
        std::string displayName{ "Down Then Recover" };
        int priority{ 0 };
        int weight{ 1 };
        float autoRecoveryDelaySeconds{ 8.0f };
        std::optional<std::string> quest;
        bool startQuest{ true };       // dispatch Quest.Start on the resolved `quest` token
        RecoveryMode recoveryMode{ RecoveryMode::kAutomatic };
        float leaseTimeoutSeconds{ 0.0f };  // watchdog deadline for held recovery; <= 0 = config watchdogSeconds
        bool enabled{ true };          // authoring kill-switch; also user-overridable via Overrides.json
        float cooldownSeconds{ 0.0f };  // real seconds between firings (session-scoped); <= 0 = none
        bool oncePerLocation{ false };  // at most once per cell per session (parentCell proxy; GetCurrentLocation is RE-blocked)
        Domain domain{ Domain::kOnFoot };
        Conditions::ConsequenceConditions conditions;

        [[nodiscard]] bool HoldsRecovery() const { return recoveryMode == RecoveryMode::kHeld; }
    };

    void Load();
    void Reload();

    // Selection: consequences that are disabled, off-domain, cooling down, spent for this
    // cell, or whose conditions fail for this victim/aggressor/cause are filtered out
    // first (each filtered consequence logs its reason); among the survivors the highest
    // priority tier wins and weights break ties randomly. Falls back to the built-in
    // down-then-recover consequence when nothing is eligible. Game thread (condition
    // evaluation and the cell read touch engine state).
    [[nodiscard]] PlayerConsequence SelectPlayerConsequence(RE::Actor* a_victim, RE::Actor* a_aggressor,
        DefeatCause a_cause = DefeatCause::kNone);

    // Arbitration bookkeeping: call once when the selected consequence actually fires.
    // Stamps the cooldown clock and the oncePerLocation cell. Game thread.
    void MarkFired(const PlayerConsequence& a_consequence, RE::Actor* a_victim);
    void ResetRuntimeState();
}
